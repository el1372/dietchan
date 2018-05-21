#ifndef SESSION_H
#define SESSION_H

#include "http.h"
#include "persistence.h"


struct session* session_update(struct session *session);
void session_destroy(struct session *session);
void purge_expired_sessions();

void print_session(http_context *http, struct session *session);

#define PARAM_SESSION() \
	if (str_equal(key, "session")) { \
		struct session *s = find_session_by_sid(val); \
		s = session_update(s); \
		if (s) \
			page->user = find_user_by_id(session_user(s)); \
		else \
			page->user = 0; \
		page->session = s; \
		return 0; \
	}

#define PRINT_SESSION() do {print_session(http, page->session);} while(0)

#endif // SESSION_H
