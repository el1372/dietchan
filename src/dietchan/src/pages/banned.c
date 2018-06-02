#include "banned.h"

#include <libowfat/array.h>
#include "../ip.h"
#include "../tpl.h"
#include "../print.h"
#include "../params.h"
#include "../bans.h"

static int banned_page_header (http_context *http, char *key, char *val);
static int banned_page_finish (http_context *http);
static void banned_page_finalize (http_context *http);

void banned_page_init(http_context *http)
{
	struct banned_page *page = malloc(sizeof(struct banned_page));
	byte_zero(page, sizeof(struct banned_page));

	http->info = page;

	http->header       = banned_page_header;
	http->finish       = banned_page_finish;
	http->finalize     = banned_page_finalize;

	byte_copy(&page->ip, sizeof(struct ip), &http->ip);
}

static int banned_page_header (http_context *http, char *key, char *val)
{
	struct banned_page *page = (struct banned_page*)http->info;

	if (case_equals("X-Forwarded-For", key)) {
		parse_x_forwarded_for(&page->x_forwarded_for, val);
		return 0;
	}

	if (case_equals("X-Real-IP", key)) {
		scan_ip(val, &page->x_real_ip);
		return 0;
	}

	return 0;
}

static void banned_page_print_header(http_context *http, const char *title)
{
	print_page_header(http, E(title));
	PRINT(S("<h1>"), E(title), S("</h1>"));
}

static void banned_page_ban_callback(struct ban *ban, struct ip *ip, void *extra)
{
	http_context *http = extra;
	struct banned_page *page = (struct banned_page*)http->info;
	uint64 now = time(NULL);
	if (ban_type(ban) == BAN_BLACKLIST &&
	    ban_target(ban) == BAN_TARGET_POST &&
	    ((ban_duration(ban) < 0) || (now <= ban_timestamp(ban) + ban_duration(ban)))) {

		uint64 *bids = ban_boards(ban);

		struct ip_range range=ban_range(ban);
		normalize_ip_range(&range);

		if (!page->any_ban)
			banned_page_print_header(http, "Gebannt");
		PRINT(S("<div class='ban'>"
		        "<p>Deine IP ("), IP(*ip), S(") gehört zum Subnetz "), IP(range.ip), S("/"), I64(range.range), S(
		        ", welches aus folgendem Grund gebannt wurde:</p>"
		        "<p>"), ban_reason(ban)?E(ban_reason(ban)):S("<i>Kein Grund angegeben</i>"), S("</p>"
		        "<p>Bretter: "));
		if (!bids) {
			PRINT(S("alle.jpg"));
		} else {
			int comma=0;
			for (int i=0; bids[i] != -1; ++i) {
				struct board *b = find_board_by_id(bids[i]);
				if (!b) continue;

				PRINT(comma?S(", "):S(""), S("/"), E(board_name(b)), S("/"));
				comma=1;
			}
		}
		PRINT(S("<br>"
		        "Gebannt seit: "), HTTP_DATE(ban_timestamp(ban)), S("<br>"
		        "Gebannt bis: "),  ban_duration(ban)<=0?S("<i>Unbegrenzt</i>"):HTTP_DATE(ban_timestamp(ban)+ban_duration(ban)), S(
		        "</p>"
		        "</div>"));

		page->any_ban = 1;
	}
}

static int banned_page_finish (http_context *http)
{
	struct banned_page *page = (struct banned_page*)http->info;

	PRINT_STATUS_HTML("200 OK");
	PRINT_BODY();


	find_bans(&page->ip, banned_page_ban_callback, http);
	if (page->x_real_ip.version)
		find_bans(&page->x_real_ip, banned_page_ban_callback, http);

	size_t count = array_length(&page->x_forwarded_for, sizeof(struct ip));
	for (size_t i=0; i<count; ++i) {
		struct ip *ip=array_get(&page->x_forwarded_for, sizeof(struct ip), i);
		find_bans(ip, banned_page_ban_callback, http);
	}

	if (!page->any_ban) {
		banned_page_print_header(http, "Nicht gebannt");
		PRINT(S("<p>Deine IP scheint derzeit nicht gebannt zu sein.</p>"));
	}

	print_page_footer(http);

	PRINT_EOF();
}

static void banned_page_finalize (http_context *http)
{
	struct banned_page *page = (struct banned_page*)http->info;
	array_reset(&page->x_forwarded_for);
	free(page);
}
