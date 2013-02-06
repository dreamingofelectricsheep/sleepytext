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

#include "sqlite3.h"


#include "bytes.c"
#include "epoll.c"


sqlite3 * db;
sqlite3_stmt * selectpassword;
sqlite3_stmt * updatepassword;
sqlite3_stmt * updatelogin;
sqlite3_stmt * updatelastseen;
sqlite3_stmt * insertuser;

int setup_socket(uint16_t port, void *ondata, void *onclose)
{
	int sock = socket(AF_INET6, SOCK_STREAM, 0);

	int r = true;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &r, sizeof(r));

	struct sockaddr_in6 def = {
		AF_INET6,
		htons(port),
		0, 0, 0
	};

	if (bind(sock, (void *)&def, sizeof(def))) {
		debug("Bind failed!");
		return -1;
	}

	listen(sock, 1024);

	struct generic_epoll_object *object = malloc(sizeof(*object));

	object->fd = sock;
	object->ondata = ondata;
	object->onclose = onclose;

	epoll_add(sock, object);

	return sock;
}

struct http_connection;

typedef void (*http_fn) (struct http_connection *);

struct http_connection {
	int socket;
	http_fn ondata;
	http_fn onclose;
	bytes buffer;
	int user;
	struct in6_addr ip;
};

void http_onclose(struct http_connection *http)
{
	debug("Closing http connection: %d", http->socket);
	free(http->buffer.as_void);
	close(http->socket);
	free(http);
}

void http_ondata(struct http_connection *http)
{
	ssize_t len = recv(http->socket,
			   http->buffer.as_void + http->buffer.len,
			   4096 - http->buffer.len, 0);

	if (len == 0) {
		http->onclose(http);
	} else if (len < 0) {
		debug("Read error! %s", strerror(errno));
		http->onclose(http);
	} else {
		http->buffer.len += len;
		write(0, http->buffer.as_void, http->buffer.len);

		bfound f = bfind(http->buffer, Bs("\r\n\r\n"));

		if (f.found.len == 0) {
			debug("Partial http header.");
			return;
		}

		bytes header = f.before;
		bytes body = f.after;

		if (http->buffer.as_char[0] == 'G') {
			f = bfind(header, Bs(" "));
			f = bfind(f.after, Bs(" "));

			bytes resource = f.before;

			if(bsame(resource, Bs("/")) == 0) {
				int f = open("html/main.html", 0);
				bytes page = balloc(1 << 20);
				page.length = read(f, page.as_void, page.length);

				bytes resp = balloc(4096);
				resp.len = snprintf(resp.as_void, resp.len,
					"HTTP/1.1 200 Ok\r\n"
					"Content-Length: %zd\r\n\r\n", page.len);

				send(http->socket, resp.as_void, resp.len, 0);
				send(http->socket, page.as_void, page.len, 0);

			}
			if(bsame(resource, Bs("/graph.js")) == 0) {
				int f = open("html/graph.js", 0);
				bytes page = balloc(1 << 20);
				page.length = read(f, page.as_void, page.length);

				bytes resp = balloc(4096);
				resp.len = snprintf(resp.as_void, resp.len,
					"HTTP/1.1 200 Ok\r\n"
					"Content-Length: %zd\r\n\r\n", page.len);

				send(http->socket, resp.as_void, resp.len, 0);
				send(http->socket, page.as_void, page.len, 0);

			}
			if(bsame(resource, Bs("/stop")) == 0) {
				epoll_stop = 1;
			}
		else {
				bytes resp = Bs("HTTP/1.1 404 Not found\r\n"
					"Content-Length: 0\r\n\r\n");
				send(http->socket, resp.as_void, resp.len, 0);
			}
			
			goto complete_request;

		} else if (http->buffer.as_char[0] == 'P') {
			f = bfind(header, Bs(" "));
			f = bfind(f.after, Bs(" "));
			bytes addr = f.before;

			if (addr.len == 0)
				goto bad_request;

			f = bfind(header, Bs("Content-Length: "));

			if (f.found.len == 0)
				goto bad_request;

			f = bfind(f.after, Bs("\r\n"));

			int contentlen = btoi(f.before);

			if (contentlen <= 0)
				goto bad_request;

			if (body.len < contentlen) {
				debug("Partial body. %zd out of %zd received.",
				      body.len, contentlen);
				return;
			}	

			bytes ack = Bs("HTTP/1.1 200 Ok\r\n"
				"Content-Length: 0\r\n\r\n");



			if(bsame(addr, Bs("/login")) == 0) {
				f = bfind(body, Bs("\n"));

				bytes user = f.before;
				bytes password = f.after;

				sqlite3_reset(selectpassword);
				sqlite3_bind_text(selectpassword, 1, user.as_void, user.len, 0);

				if(sqlite3_step(selectpassword) != SQLITE_ROW) {
					goto bad_request;
				}

				bytes dbpassword = {
					.len = sqlite3_column_bytes(selectpassword, 0),
					.as_cvoid = sqlite3_column_blob(selectpassword, 0) };

				uint64_t id = sqlite3_column_int64(selectpassword, 1);


				if(bsame(password, dbpassword) == 0) {
					http->user = id;
					send(http->socket, ack.as_void, ack.len, 0);

					goto complete_request;
				}
				else {
					goto bad_request;
				}
				

			} else if(bsame(addr, Bs("/user")) == 0) {
				f = bfind(body, Bs("\n"));

				bytes user = f.before;
				bytes password = f.after;
		
				sqlite3_reset(insertuser);
				sqlite3_bind_text(insertuser, 1, user.as_void, user.len, 0);
				sqlite3_bind_blob(insertuser, 2, password.as_void, password.len, 0);

				if(sqlite3_step(insertuser) != SQLITE_DONE) {
					goto bad_request;
				}
				else {
					http->user = sqlite3_last_insert_rowid(db);
					send(http->socket, ack.as_void, ack.len, 0);
					goto complete_request;
				}
			}
				
				

				
			send(http->socket, ack.as_void, ack.len, 0);




			goto complete_request;
		} else {
			goto bad_request;
		}
	}
	return;

 bad_request:
	debug("Bad request");

	bytes resp = Bs("HTTP/1.1 400 Bad Request\r\n\r\n");
	send(http->socket, resp.as_void, resp.len, 0);
	http->onclose(http);
	goto complete_request;

 complete_request:
	http->buffer.len = 0;
}

