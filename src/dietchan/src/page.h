#ifndef PAGE_H
#define PAGE_H

#include <libowfat/iob.h>
#include <libowfat/str.h>
#include <libowfat/case.h>
#include <libowfat/scan.h>
#include <libowfat/fmt.h>
#include "http.h"
#include "persistence.h"

#define DEFAULT_NAME "Felix"

void write_escaped(context *ctx, const char *unescaped);
void write_session(http_context *http, struct session *session);

void write_page_header(http_context *http);
void write_reply_form(http_context *http, int board, int thread, struct captcha *captcha);
void write_mod_bar(http_context *http, int ismod);
void write_page_footer(http_context *http);
void write_board_bar(http_context *http);
void write_top_bar(http_context *http, struct user *user, const char *url);
void write_bottom_bar(http_context *http);

void write_upload(http_context *http, struct upload *upload);

enum {
	WRITE_POST_IP         = 1 << 0,
	WRITE_POST_USER_AGENT = 1 << 1
};

void write_post(http_context *http, struct post *post, int absolute_url, int flags);
void write_post_url(http_context *http, struct post *post, int absolute);
void write_post_url2(http_context *http, struct board *board, struct thread *thread, struct post *post, int absolute);
void abbreviate_filename(char *buffer, size_t max_length);
size_t estimate_width(const char *buffer);

char* strescape(const char *unescaped);
size_t fmt_time(char *out, uint64 ms);
size_t html_escape_char(char *output, char character);
size_t fmt_escape(char *buf, const char *unescaped);
size_t fmt_escapen(char *buf, const char *unescaped, size_t n);

// These don't really belong here, find better location
int can_post(http_context *http, int write_header, struct ip *ip, struct board *board);

typedef void (*find_bans_callback)(struct ban *ban, void *extra);
void find_bans(struct ip *ip, struct board *board, find_bans_callback callback, void *extra);

int64 is_banned(struct ip *ip, struct board *board, enum ban_target target);
int64 is_flood_limited(struct ip *ip, struct board *board, enum ban_target target);
int64 is_captcha_required(struct ip *ip, struct board *board, enum ban_target target);

int64 any_ip_affected(struct ip *ip, struct ip *x_real_ip, array *x_forwarded_for,
                      struct board *board, enum ban_target target,
                      int64 (*predicate)(struct ip *ip, struct board *board, enum ban_target target));

size_t parse_boards(http_context *http, char *s, array *boards, int *ok);
void parse_x_forwarded_for(array *ips, const char *header);

// --- Convenience macros ---

#define PARAM_I64(name, variable) \
	if (case_equals(key, name)) { if (scan_long(val, &variable) != strlen(val)) HTTP_FAIL(BAD_REQUEST); return 0; }

#define PARAM_X64(name, variable) \
	if (case_equals(key, name)) { if (scan_xlong(val, &variable) != strlen(val)) HTTP_FAIL(BAD_REQUEST); return 0; }

#define PARAM_STR(name, variable) \
	if (case_equals(key, name)) { if (variable) free(variable); variable = strdup(val); return 0; }

#define PARAM_SESSION() \
	if (str_equal(key, "session")) { \
		struct session *s = find_session_by_sid(val); \
		s = session_update(s); \
		if (s) \
			page->user = find_user_by_id(session_user(s)); \
		else \
			page->user = 0; \
		page->session = s; \
		return 0; \
	}

#define PARAM_REDIRECT(name, variable) \
	if (case_equals(key, name)) { \
		if (!str_start(val, "/")) \
			HTTP_FAIL(BAD_REQUEST); \
		if (strchr(val, '\n') || strchr(val, '\r')) \
			HTTP_FAIL(BAD_REQUEST); \
		if (variable) \
			free(variable); \
		variable = strdup(val); \
		return 0; \
	}

#ifndef BUFFER_WRITES

#define FLUSH_THRESHOLD 65536

size_t fmt_escape(char *buf, const char *unescaped);

#define HTTP_WRITE(content) \
	do { iob_adds(((context*)http)->batch, content); \
	     if (iob_bytesleft(((context*)http)->batch) > FLUSH_THRESHOLD) context_flush((context*)http); } while(0)

#define HTTP_WRITE_DYNAMIC(content) \
	do { iob_adds_free(((context*)http)->batch, strdup(content)); \
	     if (iob_bytesleft(((context*)http)->batch) > FLUSH_THRESHOLD) context_flush((context*)http); } while(0)

