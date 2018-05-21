#include "session.h"
#include <time.h>

#include "print.h"

void print_session(http_context *http, struct session *session)
{
	if (!session)
		PRINT(S("Set-Cookie: session=; path="), S(PREFIX), S("/; expires=Thu, 01 Jan 1970 00:00:00 GMT;\r\n"));
}

struct session* session_update(struct session *session)
{
	if (!session) return 0;

	uint64 t = time(0);
	uint64 last_seen = session_last_seen(session);
	int64 timeout = session_timeout(session);
	if (timeout > 0 && t > last_seen + timeout) {
		// Expired
		begin_transaction();

		session_destroy(session);

		commit();

		return 0;
	} else {
		begin_transaction();

		session_set_last_seen(session, t);

		#if 0
		// FIXME: Update IP
		struct ip ip;
		ip.version = IP_V4; // FIXME: Support IP_V6
		memcpy(&ip.bytes[0], http->ip, 4);
		session_set_last_ip(session, ip);
		#endif

		commit();

		return session;
	}
}

void purge_expired_sessions()
{
	struct session *session = master_first_session(master);
	uint64 t = time(0);
	begin_transaction();
	while (session) {
		uint64 last_seen = session_last_seen(session);
		int64 timeout = session_timeout(session);
		struct session *next = session_next_session(session);

		if (timeout > 0 && t >last_seen+timeout)
			session_destroy(session);

		session = next;
	}
	commit();
}
