
struct tcp_connection {
	int socket;
	struct in6_addr ip;
	time_t last;
};

int tcp_ondata(struct tcp_connection *tcp, bytes * buffer)
{
	int len = recv(tcp->socket, buffer->as_void, buffer->len, 0);

	debug("Received %d bytes of data from socket %d", len, tcp->socket);

	if (len <= 0) {
		if (len < 0)
			debug("Error reading data from socket %d: %s",
			      tcp->socket, strerror(errno));

		return -1;
	}

	buffer->len = len;
	return 0;
}

void tcp_onclose(struct tcp_connection *tcp)
{
	debug("Closing http connection: %d", tcp->socket);
	close(tcp->socket);
}

void tcp_onsetup(int socket, struct tcp_connection *tcp)
{
	struct sockaddr_in6 addr;
	socklen_t len = sizeof(addr);

	int accepted = accept(socket, (void *)&addr, &len);

	int r = true;
	setsockopt(accepted, SOL_SOCKET, SO_KEEPALIVE, &r, sizeof(r));

	debug("Accepting a new tcp connection: %d", accepted);
	tcp->socket = accepted;
	memcpy(&tcp->ip, &addr.sin6_addr, sizeof(addr.sin6_addr));
}
