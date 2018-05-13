#include "util.h"

#include <ctype.h>
#include <alloca.h>
#include <libowfat/str.h>
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
	// Sadly, dietlibc 0.33 doesn't support SHA256 yet (but will in version 0.34).
	// It also doesn't throw an error for hashing methods it doesn't recognize, but returns
	// garbage that can't be read in the future, in violation of the standard.
	// In addition, it only uses the first 8 characters of the salt, unlike glibc, even for
	// advanced hashing methods.
	// Sigh...
	//strcpy(salt, "$5$"); // SHA256
	strcpy(salt, "$1$"); // MD5
	generate_random_string(&salt[strlen(salt)], 8, salt_charset);

	return crypt(plain_pw, salt);
}
