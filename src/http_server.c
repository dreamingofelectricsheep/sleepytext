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

typedef struct http_ondata_fn_result (*handler) (struct http_request * request);

struct http_callback_pair {
	handler fun;
	bytes addr;
};

struct http_server {
	SSL_CTX *tls_ctx;
	struct http_callback_pair callback[16];
	size_t callbacks;
};

void http_server_add_callback(struct http_server *s,
			      struct http_callback_pair p)
{
	s->callback[s->callbacks] = p;
	s->callbacks++;
}

#include "http.c"
#include "tls.c"
#include "tcp.c"

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
	object->auxiliary = auxilary;

	epoll_add(sock, object);

	return sock;
}

struct http_ondata_fn_result http_server_callback_dispatch(struct http_server
							   *s,
							   struct http_request
							   *request)
{

	for (int i = 0; i < s->callbacks; i++) {
		if (bsame(request->addr, s->callback[i].addr) == 0)
			return s->callback[i].fun(request);

		if (s->callback[i].addr.len <= 1)
			continue;

		bytes slice = bslice(request->addr, 0, s->callback[i].addr.len);

		if (bsame(slice, s->callback[i].addr) == 0)
			return s->callback[i].fun(request);
	}

	return (struct http_ondata_fn_result) {
		.error = 0,.payload = (bytes) {
	0, 0},.code = http_not_found};
}

struct http_tls_connection;

typedef void (*http_tls_fn) (struct http_tls_connection *);

struct http_tls_connection {
	int socket;
	http_tls_fn ondata;
	http_tls_fn onclose;
	struct tcp_connection tcp;
	struct tls_connection tls;
	bytes buffer;
	struct http_server *server;
};

void http_tls_onclose(struct http_tls_connection *http_tls)
{
	debug("Closing a connection...");
	tcp_onclose(&http_tls->tcp);
	tls_onclose(&http_tls->tls);
	bfree(&http_tls->buffer);
	free(http_tls);
	debug("Connection closed.");
}

void http_tls_ondata(struct http_tls_connection *http_tls)
{
	debug("Received data.");
	bytes buffer;
	bytes writeb = balloc(4096);

	if (http_tls->buffer.as_void != 0) {
		buffer.as_void =
		    http_tls->buffer.as_void + http_tls->buffer.len;
		buffer.len = 4096 - http_tls->buffer.len;
	} else {
		buffer = http_tls->buffer = balloc(4096);
		http_tls->buffer.len = 0;
	}

	bytes tcpbuffer = balloc(4096);
	int result = tcp_ondata(&http_tls->tcp, &tcpbuffer);
	if (result < 0)
		goto error;

	result = tls_ondata(&http_tls->tls, tcpbuffer, &buffer);
	if (result < 0)
		goto error;

	http_tls->buffer.len += buffer.len;

	debug("----------------");
	write(0, buffer.as_void, buffer.len);
	debug("----------------");

	if (buffer.len > 0) {
		debug("Crunching HTTP.");
		struct http_request parsed =
		    http_parse_request(http_tls->buffer);

		struct http_ondata_fn_result res =
		    http_server_callback_dispatch(http_tls->server, &parsed);

		struct http_ondata_result final = http_assemble_response(res);

		if (final.error < 0)
			goto error;

		debug("Response chunks: %zd", final.len);
		for (int i = 0; i < final.len; i++) {
			SSL_write(http_tls->tls.tls, final.array[i].as_void,
				  final.array[i].len);
		}

		if (final.len != 0)
			bfree(&http_tls->buffer);
	}

	BIO *mem = SSL_get_wbio(http_tls->tls.tls);

	bytes bff = balloc(1024 * 1024);
	int r = BIO_read(mem, bff.as_void, bff.len);
	if (r > 0) {
		debug("Writing %zd bytes to the socket.", r);

		write(http_tls->tcp.socket, bff.as_void, r);
	} else
		debug("Nothing to write.");
	return;

 error:
	debug("Error reached, closing connection.");
	bfree(&http_tls->buffer);
	bfree(&writeb);
	http_tls_onclose(http_tls);
}

void tls_listener_ondata(struct generic_epoll_object *data)
{
	struct http_tls_connection *con = malloc(sizeof(*con));
	con->server = data->auxiliary;

	tcp_onsetup(data->fd, &con->tcp);
	tls_onsetup(con->server->tls_ctx, &con->tls);

	con->ondata = &http_tls_ondata;
	con->onclose = &http_tls_onclose;
	con->buffer.len = 0;
	con->buffer.as_void = 0;

	epoll_add(con->tcp.socket, con);
	debug("Done with the connection setup.");
}

void tls_listener_onclose(struct generic_epoll_object *data)
{
	debug("Closing listener socket. %d", data->fd);
	close(data->fd);
	free(data);
};

struct http_server *http_init()
{
	struct http_server *s = malloc(sizeof(*s));

	SSL_library_init();
	ERR_load_crypto_strings();
	SSL_load_error_strings();
	s->tls_ctx = SSL_CTX_new(TLSv1_server_method());
	if (SSL_CTX_use_certificate_file
	    (s->tls_ctx, "./cert.pem", SSL_FILETYPE_PEM) != 1) {
		debug("Error loading the certificate.");
	}

	if (SSL_CTX_use_PrivateKey_file
	    (s->tls_ctx, "./cert.pem", SSL_FILETYPE_PEM) != 1) {
		debug("Could not load private key.");
	}

	if (SSL_CTX_set_cipher_list(s->tls_ctx, "DEFAULT") != 1) {
		debug("Adding ciphers failed.");
	}

	return s;
}
