#include "http.h"

#include <alloca.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <sys/socket.h>
#include <libowfat/socket.h>
#include <libowfat/ip4.h>
#include <libowfat/io.h>
#include <libowfat/iob.h>
#include <libowfat/stralloc.h>
#include <libowfat/byte.h>
#include <libowfat/array.h>
#include <libowfat/buffer.h>
#include <libowfat/scan.h>
#include <libowfat/str.h>
#include <libowfat/case.h>

#include "util.h"

static void    http_init(http_context *http, int socket);
static void    http_finalize(context *ctx);
static int     http_read(context *ctx, char *buf, int length);


static ssize_t http_read_until_string(http_context *http, char *buf, size_t length, const char *str, size_t max_length);
static ssize_t http_read_line(http_context *http, char *buf, size_t length, size_t max_length);
static int     http_parse_header(http_context *http, char *line, size_t length);
static ssize_t http_read_param(http_context *http, char *buf, int (*callback)(http_context *http, char *key, char *val), size_t max_length);
static int     http_parse_params(http_context *http, char *buf, int (*callback)(http_context *http, char *key, char *val), size_t max_length);
static int     http_parse_request(http_context *http, char *line, size_t length);
static int     http_parse_content_type(http_context *http, char *content_type);
static int     http_parse_cookie(http_context *http, char *cookies);
static ssize_t http_read_request(http_context *http, char *buf, size_t length);
static ssize_t http_read_header(http_context *http, char *buf, size_t length);
static ssize_t http_read_body(http_context *http, char *buf, size_t length);
static ssize_t http_read_post_data(http_context *http, char *buf, size_t length);
static ssize_t http_read_multipart_boundary(http_context *http, char *buf, size_t length);
static ssize_t http_read_multipart_after_boundary(http_context *http, char *buf, size_t length);
static ssize_t http_read_multipart_header(http_context *http, char *buf, size_t length);
static ssize_t http_read_multipart_content(http_context *http, char *buf, size_t length);

const http_error BAD_REQUEST           = {400, "Bad Request"};
const http_error FORBIDDEN             = {403, "Verboten"};
const http_error NOT_FOUND             = {404, "Not Found"};
const http_error METHOD_NOT_ALLOWED    = {405, "Method Not Allowed"};
const http_error ENTITY_TOO_LARGE      = {413, "Request Entity Too Large"};
const http_error URI_TOO_LONG          = {414, "URI Too Long "};
const http_error HEADER_TOO_LARGE      = {431, "Request Header Fields Too Large"};
const http_error INTERNAL_SERVER_ERROR = {500, "Internal Server Error"};


void http_init(http_context *http, int socket)
{
	byte_zero(http, sizeof(http_context));

	context *ctx = (context*)http;

	context_init(ctx, socket);
	ctx->read  = http_read;
	ctx->finalize = http_finalize;

	io_nonblock(socket);
	io_fd(socket);
	io_wantread(socket);
	io_setcookie(socket, http);
}

void http_finalize(context *ctx)
{
	http_context *http = (http_context*)ctx;

	if (http->finalize)
		http->finalize(http);

	array_reset(&http->read_buffer);
	array_reset(&http->multipart_boundary);
	array_reset(&http->multipart_real_boundary);
	array_reset(&http->multipart_full_boundary);
	array_reset(&http->multipart_name);
	array_reset(&http->multipart_filename);
	array_reset(&http->multipart_content_type);
}

http_context* http_new(int socket)
{
	http_context *http = malloc(sizeof(http_context));
	http_init(http, socket);
	return http;
}

static ssize_t http_read_until_string(http_context *http, char *buf, size_t length, const char *str, size_t max_length)
{
	size_t str_length = strlen(str);
	// We store the old search offset to avoid O(n^2) time complexity
	ssize_t offset = http->search_offset;
	ssize_t pos = byte_str(&buf[offset], length-offset, str);
	if (pos == length-offset) {
		if (length > str_length) {
			http->search_offset = length - str_length;
		} else {
			http->search_offset = 0;
		}
		if (max_length > 0 && pos+offset >= max_length)
			HTTP_FAIL(ENTITY_TOO_LARGE);
		else
			return AGAIN;
	}
	http->search_offset = 0;
	return pos+offset;
}

