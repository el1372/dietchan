#ifndef THREAD_PAGE_H
#define THREAD_PAGE_H

#include "../config.h"
#include "../http.h"

struct thread_page {
	char  *url;
	char  *board;
	int    thread_id;
	struct session *session;
	struct user *user;
	struct ip ip;
	struct ip x_real_ip;
	array x_forwarded_for;
};

void thread_page_init(http_context *context);

#endif // THREAD_PAGE_H
