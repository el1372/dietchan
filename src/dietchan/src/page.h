#ifndef PAGE_H
#define PAGE_H

#include <libowfat/iob.h>
#include <libowfat/str.h>
#include <libowfat/case.h>
#include <libowfat/scan.h>
#include <libowfat/fmt.h>
#include "http.h"
#include "persistence.h"
#include "tpl.h"


void print_escaped(context *ctx, const char *unescaped);
void print_session(http_context *http, struct session *session);

void print_page_header(http_context *http);
void print_reply_form(http_context *http, int board, int thread, struct captcha *captcha);
void print_mod_bar(http_context *http, int ismod);
void print_page_footer(http_context *http);
void print_board_bar(http_context *http);
void print_top_bar(http_context *http, struct user *user, const char *url);
void print_bottom_bar(http_context *http);

void print_upload(http_context *http, struct upload *upload);

enum {
	WRITE_POST_IP         = 1 << 0,
	WRITE_POST_USER_AGENT = 1 << 1
};

void print_post(http_context *http, struct post *post, int absolute_url, int flags);
void print_post_url(http_context *http, struct post *post, int absolute);
void print_post_url2(http_context *http, struct board *board, struct thread *thread, struct post *post, int absolute);
void abbreviate_filename(char *buffer, size_t max_length);
size_t estimate_width(const char *buffer);

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

#define HTTP_WRITE_FILE(fd, off, n) do {context_write_file((context*)http, fd, off, n);} while (0)
#define HTTP_WRITE_SESSION() do {print_session(http, page->session);} while(0)
#define HTTP_EOF() do { context_eof((context*)http); } while(0)

#define PRINT_STATUS(status) \
	do { \
		PRINT(S("HTTP/1.1 "), S(status), S("\r\n" \
		        "Connection: close\r\n")); \
	} while (0)

#define PRINT_STATUS_HTML(status) \
	do { \
		PRINT(S("HTTP/1.1 "), S(status), S("\r\n" \
		        "Connection: close\r\n" \
		        "Content-Language: en\r\n" \
		        "Content-Type: text/html; charset=utf-8\r\n")); \
	} while (0)

#define PRINT_STATUS_REDIRECT(status, ...) \
	do { \
		PRINT(S("HTTP/1.1 "), S(status), S("\r\n" \
		        "Connection: close\r\n" \
		        "Location: "), __VA_ARGS__, S("\r\n")); \
	} while (0)
#define PRINT_REDIRECT(status, ...) \
	do { \
		PRINT(S("HTTP/1.1 "), S(status), S("\r\n" \
		        "Connection: close\r\n" \
		        "Location: "), __VA_ARGS__, S("\r\n\r\n")); \
		HTTP_EOF(); \
	} while (0)

#define PRINT_BODY() do { PRINT(S("\r\n")); } while (0)

#endif // PAGE_H
