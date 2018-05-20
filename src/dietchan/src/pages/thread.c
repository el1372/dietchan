#include "thread.h"

#include <assert.h>
#include <string.h>
#include <libowfat/case.h>
#include <libowfat/fmt.h>
#include <libowfat/byte.h>
#include <libowfat/scan.h>

#include "../page.h"
#include "../tpl.h"
#include "../persistence.h"
#include "../captcha.h"

static int thread_page_request (http_context *http, http_method method, char *path, char *query);
static int thread_page_header (http_context *http, char *key, char *val);
static int thread_page_cookie (http_context *http, char *key, char *val);
static int thread_page_finish (http_context *http);
static void thread_page_finalize (http_context *http);

void thread_page_init(http_context *http)
{
	struct thread_page *page = malloc(sizeof(struct thread_page));
	byte_zero(page, sizeof(struct thread_page));

	http->info = page;

	http->request      = thread_page_request;
	http->header       = thread_page_header;
	http->cookie       = thread_page_cookie;
	http->finish       = thread_page_finish;
	http->finalize     = thread_page_finalize;

	byte_copy(&page->ip, sizeof(struct ip), &http->ip);
}

static int thread_page_request (http_context *http, http_method method, char *path, char *query)
{
	struct thread_page *page = (struct thread_page*)http->info;

	const char *prefix = PREFIX "/";

	if (method == HTTP_POST)
		HTTP_FAIL(METHOD_NOT_ALLOWED);

	if (!case_starts(path, prefix))
		HTTP_FAIL(NOT_FOUND);

	const char *relative_path = &path[strlen(prefix)];
	const char *board_separator = strchr(relative_path, '/');

	if (!board_separator)
		HTTP_FAIL(NOT_FOUND);

	page->board = malloc(board_separator-relative_path);
	memcpy(page->board, relative_path, board_separator-relative_path);
	page->board[board_separator-relative_path] = '\0';

	size_t consumed = scan_int(&board_separator[1], &page->thread_id);

	if (!consumed || board_separator[consumed+1] != '\0')
		HTTP_FAIL(NOT_FOUND);

	page->url = strdup(path);

	return 0;
}

static int thread_page_header (http_context *http, char *key, char *val)
{
	struct thread_page *page = (struct thread_page*)http->info;

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

static int thread_page_cookie (http_context *http, char *key, char *val)
{
	struct thread_page *page = (struct thread_page*)http->info;
	PARAM_SESSION();
	return 0;
}

static void write_thread_nav(http_context *http, struct thread *thread)
{
	struct board *board = thread_board(thread);

	PRINT(S("<div class='thread-nav'>"
	          "<a href='"), S(PREFIX), S("/"), E(board_name(board)), S("/'>[Zurück]</a>"
	        "</div>"));
}

static int thread_page_finish (http_context *http)
{
	struct thread_page *page = (struct thread_page*)http->info;

	struct board *board = find_board_by_name(page->board);
	if (!board) {
		PRINT_STATUS_HTML("404 Not Found");
		HTTP_WRITE_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>404</h1>"
		        "<p>Das Brett wurde nicht gefunden :(.<p>"));
		HTTP_EOF();
		return 0;
	}

	int ismod = is_mod_for_board(page->user, board);

	int post_render_flags = ismod?WRITE_POST_IP:0;

	struct thread *thread = find_thread_by_id(page->thread_id);

	if (!thread || thread_board(thread) != board) {
		PRINT_STATUS_HTML("404 Not Found");
		HTTP_WRITE_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>404</h1>"
		        "<p>Faden existiert nicht :(.<p>"));
		HTTP_EOF();
		return 0;
	}

	PRINT_STATUS_HTML("200 OK");
	HTTP_WRITE_SESSION();
	PRINT_BODY();

	print_page_header(http);

	print_top_bar(http, page->user, page->url);

	PRINT(S("<h1>/"),E(board_name(board)),S("/ – "),E(board_title(board)),S("</h1>"
	      "<hr>"));

	struct captcha *captcha = 0;
	if (any_ip_affected(&page->ip, &page->x_real_ip, &page->x_forwarded_for,
	                    board, BAN_TARGET_POST, is_captcha_required)) {
		captcha = random_captcha();
	}

	print_reply_form(http, board_id(board), page->thread_id, captcha);

	write_thread_nav(http, thread);

	PRINT(S("<hr>"));

	struct post *post = thread_first_post(thread);

	PRINT(S("<form action='"),S(PREFIX), S("/mod' method='post'>"
	        "<div class='thread'>"));
	print_post(http, post, 0, post_render_flags);
	PRINT(S(  "<div class='replies'>"));
	post = post_next_post(post);

	while (post) {
		print_post(http, post, 0, post_render_flags);

		post = post_next_post(post);
	}
	PRINT(S(  "</div>"
	        "</div>"
	        "<div class='clear'></div>"
	        "<hr>"
	        "<input type='hidden' name='redirect' value='"),
	          S(PREFIX), S("/"), E(board_name(board)), S("/"), UL(post_id(thread_first_post(thread))), S("'>"));

	print_mod_bar(http, is_mod_for_board(page->user, board));
	PRINT(S("</form><hr>"));

	write_thread_nav(http, thread);

	print_bottom_bar(http);

	print_page_footer(http);

	HTTP_EOF();
}

static void thread_page_finalize (http_context *http)
{
	struct thread_page *page = (struct thread_page*)http->info;
	if (page->url)   free(page->url);
	if (page->board) free(page->board);
	array_reset(&page->x_forwarded_for);
}
