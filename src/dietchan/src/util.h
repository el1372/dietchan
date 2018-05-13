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

#endif // UTIL_H
