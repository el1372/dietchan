#include "board.h"

#include <libowfat/case.h>
#include <libowfat/fmt.h>
#include <libowfat/byte.h>

#include "../util.h"
#include "../tpl.h"
#include "../persistence.h"
#include "../captcha.h"
#include "../bans.h"
#include "../permissions.h"

#include "../locale.h"

static int board_page_request (http_context *http, http_method method, char *path, char *query);
static int board_page_header (http_context *http, char *key, char *val);
static int board_page_get_param (http_context *http, char *key, char *val);
static int board_page_cookie (http_context *http, char *key, char *val);
static int board_page_finish (http_context *http);
static void board_page_finalize (http_context *http);

void board_page_init(http_context *http)
{
	struct board_page *page = malloc(sizeof(struct board_page));
	byte_zero(page, sizeof(struct board_page));

	http->info = page;

	http->request      = board_page_request;
	http->header       = board_page_header;
	http->get_param    = board_page_get_param;
	http->cookie       = board_page_cookie;
	http->finish       = board_page_finish;
	http->finalize     = board_page_finalize;

	byte_copy(&page->ip, sizeof(struct ip), &http->ip);
}

static int board_page_request (http_context *http, http_method method, char *path, char *query)
{
	struct board_page *page = (struct board_page*)http->info;

	char *prefix = alloca(strlen(PREFIX) + 2);
	strcpy(prefix, PREFIX);
	strcat(prefix, "/");

	if (method == HTTP_POST)
		HTTP_FAIL(METHOD_NOT_ALLOWED);

	if (!case_starts(path, prefix))
		HTTP_FAIL(NOT_FOUND);

	// Only missing trailing slash? Redirect.
	if (path[strlen(path)-1] != '/') {
		const char *board_name = path+strlen(prefix);
		if (find_board_by_name(board_name)) {
			PRINT_REDIRECT("301 Moved Permanently", S(prefix), S(board_name), S("/"));
			return ERROR;
		} else
			HTTP_FAIL(NOT_FOUND);
	}

	page->board = strdup(&path[strlen(prefix)]);
	page->board[strlen(page->board)-1] = '\0';

	page->url = strdup(path);

	return 0;
}

static int board_page_header (http_context *http, char *key, char *val)
{
	struct board_page *page = (struct board_page*)http->info;

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

static int board_page_get_param (http_context *http, char *key, char *val)
{
	struct board_page *page = (struct board_page*)http->info;
	PARAM_I64("p", page->page);
	HTTP_FAIL(BAD_REQUEST);
}

static int board_page_cookie (http_context *http, char *key, char *val)
{
	struct board_page *page = (struct board_page*)http->info;
	PARAM_SESSION();
	return 0;
}

static void print_board_nav(http_context *http, struct board *board, int64 current_page)
{
	struct board_page *page = (struct board_page*)http->info;

	int64 page_count=((int64)board_thread_count(board)-1)/THREADS_PER_PAGE + 1;

	PRINT(S("<div class='board-nav'>"));

	if (page_count > 1) {
		for (int64 i=0; i<page_count; ++i) {
			if (i != current_page) {
				PRINT(S("<a class='page' href='"), S(PREFIX), S("/"), E(board_name(board)), S("/"));
				if (i>0)
					PRINT(S("?p="),I64(i));
				PRINT(S("'>["), I64(i), S("]</a>"
				        "<span class='space'> </span>"));
			} else {
				PRINT(S("<span class='page current'>["), I64(i), S("]</span>"
				        "<span class='space'> </span>"));
			}
		}
	}

	PRINT(S("</div>"));

}

static int  board_page_finish (http_context *http)
{
	struct board_page *page = (struct board_page*)http->info;
	struct board* board = (page->board)?find_board_by_name(page->board):0;
	if (!board) {
		PRINT_STATUS_HTML("404 Not Found");
		PRINT_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>404</h1>"
		        "<p>" _("The board was not found") " :(.</p>"));
		PRINT_EOF();
		return ERROR;
	}

	int64 page_count=((int64)board_thread_count(board)-1)/THREADS_PER_PAGE + 1;
	int64 range_start=page->page*THREADS_PER_PAGE;
	int64 range_end=range_start+THREADS_PER_PAGE;

	if (range_start < 0 || page->page >= page_count) {
		PRINT_STATUS_HTML("404 Not Found");
		PRINT_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>404</h1>"
		        "<p>" _("Page not found") ".</p>"));
		PRINT_EOF();
		return ERROR;
	}

	int ismod = is_mod_for_board(page->user, board);

	int post_render_flags = ismod?WRITE_POST_IP:0;

	struct captcha *captcha = 0;
	if (any_ip_affected(&page->ip, &page->x_real_ip, &page->x_forwarded_for,
	                    board, BAN_TARGET_POST, is_captcha_required)) {
		captcha = random_captcha();
	}

	PRINT_STATUS_HTML("200 OK");
	PRINT_SESSION();
	PRINT_BODY();
	print_page_header(http, S("/"), E(board_name(board)), S("/ – "), E(board_title(board)));
	print_top_bar(http, page->user, page->url);
	PRINT(S("<h1>/"), E(board_name(board)), S("/ – "), E(board_title(board)), S("</h1>"
	        "<hr>"));
	print_reply_form(http, board, 0, captcha, page->user),
	print_board_nav(http, board, page->page);
	PRINT(S("<hr>"
	        "<form action='"), S(PREFIX), S("/mod' method='post'>"));

	struct thread* thread = board_first_thread(board);
	int64 i=0;
	while (thread) {
		if (i>=range_end)
			break;

		if (i>=range_start) {
			struct post* post = thread_first_post(thread);
			print_post(http, post, 1, post_render_flags);

			if (thread_post_count(thread) > PREVIEW_REPLIES + 1) {
				uint64 n = thread_post_count(thread) - PREVIEW_REPLIES-1;
				PRINT(S("<div class='thread-stats'>"), U64(n), S(" "),
				       (n > 1)?S(_("Replies")):S(_("Reply")), S(" " _("not displayed") ".</div>"));
			}

			struct post *reply = thread_last_post(thread);
			if (reply != post && PREVIEW_REPLIES>0) {
				for (int i=1; i<PREVIEW_REPLIES; ++i) {
					struct post *prev = post_prev_post(reply);
					if (prev == post) break;
					reply = prev;
				}
				PRINT(S("<div class='replies'>"));
				while (reply) {
					print_post(http, reply, 1, post_render_flags);
					reply = post_next_post(reply);
				}
				PRINT(S("</div>"));
			}

			PRINT(S("<div class='clear'></div>"));

		}
		thread = thread_next_thread(thread);
		++i;
		if (thread && i>range_start && i<range_end)
			PRINT(S("<hr>"));
	}

	PRINT(S("<div class='clear'></div>"
	        "<hr>"
	        "<input type='hidden' name='redirect' value='"), S(PREFIX), S("/"), E(board_name(board)), S("/'>"));

	print_mod_bar(http, is_mod_for_board(page->user, board));

	PRINT(S("</form><hr>"));

	print_board_nav(http, board, page->page);

	print_bottom_bar(http);

	print_page_footer(http);

	PRINT_EOF();
}

static void board_page_finalize (http_context *http)
{
	struct board_page *page = (struct board_page*)http->info;
	if (page->url)   free(page->url);
	if (page->board) free(page->board);
	array_reset(&page->x_forwarded_for);
	free(page);
}
