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
	object->auxilary = auxilary;

	epoll_add(sock, object);

	return sock;
}





struct http_tls_connection;

typedef void (*http_tls_fn)(struct http_tls_connection *);

struct http_tls_connection {
	int socket;
	http_tls_fn ondata;
	http_tls_fn onclose;
	struct tcp_connection tcp;
	struct tls_connection tls;
	bytes buffer;
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

		

