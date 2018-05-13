#ifndef LOGIN_H
#define LOGIN_H

#include "../config.h"
#include "../http.h"

struct login_page {
	char *username;
	char *password;
	char *redirect;
	int logout;
	struct session *session;
	struct user *user;
	struct ip ip;
};

void login_page_init(http_context *context);

#endif // LOGIN_H
