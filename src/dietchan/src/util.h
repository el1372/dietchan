#ifndef UTIL_H
#define UTIL_H

#include<unistd.h>
#include<libowfat/array.h>

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

size_t byte_str(const char *haystack, size_t haystack_length, const char *needle);
int str_equalb(const char *a, size_t a_length, const char *b);
int str_startb(const char *a, size_t a_length, const char *b);
void array_chop_beginning(array *a, size_t bytes);
size_t scan_whiteskip(const char *s);
size_t scan_nonwhiteskip(const char *s);
size_t scan_quoted_str(const char *s, char *unquoted, size_t *unquoted_length);
size_t scan_percent_str(const char *s, char *decoded, size_t *decoded_length);

void generate_random_string(char *output, size_t length, const char *charset);

int check_password(const char *crypted_pw, const char *input);
const char *crypt_password(const char *plain_pw);

size_t fmt_time(char *out, uint64 ms);
size_t fmt_escape(char *buf, const char *unescaped);
size_t fmt_escapen(char *buf, const char *unescaped, size_t n);

size_t scan_duration(const char *s, uint64 *duration);
size_t fmt_duration(char *out, uint64 duration);

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
