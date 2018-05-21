#ifndef PARAMS_H
#define PARAMS_H

#include <libowfat/array.h>
#include <libowfat/str.h>
#include <libowfat/case.h>
#include <libowfat/scan.h>

#include "http.h"
#include "persistence.h"

#define PARAM_I64(name, variable) \
	if (case_equals(key, name)) { if (scan_long(val, &variable) != strlen(val)) HTTP_FAIL(BAD_REQUEST); return 0; }

#define PARAM_X64(name, variable) \
	if (case_equals(key, name)) { if (scan_xlong(val, &variable) != strlen(val)) HTTP_FAIL(BAD_REQUEST); return 0; }

#define PARAM_STR(name, variable) \
	if (case_equals(key, name)) { if (variable) free(variable); variable = strdup(val); return 0; }

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

size_t parse_boards(http_context *http, char *s, array *boards, int *ok);
void parse_x_forwarded_for(array *ips, const char *header);


#endif // PARAMS_H