static ssize_t http_read_line(http_context *http, char *buf, size_t length, size_t max_length)
{
	ssize_t ret = http_read_until_string(http, buf, length, "\r\n", max_length);
	if (ret < 0)
		return ret;
	return ret + 2;
}

static int http_process_header(http_context *http, char *key, char *val)
{
	if (case_equals(key, "Content-Length")) {
		if (scan_long(val, &http->content_length) != strlen(val))
			HTTP_FAIL(BAD_REQUEST);
	} else if (case_equals(key, "Content-Type")) {
		if (http_parse_content_type(http, val) == ERROR)
			HTTP_FAIL(BAD_REQUEST);
	} else if (case_equals(key, "Cookie")) {
		if (http_parse_cookie(http, val) == ERROR)
			HTTP_FAIL(BAD_REQUEST);
	}

	if (http->header != NULL)
		return http->header(http, key, val);
	else
		return 0;
}

static int http_parse_header(http_context *http, char *line, size_t length)
{
	// Header = <key><whitespace*>:<whitespace*><val>

	size_t colon = byte_chr(line, length, ':');
	if (colon == length || colon == 0)
		HTTP_FAIL(BAD_REQUEST);

	size_t key_start = 0;
	size_t key_end = colon;
	while (key_end > 0 && isspace(line[key_end-1])) --key_end;
	if (key_end-key_start == 0)
		HTTP_FAIL(BAD_REQUEST);
	if (key_end-key_start > MAX_HEADER_LENGTH)
		HTTP_FAIL(HEADER_TOO_LARGE);

	size_t val_start = colon+1;
	size_t val_end = length;
	val_start += scan_whitenskip(&line[val_start], length - val_start);
	if (val_end-val_start == 0)
		HTTP_FAIL(BAD_REQUEST);
	if (val_end-val_start > MAX_HEADER_LENGTH)
		HTTP_FAIL(HEADER_TOO_LARGE);

	char *key = alloca(key_end-key_start+1);
	memcpy(key, &line[key_start], key_end-key_start);
	key[key_end-key_start] = '\0';

	char *val = alloca(val_end-val_start+1);
	memcpy(val, &line[val_start], val_end-val_start);
	val[val_end-val_start] = '\0';

	return http_process_header(http, key, val);
}

static ssize_t http_read_param(http_context *http, char *buf, int (*callback)(http_context *http, char *key, char *val), size_t max_length)
{
	ssize_t offset = 0;
	ssize_t key_length = 0;
	char *key;

	scan_percent_str(&buf[offset], NULL, &key_length);
	if (key_length > max_length)
		HTTP_FAIL(HEADER_TOO_LARGE);
	key = alloca(key_length+1);
	offset += scan_percent_str(&buf[offset], key, NULL);
	key[key_length] = '\0';

	ssize_t val_length = 0;
	char *val;

	if (buf[offset] == '=')
		++offset;

	scan_percent_str(&buf[offset], NULL, &val_length);
	if (val_length > max_length)
		HTTP_FAIL(HEADER_TOO_LARGE);
	val = alloca(val_length+1);
	offset += scan_percent_str(&buf[offset], val, NULL);
	val[val_length] = '\0';

	if (callback && callback(http, key, val) == ERROR)
		return ERROR;

	return offset;
}

static int http_parse_params(http_context *http, char *buf, int (*callback)(http_context *http, char *key, char *val), size_t max_length)
{
	ssize_t offset = 0;
	if (buf[offset] == '?')
		++offset;

	while (buf[offset] != '\0') {
		ssize_t consumed = http_read_param(http, &buf[offset], callback, max_length);
		if (consumed == ERROR)
			return ERROR;
		offset += consumed;
		if (buf[offset] == '&')
			++offset;
	}
	return 0;
}

