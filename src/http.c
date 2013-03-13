

struct http_request 
{
	bytes header;
	bytes payload;
	bytes addr;
};

enum http_result 
{
	http_ok = 0,
	http_bad_request,
	http_not_found,
	http_forbidden,
	http_switching_protocols,
	http_internal_server_error,
	http_last
};

bytes http_result_text[] = 
{
	Bs("200 Ok"),
	Bs("400 Bad Request"),
	Bs("404 Not Found"),
	Bs("403 Forbidden"),
	Bs("101 Switching Protocols"),
	Bs("500 Internal Server Error")
};


const size_t http_max_headers = 8;


struct http_raw_response 
{
	enum http_result status;
	bytes payload;
	size_t headers;
	bytes header_val[http_max_headers];
	bytes header_name[http_max_headers];
} 
http_ondata_callback(struct http_request *request);

struct http_response 
{
	bytes header, 
		payload;
};

struct http_raw_response http_result(enum http_result status)
{
	return (struct http_raw_response) 
	{
		.headers = 0,
		.status = status,
		.payload = bvoid 
	};
}

bytes http_extract_param(bytes header, bytes field)
{
	bfound f = bfind(header, field);
	f = bfind(bslice(f.after, 2, 0), Bs("\r\n"));
	return f.before;
}

struct http_request http_parse_request(bytes buffer)
{
	write(0, buffer.as_void, buffer.len);

	bfound f = bfind(buffer, Bs("\r\n\r\n"));

	if (f.found.len == 0) 
	{
		debug("Partial http header.");

		return (struct http_request) {
			.addr = (bytes) bvoid
		};
	}

	struct http_request request = {
		.header = f.before,
		.payload = f.after,
	};

	f = bfind(request.header, Bs("Content-Length: "));

	if (f.found.len > 0) {
		f = bfind(f.after, Bs("\r\n"));

		int contentlen = btoi(f.before);

		if (contentlen != request.payload.len) {
			debug
			    ("Partial body received. %lu out of %d bytes available.",
			     request.payload.len, contentlen);

			return (struct http_request) {
				.addr = bvoid
			};
		}
	}

	f = bfind(request.header, Bs(" "));
	f = bfind(f.after, Bs(" "));

	request.addr = f.before;

	return request;
}

struct http_raw_response http_websocket_accept(struct http_request * request) 
{
	int version = btoi(http_extract_param(request->header, Bs("Sec-WebSocket-Version")));
	debug("Websocket protocol version: %d", version);

	if(version != 13) return http_result(http_bad_request);
	debug("Websocket request received.");

	bytes key = http_extract_param(request->header, Bs("Sec-WebSocket-Key"));
	bytes rfc6455 = Bs("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

	SHA_CTX sha;
	SHA1_Init(&sha);

	SHA1_Update(&sha, key.as_void, key.len);
	SHA1_Update(&sha, rfc6455.as_void, rfc6455.len);

	unsigned char digest[20];
	SHA1_Final(digest, &sha);
	
	struct http_raw_response result = http_result(http_switching_protocols);
	
	BIO * b64 = BIO_new(BIO_f_base64());
	BIO * mem = BIO_new(BIO_s_mem());
	BIO_push(b64, mem);
	BIO_write(b64, digest, 20);
	BIO_flush(b64);
	bytes enstatusd = balloc(128);
	enstatusd.len = BIO_read(mem, enstatusd.as_void, enstatusd.len);
	BIO_free_all(b64);
	
	result.headers = 1;
	result.header_name[0] = Bs("Sec-WebSocket-Accept");
	result.header_val[0] = enstatusd;
		
	
	return result;
}

struct http_websocket_frame 
{
	size_t len;
	union
	{
		uint32_t mask;
		uint8_t maskbytes[4];
	};
	bool parsed_header;
	uint8_t buffer[14];
	size_t header_len;
	size_t state;
};

#define websocket_helper_7
#define websocket_helper_16 uint16_t exlen;
#define websocket_helper_64 uint64_t exlen;
#define make_header_struct(i) \
struct http_websocket_header ## i \
{ \
	uint8_t flags; \
	uint8_t len; \
	websocket_helper_ ## i \
	uint32_t mask; \
};

make_header_struct(7)
make_header_struct(16)
make_header_struct(64)






int http_websocket_parse_header(struct http_websocket_frame *frame)
{
	struct http_websocket_header7 * h = (void *) frame->buffer;
	frame->state = 0;

	if(frame->header_len < sizeof(struct http_websocket_header7)) 
	{
		return 0;
	}
	else if(h->len < 255)
	{
		frame->parsed_header = true;
		frame->len = h->len - 128;
		frame->mask = h->mask;
		return sizeof(*h);
	}
	else if(frame->header_len >= sizeof(struct http_websocket_header16))
	{
		if(h->len == 255)
		{
			struct http_websocket_header16 * h16 = (void*)frame->buffer;
			frame->parsed_header = true;
			frame->len = h16->exlen;
			frame->mask = h16->mask;
			return sizeof(*h16);
		}
		else if(h->len == sizeof(struct http_websocket_header64))
		{
			struct http_websocket_header64 * h64 = (void*)frame->buffer;

			frame->parsed_header = true;
			frame->len = h64->exlen;
			frame->mask = h64->mask;
			return sizeof(*h64);
		}
	}
	
	return 0;
}



int http_websocket_parse(bytes data, bytes *out, struct http_websocket_frame *frame)
{
	int i = 0;
	int k = 0;
	while(data.len < i) 
	{
		if(frame->parsed_header == false)
		{
			int j = i;
			for(; j < data.len && j - i < 14; j++, frame->header_len++)
				frame->buffer[frame->header_len] = data.as_char[i];

			int r = http_websocket_parse_header(frame);
			
			if(r == 0)
			{
				if(data.len > i + 1)
					debug("Error parsing websocket frames.")
				return -1;
			}

			i += r;
		}
		else
		{
			out->as_char[k] = data.as_char[i] ^ frame->maskbytes[frame->state % 4];
			frame->state++;
			i++;
			k++;

			if(k == frame->len) frame->parsed_header = false;
		}
	}

	out->len = k;
	return 0;
}


// The result needs freeing
struct http_response http_assemble_response(struct http_raw_response raw)
{
	struct http_response result = { bvoid, bvoid };

	if(raw.status >= http_last)
		raw = http_result(http_internal_server_error);
	


	bytes response_header_mem = balloc(4096);


	bytes status = http_result_text[raw.status];
	bpair p = bprintf(response_header_mem, "HTTP/1.1 %*s\r\n", status.len,
		status.as_void);

	
	if(raw.status == http_switching_protocols)
	{
		p = bprintf(p.second, "Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n");
	}
	else
	{
		p = bprintf(p.second, "Content-Length: %zd\r\n", raw.payload.len);
	}

	
	for(int i = 0; i < raw.headers; i++)
	{
		p = bprintf(p.second, "%*s: %*s\r\n", 
			raw.header_name[i].len, raw.header_name[i].as_void,
			raw.header_val[i].len, raw.header_val[i].as_void);

		bfree(&raw.header_val[i]);
	}

	p = bprintf(p.second, "\r\n");

	result.header.as_void = response_header_mem.as_void;
	result.header.len = p.second.as_void - response_header_mem.as_void;
	
	result.payload = raw.payload;
	
	return result;
}



