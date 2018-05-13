#ifndef BOARD_H
#define BOARD_H

#include "../config.h"
#include "../http.h"

struct board_page {
	char  *url;
	char  *board;
	struct session *session;
	struct user *user;
	struct ip ip;
	struct ip x_real_ip;
	array x_forwarded_for;
	int64 page;
};

void board_page_init(http_context *context);

#endif // BOARD_H