static int http_parse_request(http_context *http, char *line, size_t length)
{
	// Request = <method><whitespace+><url><whitespace+><protocol>

	size_t offset = 0;

	size_t method_length;
	char *url;
	size_t url_length;
	size_t protocol_length;

	method_length = scan_nonwhitenskip(&line[offset], length-offset);
	if (str_equalb(&line[offset], method_length, "GET")) {
		http->method = HTTP_GET;
	} else if (str_equalb(&line[offset], method_length, "POST")) {
		http->method = HTTP_POST;
	} else if (str_equalb(&line[offset], method_length, "HEAD")) {
		http->method = HTTP_HEAD;
	} else {
		HTTP_FAIL(METHOD_NOT_ALLOWED);
	}
	offset += method_length;

	offset += scan_whitenskip(&line[offset], length-offset);
	url_length = scan_nonwhitenskip(&line[offset], length-offset);
	if (url_length == 0)
		HTTP_FAIL(BAD_REQUEST);
	if (url_length > MAX_URL_LENGTH)
		HTTP_FAIL(URI_TOO_LONG);
	url = &line[offset];
	offset += url_length;

	offset += scan_whitenskip(&line[offset], length-offset);
	protocol_length = scan_nonwhitenskip(&line[offset], length-offset);
	if (!str_equalb(&line[offset], protocol_length, "HTTP/"))
		HTTP_FAIL(BAD_REQUEST);
	offset += protocol_length;

	if (offset < length)
		HTTP_FAIL(BAD_REQUEST);

	size_t query_start = byte_chr(url, url_length, '?');
	size_t query_length = url_length - query_start;
	char *path = alloca(query_start + 1);
	char *query = alloca(query_length + 1);
	memcpy(path, url, query_start);
	path[query_start] = '\0';
	memcpy(query, &url[query_start], query_length);
	query[query_length] = '\0';

	if (http->request && http->request(http, http->method, path, query) == ERROR)
		return ERROR;

	if (http->get_param && http_parse_params(http, query, http->get_param, MAX_GET_PARAM_LENGTH) == ERROR)
		return ERROR;

	return 0;
}

static ssize_t http_read_request(http_context *http, char *buf, size_t length)
{
	ssize_t line_length = http_read_line(http, buf, length, MAX_REQUEST_LINE_LENGTH);
	if (line_length == ERROR)
		HTTP_FAIL(HEADER_TOO_LARGE);
	if (line_length == AGAIN)
		return AGAIN;

	if (http_parse_request(http, array_start(&http->read_buffer), line_length-2) == ERROR)
		return ERROR;

	http->state = HTTP_STATE_HEADERS;

	return line_length;
}

static int http_parse_content_type(http_context *http, char *content_type)
{
	// Multipart content type looks like this:
	//   multipart/form-data; boundary=---------------------------3043605411786511911326376277
	size_t offset = 0;
	const char *multipart_content = "multipart/form-data";
	const char *boundary = "boundary";
	if (str_start(content_type, multipart_content)) {
		offset += strlen(multipart_content);
		offset += scan_whiteskip(&content_type[offset]);
		if (content_type[offset] != ';')
			HTTP_FAIL(BAD_REQUEST);
		++offset;
		offset += scan_whiteskip(&content_type[offset]);
		if (!str_start(&content_type[offset], boundary))
			HTTP_FAIL(BAD_REQUEST);
		offset += strlen(boundary);
		offset += scan_whiteskip(&content_type[offset]);
		if (content_type[offset] != '=')
			HTTP_FAIL(BAD_REQUEST);
		++offset;
		offset += scan_whiteskip(&content_type[offset]);
		if (content_type[offset] == '\0')
			HTTP_FAIL(BAD_REQUEST);
		array_cats(&http->multipart_boundary, &content_type[offset]);
		array_cat0(&http->multipart_boundary);
		if (array_bytes(&http->multipart_boundary) > MAX_MULTIPART_BOUNDARY_LENGTH)
			HTTP_FAIL(BAD_REQUEST);
		array_cats(&http->multipart_real_boundary, "--");
		array_cat(&http->multipart_real_boundary, &http->multipart_boundary);
		array_cats(&http->multipart_full_boundary, "\r\n--");
		array_cat(&http->multipart_full_boundary, &http->multipart_boundary);

		http->multipart_state = MULTIPART_STATE_BOUNDARY;
	}
	return 0;
}

static int http_parse_cookie(http_context *http, char *cookies)
{
	char *key_start;
	char *key_end;
	char *val_start;
	char *val_end;

	int end = 0;

	key_start = cookies;

	while (!end) {
		if (*key_start == '\0')
			return 0;

		key_end = &key_start[str_chr(key_start, '=')];
		if (*key_end == '\0')
			return ERROR;
		*key_end = '\0';
		val_start = key_end + 1;
		val_end = &val_start[str_chr(val_start, ';')];
		end = *val_end == '\0';
		*val_end = '\0';
		if (http->cookie && http->cookie(http, key_start, val_start) == ERROR)
			return ERROR;

		if (!end) {
			key_start = val_end+1;
			key_start += scan_whiteskip(key_start);
		}
	}
}