#define HTTP_WRITE_ESCAPED(content) \
	do { size_t len=fmt_escape(0,content); \
	     char *buf=malloc(len); \
	     fmt_escape(buf,content); \
	     iob_addbuf_free(((context*)http)->batch, buf, len); \
	     if (iob_bytesleft(((context*)http)->batch) > FLUSH_THRESHOLD) context_flush((context*)http); } while(0)

#define HTTP_WRITE_FILE(fd, off, n) \
	do { iob_addfile_close(((context*)http)->batch, fd, off, n); \
	     if (iob_bytesleft(((context*)http)->batch) > FLUSH_THRESHOLD) context_flush((context*)http); } while(0)

#else

#define HTTP_WRITE(content) do {context_write_data((context*)http, content, strlen(content));} while(0)
#define HTTP_WRITE_DYNAMIC(content)  HTTP_WRITE(content)
#define HTTP_WRITE_FILE(fd, off, n) do {context_write_file((context*)http, fd, off, n);} while (0)
#define HTTP_WRITE_ESCAPED(content)  do {write_escaped((context*)http, content);} while(0)
#endif

#define HTTP_WRITE_ULONG(n) do { \
	char buf[FMT_ULONG]; \
	buf[fmt_ulong(buf, n)] = '\0'; \
	HTTP_WRITE_DYNAMIC(buf); \
	} while (0)

#define HTTP_WRITE_XLONG(n) do { \
	char buf[FMT_XLONG]; \
	buf[fmt_xlong(buf, n)] = '\0'; \
	HTTP_WRITE_DYNAMIC(buf); \
	} while (0)

#define HTTP_WRITE_LONG(n) do { \
	char buf[FMT_LONG]; \
	buf[fmt_long(buf, n)] = '\0'; \
	HTTP_WRITE_DYNAMIC(buf); \
	} while (0)

#define HTTP_WRITE_HUMAN(n) do { \
	char buf[256]; \
	buf[fmt_human(buf, n)] = '\0'; \
	HTTP_WRITE_DYNAMIC(buf); \
	} while (0)

#define HTTP_WRITE_HUMANK(n) do { \
	char buf[256]; \
	buf[fmt_humank(buf, n)] = '\0'; \
	HTTP_WRITE_DYNAMIC(buf); \
	} while (0)

#define HTTP_WRITE_HTTP_DATE(timestamp) do { \
	char buf[256]; \
	buf[fmt_httpdate(buf, timestamp)] = '\0'; \
	HTTP_WRITE_DYNAMIC(buf); \
	} while (0)

#define HTTP_WRITE_TIME(ms) do { \
	char buf[256]; \
	buf[fmt_time(buf, ms)] = '\0'; \
	HTTP_WRITE_DYNAMIC(buf); \
	} while (0)

#define HTTP_WRITE_IP(ip) do { \
	char buf[256]; \
	buf[fmt_ip(buf, &ip)] = '\0'; \
	HTTP_WRITE_DYNAMIC(buf); \
	} while(0)


#define HTTP_WRITE_SESSION() do { write_session(http, page->session); } while(0)
#define HTTP_EOF() do { context_eof((context*)http); } while(0)

#define HTTP_STATUS(status) \
	HTTP_WRITE("HTTP/1.1 " status "\r\n" \
	           "Connection: close\r\n")

#define HTTP_STATUS_HTML(status) \
	HTTP_WRITE("HTTP/1.1 " status "\r\n" \
	           "Connection: close\r\n" \
	           "Content-Language: en\r\n" \
	           "Content-Type: text/html; charset=utf-8\r\n")

#define HTTP_STATUS_REDIRECT(status, location) \
	do { \
		HTTP_WRITE("HTTP/1.1 " status "\r\n" \
		           "Connection: close\r\n" \
		           "Location: "); \
		HTTP_WRITE_DYNAMIC(location); \
		HTTP_WRITE("\r\n"); \
	} while (0)

#define HTTP_REDIRECT(status, location) \
	do { \
		HTTP_WRITE("HTTP/1.1 " status "\r\n" \
		           "Connection: close\r\n" \
		           "Location: "); \
		HTTP_WRITE_DYNAMIC(location); \
		HTTP_WRITE("\r\n\r\n"); \
		HTTP_EOF(); \
	} while (0)

#define HTTP_BODY() HTTP_WRITE("\r\n")

#endif // PAGE_H
