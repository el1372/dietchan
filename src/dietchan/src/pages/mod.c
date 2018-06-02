#define _XOPEN_SOURCE
#define _GNU_SOURCE

#include "mod.h"

#include <unistd.h>
#include <assert.h>
#include <libowfat/case.h>
#include <libowfat/scan.h>
#include <libowfat/byte.h>
#include <libowfat/fmt.h>
#include <libowfat/str.h>
#include <libowfat/ip4.h>
#include <libowfat/ip6.h>

#include "../tpl.h"
#include "../util.h"
#include "../permissions.h"


static int  mod_page_get_param (http_context *http, char *key, char *val);
static int  mod_page_post_param (http_context *http, char *key, char *val);
static int  mod_page_cookie (http_context *http, char *key, char *val);
static int  mod_page_finish (http_context *http);
static void mod_page_finalize(http_context *http);

static int  can_delete_post(struct mod_page *page, struct post *post);
static int can_delete_ban(struct user *user, struct ban *ban);

void mod_page_init(http_context *http)
{
	struct mod_page *page = malloc(sizeof(struct mod_page));
	byte_zero(page, sizeof(struct mod_page));
	http->info = page;

	http->get_param    = mod_page_get_param;
	http->post_param   = mod_page_post_param;
	http->cookie       = mod_page_cookie;
	http->finish       = mod_page_finish;
	http->finalize     = mod_page_finalize;

	page->action = strdup("");
	page->ban_id = 0;
	page->enabled = 1L;
}

static int  mod_page_get_param (http_context *http, char *key, char *val)
{
	struct mod_page *page = (struct mod_page*)http->info;

	PARAM_STR("action", page->action);
	PARAM_I64("ban_id", page->ban_id);
	PARAM_REDIRECT("redirect", page->redirect);

	return 0;
	//HTTP_FAIL(BAD_REQUEST);
}

static int  mod_page_post_param (http_context *http, char *key, char *val)
{
	struct mod_page *page = (struct mod_page*)http->info;

	PARAM_STR("action", page->action);
	PARAM_I64("submitted", page->submitted);
	PARAM_REDIRECT("redirect", page->redirect);

	// General parameters for deleting/banning/closing/pinning
	if (case_equals(key, "post")) {
		uint64 tmp;
		if (val[scan_uint64(val, &tmp)] != '\0')
			HTTP_FAIL(BAD_REQUEST);
		int64 count=array_length(&page->posts, sizeof(uint64));
		uint64 *id=array_allocate(&page->posts, sizeof(uint64), count);
		*id = tmp;
		return 0;
	}

	// Params for deletion
	PARAM_STR("password", page->password);

	// Params for ban
	PARAM_I64("ban_id",   page->ban_id);
	PARAM_STR("ip_ranges", page->ip_ranges);
	PARAM_STR("reason",   page->reason);
	PARAM_STR("duration", page->duration);

	PARAM_STR("boards", page->boards);
	PARAM_I64("global", page->global);
	PARAM_I64("enabled", page->enabled);
	PARAM_STR("ban_target",  page->ban_target);
	PARAM_STR("ban_type",  page->ban_type);
	PARAM_STR("ban_message", page->ban_message);
	PARAM_I64("attach_ban_message", page->attach_ban_message);

	// Params for report
	PARAM_STR("comment", page->comment);
	if (case_equals(key, "report")) {
		uint64 tmp;
		if (val[scan_uint64(val, &tmp)] != '\0')
			HTTP_FAIL(BAD_REQUEST);
		int64 count=array_length(&page->reports, sizeof(uint64));
		uint64 *id=array_allocate(&page->reports, sizeof(uint64), count);
		*id = tmp;
		return 0;
	}

	return 0;
	//HTTP_FAIL(BAD_REQUEST);
}

static int mod_page_cookie (http_context *http, char *key, char *val)
{
	struct mod_page *page = (struct mod_page*)http->info;
	PARAM_SESSION();
	return 0;
}

