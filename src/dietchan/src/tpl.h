#ifndef TPL_H
#define TPL_H

#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include "context.h"

#define S(s) ((struct tpl_part){T_STR, (long long)s})
#define E(s) ((struct tpl_part){T_ESC_HTML, (long long)s})
#define I(i) ((struct tpl_part){T_INT, (long long)i})
#define U(u) ((struct tpl_part){T_UINT, (long long)u})
#define L(l) ((struct tpl_part){T_LONG, (long long)l})
#define UL(ul) ((struct tpl_part){T_ULONG, (long long)ul})
#define X(ul) ((struct tpl_part){T_XLONG, (long long)ul})
#define H(ull) ((struct tpl_part){T_HUMAN, (long long)ull})
#define HK(ull) ((struct tpl_part){T_HUMANK, (long long)ull})
#define HTTP_DATE(t) ((struct tpl_part){T_HTTP_DATE, (long long)t})
#define IP(ip) ((struct tpl_part){T_IP, (long long)(&ip)})
#define TIME_MS(ms) ((struct tpl_part){T_TIME_MS, (long long)ms})
#define TEND    ((struct tpl_part){0})

#define CTX_PRINT(ctx, ...) _print((context*)(ctx), __VA_ARGS__, TEND)
#define PRINT(...) CTX_PRINT(http, __VA_ARGS__)

enum tpl_part_type {
	T_STR = 1,
	T_ESC_HTML,
	T_INT,
	T_UINT,
	T_LONG,
	T_ULONG,
	T_XLONG,
	T_HTTP_DATE,
	T_HUMAN,
	T_HUMANK,
	T_IP,
	T_TIME_MS
};

struct tpl_part {
	int type;
	const long long param0;
};

void _print(context *ctx, ...);

#endif // TPL_H