void httplistener_ondata(struct generic_epoll_object *data)
{
	int socket = data->fd;
	struct sockaddr_in6 addr;
	int accepted = accept(socket, &addr, sizeof(addr));

	debug("Accepting a new connection: %d", accepted);
	struct http_connection *c = malloc(sizeof(*c));
	c->user = 0;
	c->socket = accepted;
	c->ondata = http_ondata;
	c->onclose = http_onclose;
	c->buffer.len = 0;
	c->buffer.as_void = malloc(4096);
	memcpy(c->ip, addr.sin6_addr, sizeof(addr.sin6_addr);

	epoll_add(accepted, c);
}

void httplistener_onclose(struct generic_epoll_object *data)
{
	debug("Closing listener socket. %d", data->fd);
	close(data->fd);
	free(data);
};

void stop() {
}

int main(int argc, char **argv)
{
	signal(SIGABRT, &stop);
	signal(SIGHUP, &stop);
	signal(SIGINT, &stop);
	signal(SIGTERM, &stop);

	epoll = epoll_create(1);
	sqlite3_open("./db", &db);


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

		
	int http = setup_socket(8080, httplistener_ondata,
				httplistener_onclose);

	
	epoll_listen();

cleanup:

	sqlite3_finalize(selectpassword);
	sqlite3_finalize(updatepassword);
	sqlite3_finalize(updatelogin);
	sqlite3_finalize(updatelastseen);
	sqlite3_finalize(insertuser);

	sqlite3_close(db);
	return 0;
}
