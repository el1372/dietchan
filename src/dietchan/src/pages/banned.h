#ifndef BANNED_H
#define BANNED_H

#include "../config.h"
#include "../http.h"

struct banned_page {
	struct ip ip;
	struct ip x_real_ip;
	array x_forwarded_for;

	int any_ban;
};

void banned_page_init(http_context *context);

#endif // BANNED_H
