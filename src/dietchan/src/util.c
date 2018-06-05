#include "util.h"

#include <ctype.h>
#include <alloca.h>
#include <libowfat/str.h>
#include <libowfat/fmt.h>
#include <libowfat/scan.h>
#include "arc4random.h"

size_t byte_str(const char *haystack, size_t haystack_length, const char *needle)
{
	size_t needle_length = strlen(needle);
	if (haystack_length >= needle_length) {
		for (size_t i = 0; i<=haystack_length-needle_length; ++i) {
			if (str_start(&haystack[i], needle)) return i;
		}
	}
	return haystack_length;
}

int str_equalb(const char *a, size_t a_length, const char *b)
{
	int i;
	for (i=0; i<a_length && b[i] != '\0'; ++i) {
		if (a[i] != b[i]) return 0;
	}
	return b[i] == '\0';
}

int str_startb(const char *a, size_t a_length, const char *b)
{
	int i;
	for (i=0; i<a_length && b[i] != '\0'; ++i) {
		if (a[i] != b[i]) return 0;
	}
	return 1;
}

void array_chop_beginning(array *a, size_t bytes)
{
	char *p = array_start(a);
	memmove(p, p+bytes, array_bytes(a)-bytes);
	array_truncate(a, 1, array_bytes(a)-bytes);
}

size_t scan_whiteskip(const char *s)
{
	register const char *t=s;
	while (isspace(*t)) ++t;
	return (size_t)(t-s);
}

size_t scan_nonwhiteskip(const char *s)
{
	register const char *t=s;
	while (*t && !isspace(*t)) ++t;
	return (size_t)(t-s);
}

size_t scan_quoted_str(const char *s, char *unquoted, size_t *unquoted_length)
{
	size_t l = 0;
	const char *t = s;
	char *o = unquoted;
	int escaped = 0;
	for (; *t; ++t) {
		if (!escaped) {
			if (*t == '"') {
				++t;
				break;
			} else if (*t == '\\')
				escaped = 1;
			else {
				if (o) {
					*o = *t;
					++o;
				}
				++l;
			}
		} else {
			escaped = 0;
			if (o) {
				*o = *t;
				++o;
			}
			++l;
		}
	}
	if (unquoted_length)
		*unquoted_length = l;

	return (size_t)(t-s);
}

size_t scan_json_str(const char *s, char *unquoted, size_t *unquoted_length)
{
	size_t l = 0;
	const char *t = s;
	char *o = unquoted;
	int escaped = 0;
	for (; *t; ++t) {
		if (!escaped) {
			if (*t == '"') {
				++t;
				break;
			} else if (*t == '\\')
				escaped = 1;
			else {
				if (o) {
					*o = *t;
					++o;
				}
				++l;
			}
		} else {
			escaped = 0;
			if (unlikely(*t == 'u')) {
				uint64 code=0;
				size_t scanned= scan_xint64(t+1, &code);
				if (scanned) {
					size_t charlen = fmt_utf8(o, code);
					l += charlen;
					if (o)
						o+=charlen;
					t += scanned;
				}
			} else {
				char c = 0;
				switch (*t) {
					case 'b':  c = '\b'; break;
					case 'f':  c = '\f'; break;
					case 'n':  c = '\n'; break;
					case 'r':  c = '\r'; break;
					case 't':  c = '\t'; break;
					default:   c = *t; break;
				}
				if (o) {
					*o = c;
					++o;
				}
				++l;
			}
		}
	}
	if (unquoted_length)
		*unquoted_length = l;

	return (size_t)(t-s);
}

size_t scan_percent_str(const char *s, char *decoded, size_t *decoded_length)
{
	size_t l = 0;
	const char *t = s;
	char *o = decoded;
	int reached_end = 0;
	for (; *t; ++t) {
		long c;
		switch (*t) {
			case '%':
				++t;
				c='%';
				t += scan_xlongn(t, 2, &c) - 1;
				break;
			case '+':
				c=' ';
				break;
			case '&':
			case '=':
			case '#':
				reached_end = 1;
			default:
				c = *t;
		}

		if (reached_end)
			break;

		if (o) {
			*o = (char)c;
			++o;
		}
		++l;
	}
	if (decoded_length)
		*decoded_length = l;

	return (size_t)(t-s);

}


void generate_random_string(char *output, size_t length, const char *charset)
{
	size_t charset_size = strlen(charset);
	for (size_t i=0; i<length; ++i) {
		int index = arc4random_uniform(charset_size);
		output[i] = charset[index];
	}
}

