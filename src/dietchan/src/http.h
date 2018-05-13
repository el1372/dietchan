#ifndef HTTP_H
#define HTTP_H

#include <unistd.h>
#include <libowfat/array.h>
#include <libowfat/uint16.h>
#include <libowfat/iob.h>

#include "context.h"
#include "ip.h"
#include "config.h"


typedef enum http_method {
	HTTP_GET,
	HTTP_POST,
	HTTP_HEAD
} http_method;

typedef enum http_state {
	HTTP_STATE_REQUEST,
	HTTP_STATE_HEADERS,
	HTTP_STATE_BODY,
	HTTP_STATE_EOF
} http_state;

typedef enum http_multipart_state {
	MULTIPART_STATE_NONE,
	MULTIPART_STATE_BOUNDARY,
	MULTIPART_STATE_AFTER_BOUNDARY,
	MULTIPART_STATE_HEADERS,
	MULTIPART_STATE_CONTENT
} http_multipart_state;

typedef struct http_error {
	int status;
	char *message;
} http_error;

typedef struct http_context {
	context parent_instance;

	//char ip[4];
	struct ip ip;
	uint16 port;

	array read_buffer;
	size_t search_offset;

	http_state state;
	http_method method;
	int64 content_length;
	int64 content_received;

	int error_status;
	char *error_message;

	http_multipart_state multipart_state;
	array multipart_boundary;
	array multipart_real_boundary;
	array multipart_full_boundary;
	array multipart_name;
	array multipart_filename;
	array multipart_content_type;

	// Data for callbacks
	void *info;

	// Callbacks
	int (*request)      (struct http_context *http, http_method method, char *path, char *query);
	int (*get_param)    (struct http_context *http, char *key, char *val);
	int (*header)       (struct http_context *http, char *key, char *val);
	int (*cookie)       (struct http_context *http, char *key, char *val);
	int (*post_param)   (struct http_context *http, char *key, char *val);
	int (*file_begin)   (struct http_context *http, char *name, char *filename, char *content_type);
	int (*file_content) (struct http_context *http, char *buf, size_t length);
	int (*file_end)     (struct http_context *http);
	int (*body_content) (struct http_context *http, char *buf, size_t length);
	int (*finish)       (struct http_context *http);
	void (*finalize)    (struct http_context *http);
	void (*error)       (struct http_context *http);

} http_context;

http_context* http_new(int socket);

extern const http_error BAD_REQUEST;
extern const http_error FORBIDDEN;
extern const http_error NOT_FOUND;
extern const http_error METHOD_NOT_ALLOWED;
extern const http_error ENTITY_TOO_LARGE;
extern const http_error URI_TOO_LONG;
extern const http_error HEADER_TOO_LARGE;
extern const http_error INTERNAL_SERVER_ERROR;

#define HTTP_FAIL(e) do { http->error_status = e.status; \
                          http->error_message = e.message; \
                          return ERROR; } while (0)


#endif // HTTP_H
