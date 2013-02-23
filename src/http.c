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

enum http_result {
	http_ok,
	http_bad_request,
	http_not_found,
	http_forbidden
};

struct http_request {
	bytes header;
	bytes payload;
	bytes addr;
};
	

int setup_socket(uint16_t port, void *ondata, void *onclose, void *auxilary)
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
	object->auxilary = auxilary;

	epoll_add(sock, object);

	return sock;
}







struct tcp_connection {
	int socket;
	struct in6_addr ip;
	time_t last;
};

int tcp_ondata(struct tcp_connection *tcp, bytes buffer, bytes * out)
{
	int len = recv(tcp->socket, buffer.as_void, buffer.len, 0);

	debug("Received %d bytes of data from socket %d", len, tcp->socket);

	if (len <= 0) {
		if (len < 0)
			debug("Error reading data from socket %d: %s", tcp->socket, strerror(errno));

		return return -1; 
	}
	
	out->as_void = buffer.as_void;
	out->len = len;
}

void tcp_onclose(struct tcp_connection *tcp)
{
	debug("Closing http connection: %d", tcp->socket);
	close(tcp->socket);
}

void tcp_onsetup(int socket, struct tcp_connection * tcp) {
	struct sockaddr_in6 addr;
	socklen_t len = sizeof(addr);

	int accepted = accept4(socket, (void*) &addr, &len, SOCK_NONBLOCK);

	int r = true;
	setsockopt(accepted, SOL_SOCKET, SO_KEEPALIVE, &r, sizeof(r));

	debug("Accepting a new tcp connection: %d", accepted);
	tcp->socket = accepted;
	tcp->request.len = 0;
	tcp->request.as_void = 0;
	tcp->pagelen = 16 * 4096;
	memcpy(&tcp->.ip, &addr.sin6_addr, sizeof(addr.sin6_addr));
}








struct tls_connection {
	SSL * tls;
};

int tls_ondata(struct tls_connection *tls, bytes buffer, bytes *out, bytes *writeout)
{
	BIO * read = SSL_get_rbio(tls->tls);
	BIO * write = SSL_get_wbio(tls->tls);


	BIO_write(read, buffer.as_void, buffer.len);

	int len = SSL_pending(tls->tls);
	
	debug("%d bytes available in tls.", len);
	if(len > 0) *out = balloc(len);

	len = SSL_read(tls->tls, out->as_void, out->len);

	if(len > out->len) debug("ERROR: too much data to read from TLS.");

	if(len == 0) {
		debug("Peer requested connection closure.");
		return -1;
	}

	if(len < 0) {
		debug("Tls errors.");
		ERR_print_errors_fp(stderr);
		
		return -1;


	}

	return 0;
}

void tls_onclose(struct tls_conncection *tls)
{
	SSL_free(tls->tls);
}

void tls_onsetup(SSL_CTX * ctx, struct tls_connection *tls)
{
	tls->tls = SSL_new(ctx);
	BIO * rbio = BIO_new(BIO_s_mem()),
		wbio = BIO_new(BIO_s_mem());

	SSL_set_bio(tls->tls, BIO_new(BIO_s_mem()), BIO_new(BIO_s_mem()));
	SSL_set_accept_state(tls->tls);
}

	
	

struct http_ondata_fn_result {
	int error;
	int code;
	bytes payload; } http_ondata_callback(struct http_request * request);


