#include <stdint.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#define _GNU_SOURCE
#include <dirent.h>

#include "sqlite3.h"

#include "http.c"

sqlite3 * db;
sqlite3_stmt * selectpassword;
sqlite3_stmt * updatepassword;
sqlite3_stmt * updatelogin;
sqlite3_stmt * updatelastseen;
sqlite3_stmt * insertuser;

sqlite3_stmt * insertbranch;
sqlite3_stmt * selectdocument;

int lastblob = 0;


int http_ondata_callback(struct http_connection *http, struct http_request * request) {
		if (request->type ==  http_get) {
			if(bsame(request->addr, Bs("/")) == 0) {
				int f = open("html/main.html", 0);
				bytes page = balloc(1 << 20);
				page.length = read(f, page.as_void, page.length);

				bytes resp = balloc(4096);
				resp.len = snprintf(resp.as_void, resp.len,
					"HTTP/1.1 200 Ok\r\n"
					"Content-Length: %zd\r\n\r\n", page.len);

				tls_writeb2(http->tls, resp, page);
				return http_complete;
			}
			if(bsame(request->addr, Bs("/graph.js")) == 0) {
				int f = open("html/graph.js", 0);
				bytes page = balloc(1 << 20);
				page.length = read(f, page.as_void, page.length);

				bytes resp = balloc(4096);
				resp.len = snprintf(resp.as_void, resp.len,
					"HTTP/1.1 200 Ok\r\n"
					"Content-Length: %zd\r\n\r\n", page.len);

				tls_writeb2(http->tls, resp, page);
				return http_complete;
			}
			if(bsame(bslice(request->addr, 0, 8), Bs("/branch/")) == 0) {

				int blobid = btoi(bslice(request->addr, 8, 0));

				bytes path = balloc(256);
				snprintf(path.as_void, path.len, "./branches/%d", blobid);
				
				int f = open(path.as_char, O_RDONLY);
				bytes page = balloc(1 << 20);
				page.length = read(f, page.as_void, page.length);

				bytes resp = balloc(4096);
				resp.len = snprintf(resp.as_void, resp.len,
					"HTTP/1.1 200 Ok\r\n"
					"Content-Length: %zd\r\n\r\n", page.len);

				tls_writeb2(http->tls, resp, page);
				return http_complete;
			}
			if(bsame(request->addr, Bs("/stop")) == 0) {
				epoll_stop = 1;
				return http_ok;
			}

			return http_not_found;
		}

		if (request->type == http_post) {

			if(bsame(request->addr, Bs("/login")) == 0) {
				bfound f = bfind(request->payload, Bs("\n"));

				bytes user = f.before;
				bytes password = f.after;

				sqlite3_reset(selectpassword);
				sqlite3_bind_text(selectpassword, 1, user.as_void, user.len, 0);

				if(sqlite3_step(selectpassword) != SQLITE_ROW) {
					return http_bad_request;
				}

				bytes dbpassword = {
					.len = sqlite3_column_bytes(selectpassword, 0),
					.as_cvoid = sqlite3_column_blob(selectpassword, 0) };

				uint64_t id = sqlite3_column_int64(selectpassword, 1);


				if(bsame(password, dbpassword) == 0) {
					http->user = id;

					sqlite3_reset(updatelastseen);
					sqlite3_step(updatelastseen);

					return http_ok;
				}
				else {
					return http_bad_request;
				}
			} 

			if(bsame(request->addr, Bs("/user")) == 0) {
				bfound f = bfind(request->payload, Bs("\n"));

				bytes user = f.before;
				bytes password = f.after;
		
				sqlite3_reset(insertuser);
				sqlite3_bind_text(insertuser, 1, user.as_void, user.len, 0);
				sqlite3_bind_blob(insertuser, 2, password.as_void, password.len, 0);

				if(sqlite3_step(insertuser) != SQLITE_DONE) {
					return http_bad_request;
				}
				else {
					http->user = sqlite3_last_insert_rowid(db);
					return http_ok;
				}
			}
				
			return http_ok;
		}

		return http_ok;

}

void stop() {
	sqlite3_interrupt(db);

	sqlite3_stmt * s = 0;
	
	do {
		sqlite3_finalize(s);
		s = sqlite3_next_stmt(db, s);
	} while(s != 0);

	sqlite3_close(db);

	exit(0);
}

int main(int argc, char **argv)
{
	signal(SIGABRT, &stop);
	signal(SIGHUP, &stop);
	signal(SIGINT, &stop);
	signal(SIGTERM, &stop);

	epoll = epoll_create(1);
	sqlite3_open("./db", &db);


	DIR * d = opendir("./branches/");
	
	if(d == 0) 
		debug("Could not open branches directory: %s", strerror(errno));

	struct dirent * ent = readdir(d);
	while(ent != 0) {
		if(ent->d_type == DT_REG)
			lastblob++;
		ent = readdir(d);
	}	


#define ps(s, v) \
	if(sqlite3_prepare_v2(db,\
		s, -1, &v, 0) != SQLITE_OK) {\
		debug(s); \
		goto cleanup; }
	
	ps("select password, id from users where login = ?", selectpassword)
	ps("update users set password = ? where id = ?", updatepassword)
	ps("update users set login = ? where id = ?", updatelogin)
	ps("update users set lastseen = date('now') where id = ?", updatelastseen)
	ps("insert into users(login, password, created, lastseen) "
		"values(?, ?, datetime('now'), datetime('now'))", insertuser)

	ps("insert into branches(userid, docid, blobid, parentid, pos) "
		"values(?, ?, ?, ?, ?)", insertbranch);
	ps("select id, blobid, parentid, pos from branches where "
		"docid = ?", selectdocument);


	http_init();
		
	int http = setup_socket(8080, httplistener_ondata,
				httplistener_onclose);

	
	epoll_listen();

cleanup:

	stop();

	return 0;
}
