
struct http_ondata_fn_result {
	int error;
	int code;
	bytes payload;
	bytes header;
} http_ondata_callback(struct http_request *request);

struct http_ondata_result {
	bytes array[5];
	size_t len;
	int error;
};

struct http_request http_parse_request(bytes buffer)
{
	write(0, buffer.as_void, buffer.len);

	bfound f = bfind(buffer, Bs("\r\n\r\n"));

	if (f.found.len == 0) {
		debug("Partial http header.");

		return (struct http_request) {
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
			};
		}
	}

	f = bfind(request.header, Bs(" "));
	f = bfind(f.after, Bs(" "));

	request.addr = f.before;

	return request;
}

struct http_ondata_result http_assemble_response(struct http_ondata_fn_result
						 fnresult)
{
	struct http_ondata_result result = {
		.len = 0,
		.error = 0
	};

	if (fnresult.error)
		goto bad_request;

	switch (fnresult.code) {
	case http_not_found:
		debug("Not found.");
		result.array[0] = Bs("HTTP/1.1 404 Not found\r\n"
				     "Content-Length: 0\r\n\r\n");
		result.len = 1;
		break;

	case http_ok:{
			debug("Ok.");
			bytes r = balloc(256);
			r.len = snprintf(r.as_void, 256, "HTTP/1.1 200 Ok\r\n"
					 "Content-Length: %zd\r\n",
					 fnresult.payload.len);

			result.array[0] = r;
			if(fnresult.header.len > 0)
				result.array[1] = fnresult.header;
			else
				result.array[1] = Bs("\r\n");
			result.array[2] = fnresult.payload;

			result.len = 3;
			break;
		}

 bad_request:
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
		debug("ERROR!");
		result.error = -1;
		break;
	};

	return result;
}

bytes http_extract_param(bytes header, bytes field)
{
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
