#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "../config.h"
#include "../http.h"
#include "../persistence.h"

struct dashboard_page {
	struct session *session;
	struct user *user;

	// Reorder boards form fields
	char *boards_order;
};

void dashboard_page_init(http_context *context);

void write_dashboard_header(http_context *http, uint64 user_id);
void write_dashboard_footer(http_context *http);

#endif // DASHBOARD_H
