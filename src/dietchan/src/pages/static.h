#ifndef STATIC_H
#define STATIC_H

#include <time.h>

#include "../config.h"
#include "../http.h"

struct static_page {
	char *real_path;
	time_t if_modified_since;
};

void static_page_init(http_context *context);

#endif // STATIC_H
