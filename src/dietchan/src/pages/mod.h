#ifndef MOD_H
#define MOD_H

#include "../config.h"
#include "../http.h"

struct mod_page {
	struct session *session;
	struct user *user;
	int64 submitted;
	char *action;
	char *redirect;

	// Posts to be deleted
	array posts;
	// Password for deletion (unless mod)
	char *password;

	// Ban stuff
	// IP range for ban. (or multiple ranges if multiple posts were selected, delimited by \n)
	char *ip_ranges;
	int64 global;
	char *boards;
	char *duration;
	char *reason;
	int64 enabled;
	char *ban_target;
	char *ban_type;
	char *ban_message;
	int64 attach_ban_message;
	// When editing or deleting an existing ban
	uint64 ban_id;

	// Report stuff
	char *comment;
	array reports;
};

void mod_page_init(http_context *context);

#endif // MOD_H
