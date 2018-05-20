#ifndef UTIL_H
#define UTIL_H

#include<unistd.h>
#include<libowfat/array.h>

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

#define FMT_ESC_HTML_CHAR 6

static inline size_t html_escape_char(char *output, char character)
{
	const char *entity;
	switch (character) {
		case '&':  entity = "&amp;";  break;
		case '<':  entity = "&lt;";   break;
		case '>':  entity = "&gt;";   break;
		case '"':  entity = "&quot;"; break;
		case '\'': entity = "&#x27;"; break;
		case '/':  entity = "&#x2F;"; break;
		default:
			if (output)
				*output = character;
			return 1;
	}
	if (output)
		strncpy(output, entity, strlen(entity));
	return strlen(entity);
}

#endif // UTIL_H
