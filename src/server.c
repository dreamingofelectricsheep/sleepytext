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

#include "http_server.c"

sqlite3 *db;
sqlite3_stmt *selectpassword;
sqlite3_stmt *selectsession;
sqlite3_stmt *updatepassword;
sqlite3_stmt *updatelogin;
sqlite3_stmt *updatelastseen;
sqlite3_stmt *insertuser;
sqlite3_stmt *selectuserid;

sqlite3_stmt *insertbranch;
sqlite3_stmt *selectdocument;

int lastblob = 0;

struct insert {
	uint32_t pos;
	uint32_t lastlen;
	uint32_t newlen;
};

#define http_callback_fun(id) struct http_ondata_fn_result \
	http_callback_ ## id(struct http_request * request)

struct http_ondata_fn_result http_quick_result(int code)
{
	return (struct http_ondata_fn_result) {
		.header = (bytes) { 0, 0 }, 
		.error = 0,
		.code = code,
		.payload = (bytes) { 0, 0 }};
}


http_callback_fun(root)
{
	int f = open("html/main.html", 0);
	bytes page = balloc(1 << 20);
	page.length = read(f, page.as_void, page.length);

	struct http_ondata_fn_result result = {
		.error = 0,
		.code = http_ok,
		.payload = page
	};

	return result;
}

http_callback_fun(branch)
{
	int blobid = btoi(bslice(request->addr, Bs("/api/0/branch/").len, 0));

	bytes path = balloc(256);
	snprintf(path.as_void, path.len, "branches/%d", blobid);

	int f = open(path.as_char, O_RDONLY);
	bytes page = balloc(1 << 20);
	page.length = read(f, page.as_void, page.length);

	struct http_ondata_fn_result result = http_quick_result(http_ok);
	result.payload = page;

	return result;
}

http_callback_fun(login)
{
	bfound f = bfind(request->payload, Bs("\n"));

	bytes user = f.before;
	bytes password = f.after;

	sqlite3_reset(selectpassword);
	sqlite3_bind_text(selectpassword, 1, user.as_void, user.len, 0);

	if (sqlite3_step(selectpassword) != SQLITE_ROW) {
		return http_quick_result(http_bad_request);
	}

	bytes dbpassword = {
		.len = sqlite3_column_bytes(selectpassword, 0),
		.as_cvoid = sqlite3_column_blob(selectpassword, 0)
	};

	uint64_t session = sqlite3_column_int64(selectpassword, 1);

	if (bsame(password, dbpassword) == 0) {
		sqlite3_reset(updatelastseen);
		sqlite3_bind_int(updatelastseen, 1, session);
		sqlite3_step(updatelastseen);

		struct http_ondata_fn_result result = http_quick_result(http_ok);
		bytes text = balloc(64);
		text.len = snprintf(text.as_void, text.len, 
			"Set-Cookie: %ld; secure\r\n\r\n", session);
		result.header = text;

		return result;
	} else {
		return http_quick_result(http_bad_request);
	}
}

http_callback_fun(user)
{
	bfound f = bfind(request->payload, Bs("\n"));

	bytes user = f.before;
	bytes password = f.after;

	sqlite3_reset(insertuser);
	sqlite3_bind_text(insertuser, 1, user.as_void, user.len, 0);
	sqlite3_bind_blob(insertuser, 2, password.as_void, password.len, 0);

	if (sqlite3_step(insertuser) != SQLITE_DONE) {
		return http_quick_result(http_bad_request);
	} else {
		return http_quick_result(http_ok);
	}
}

http_callback_fun(void)
{
	return http_quick_result(http_ok);
}

http_callback_fun(newbranch)
{
/*	bytes bsession = http_extract_param(request->header, Bs("Cookie"));
	int session = itob(bsession);

	sqlite3_reset(selectuserid);
	sqlite3_bind_int(selectuserid, 1, session);

	if(sqlite3_step(selectuserid) != SQLITE_ROW)
		return http_quick_result(http_bad_request);

	

	sqlite3_reset(insertbranch);

	
	int user = 1;
	if (user == 0)

	sqlite3_bind_int(insertbranch, 1, user);
	sqlite3_bind_int(insertbranch, 2, 0);

	lastblob++;
	sqlite3_bind_int(insertbranch, 3, lastblob);

	bfound f = bfind(request->payload, Bs("\n"));
	int parent = 0, pos = 0;

	if (f.found.length != 0) {
		parent = btoi(f.before);
		pos = btoi(f.after);
	}

	sqlite3_bind_int(insertbranch, 4, parent);
	sqlite3_bind_int(insertbranch, 5, pos);

	if (sqlite3_step(insertbranch) != SQLITE_DONE)
		return http_quick_result(http_bad_request);
	else {
		bytes id = balloc(256);
		bytes formatted = itob(id, sqlite3_last_insert_rowid(db));

		struct http_ondata_fn_result result = {
			.error = 0,
			.code = http_ok,
			.payload = formatted
		};
		return result;
	}
*/	
	return http_quick_result(http_bad_request);
}

