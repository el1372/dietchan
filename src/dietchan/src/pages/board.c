#include "board.h"

#include <libowfat/case.h>
#include <libowfat/fmt.h>
#include <libowfat/byte.h>

#include "../page.h"
#include "../persistence.h"
#include "../captcha.h"

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

	const char *prefix = PREFIX "/";

	if (method == HTTP_POST)
		HTTP_FAIL(METHOD_NOT_ALLOWED);

	if (!case_starts(path, prefix) || path[strlen(path)-1] != '/')
		HTTP_FAIL(NOT_FOUND);

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

static void write_board_nav(http_context *http, struct board *board, int64 current_page)
{
	struct board_page *page = (struct board_page*)http->info;

	int64 page_count=(board_thread_count(board))/THREADS_PER_PAGE + 1;

	HTTP_WRITE("<div class='board-nav'>");

	if (page_count > 1) {
		for (int64 i=0; i<page_count; ++i) {
			if (i != current_page) {
				HTTP_WRITE("<a class='page' href='" PREFIX "/");
				HTTP_WRITE_ESCAPED(board_name(board));
				HTTP_WRITE("/");
				if (i>0) {
					HTTP_WRITE("?p=");
					HTTP_WRITE_LONG(i);
				}
				HTTP_WRITE("'>[");
				HTTP_WRITE_LONG(i);
				HTTP_WRITE("]</a>"
				           "<span class='space'> </span>");
			} else {
				HTTP_WRITE("<span class='page current'>[");
				HTTP_WRITE_LONG(i);
				HTTP_WRITE("]</span>"
				           "<span class='space'> </span>");
			}
		}
	}

	HTTP_WRITE("</div>");

}

static int  board_page_finish (http_context *http)
{
	struct board_page *page = (struct board_page*)http->info;
	struct board* board = (page->board)?find_board_by_name(page->board):0;
	if (!board) {
		HTTP_STATUS_HTML("404 Not Found");
		HTTP_WRITE_SESSION();
		HTTP_BODY();
		HTTP_WRITE("<h1>404</h1>"
		           "<p>Das Brett wurde nicht gefunden :(.<p>");
		HTTP_EOF();
		return ERROR;
	}

	int64 page_count=(board_thread_count(board))/THREADS_PER_PAGE + 1;
	int64 range_start=page->page*THREADS_PER_PAGE;
	int64 range_end=range_start+THREADS_PER_PAGE;

	if (range_start < 0 || page->page >= page_count) {
		HTTP_STATUS_HTML("404 Not Found");
		HTTP_WRITE_SESSION();
		HTTP_BODY();
		HTTP_WRITE("<h1>404</h1>"
		           "<p>Die Seite wurde nicht gefunden.<p>");
		HTTP_EOF();
		return ERROR;
	}

	int ismod = is_mod_for_board(page->user, board);

	int post_render_flags = ismod?WRITE_POST_IP:0;

	HTTP_STATUS_HTML("200 OK");
	HTTP_WRITE_SESSION();
	HTTP_BODY();

	write_page_header(http);

	write_top_bar(http, page->user, page->url);

	HTTP_WRITE("<h1>/");
	HTTP_WRITE_ESCAPED(board_name(board));
	HTTP_WRITE("/ – ");
	HTTP_WRITE_ESCAPED(board_title(board));
	HTTP_WRITE("</h1>");

	HTTP_WRITE("<hr>");

	struct captcha *captcha = 0;
	if (any_ip_affected(&page->ip, &page->x_real_ip, &page->x_forwarded_for,
	                    board, BAN_TARGET_POST, is_captcha_required)) {
		captcha = random_captcha();
	}

	write_reply_form(http, board_id(board), -1, captcha);

	write_board_nav(http, board, page->page);

	HTTP_WRITE("<hr>");

	HTTP_WRITE("<form action='" PREFIX "/mod' method='post'>");

	struct thread* thread = board_first_thread(board);
	int64 i=0;
	while (thread) {
		if (i>=range_end)
			break;

		if (i>=range_start) {
			struct post* post = thread_first_post(thread);
			write_post(http, post, 1, post_render_flags);

			if (thread_post_count(thread) > PREVIEW_REPLIES + 1) {
				HTTP_WRITE("<div class='thread-stats'>");
				uint64 n = thread_post_count(thread) - PREVIEW_REPLIES-1;
				HTTP_WRITE_ULONG(n);
				if (n > 1)
					HTTP_WRITE(" Antworten");
				else
					HTTP_WRITE(" Antwort");
				HTTP_WRITE(" nicht angezeigt.</div>");
			}

			struct post *reply = thread_last_post(thread);
			if (reply != post && PREVIEW_REPLIES>0) {
				for (int i=1; i<PREVIEW_REPLIES; ++i) {
					struct post *prev = post_prev_post(reply);
					if (prev == post) break;
					reply = prev;
				}
				HTTP_WRITE("<div class='replies'>");
				while (reply) {
					write_post(http, reply, 1, post_render_flags);
					reply = post_next_post(reply);
				}
				HTTP_WRITE("</div>");
			}

			HTTP_WRITE("<div class='clear'></div>");

		}
		thread = thread_next_thread(thread);
		++i;
		if (thread && i>range_start && i<range_end)
			HTTP_WRITE("<hr>");
	}

	HTTP_WRITE("<div class='clear'></div>");

	HTTP_WRITE("<hr>");


	HTTP_WRITE("<input type='hidden' name='redirect' value='" PREFIX "/");
	HTTP_WRITE_ESCAPED(board_name(board));
	HTTP_WRITE("/'>");

	write_mod_bar(http, is_mod_for_board(page->user, board));
	HTTP_WRITE("</form><hr>");

	write_board_nav(http, board, page->page);

	write_bottom_bar(http);

	write_page_footer(http);


	HTTP_EOF();
}

static void board_page_finalize (http_context *http)
{
	struct board_page *page = (struct board_page*)http->info;
	if (page->url)   free(page->url);
	if (page->board) free(page->board);
	array_reset(&page->x_forwarded_for);
}