int check_password(const char *crypted_pw, const char *input)
{
	char *salt = alloca(strlen(crypted_pw) + strlen(input)+1);
	strcpy(salt, crypted_pw);
	char *delimiter = &salt[str_rchr(salt, '$')];
	if (!delimiter || *delimiter == '\0')
		return 0; // Should not happen
	*delimiter = '\0';

	char *crypted = crypt(input, salt);
	return str_equal(crypted, crypted_pw);
}

const char *crypt_password(const char *plain_pw)
{
	static const char *salt_charset = "abcdefghijklmnopqrstuvwxyz"
	                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	                                  "0123456789";
	char salt[16];
	// Note: dietlibc <= 0.33 doesn't support sha256.
	strcpy(salt, "$5$"); // SHA256
	//strcpy(salt, "$1$"); // MD5
	generate_random_string(&salt[strlen(salt)], sizeof(salt)-strlen(salt)-1, salt_charset);
	salt[sizeof(salt)-1] = '\0';

	return crypt(plain_pw, salt);
}

size_t fmt_escape(char *buf, const char *unescaped)
{
	size_t escaped_length = 0;
	const char *c;
	if (!buf) {
		for (c = unescaped; *c != '\0'; ++c) {
			escaped_length += html_escape_char(FMT_LEN, *c);
		}
	} else {
		char *o = buf;
		for (c = unescaped; *c != '\0'; ++c) {
			size_t d = html_escape_char(o, *c);
			o += d;
			escaped_length += d;
		}
	}
	return escaped_length;
}

size_t fmt_escapen(char *buf, const char *unescaped, size_t n)
{
	size_t escaped_length = 0;
	const char *c;
	if (!buf) {
		for (c = unescaped; (*c != '\0') && ((c-unescaped) < n); ++c) {
			escaped_length += html_escape_char(FMT_LEN, *c);
		}
	} else {
		char *o = buf;
		for (c = unescaped; (*c != '\0') && ((c-unescaped) < n); ++c) {
			size_t d = html_escape_char(o, *c);
			o += d;
			escaped_length += d;
		}
	}
	return escaped_length;
}

size_t fmt_time(char *out, uint64 ms)
{
	uint64 t = ms;
	/*uint64 msecs  = t % 1000;*/ t /= 1000;
	uint64 secs   = t % 60;   t /= 60;
	uint64 mins   = t % 60;   t /= 60;
	uint64 hours  = t;

	char *o = out;
	if (hours > 0) {
		o += fmt_int(out?o:FMT_LEN, hours);
		if (out) *o = ':';
		++o;
	}
	o += fmt_uint0(out?o:FMT_LEN, mins, (hours>0)?2:1);
	if (out) *o = ':';
	++o;
	o += fmt_uint0(out?o:FMT_LEN, secs, 2);

	return o-out;
}

size_t scan_duration(const char *s, uint64 *duration)
{
	const char *e = s;
	*duration = 0;
	while (1) {
		size_t d;

		e += scan_whiteskip(e);

		int64 t=0;
		e += (d = scan_int64(e,&t));
		if (d<=0)
			break;
		if (t <= 0) {
			if (*duration > 0)
				e -= d;
			break;
		}

		e += scan_whiteskip(e);

		switch (*e++) {
		case 's': t *= 1; break;
		case 'm': t *= 60; break;
		case 'h': t *= 60*60; break;
		case 'd': t *= 60*60*24; break;
		case 'w': t *= 60*60*24*7; break;
		case 'M': t *= 60*60*24*30; break;
		case 'y': t *= 60*60*24*365; break;
		default: return 0;
		}

		*duration += t;
	}
	return e-s;
}

size_t fmt_duration(char *out, uint64 duration)
{
	char *s=out;
	uint64 t = duration;
	uint64 r;
	r = t / (60*60*24*365); t %= (60*60*24*365);
	if (r>0) {
		s += fmt_uint64(s, r);
		*s++ = 'y';
	}
	r = t / (60*60*24*30); t %= (60*60*24*30);
	if (r>0) {
		s += fmt_uint64(s, r);
		*s++ = 'M';
	}
	r = t / (60*60*24*7); t %= (60*60*24*7);
	if (r>0) {
		s += fmt_uint64(s, r);
		*s++ = 'w';
	}
	r = t / (60*60*24); t %= (60*60*24);
	if (r>0) {
		s += fmt_uint64(s, r);
		*s++ = 'd';
	}
	r = t / (60*60); t %= (60*60);
	if (r>0) {
		s += fmt_uint64(s, r);
		*s++ = 'h';
	}
	r = t / (60); t %= (60);
	if (r>0) {
		s += fmt_uint64(s, r);
		*s++ = 'm';
	}
	if (r>0) {
		s += fmt_uint64(s, r);
		*s++ = 's';
	}
	return s-out;
}
