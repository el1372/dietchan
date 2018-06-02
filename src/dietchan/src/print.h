#ifndef PRINT_H
#define PRINT_H

#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <libowfat/uint64.h>
#include "context.h"

#define S(s) ((struct tpl_part){T_STR, (uint64)s, strlen(s)})
#define E(s) ((struct tpl_part){T_ESC_HTML, (uint64)s, strlen(s)})
#define I64(i) ((struct tpl_part){T_I64, (uint64)i})
#define U64(u) ((struct tpl_part){T_U64, (uint64)u})
#define X64(ul) ((struct tpl_part){T_X64, (uint64)ul})
#define H(ull) ((struct tpl_part){T_HUMAN, (uint64)ull})
#define HK(ull) ((struct tpl_part){T_HUMANK, (uint64)ull})
#define HTTP_DATE(t) ((struct tpl_part){T_HTTP_DATE, (uint64)t})
#define IP(ip) ((struct tpl_part){T_IP, (uint64)(&ip)})
#define TIME_MS(ms) ((struct tpl_part){T_TIME_MS, (uint64)ms})
#define TEND    ((struct tpl_part){0})

#define PRINT(...) _print((context*)http, __VA_ARGS__, TEND)

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
		PRINT_EOF(); \
	} while (0)

#define PRINT_BODY() do { PRINT(S("\r\n")); } while (0)
#define PRINT_EOF() do { context_eof((context*)http); } while(0)

enum tpl_part_type {
	T_STR = 1,
	T_ESC_HTML,
	T_INT,
	T_UINT,
	T_I64,
	T_U64,
	T_X64,
	T_HTTP_DATE,
	T_HUMAN,
	T_HUMANK,
	T_IP,
	T_TIME_MS
};

struct tpl_part {
	int type;
	const uint64 param0;
	const size_t param1;
};

void _print(context *ctx, ...);
void _print_esc_html(context *ctx, const char *unescaped, ssize_t max_length);

#endif // PRINT_H
