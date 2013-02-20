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
#include <sys/types.h>

#include "bytes.c"
#include "epoll.c"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>

struct http_connection;

typedef void (*http_fn) (struct http_connection *);

struct http_connection {
	int socket;
	http_fn ondata;
	http_fn onclose;
	bytes request;
	int user;
	int file;
	struct in6_addr ip;
	time_t last;
	SSL * tls;
};


enum http_result {
	http_complete,
	http_ok,
	http_bad_request,
	http_not_found,
	http_forbidden
};

enum http_request_type {
	http_unknown,
	http_get,
	http_post,
	http_put,
	http_head
};

struct http_request {
	bytes header;
	bytes payload;
	bytes addr;
	int type;
};
	
int http_ondata_callback(struct http_connection *http, struct http_request * request);

SSL_CTX * tls = 0;

int setup_socket(uint16_t port, void *ondata, void *onclose)
{
	int sock = socket(AF_INET6, SOCK_STREAM, 0);

	int r = true;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &r, sizeof(r));
	setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &r, sizeof(r));

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


void http_onclose(struct http_connection *http)
{
	debug("Closing http connection: %d", http->socket);
	debug("Last error: %s", strerror(errno));
	bfree(&http->request);
	close(http->socket);
	SSL_free(http->tls);
	free(http);
}

const size_t tls_frame_len = 16 * 1024;
int tls_request_frames = 10;



int tls_writeb(SSL * tls, bytes b) {
	return SSL_write(tls, b.as_void, b.len);
}

int tls_writeb2(SSL * tls, bytes b, bytes b2) {
	SSL_write(tls, b.as_void, b.len);
	return SSL_write(tls, b2.as_void, b2.len);
}

void http_ondata(struct http_connection *http)
{
	http->last = epoll_time;
	bool data_available = true;

	while(data_available) {
		// If all space is filled, we increase it by one page.
		size_t space_available = http->request.len % tls_frame_len;
		space_available = tls_frame_len- space_available;

		if(space_available == tls_frame_len) {
			http->request.as_void = realloc(http->request.as_void, 
				http->request.len + tls_frame_len);
			space_available = tls_frame_len;
		}


		int len = SSL_read(http->tls,
			http->request.as_void + http->request.len,
			space_available);


		if (len == 0) {
			ERR_print_errors_fp(stderr);
			http->onclose(http);
			return;
		}

		if (len < 0) {
		//	if (len == EAGAIN || len == EWOULDBLOCK) {
		//		data_available = false;
		//	}
		//	else {
				
				ERR_print_errors_fp(stderr);
				http->onclose(http);
				return;
		//	}
		}
	
		http->request.len += len;
		
		if(len != tls_frame_len) {
			data_available = false;
		}
	}

	write(0, http->request.as_void, http->request.len);

	bfound f = bfind(http->request, Bs("\r\n\r\n"));

	if (f.found.len == 0) {
		debug("Partial http header.");
		return;
	}

	struct http_request request = {
		.header = f.before,
		.payload = f.after,
		.type = http_unknown
	};

	f = bfind(request.header, Bs("Content-Length: "));

	if(f.found.len > 0) {
		f = bfind(f.after, Bs("\r\n"));

		int contentlen = btoi(f.before);

		if (contentlen <= 0)
			goto bad_request;
	
		if (contentlen != request.payload.len) {
			debug("Partial body received. %lu out of %d bytes available.", 
				request.payload.len, contentlen);
			return;
		}
	}

	if(bsame(bslice(request.header, 0, 3), Bs("GET")) == 0)
		request.type = http_get;
	else if(bsame(bslice(request.header, 0, 3), Bs("PUT")) == 0)
		request.type = http_put;
	else if(bsame(bslice(request.header, 0, 4), Bs("POST")) == 0)
		request.type = http_post;
	else if(bsame(bslice(request.header, 0, 4), Bs("HEAD")) == 0)
		request.type = http_head;
		

	f = bfind(request.header, Bs(" "));
	f = bfind(f.after, Bs(" "));

	request.addr = f.before;

	int result = http_ondata_callback(http, &request);

	switch(result) {
	case http_not_found:
not_found:
		tls_writeb(http->tls, Bs("HTTP/1.1 404 Not found\r\n"
			"Content-Length: 0\r\n\r\n"));
		break;

	case http_ok:
		tls_writeb(http->tls, Bs("HTTP/1.1 200 Ok\r\n"
			"Content-Length: 0\r\n\r\n"));
		break;

	case http_bad_request:
bad_request:
		debug("Bad request");
		tls_writeb(http->tls, Bs("HTTP/1.1 400 Bad Request\r\n\r\n"));
		http->onclose(http);
		break;
	
	case http_forbidden:
		tls_writeb(http->tls, Bs("HTTP/1.1 403 Forbidden\r\n"
			"Content-Length: 0\r\n\r\n"));
	case http_complete:
		break;
	default:
		goto not_found;
		break;
	};
	
	bfree(&http->request);
}

bytes http_extract_param(bytes header, bytes field) {	
	bfound f = bfind(header, field);
	f = bfind(f.after, Bs("\r\n"));
	return f.before;
}

int http_websocket_accept(struct http_connection *http, struct http_request * request) {
	int version = btoi(http_extract_param(request->header, Bs("Sec-WebSocket-Version")));

	if(version != 13) return http_bad_request;

	bytes key = http_extract_param(request->header, Bs("Sec-WebSocket-Key"));
	bytes rfc6455 = Bs("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

	SHA_CTX sha;
	SHA1_Init(&sha);

	SHA1_Update(&sha, key.as_void, key.len);
	SHA1_Update(&sha, rfc6455.as_void, rfc6455.len);

	unsigned char digest[20];
	SHA1_Final(digest, &sha);
	


	return http_complete;
}

void tls_ondata(struct http_connection *http) {
	debug("SSL_accept on %d", http->socket);
	http->ondata = http_ondata;
	
				ERR_print_errors_fp(stderr);
	debug("done accepting");
}

void httplistener_ondata(struct generic_epoll_object *data)
{
	int socket = data->fd;
	struct sockaddr_in6 addr;
	socklen_t len = sizeof(addr);
	int accepted = accept(socket, (void*) &addr, &len);

	debug("Accepting a new connection: %d", accepted);
	struct http_connection *c = malloc(sizeof(*c));
	c->user = 0;
	c->file = -1;
	c->socket = accepted;
	c->ondata = http_ondata;
	c->onclose = http_onclose;
	c->request.len = 0;
	c->request.as_void = 0;
	memcpy(&c->ip, &addr.sin6_addr, sizeof(addr.sin6_addr));

	c->tls = SSL_new(tls);
	SSL_set_fd(c->tls, c->socket);
	SSL_set_accept_state(c->tls);

	epoll_add(accepted, c);
}

void httplistener_onclose(struct generic_epoll_object *data)
{
	debug("Closing listener socket. %d", data->fd);
	close(data->fd);
	free(data);
};

void http_init() {
	SSL_library_init();
	ERR_load_crypto_strings();
	SSL_load_error_strings();
	tls = SSL_CTX_new(TLSv1_server_method());
	if(SSL_CTX_use_certificate_file(tls, "./cert.pem", SSL_FILETYPE_PEM) != 1) {
		debug("Error loading the certificate.");
	}

	if(SSL_CTX_use_PrivateKey_file(tls, "./cert.pem", SSL_FILETYPE_PEM) != 1) {
		debug("Could not load private key.");
	}

	if(SSL_CTX_set_cipher_list(tls, "DEFAULT") != 1) {
		debug("Adding ciphers failed.");
	}

}

		

