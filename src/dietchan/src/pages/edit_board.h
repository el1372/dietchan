#ifndef EDIT_BOARD_H
#define EDIT_BOARD_H

#include "../config.h"
#include "../http.h"
#include "../persistence.h"

struct edit_board_page {
	struct session *session;
	struct user *user;
	int64 submitted;
	int64 confirmed;
	char *action;
	int64 move;
	int64 board_id;
	char *board_name;
	char *board_title;
	char *board_banners;
};

void edit_board_page_init(http_context *http);

#endif // EDIT_BOARD_H