static ssize_t http_read_header(http_context *http, char *buf, size_t length)
{
	ssize_t line_length = http_read_line(http, buf, length, MAX_HEADER_LENGTH);
	if (line_length < 0)
		return line_length;

	if (line_length-2 == 0) {
		// Empty line, all headers received
		if (http->content_length > 0) {
			http->state = HTTP_STATE_BODY;
		} else {
			http->state = HTTP_STATE_EOF;
			if (http->finish && http->finish(http) == ERROR)
				return ERROR;
		}
	} else {
		if (http_parse_header(http, buf, line_length-2) == ERROR)
			return ERROR;
	}

	return line_length;
}

static ssize_t http_read_body(http_context *http, char *buf, size_t length)
{
	ssize_t offset = 0;
	ssize_t consumed = 0;

	// We don't support persistent connections yet
	if (http->content_received > http->content_length)
		return ERROR;

	switch(http->multipart_state) {
		case MULTIPART_STATE_NONE:
			// Could be POST data
			if (http->method == HTTP_POST) {
				consumed = http_read_post_data(http, &buf[offset], length-offset);
			} else {
				consumed = length;
			}
			break;
		case MULTIPART_STATE_BOUNDARY:
			consumed = http_read_multipart_boundary(http, &buf[offset], length-offset);
			break;
		case MULTIPART_STATE_AFTER_BOUNDARY:
			consumed = http_read_multipart_after_boundary(http, &buf[offset], length-offset);
			break;
		case MULTIPART_STATE_HEADERS:
			consumed = http_read_multipart_header(http, &buf[offset], length-offset);
			break;
		case MULTIPART_STATE_CONTENT:
			consumed = http_read_multipart_content(http, &buf[offset], length-offset);
			break;
	}

	if (consumed >= 0)
		http->content_received += consumed;

	//if (http->content_received > http->content_length)
	//	HTTP_FAIL(BAD_REQUEST);

	if (http->content_received == http->content_length) {
		http->state = HTTP_STATE_EOF;

		if (http->finish && http->finish(http) == ERROR)
			return ERROR;
	}
	return consumed;
}

static ssize_t http_read_post_data(http_context *http, char *buf, size_t length)
{
	ssize_t param_length;

	param_length = http_read_until_string(http, buf, length, "&", MAX_POST_PARAM_LENGTH);
	if (param_length == ERROR)
		HTTP_FAIL(ENTITY_TOO_LARGE);
	if (param_length == AGAIN) {
		// The last parameter is not followed by a "&", so we have to use this workaround.
		if (http->content_length != 0 && http->content_received+length >= http->content_length)
			param_length = http->content_length - http->content_received;
		else
			return param_length;
	} else {
		++param_length; // Return length including the & at the end
	}

	char *zero_terminated = alloca(param_length+1);
	memcpy(zero_terminated, buf, param_length);
	zero_terminated[param_length] = '\0';

	if (http_read_param(http, buf, http->post_param, MAX_POST_PARAM_LENGTH) == ERROR)
		return ERROR;
	return param_length;
}

static ssize_t http_read_multipart_boundary(http_context *http, char *buf, size_t length)
{
	char *boundary = array_start(&http->multipart_real_boundary);
	size_t boundary_length = array_bytes(&http->multipart_real_boundary) - 1;
	if (!str_startb(buf, length, boundary))
		HTTP_FAIL(BAD_REQUEST);
	if (length < boundary_length)
		return AGAIN;


	http->multipart_state = MULTIPART_STATE_AFTER_BOUNDARY;
	return boundary_length;
}

static ssize_t http_read_multipart_after_boundary(http_context *http, char *buf, size_t length)
{
	// after boundary = --\n\r | \n\r
	ssize_t line_length = http_read_line(http, buf, length, 4);
	if (line_length < 0)
		return line_length;
	if (line_length > 2 &&
	    !str_equalb(buf, line_length-2, "--"))
		HTTP_FAIL(BAD_REQUEST);

	array_trunc(&http->multipart_name);
	array_trunc(&http->multipart_filename);
	array_trunc(&http->multipart_content_type);
	http->multipart_state = MULTIPART_STATE_HEADERS;
	return line_length;
}

