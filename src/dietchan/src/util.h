#ifndef UTIL_H
#define UTIL_H

#include <assert.h>
#include <libowfat/array.h>
#include <libowfat/scan.h>
#include <libowfat/fmt.h>

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

size_t byte_str(const char *haystack, size_t haystack_length, const char *needle);
int str_equalb(const char *a, size_t a_length, const char *b);
int str_startb(const char *a, size_t a_length, const char *b);
void array_chop_beginning(array *a, size_t bytes);
size_t scan_whiteskip(const char *s);
size_t scan_nonwhiteskip(const char *s);
size_t scan_quoted_str(const char *s, char *unquoted, size_t *unquoted_length);
size_t scan_json_str(const char *s, char *unquoted, size_t *unquoted_length);
size_t scan_percent_str(const char *s, char *decoded, size_t *decoded_length);

void generate_random_string(char *output, size_t length, const char *charset);

int check_password(const char *crypted_pw, const char *input);
const char *crypt_password(const char *plain_pw);

size_t fmt_time(char *out, uint64 ms);
size_t fmt_escape(char *buf, const char *unescaped);
size_t fmt_escapen(char *buf, const char *unescaped, size_t n);

size_t scan_duration(const char *s, uint64 *duration);
size_t fmt_duration(char *out, uint64 duration);

static inline size_t scan_uint64(const char *src, uint64 *dest)
{
	if (sizeof(unsigned long)==8)
		return scan_ulong(src, (unsigned long*)dest);
	else if (sizeof(unsigned long)==4)
		return scan_ulonglong(src, (unsigned long long*)dest);
	else
		assert(0);
}

static inline size_t scan_int64(const char *src, int64 *dest)
{
	if (sizeof(long)==8)
		return scan_long(src, (long*)dest);
	else if (sizeof(long)==4)
		return scan_longlong(src, (long long*)dest);
	else
		assert(0);
}

static inline size_t scan_xint64(const char *src, uint64 *dest)
{
	if (sizeof(unsigned long)==8)
		return scan_xlong(src, (unsigned long*)dest);
	else if (sizeof(unsigned long)==4)
		return scan_xlonglong(src, (unsigned long long*)dest);
	else
		assert(0);
}

static inline size_t fmt_uint64(char *dest, uint64 src)
{
	if (sizeof(unsigned long)==8)
		return fmt_ulong(dest, (unsigned long)src);
	else if (sizeof(unsigned long)==4)
		return fmt_ulonglong(dest, (unsigned long long)src);
	else
		assert(0);
}

static inline size_t fmt_int64(char *dest, uint64 src)
{
	if (sizeof(long)==8)
		return fmt_long(dest, (long)src);
	else if (sizeof(long)==4)
		return fmt_longlong(dest, (long long)src);
	else
		assert(0);
}

static inline size_t fmt_xint64(char *dest, uint64 src)
{
	if (sizeof(unsigned long)==8)
		return fmt_xlong(dest, (unsigned long)src);
	else if (sizeof(unsigned long)==4)
		return fmt_xlonglong(dest, (unsigned long long)src);
	else
		assert(0);
}

#define FMT_ESC_HTML_CHAR 6

static inline size_t html_escape_char(char *output, char character)
{
	const char *entity = &character;
	size_t len = 1;

	#define ENTITY(s) do { \
		entity = s; \
		len = strlen(s); \
	}  while (0)

	switch (character) {
		case '&':  ENTITY("&amp;");  break;
		case '<':  ENTITY("&lt;");   break;
		case '>':  ENTITY("&gt;");   break;
		case '"':  ENTITY("&quot;"); break;
		case '\'': ENTITY("&#x27;"); break;
		case '/':  ENTITY("&#x2F;"); break;
	}
	#undef ENTITY

	if (likely(output))
		strncpy(output, entity, len);
	return len;
}

#endif // UTIL_H
