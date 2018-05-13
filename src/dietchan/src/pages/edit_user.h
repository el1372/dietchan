#ifndef EDIT_USER_H
#define EDIT_USER_H

#include "../config.h"
#include "../http.h"
#include "../persistence.h"

struct edit_user_page {
	struct session *session;
	struct user *user;
	int64 submitted;
	int64 confirmed;
	char *action;

	int64 user_id;
	char *user_name;
	enum user_type user_type;
	char *boards;
	char *user_email;
	char *user_password;
	char *user_password_confirm;
};

void edit_user_page_init(http_context *http);

#endif // EDIT_USER_H