static int http_parse_content_disposition(http_context *http, char *content_disposition)
{
	// Content-Disposition looks like this:
	//   form-data; name="foo"; filename="bar.txt"
	size_t offset = 0;

	if (strlen(content_disposition) > MAX_HEADER_LENGTH)
		HTTP_FAIL(HEADER_TOO_LARGE);

	char *val_buffer = alloca(strlen(content_disposition)+1);
	size_t val_length = 0;

	if (!str_start(content_disposition, "form-data"))
		HTTP_FAIL(BAD_REQUEST);
	offset += strlen("form-data");
	offset += scan_whiteskip(&content_disposition[offset]);
	while (content_disposition[offset] != '\0') {
		if (content_disposition[offset] != ';')
			HTTP_FAIL(BAD_REQUEST);
		++offset;
		offset += scan_whiteskip(&content_disposition[offset]);

		size_t key_start = offset;

		offset += str_chr(&content_disposition[offset], '=');
		size_t assign = offset;
		if (content_disposition[assign] != '=')
			HTTP_FAIL(BAD_REQUEST);

		size_t key_end = assign-1;
		while (isspace(content_disposition[key_end])) --key_end;

		++offset;

		offset += scan_whiteskip(&content_disposition[offset]);

		if (content_disposition[offset] == '\0')
			HTTP_FAIL(BAD_REQUEST);
		if (content_disposition[offset] == '"') {
			++offset;
			if (content_disposition[offset] == '\0')
				HTTP_FAIL(BAD_REQUEST);

			offset += scan_quoted_str(&content_disposition[offset], val_buffer, &val_length);
		} else {
			size_t val_start = offset;
			while (!isspace(content_disposition[offset]) && content_disposition[offset] != '\0' &&
			       content_disposition[offset] != ';') ++offset;
			size_t val_end = offset;
			val_length = val_end-val_start;
			memcpy(val_buffer, &content_disposition[val_start], val_length);
		}

		// Process "name" and "filename" parameters
		if (case_equalb(&content_disposition[key_start], key_end-key_start, "name")) {
			array_trunc(&http->multipart_name);
			array_catb(&http->multipart_name, val_buffer, val_length);
			array_cat0(&http->multipart_name);
		} else if (case_equalb(&content_disposition[key_start], key_end-key_start, "filename")) {
			array_trunc(&http->multipart_filename);
			array_catb(&http->multipart_filename, val_buffer, val_length);
			array_cat0(&http->multipart_filename);
		}

		offset += scan_whiteskip(&content_disposition[offset]);
	}
	return 0;
}

static int http_process_multipart_header(http_context *http, char *key, char *val)
{
	if (case_equals(key, "Content-Disposition")) {
		return http_parse_content_disposition(http, val);
	} else if (case_equals(key, "Content-Type")) {
		array_trunc(&http->multipart_content_type);
		array_cats(&http->multipart_content_type, val);
		array_cat0(&http->multipart_content_type);
	}
	return 0;
}

static int http_parse_multipart_header(http_context *http, char *line, size_t length)
{
	size_t colon = byte_chr(line, length, ':');
	if (colon == length || colon == 0)
		HTTP_FAIL(BAD_REQUEST);

	size_t key_start = 0;
	size_t key_end = colon;
	while (key_end > 0 && scan_whitenskip(&line[key_end-1], 1)) --key_end;
	if (key_end-key_start == 0)
		HTTP_FAIL(BAD_REQUEST);
	if (key_end-key_start > MAX_HEADER_LENGTH)
		HTTP_FAIL(HEADER_TOO_LARGE);

	size_t val_start = colon+1;
	size_t val_end = length;
	val_start += scan_whitenskip(&line[val_start], length - val_start);
	if (val_end-val_start == 0)
		HTTP_FAIL(BAD_REQUEST);
	if (val_end-val_start > MAX_HEADER_LENGTH)
		HTTP_FAIL(HEADER_TOO_LARGE);

	char *key = alloca(key_end-key_start+1);
	memcpy(key, &line[key_start], key_end-key_start);
	key[key_end-key_start] = '\0';

	char *val = alloca(val_end-val_start+1);
	memcpy(val, &line[val_start], val_end-val_start);
	val[val_end-val_start] = '\0';

	return http_process_multipart_header(http, key, val);
}

