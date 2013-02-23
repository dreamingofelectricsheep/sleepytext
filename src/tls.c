



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

	
	