http_callback_fun(feed)
{
/*
	int user = 1;
	int socket = 1;
	sqlite3_reset(selectlock);
	sqlite3_bind_int(selectlock, 1, user);
	if (sqlite3_step(selectlock) != SQLITE_ROW)
		return http_quick_result(http_bad_request);

	int lock = sqlite3_column_int(selectlock, 0);
	debug("Locked to %d, trying %d", lock, socket);
	if (lock != socket)
		return http_quick_result(http_forbidden);

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
				return http_quick_result(400);
			}

			p = bslice(p, 5, 0);
			commit = (bytes) { .len = 0, .as_void = p.as_void };
		}
		else if(p.as_char[0] == 'i') {
			if(http->file == -1) { debug("No file."); return http_bad_request; }
			if(p.len < 13) {
				debug("Incomplete data structure.");
				return http_quick_result(400);
			}
			
			struct insert * i = (void *) (p.as_char + 1);
			i->lastlen = ntohl(i->lastlen);
			i->newlen = ntohl(i->newlen);

			size_t len = 13 + i->lastlen + i->newlen;

			if(p.len < len) {
				debug("Change lies."); 
				return http_quick_result(400);
			}

			commit.len += len;
			p = bslice(p, len, 0);
		}
		else {
			debug("Unknown opcode: %d", p.as_char[0]);
			return http_quick_result(400);
		}
	}
	write(http->file, commit.as_void, commit.len);*/
	return http_quick_result(http_ok);
}

void stop()
{
	sqlite3_interrupt(db);

	sqlite3_stmt *s = 0;

	do {
		sqlite3_finalize(s);
		s = sqlite3_next_stmt(db, s);
	} while (s != 0);

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

	DIR *d = opendir("./branches/");

	if (d == 0)
		debug("Could not open branches directory: %s", strerror(errno));

	struct dirent *ent = readdir(d);
	while (ent != 0) {
		if (ent->d_type == DT_REG)
			lastblob++;
		ent = readdir(d);
	}

#define ps(s, v) \
	if(sqlite3_prepare_v2(db,\
		s, -1, &v, 0) != SQLITE_OK) {\
		debug(s); \
		goto cleanup; }

	ps("select password, session from users where login = ?", selectpassword)
	ps("select session from users where id = ?", selectsession)
	ps("select id from users where session = ?", selectuserid)
	ps("update users set password = ? where session = ?", updatepassword)
	ps("update users set login = ? where session = ?", updatelogin)
	ps("update users set lastseen = datetime('now') where session = ?",
	   updatelastseen)
	ps("insert into users(login, password, created, lastseen, session) "
	   "values(?, ?, datetime('now'), datetime('now'), random())", insertuser)

	ps("insert into branches(userid, docid, blobid, parentid, pos) "
	   "values(?, ?, ?, ?, ?)", insertbranch);
	ps("select id, blobid, parentid, pos from branches where "
	   "docid = ?", selectdocument);

#define q(fun, path) { http_callback_ ## fun, Bs(path) }

	struct http_callback_pair pairs[] = {
		q(root, "/"),
		q(newbranch, "/api/0/newbranch"),
		q(branch, "/api/0/branch/"),
		q(feed, "/api/0/feed"),
		q(user, "/api/0/user"),
		q(login, "/api/0/login"),
		q(void, "")
	};
#undef q

	struct http_server *http = http_init();

	int i = 0;
	while (pairs[i].addr.len > 0) {
		http_server_add_callback(http, pairs[i]);
		i++;
	}

	int tls = setup_socket(8080, tls_listener_ondata,
			       tls_listener_onclose, http);

	epoll_listen();

 cleanup:

	stop();

	return 0;
}
