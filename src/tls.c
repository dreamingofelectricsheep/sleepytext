
struct tls_connection {
	SSL *tls;
};

int tls_ondata(struct tls_connection *tls, bytes encrypted, bytes * plaintext)
{
	BIO *rbio = SSL_get_rbio(tls->tls);
	BIO *wbio = SSL_get_wbio(tls->tls);

	BIO_write(rbio, encrypted.as_void, encrypted.len);

	int stop = 0;
	int last = plaintext->len;
	void *pos = plaintext->as_void;
	plaintext->len = 0;

	while (stop == 0) {
		int len = SSL_read(tls->tls, pos, last);

		debug("%d bytes available in tls for reading.", len);

		int code = SSL_get_error(tls->tls, len);

		if (code == SSL_ERROR_ZERO_RETURN) {
			debug("TLS connection closed.");
			return -1;
		} else if (code == SSL_ERROR_SSL) {
			debug("Tls errors.");
			ERR_print_errors_fp(stderr);

			return -1;
		} else if (code == SSL_ERROR_SYSCALL) {
			debug("TLS syscall error.");
			ERR_print_errors_fp(stderr);

			return -1;
		}
		debug("Looping over code: %d", code);
		plaintext->len += len < 0 ? 0 : len;
		if (code == SSL_ERROR_WANT_READ)
			stop = 1;
		last -= len < 0 ? 0 : len;
		pos += len < 0 ? 0 : len;
	}

	return 0;
}

void tls_onclose(struct tls_connection *tls)
{
	SSL_free(tls->tls);
}

void tls_onsetup(SSL_CTX * ctx, struct tls_connection *tls)
{
	tls->tls = SSL_new(ctx);
	BIO *rbio = BIO_new(BIO_s_mem()), *wbio = BIO_new(BIO_s_mem());

	SSL_set_bio(tls->tls, BIO_new(BIO_s_mem()), BIO_new(BIO_s_mem()));
	SSL_set_accept_state(tls->tls);
}
