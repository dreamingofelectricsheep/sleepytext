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
sqlite3_stmt * selectlock;
sqlite3_stmt * updatelock;

sqlite3_stmt * insertbranch;
sqlite3_stmt * selectdocument;

int lastblob = 0;


struct insert {
	uint32_t pos;
	uint32_t lastlen;
	uint32_t newlen;
};

typedef int (*http_handler_callback)(struct http_connection *http, struct http_request
	*request);

struct http_callback_pair {
	http_handler_callback fun;
	bytes addr;
};

struct http_callback_pair * callbacks;
size_t callbacks_len = 0;

int http_ondata_callback(struct http_connection *http, struct http_request * request) {
	for(int i = 0; i < callbacks_len; i++) {
		if(bsame(request->addr, callbacks[i].addr) == 0)
			return callbacks[i].fun(http, request);

		if(callbacks[i].addr.len <= 1) continue;

		bytes slice = bslice(request->addr, 0, callbacks[i].addr.len);

		
		if(bsame(slice, callbacks[i].addr) == 0)
			return callbacks[i].fun(http, request);
	}

	return http_not_found;
}

#define http_callback_fun(id) int http_callback_ ## id(struct http_connection *http, \
	struct http_request * request)

http_callback_fun(root) {
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


http_callback_fun(branch) {

	int blobid = btoi(bslice(request->addr, Bs("/api/0/branch/").len, 0));

	bytes path = balloc(256);
	snprintf(path.as_void, path.len, "branches/%d", blobid);
	
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

http_callback_fun(stop) {
	epoll_stop = 1;
	return http_ok;
}

int update_lock(int user, int lock) {
	sqlite3_reset(updatelock);
	sqlite3_bind_int(updatelock, 1, lock);
	sqlite3_bind_int(updatelock, 2, user);

	return sqlite3_step(updatelock);
}

http_callback_fun(login) {
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
		sqlite3_bind_int(updatelastseen, 1, http->user);
		sqlite3_step(updatelastseen);

		if(update_lock(http->user, http->socket) != SQLITE_DONE)
			debug("Could not lock the user.");

	
		return http_ok;
	}
	else {
		return http_bad_request;
	}
} 

	

http_callback_fun(user) {
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
		update_lock(http->user, http->socket);
		return http_ok;
	}
}

http_callback_fun(lock) {
	debug("Locking user: %d", http->user);
	if(http->user == 0) return http_bad_request;

	if(update_lock(http->user, http->socket) != SQLITE_DONE)
		return http_bad_request;
	else return http_ok;

}

http_callback_fun(newbranch) {
	sqlite3_reset(insertbranch);

	if(http->user == 0) return http_bad_request;

	sqlite3_bind_int(insertbranch, 1, http->user);				
	sqlite3_bind_int(insertbranch, 2, 0);
	
	lastblob++;				
	sqlite3_bind_int(insertbranch, 3, lastblob);				

	bfound f = bfind(request->payload, Bs("\n"));
	int parent = 0, pos = 0;

	if(f.found.length != 0) {
		parent = btoi(f.before);
		pos = btoi(f.after);
	}

	sqlite3_bind_int(insertbranch, 4, parent);
	sqlite3_bind_int(insertbranch, 5, pos);


	if(sqlite3_step(insertbranch) != SQLITE_DONE)
		return http_bad_request;
	else {
		bytes id = balloc(256);
		bytes formatted = itob(id, sqlite3_last_insert_rowid(db));

		bytes resp = balloc(1024);
		resp.len = snprintf(resp.as_char, 1024, "HTTP/1.1 200 Ok\r\n"
			"Content-Length: %zd\r\n\r\n", formatted.len);

		tls_writeb2(http->tls, resp, formatted);
		bfree(&id);
		bfree(&resp);
		return http_complete;
	}
}

http_callback_fun(feed) {
	sqlite3_reset(selectlock);
	sqlite3_bind_int(selectlock, 1, http->user);
	if(sqlite3_step(selectlock) != SQLITE_ROW)
		return http_bad_request;

	int lock = sqlite3_column_int(selectlock, 0);
	debug("Locked to %d, trying %d", lock, http->socket);
	if(lock != http->socket)
		return http_forbidden;


	bytes p = request->payload;
	bytes commit = { .len = 0, .as_void = p.as_void };
	while(p.len > 0) {
		if(p.as_char[0] == 'b') {
			write(http->file, commit.as_void, commit.len);

			if(http->file != -1) { close(http->file); http->file = -1; }
			bytes bid = bslice(p, 1, 5);
			if(bid.len != 4) { debug("Needs 4 bytes."); return http_bad_request; }

			uint32_t id = ntohl(*((uint32_t*)bid.as_void));

			bytes path = balloc(256);
			snprintf(path.as_void, path.len, "./branches/%d", id);
			http->file = open(path.as_char, O_APPEND | O_CREAT | O_RDWR);
			bfree(&path);

			if(http->file == -1) { 
				debug("Could not open a file: %s",strerror(errno));
				return http_bad_request;
			}

			p = bslice(p, 5, 0);
			commit = (bytes) { .len = 0, .as_void = p.as_void };
		}
		else if(p.as_char[0] == 'i') {
			if(http->file == -1) { debug("No file."); return http_bad_request; }
			if(p.len < 13) {
				debug("Incomplete data structure.");
				return http_bad_request;
			}
			
			struct insert * i = (void *) (p.as_char + 1);
			i->lastlen = ntohl(i->lastlen);
			i->newlen = ntohl(i->newlen);

			size_t len = 13 + i->lastlen + i->newlen;

			if(p.len < len) {
				debug("Change lies."); 
				return http_bad_request;
			}

			commit.len += len;
			p = bslice(p, len, 0);
		}
		else {
			debug("Unknown opcode: %d", p.as_char[0]);
			return http_bad_request;
		}
	}
	write(http->file, commit.as_void, commit.len);
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
	ps("select lock from users where id = ?", selectlock)
	ps("update users set password = ? where id = ?", updatepassword)
	ps("update users set login = ? where id = ?", updatelogin)
	ps("update users set lock = ? where id = ?", updatelock)
	ps("update users set lastseen = datetime('now') where id = ?", updatelastseen)
	ps("insert into users(login, password, created, lastseen) "
		"values(?, ?, datetime('now'), datetime('now'))", insertuser)

	ps("insert into branches(userid, docid, blobid, parentid, pos) "
		"values(?, ?, ?, ?, ?)", insertbranch);
	ps("select id, blobid, parentid, pos from branches where "
		"docid = ?", selectdocument);
#define qp(fun, path) (struct http_callback_pair) { http_callback_ ## fun, \
	Bs(path) }
	

	struct http_callback_pair pairs[] = {
		qp(root, "/"),
		qp(newbranch, "/api/0/newbranch"),
		qp(branch, "/api/0/branch/"),
		qp(feed, "/api/0/feed"),
		qp(lock, "/api/0/lock"),
		qp(user, "/api/0/user"),
		qp(login, "/api/0/login"),
		qp(stop, "/api/0/stop")

	};

	callbacks_len = 8;

	callbacks = pairs;	


	http_init();
		
	int http = setup_socket(8080, httplistener_ondata,
				httplistener_onclose);

	
	epoll_listen();

cleanup:

	stop();

	return 0;
}