static ssize_t http_read_multipart_header(http_context *http, char *buf, size_t length)
{
	size_t offset = 0;

	ssize_t line_length = http_read_line(http, &buf[offset], length-offset, MAX_HEADER_LENGTH);
	if (line_length == ERROR)
		HTTP_FAIL(ENTITY_TOO_LARGE);
	if (line_length == AGAIN)
		return AGAIN;

	if (line_length-2 == 0) {
		// All headers received
		http->multipart_state = MULTIPART_STATE_CONTENT;
		if (array_bytes(&http->multipart_content_type) == 0) {
			// Normal form data
			// nothing to do here
		} else {
			// File upload
			if (http->file_begin &&
			    http->file_begin(http, array_start(&http->multipart_name),
			                           array_start(&http->multipart_filename),
			                           array_start(&http->multipart_content_type)) == ERROR) {
				return ERROR;
			}
		}
	} else {
		if (http_parse_multipart_header(http, buf, line_length-2) == ERROR)
			return ERROR;
	}

	return line_length;
}


static ssize_t http_read_multipart_content(http_context *http, char *buf, size_t length)
{
	if (array_bytes(&http->multipart_content_type) == 0) {
		// Normal form data (POST)
		ssize_t consumed = http_read_until_string(http, buf, length, array_start(&http->multipart_full_boundary), MAX_POST_PARAM_LENGTH);
		if (consumed < 0)
			return consumed;

		buf[consumed] = '\0'; // a bit dirty, overwrites the first "-" of "--" in the buffer
		if (http->post_param && http->post_param(http,
		                                         array_start(&http->multipart_name),
		                                         buf) == ERROR) {
		 	return ERROR;
		}

		http->multipart_state = MULTIPART_STATE_BOUNDARY;
		return consumed+2; // +2 because inner boundary has additional "--" prefix
	} else {
		// File upload
		ssize_t consumed = http_read_until_string(http, buf, length, array_start(&http->multipart_full_boundary), 0);
		ssize_t safe_prefix;
		if (consumed == AGAIN) {
			// File upload
			safe_prefix = http->search_offset;
			if (safe_prefix <= 0)
				return AGAIN;
			http->search_offset = 0;
			if (http->file_content && http->file_content(http, buf, safe_prefix) == ERROR)
				return ERROR;
			return safe_prefix;
		} else {
			if (consumed > 0) {
				if (http->file_content && http->file_content(http, buf, consumed) == ERROR)
					return ERROR;
			}
			if (http->file_end && http->file_end(http) == ERROR)
				return ERROR;
			http->multipart_state = MULTIPART_STATE_BOUNDARY;
			return consumed+2; // +2 because inner boundary has additional "--" prefix
		}
	}

	return 0;
}

int http_read(context *ctx, char *buf, int length)
{
	http_context *http = (http_context*)ctx;

	ssize_t offset = 0;
	ssize_t consumed = 0;

	array_catb(&http->read_buffer, buf, length);

	if (length == 0)
		http->state = HTTP_STATE_EOF;

	char *total_buf = array_start(&http->read_buffer);
	size_t total_length = array_bytes(&http->read_buffer);

	while (1) {
		switch(http->state) {
			case HTTP_STATE_REQUEST:
				consumed = http_read_request(http, &total_buf[offset], total_length-offset);
				break;
			case HTTP_STATE_HEADERS:
				consumed = http_read_header(http, &total_buf[offset], total_length-offset);
				break;
			case HTTP_STATE_BODY:
				consumed = http_read_body(http, &total_buf[offset], total_length-offset);
				break;
			case HTTP_STATE_EOF:
				return 0;
		}
		if (consumed <= 0)
			break;
		offset += consumed;
		if (offset == total_length)
			break;
	}

	array_chop_beginning(&http->read_buffer, offset);

	if (consumed < 0) {
		if (consumed == ERROR && http->error_status && http->error)
			http->error(http);
		return consumed;
	} else
		return 0;
}
