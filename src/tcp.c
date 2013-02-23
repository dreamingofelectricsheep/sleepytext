
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





