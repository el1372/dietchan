#include "print.h"

#include <libowfat/fmt.h>
#include "util.h"
#include "ip.h"

void _print_esc_html(context *ctx, const char *unescaped, ssize_t max_length)
{
	const char *s = unescaped;
	const char *e = max_length>=0?s + max_length - 1:0;
	while (*s != '\0' && (!e || s<=e)) {
		// 0. Zero copy
		char *buf;
		size_t available = context_get_buffer(ctx, (void**)&buf);
		size_t written = 0;
		while (available >= FMT_ESC_HTML_CHAR && *s != '\0' && (!e || s<=e)) {
			written += html_escape_char(buf + written, *s);
			available -= written;
			++s;
		}
		context_consume_buffer(ctx, written);
		if (*s == '\0')
			return;

		// 1. Use buffer for remainder
		char tmp[FMT_ESC_HTML_CHAR*FMT_ESC_HTML_CHAR];
		written = 0;
		for (int i=0; i<FMT_ESC_HTML_CHAR && *s != '\0' && (!e || s<=e); ++i) {
			written += html_escape_char(&tmp[written], *s);
			++s;
		}
		context_write_data(ctx, tmp, written);
	}
}

static void _print_internal(context *ctx, const struct tpl_part *part)
{
	char buf[256];
	switch (part->type) {
		case T_STR:
			context_write_string(ctx, (const char*)part->param0);
			break;
		case T_ESC_HTML:
			_print_esc_html(ctx, (const char*)part->param0, -1);
			break;
		case T_INT:
			context_write_data(ctx, buf, fmt_int(buf, ((int)part->param0)));
			break;
		case T_UINT:
			context_write_data(ctx, buf, fmt_uint(buf, ((unsigned int)part->param0)));
			break;
		case T_LONG:
			context_write_data(ctx, buf, fmt_long(buf, ((long)part->param0)));
			break;
		case T_ULONG:
			context_write_data(ctx, buf, fmt_ulong(buf, ((unsigned long)part->param0)));
			break;
		case T_XLONG:
			context_write_data(ctx, buf, fmt_xlong(buf, ((unsigned long)part->param0)));
			break;
		case T_HTTP_DATE:
			context_write_data(ctx, buf, fmt_httpdate(buf, ((time_t)part->param0)));
			break;
		case T_HUMAN:
			context_write_data(ctx, buf, fmt_human(buf, ((unsigned long long)part->param0)));
			break;
		case T_HUMANK:
			context_write_data(ctx, buf, fmt_humank(buf, ((unsigned long long)part->param0)));
			break;
		case T_IP:
			context_write_data(ctx, buf, fmt_ip(buf, ((struct ip*)part->param0)));
			break;
		case T_TIME_MS:
			context_write_data(ctx, buf, fmt_time(buf, (uint64)part->param0));
			break;
	}
}

void _print(context *ctx, ...)
{
	va_list args;
	va_start(args, ctx);

	while (1) {
		struct tpl_part part=va_arg(args, struct tpl_part);
		if (!part.type) break;
		_print_internal(ctx, &part);
	}

	va_end(args);
}