struct http_ondata_result {
	bytes array[5];
	size_t len;
	int error;
} http_ondata(bytes buffer)
{
	write(0, buffer->as_void, buffer.len);

	bfound f = bfind(buffer, Bs("\r\n\r\n"));

	if (f.found.len == 0) {
		debug("Partial http header.");

		return (struct http_ondata_result) {
			.len = 0, .error = 0 };
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

	
		if (contentlen != request.payload.len) {
			debug("Partial body received. %lu out of %d bytes available.", 
				request.payload.len, contentlen);

			return (struct http_ondata_result) {
				.len = 0, .error = 0 };
		}
	}

	

	f = bfind(request.header, Bs(" "));
	f = bfind(f.after, Bs(" "));

	request.addr = f.before;

	struct http_ondata_fn_result fnresult = http_ondata_callback(&request);
	if(fnresult.error) goto bad_request;

	struct http_ondata_result result = {
		.len = 0,
		.error = 0 }

	switch(result.code) {
	case http_not_found:
		result.array[0] = Bs("HTTP/1.1 404 Not found\r\n"
			"Content-Length: 0\r\n\r\n");
		result.len = 1;
		break;

	case http_ok:
		bytes r = balloc(256);
		r.len = snprintf(r.as_void, 256, "HTTP/1.1 200 Ok\r\n"
			"Content-Length: %zd\r\n\r\n", fnresult.payload.len);
		
		result.array[0] = r;
		result.array[1] = fnresult.payload;
			
		result.len = 2;
		break;

	case http_bad_request:
		debug("Bad request");
		result.array[0] = Bs("HTTP/1.1 400 Bad Request\r\n\r\n");
		result.len = 1;
		break;
	
	case http_forbidden:
		debug("Forbidden!");
		result.array[0] = Bs("HTTP/1.1 403 Forbidden\r\n\r\n");
		result.len = 1;
	default:
		result.error = -1;
		break;
	};
	
	return result;
}

bytes http_extract_param(bytes header, bytes field) {	
	bfound f = bfind(header, field);
	f = bfind(f.after, Bs("\r\n"));
	return f.before;
}
/*
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
*/


struct http_tls_connection;

typedef void (*http_tls_fn)(struct http_tls_connection *);

struct http_tls_connection {
	int socket;
	http_tls_fn ondata;
	http_tls_fn onclose;
	struct tcp_connection tcp;
	struct tls_connection tls;
	struct http_connection http;
};

void http_tls_ondata(struct http_tls_connection *http_tls) {
	bytes buffer = balloc(4096);
	int result = tcp_ondata(&http_tls->tcp, &buffer)

	bytes write = balloc(4096);
	result = tls_ondata(http_tls->tls, &buffer, &write)

	if(write.len > 0)
		send(http_tls->tcp.socket, write.as_void, write.len, 0);

	
	struct http_ondata_result final = http_ondata(buffer);

	for(int i = 0; i < 
}

void http_tls_onclose(struct http_tls_connection *http_tls) {
	tcp_onclose(http_tls->tcp);
	tls_onclose(http_tls->tls);
	free(http_tls);
}





void tls_listener_ondata(struct generic_epoll_object *data)
{
	struct http_tls_connection * con = malloc(sizeof(*con));
	tcp_onsetup(data->socket, &con->tcp);
	tls_onsetup(data->auxilary, &con->tls);

	con->ondata = &http_tls_ondata;
	con->onclose = &http_tls_onclose;

	epoll_add(con->tcp.socket, con);
}


void tls_listener_onclose(struct generic_epoll_object *data)
{
	debug("Closing listener socket. %d", data->fd);
	close(data->fd);
	free(data);
};



struct http_server_internals {
	SSL_CTX * tls_ctx;
};

struct http_server_internals * http_init() {
	struct http_server_internals * server = malloc(*server);
	
	SSL_library_init();
	ERR_load_crypto_strings();
	SSL_load_error_strings();
	server->tls_ctx = SSL_CTX_new(TLSv1_server_method());
	if(SSL_CTX_use_certificate_file(server->tls_ctx, "./cert.pem", SSL_FILETYPE_PEM) != 1) {
		debug("Error loading the certificate.");
	}

	if(SSL_CTX_use_PrivateKey_file(server->tls_ctx, "./cert.pem", SSL_FILETYPE_PEM) != 1) {
		debug("Could not load private key.");
	}

	if(SSL_CTX_set_cipher_list(server->tls_ctx, "DEFAULT") != 1) {
		debug("Adding ciphers failed.");
	}

}

		