static void mod_write_header(http_context *http)
{
	PRINT(S("<!DOCTYPE html>"
	        "<html>"
	          "<head>"
	            "<style>"
	              "th {"
	                "text-align:left;"
	              "}"
	              "th, td {"
	                "padding-left: 8px;"
	                "padding-right: 8px;"
	                "vertical-align: baseline;"
	              "}"
	              "table {"
	                "border-spacing: 0;"
	                "margin-left: -6px;"
	                "margin-right: -6px;"
	              "}"
	              "label {"
	                "font-weight: bold;"
	              "}"
	              "tr input[type=text],"
	              "tr input[type=password],"
	              "tr textarea {"
	                "box-sizing: border-box;"
	                "width: 100%;"
	                "vertical-align: baseline;"
	              "}"
	              "tr input[type=checkbox],"
	              "tr input[type=radio] {"
	                "position: relative;"
	                "top: 2px;"
	              "}"
	              "p.error {"
	                "color: #f00;"
	              "}"
	            "</style>"
	          "</head>"
	          "<body>"));
}

static void mod_write_footer(http_context *http)
{
	PRINT(S("</body></html>"));
}
static void add_default_range_for_ip(array *ranges, struct ip *ip)
{
	// Create default range ban for IP
	struct ip_range range = {0};
	range.ip = *ip;
	switch (range.ip.version) {
		case IP_V4: range.range = 32; break;
		case IP_V6: range.range = 64; break;
	}

	size_t count = array_length(ranges, sizeof(struct ip));
	// Check if ip is duplicate (O(n^2))
	int dup=0;
	for (int j=0; j<count; ++j) {
		if (byte_equal(&range, sizeof(struct ip_range), array_get(ranges, sizeof(struct ip_range), j))) {
			dup = 1;
			break;
		}
	}
	// Add
	if (!dup) {
		struct ip_range *r =  array_allocate(ranges, sizeof(struct ip_range), count);
		*r = range;
	}
}

static int  mod_page_finish (http_context *http)
{
	struct mod_page *page = (struct mod_page*)http->info;

	array ranges = {0};
	array boards = {0};

	struct ban *ban = find_ban_by_id(page->ban_id);

	if (!page->duration) {
		if (ban) {
			page->duration = malloc(256);
			page->duration[fmt_duration(page->duration, ban_duration(ban))] = '\0';
		} else {
			page->duration = strdup("");
		}
	}
	if (!page->reason)
		page->reason = (ban&&ban_reason(ban))?strdup(ban_reason(ban)):strdup("");
	if (!page->ban_target) {
		if (ban) {
			switch (ban_target(ban)) {
				case BAN_TARGET_POST:   page->ban_target=strdup("posts"); break;
				case BAN_TARGET_REPORT: page->ban_target=strdup("reports"); break;
			}
		} else {
			page->ban_target = strdup("posts");
		}
	}
	if (!page->ban_type) {
		if (ban) {
			switch (ban_type(ban)) {
				case BAN_BLACKLIST:         page->ban_type=strdup("blacklist"); break;
				case BAN_CAPTCHA_PERMANENT: page->ban_type=strdup("captcha"); break;
				case BAN_CAPTCHA_ONCE:      page->ban_type=strdup("captcha_once"); break;
			}
		} else {
			page->ban_type = strdup("blacklist");
		}
	}
	if (!page->ban_message)
		page->ban_message = strdup(DEFAULT_BAN_MESSAGE);


	int do_ban, do_delete, do_close, do_pin, do_report, do_delete_report, do_delete_ban, do_it;
	do_ban = do_delete = do_close = do_pin = do_report = do_delete_report = do_delete_ban = do_it = 0;
	if (case_equals(page->action, "ban"))            { do_ban = 1;                 } else
	if (case_equals(page->action, "edit_ban"))       { do_ban = 1;                 } else
	if (case_equals(page->action, "delete"))         { do_delete = 1; do_it  = 1;  } else
	if (case_equals(page->action, "delete_and_ban")) { do_delete = 1; do_ban = 1;  } else
	if (case_equals(page->action, "close"))          { do_close = 1;  do_it  = 1;  } else
	if (case_equals(page->action, "pin"))            { do_pin = 1;    do_it  = 1;  } else
	if (case_equals(page->action, "report"))         { do_report = 1;              } else
	if (case_equals(page->action, "delete_report"))  { do_it = 1;                  } else
	if (case_equals(page->action, "delete_ban"))     { do_delete_ban = 1;          }
	if (page->submitted)
		do_it = 1;

	// Deal with reports
	if (array_bytes(&page->reports)) {
		// We always want to delete the report after dealing with it
		do_delete_report = 1;

		// Select the corresponding posts
		size_t length = array_length(&page->reports, sizeof(uint64));
		size_t j=0;
		for (size_t i=0; i<length; ++i) {
			uint64 *id = array_get(&page->reports, sizeof(uint64), i);
			struct report *report = find_report_by_id(*id);
			if (report) {
				struct board *board = find_board_by_id(report_board_id(report));
				if (is_mod_for_board(page->user, board)) {
					uint64 *post  = array_allocate(&page->posts, sizeof(uint64), j++);
					*post = report_post_id(report);
				}
			}
		}
	}

	int do_something_with_post = do_ban || do_delete || do_close || do_pin || do_report;

	if (!page->user && (do_ban || do_close || do_pin || do_delete_report)) {
		PRINT_STATUS_HTML("403 Verboten");
		PRINT_BODY();
		PRINT(S("<p>Netter Versuch.</p><p><small><a href='"),S(PREFIX),S("/login'>Sitzung abgelaufen?</a></small></p>"));
		PRINT_EOF();
		goto cleanup;
	}

	if (ban && !can_see_ban(page->user, ban)) {
		PRINT_STATUS_HTML("403 Verboten");
		PRINT_BODY();
		PRINT(S("<p>Du hast keine Zugriffsrechte für diesen Bann.</p>"));
		PRINT_EOF();
		goto cleanup;
	}

	PRINT_STATUS_HTML("200 Ok");
	PRINT_SESSION();
	PRINT_BODY();
	mod_write_header(http);
	PRINT(S("<form method='post'>"
	        "<input type='hidden' name='submitted' value='1'>"
	        "<input type='hidden' name='action' value='"), E(page->action), S("'>"));

	// Parse IPs
	int collect_ips = 0;
	int parse_error_ip = 0;
	if (page->ip_ranges && !str_equal(page->ip_ranges, "")) {
		char *s = page->ip_ranges;
		int i = 0;
		while (1) {
			i += scan_whiteskip(&s[i]);

			if (!s[i])
				break;

			struct ip_range range = {0};
			int d = 0;
			if (d = scan_ip4(&s[i], &range.ip.bytes[0])) {
				range.ip.version = IP_V4;
				range.range = 32;
			} else if (d = scan_ip6(&s[i], &range.ip.bytes[0])) {
				range.ip.version = IP_V6;
				range.range = 64;
			} else {
				parse_error_ip = 1;
				i += scan_nonwhiteskip(&s[i]);
				continue;
			}

			i += d;

			if (s[i] == '/') {
				i++;
				i += (d=scan_uint(&s[i], &range.range));
				if (!d) {
					parse_error_ip = 1;
					i += scan_nonwhiteskip(&s[i]);
					continue;
				}
			}

			size_t count = array_length(&ranges, sizeof(struct ip_range));
			struct ip_range *member = array_allocate(&ranges, sizeof(struct ip_range), count);
			*member = range;
		}
	} else {
		collect_ips = 1;
		if (ban) {
			size_t count = array_length(&ranges, sizeof(struct ip_range));
			struct ip_range *member = array_allocate(&ranges, sizeof(struct ip_range), count);
			*member = ban_range(ban);
		}
	}

	if (parse_error_ip) {
		PRINT(S("<p class='error'>Ungültiger IP-Adress-Bereich</p>"));
		do_it = 0;
	}

	// Parse boards
	size_t boards_count = 0;
	int collect_boards = 0;
	if (!page->global) {
		if (page->boards && !str_equal(page->boards, "")) {
			boards_count = parse_boards(http, page->boards, &boards, &do_it);
		} else {
			collect_boards = 1;
			if (ban) {
				if (!ban_boards(ban)) {
					page->global = 1;
				} else {
					uint64 *bid = ban_boards(ban);
					while (*bid != -1) {
						struct board *board = find_board_by_id(*bid);
						if (board) {
							struct board **member = array_allocate(&boards, sizeof(struct board*), boards_count);
							*member = board;
							++boards_count;
						}
						++bid;
					}
				}
			}
		}
	}

	// Parse ban duration
	int64 duration = 0;
	if (do_ban && page->submitted) {
		// FIXME: Right now parsed as seconds.
		// TODO: Allow more complex format like (1d 12h)

		if (str_equal(page->duration, "") ||
			page->duration[scan_duration(page->duration, &duration)] != '\0') {
			PRINT(S("<p class='error'>Ungültige Bann-Dauer</p>"));
			do_it = 0;
		}
	}

	// Ban form validation
	int64 post_count = array_length(&page->posts, sizeof(uint64));
	ssize_t range_count = array_length(&ranges, sizeof(struct ip_range));
	if (do_ban && page->submitted) {
		if (range_count <= 0) {
			PRINT(S("<p class='error'>Es muss mindestens eine IP-Adresse eingegeben werden.</p>"));
			do_it = 0;
		}

		if (!page->global && boards_count <= 0) {
			PRINT(S("<p class='error'>Es muss mindestens ein Brett angegeben werden.</p>"));
			do_it = 0;
		}
	}
	if (do_delete_ban) {
		if (!ban) {
			PRINT(S("<p class='error'>Bann existiert nicht.</p>"));
			do_it = 0;
		}
		if (!can_delete_ban(page->user,ban)) {
			PRINT(S("<p class='error'>Du kannst diesen Bann nicht löschen.</p>"));
			do_it = 0;
		}
	}

	if ((do_delete || do_report || do_pin || do_close) && post_count <= 0) {
		PRINT(S("<p>Kein Post ausgewählt.</p>"));
		do_it = 0;
	}

	// ---------------------------------------------------------------------------------------------

	if (do_it)
		begin_transaction();


	// Do bans first as they must happen before the posts are deleted
	uint64 timestamp = time(NULL);

	if (do_ban && do_it) {
		// Create bans for all ranges
		for (ssize_t i=0; i<range_count; ++i) {
			struct ip_range *range = array_get(&ranges, sizeof(struct ip_range), i);

			// Mark all corresponding posts as banned (O(n^2)).
			// Also used to find post that contributed to ban.
			uint64 pid = 0;
			for (ssize_t j=0; j<post_count; ++j) {
				uint64 _pid = *((uint64*)array_get(&page->posts, sizeof(uint64), j));
				struct post *post = find_post_by_id(_pid);
				if (!post) continue;
				if (page->attach_ban_message) {
					post_set_banned(post, 1);
					post_set_ban_message(post, page->ban_message);
				}
				struct ip pip = post_ip(post);
				if (ip_in_range(range, &pip)) {
					pid = _pid;
				}
			}

			int new_ban = !ban;
			if (!new_ban) {
				// Remember old timestamp in case we have to split the ban
				timestamp = ban_timestamp(ban);

				// Check if user can edit the entire ban or if we have to split it into two bans
				if (user_boards(page->user)) {
					int64 *bids = ban_boards(ban);
					// If the ban was global, first replace it with a ban on all boards.
					if (!bids) {
						size_t board_count = 0;
						for (struct board *b = master_first_board(master); b; b=board_next_board(b))
							++board_count;
						bids = db_alloc(db, sizeof(int64)*(board_count+1));
						int i = 0;
						for (struct board *b = master_first_board(master); b; b=board_next_board(b))
							bids[i++] = board_id(b);
						bids[board_count] = -1;
						db_invalidate_region(db, bids, sizeof(int64)*(board_count+1));
						ban_set_boards(ban, bids);
					}

					// Remove all the boards for which the user has mod rights.
					// Keep remaining boards
					int j=0;
					for (int i=0; bids[i] != -1; ++i) {
						struct board *b = find_board_by_id(bids[i]);
						if (!is_mod_for_board(page->user, b))
							bids[j++] = bids[i];
					}
					bids[j] = -1;
					db_invalidate_region(db, bids, sizeof(int64)*(boards_count+1));

					// If no boards remain, it means we have mod rights for all boards of the ban,
					// so we will edit the old ban. Otherwise we create a new ban.
					new_ban = (j != 0);
				}
			}

			if (new_ban) {
				// Create ban
				ban = ban_new();
				uint64 ban_counter = master_ban_counter(master) +1;
				master_set_ban_counter(master, ban_counter);
				ban_set_id(ban, ban_counter);
			}
			ban_set_enabled(ban,    page->enabled);

			if (case_equals(page->ban_type, "blacklist"))
				ban_set_type(ban,   BAN_BLACKLIST);
			if (case_equals(page->ban_type, "captcha"))
				ban_set_type(ban,   BAN_CAPTCHA_PERMANENT);
			if (case_equals(page->ban_type, "captcha_once"))
				ban_set_type(ban,   BAN_CAPTCHA_ONCE);

			if (case_equals(page->ban_target, "posts"))
				ban_set_target(ban, BAN_TARGET_POST);
			if (case_equals(page->ban_target, "reports"))
				ban_set_target(ban, BAN_TARGET_REPORT);

			ban_set_range(ban,     *range);
			ban_set_timestamp(ban,  timestamp);
			ban_set_duration(ban,   duration);
			ban_set_post(ban,       pid);
			ban_set_reason(ban,     page->reason);
			ban_set_mod(ban,        user_id(page->user));
			ban_set_mod_name(ban,   user_name(page->user));

			if (ban_boards(ban)) {
				db_free(db, ban_boards(ban));
				ban_set_boards(ban, 0);
			}
			if (!page->global) {
				// Board-specific ban
				if (ban_boards(ban))
					db_free(db, ban_boards(ban));

				int64 *bids = db_alloc(db, sizeof(int64)*(boards_count+1));
				int j=0;
				for (int i=0; i<boards_count; ++i) {
					struct board **board = array_get(&boards, sizeof(struct board*), i);
					if (is_mod_for_board(page->user, *board))
						bids[j++] = board_id(*board);
				}
				bids[j] = -1;
				db_invalidate_region(db, bids, sizeof(int64)*(boards_count+1));
				ban_set_boards(ban, bids);

				if (j==0) {
					// This is a problem: Ban has no boards. Can happen. Bail out.
					ban_free(ban);
					ban = 0;
					PRINT(S("<p>Ungültiger Bann. Der Bann enthält keine gültigen Bretter (möglicherweise"
					        " hast du Bretter angegeben, für die du keine Moderationsrechte besitzt). Verworfen.</p>"));
				}
			} else if (user_boards(page->user)) {
				// Not a global mod, restrict "global" ban to boards the user has access to
				int64 *u_bids = user_boards(page->user);
				size_t boards_count=0;
				while (u_bids[boards_count] != -1) ++boards_count;
				int64 *b_bids = db_alloc(db, (boards_count+1)*sizeof(int64));
				for (int i=0; i < boards_count+1; ++i)
					b_bids[i] = u_bids[i];
				db_invalidate_region(db, b_bids, (boards_count+1)*sizeof(int64));

				ban_set_boards(ban, b_bids);
			}

			if (ban) {
				if (new_ban)
					insert_ban(ban);
				else
					update_ban(ban);

				if (case_equals(page->action, "ban") || case_equals(page->action, "delete_and_ban")) {
					PRINT(S("<p>Bann erstellt</p>"));
				} else {
					PRINT(S("<p>Bann geändert</p>"));
				}

				ban = 0;
			}
		}
	}

	// Do deletion/closing/pinning

	int any_valid_post = 0;

	if (do_something_with_post) {
		for (ssize_t i=0; i<post_count; ++i) {
			uint64 *id = array_get(&page->posts, sizeof(uint64), i);
			struct post *post = find_post_by_id(*id);

			if (!post) {
				PRINT(S("<p>Post "), U64(*id), S(" nicht gefunden.</p>"));
				continue;
			}

			if (do_close || do_pin) {
				struct board *board = thread_board(post_thread(post));
				if (!is_mod_for_board(page->user, board)) {
					PRINT(S("<p>Post "), U64(*id), S(" gehört zum Brett /"), E(board_name(board)), S("/,"
					        " für welches du keine Moderationsrechte besitzt. IGNORIERT.</p>"));
					continue;
				}
			}

			if (!do_it) {
				if (do_ban && collect_ips) {
					// Add all recorded IPs unless they are internal (like 127.0.0.1)
					if (is_external_ip(&post_ip(post)))
						add_default_range_for_ip(&ranges, &post_ip(post));
					if (is_external_ip(&post_x_real_ip(post)))
						add_default_range_for_ip(&ranges, &post_x_real_ip(post));
					struct ip *ips = post_x_forwarded_for(post);
					for (size_t i; i<post_x_forwarded_for_count(post); ++i) {
						if (is_external_ip(&ips[i]))
							add_default_range_for_ip(&ranges, &ips[i]);
					}
				}

				if (do_ban && collect_boards) {
					struct board *board = thread_board(post_thread(post));
					int dup = 0;
					for (int j=0; j<boards_count; ++j) {
						struct board **member = array_get(&boards, sizeof(struct board*), j);
						if (*member == board) {
							dup = 1;
							break;
						}
					}
					if (!dup) {
						struct board **member = array_allocate(&boards, sizeof(struct board*), boards_count);
						*member = board;
						++boards_count;
					}
				}

				if (do_report) {
					if (post_reported(post)) {
						PRINT(S("<p>Post "), U64(*id), S(" wurde bereits gemeldet."));
						continue;
					}
				}

				any_valid_post = 1;
				PRINT(S("<input type='hidden' name='post' value='"), U64(*id), S("'>"));
			} else {
				struct thread *thread = post_thread(post);
				if (do_pin) {
					if (thread_first_post(thread) != post) {
						PRINT(S("<p>Post "), U64(*id), S(" ist kein Thread und kann daher nicht angepinnt werden."));
						continue;
					}

					thread_set_pinned(thread, !thread_pinned(thread));

					if (thread_pinned(thread))
						bump_thread(thread);

				}
				if (do_close) {
					if (thread_first_post(thread) != post) {
						PRINT(S("<p>Post "), U64(*id), S(" ist kein Thread und kann daher nicht geschlossen werden."));
						continue;
					}

					thread_set_closed(thread, !thread_closed(thread));
				}
				if (do_report) {
					if (!post_reported(post)) {
						post_set_reported(post, 1);
						struct report *report = report_new();
						uint64 report_counter = master_report_counter(master) +1;
						master_set_report_counter(master, report_counter);
						report_set_id(report, report_counter);
						report_set_post_id(report, post_id(post));
						report_set_thread_id(report, post_id(thread_first_post(thread)));
						report_set_board_id(report, board_id(thread_board(thread)));
						if (case_equals(page->reason, "spam"))
							report_set_type(report, REPORT_SPAM);
						else if (case_equals(page->reason, "illegal"))
							report_set_type(report, REPORT_ILLEGAL);
						else
							report_set_type(report, REPORT_OTHER);
						if (page->comment)
							report_set_comment(report, page->comment);

						report_set_timestamp(report, timestamp);

						struct report *prev = master_last_report(master);
						report_set_prev_report(report, prev);
						if (prev)
							report_set_next_report(prev, report);
						else
							master_set_first_report(master, report);
						master_set_last_report(master, report);
					}
				}
				if (do_delete) {
					if (!can_delete_post(page, post)) {
						PRINT(S("<p>Post "), U64(*id), S(" NICHT gelöscht. Falsches Passwort.</p>"));
						continue;
					}

					PRINT(S("<p>Post "), U64(*id), S(" gelöscht.</p>"));

					if (thread_first_post(thread) == post)
						delete_thread(thread);
					else
						delete_post(post);
				}
			}
		}
	}

	// Remove reports
	if (do_delete_report && do_it) {
		size_t length = array_length(&page->reports, sizeof(uint64));
		for (size_t i=0; i<length; ++i) {
			uint64 *id = array_get(&page->reports, sizeof(uint64), i);
			struct report *report = find_report_by_id(*id);
			if (report) {
				struct board *board = find_board_by_id(report_board_id(report));
				if (is_mod_for_board(page->user, board)) {
					delete_report(report);
				}
			}
		}
	}

	// Delete ban
	if (do_delete_ban && do_it) {
		delete_ban(ban);
		PRINT(S("<p>Bann gelöscht.</p>"));
	}

	if (do_it)
		commit();

	// ---------------------------------------------------------------------------------------------

	// Maybe print form (again)
	if (!do_it) {
		// Ban form
		if (do_ban && can_see_ban(page->user, ban)) {
			if (do_delete)
				PRINT(S("<h1>Bannen &amp; Löschen</h1>"));
			else if (ban)
				PRINT(S("<h1>Bann bearbeiten</h1>"));
			else
				PRINT(S("<h1>Bannen</h1>"));

			if (array_bytes(&page->posts) != 0)
				PRINT(S("<p>"), U64(post_count), S(" Post(s)</p>"));

			PRINT(S("<p><table>"));

			if (ban) {
				PRINT(S("<tr>"
				          "<th><label for='enabled'>Aktiv</label></th>"
				          "<td colspan='3'><input type='checkbox' name='enabled' id='enabled' value='1'"),
				          ban_enabled(ban)?S(" checked"):S(""), S(">"
				          "</td>"
				        "</tr>"));
			}
			if (ban || !array_bytes(&page->posts)) {
				PRINT(S("<tr>"
				          "<th><label>Gilt für</label></th>"
				          "<td colspan='3'>"
				            "<select name='ban_target'>"
				              "<option value='posts'"),   (case_equals(page->ban_target, "posts"))?S(" selected"):S(""),   S(">Posts</option>"
				              "<option value='reports'"), (case_equals(page->ban_target, "reports"))?S(" selected"):S(""), S(">Reports</option>"
				            "</select>"
				          "</td>"
				        "</tr>"
				        "<tr>"
				          "<th><label>Bann-Art</label></th>"
				          "<td colspan='3'>"
				            "<select name='ban_type'>"
				              "<option value='blacklist'"), (case_equals(page->ban_type, "blacklist"))?S(" selected"):S(""), S(">Bann</option>"
				              "<option value='captcha'"), (case_equals(page->ban_type, "captcha"))?S(" selected"):S(""), S(">Captcha</option>"
				            "</select>"
				          "</td>"
				        "</tr>"));
			}

			PRINT(S(    "<tr>"
			              "<th><label for='ip_ranges'>IP-Range(s)</label></th>"
			              "<td colspan='3'>"
			                "<textarea name='ip_ranges'>"));
			if (collect_ips) {
				// Print automatically collected IPs if no IPs were submitted
				int count = array_length(&ranges, sizeof(struct ip_range));
				for (int i=0; i<count; ++i) {
					struct ip_range *range = array_get(&ranges, sizeof(struct ip_range), i);
					PRINT(IP(range->ip),S("/"),U64(range->range),S("\n"));
				}
			} else if (page->ip_ranges) {
				// Otherwise echo user submitted ips
				PRINT(E(page->ip_ranges));
			}
			PRINT(S(        "</textarea>"
			              "</td>"
			            "</tr>"
			            "<tr>"
			              "<th rowspan='2'><label>Bretter</label></th>"
			              "<td colspan='3'>"
			                "<input type='radio' name='global' value='1' id='global-1'"), page->global?S(" checked"):S(""),S(">"
			                "<label for='global-1'>Global</label>"
			              "</td>"
			            "</tr>"
			            "<tr>"
			              "<td colspan='2'>"
			                "<input type='radio' name='global' value='0' id='global-0'"), (!page->global)?S(" checked"):S(""),S(">"
			                "<label for='global-0'>Folgende:</label>"
			              "</td>"
			              "<td>"
			                "<input type='text' name='boards' value='"));
			for (int i=0; i<boards_count; ++i) {
				struct board **board = array_get(&boards, sizeof(struct board*), i);
				PRINT(S(board_name(*board)), S(" "));
			}
			PRINT(S(        "'>"
			              "</td>"
			            "</tr>"
			            "<tr>"
			              "<th><label for='duration'>Dauer</label></th>"
			              "<td colspan='3'><input type='text' name='duration' value='"), E(page->duration), S("'></td>"
			            "</tr>"
			            "<tr>"
			              "<th><label for='reason'>Grund</label></th>"
			              "<td colspan='3'><textarea name='reason'>"), E(page->reason), S("</textarea></td>"
			            "</tr>"));
			if (any_valid_post) {
				PRINT(S("<tr>"
				          "<th></th>"
				          "<td width='1'><input type='checkbox' name='attach_ban_message' id='attach_ban_message' value='1' checked></td>"
				          "<td colspan='2'><input type='text' name='ban_message' value='"), E(page->ban_message), S("'></td>"));
			}
			PRINT(S(    "</tr>"
			          "</table></p>"
			          "<p><input type='submit' value='Übernehmen'></p>"));
		}
		// "Delete ban" form
		if (do_delete_ban) {
			PRINT(S("<p><input type='checkbox' name='submitted' id='submitted' value='1'>"
			           "<label for='submitted'>Bann wirklich löschen</label></p>"
			         "<p><input type='submit' value='Löschen'></p>"));
		}

		// Report form
		if (do_report && any_valid_post) {
			PRINT(S("<p><label for='reason'>Grund</label><br>"
			        "<select name='reason'>"
			          "<option value='spam'>Spam</option>"
			          "<option value='illegal'>Illegale Inhalte</option>"
			          "<option value='other'>Sonstiges</option>"
			        "</select></p>"
			        "<p><label for='comment'>Kommentar</label><br>"
			        "<input type='text' name='comment'></p>"
			        "<p><input type='submit' value='Petzen'></p>"));
		}

		// Remember reports
		for (size_t i=0; i<array_length(&page->reports, sizeof(uint64)); ++i) {
			uint64 *id = array_get(&page->reports, sizeof(uint64), i);
			PRINT(S("<input type='hidden' name='report' value='"), U64(*id), S("'>"));
		}

		// Remember redirect
		if (page->redirect)
			PRINT(S("<input type='hidden' name='redirect' value='"), E(page->redirect), S("'>"));
	}

	if (do_report && do_it)
		PRINT(S("<p>Anzeige ist raus.</p>"));

end:
	PRINT(S("</form>"));
	mod_write_footer(http);

	if (do_it && page->redirect)
		PRINT(S("<meta http-equiv='refresh' content='1; "), E(page->redirect), S("'>"));

	PRINT_EOF();

cleanup:
	array_reset(&ranges);
	array_reset(&boards);

	return 0;
}

static void mod_page_finalize(http_context *http)
{
	struct mod_page *page = (struct mod_page*)http->info;
	array_reset(&page->posts);
	array_reset(&page->reports);
	if (page->action)      free(page->action);
	if (page->password)    free(page->password);
	if (page->ip_ranges)   free(page->ip_ranges);
	if (page->boards)      free(page->boards);
	if (page->duration)    free(page->duration);
	if (page->reason)      free(page->reason);
	if (page->ban_target)  free(page->ban_target);
	if (page->ban_type)    free(page->ban_type);
	if (page->comment)     free(page->comment);
	if (page->ban_message) free(page->ban_message);
	free(page);
}

static int  can_delete_post(struct mod_page *page, struct post *post)
{
	// Check if we can delete by password
	if (post_password(post) && !str_equal(post_password(post), "") && page->password &&
	    check_password(post_password(post), page->password))
	    return 1;

	// Check if user is mod
	struct board *board = thread_board(post_thread(post));
	return is_mod_for_board(page->user, board);

	return 0;
}

static int can_delete_ban(struct user *user, struct ban *ban)
{
	if (!user) return 0;
	if (!user_boards(user)) return 1;
	if (user_boards(user) && !ban_boards(ban)) return 0;

	for (int64 *bid = ban_boards(ban); *bid != -1; ++bid) {
		struct board *board = find_board_by_id(*bid);
		if (!board)
			continue;
		if (!is_mod_for_board(user, find_board_by_id(*bid)))
			return 0;
	}

	return 1;
}
