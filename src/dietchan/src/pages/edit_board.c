#include "edit_board.h"

#include <libowfat/byte.h>
#include <libowfat/fmt.h>
#include <assert.h>

#include "../page.h"
#include "../util.h"

static int  edit_board_page_get_param (http_context *http, char *key, char *val);
static int  edit_board_page_post_param (http_context *http, char *key, char *val);
static int  edit_board_page_cookie (http_context *http, char *key, char *val);
static int  edit_board_page_finish (http_context *http);
static void edit_board_page_finalize(http_context *http);

void edit_board_page_init(http_context *http)
{
	struct edit_board_page *page = malloc(sizeof(struct edit_board_page));
	byte_zero(page, sizeof(struct edit_board_page));
	http->info = page;

	http->get_param          = edit_board_page_get_param;
	http->post_param         = edit_board_page_post_param;
	http->cookie             = edit_board_page_cookie;
	http->finish             = edit_board_page_finish;
	http->finalize           = edit_board_page_finalize;

	page->board_id = -1L;
	page->action = strdup("");
}

static int  edit_board_page_get_param (http_context *http, char *key, char *val)
{
	struct edit_board_page *page = (struct edit_board_page*)http->info;

	PARAM_STR("action", page->action);
	PARAM_I64("board_id", page->board_id);
	PARAM_I64("move", page->move);

	HTTP_FAIL(BAD_REQUEST);
}

static int  edit_board_page_post_param (http_context *http, char *key, char *val)
{
	struct edit_board_page *page = (struct edit_board_page*)http->info;

	PARAM_STR("action", page->action);
	PARAM_I64("submitted", page->submitted);
	PARAM_I64("confirmed", page->confirmed);

	PARAM_I64("board_id", page->board_id);
	PARAM_STR("board_name", page->board_name);
	PARAM_STR("board_title", page->board_title);
	PARAM_STR("board_banners", page->board_banners);

	HTTP_FAIL(BAD_REQUEST);
}

static int  edit_board_page_cookie (http_context *http, char *key, char *val)
{
	struct edit_board_page *page = (struct edit_board_page*)http->info;
	PARAM_SESSION();
	return 0;
}

static void edit_board_page_print_form(http_context *http)
{
	struct edit_board_page *page = (struct edit_board_page*)http->info;

	if (case_equals(page->action, "add"))
		HTTP_WRITE("<h2>Brett hinzufügen</h2>");
	else
		HTTP_WRITE("<h2>Brett bearbeiten</h2>");

	HTTP_WRITE("<form method='post'>"
	           "<input type='hidden' name='action' value='");
	HTTP_WRITE_ESCAPED(page->action);
	HTTP_WRITE("'>"
	           "<input type='hidden' name='submitted' value='1'>"
	           "<input type='hidden' name='board_id' value='");
	HTTP_WRITE_LONG(page->board_id);
	HTTP_WRITE("'>"
	           "<p><label for='board_name'>Name (URL): </label>"
	           "<input type='text' name='board_name' value='");
	HTTP_WRITE_ESCAPED(page->board_name);
	HTTP_WRITE("' required></p>"
	           "<p><label for='board_title'>Titel: </label>"
	           "<input type='text' name='board_title' value='");
	HTTP_WRITE_ESCAPED(page->board_title);
	HTTP_WRITE("'></p><input type='submit' value='Übernehmen'></form>");
}

static void edit_board_page_print_confirmation(http_context *http)
{
	struct edit_board_page *page = (struct edit_board_page*)http->info;
	struct board *board = find_board_by_id(page->board_id);
	HTTP_WRITE("<form method='post'>"
	           "<input type='hidden' name='board_id' value='");
	HTTP_WRITE_LONG(page->board_id);
	HTTP_WRITE("'><p><label><input type='checkbox' name='confirmed' value='1'>Brett /");
	HTTP_WRITE_ESCAPED(board_name(board));
	HTTP_WRITE("/ wirklich löschen.</label></p>"
	           "<input type='submit' value='Löschen'></form>");
}

static int edit_board_page_finish (http_context *http)
{
	struct edit_board_page *page = (struct edit_board_page*)http->info;

	// Check permission

	if (!page->user || user_type(page->user) != USER_ADMIN) {
		HTTP_STATUS_HTML("403 Verboten");
		HTTP_WRITE_SESSION();
		HTTP_WRITE("<h1>403 Verboten</h1>"
		           "Du kommst hier nid rein.");
		HTTP_EOF();
		return 0;
	}

	// Initialize

	struct board *board = (case_equals(page->action, "add"))?0:find_board_by_id(page->board_id);
	if (!page->board_name)  page->board_name = strdup(board?board_name(board):"");
	if (!page->board_title) page->board_title = strdup(board?board_title(board):"");

	// Validate

	if (case_equals(page->action, "edit") ||
	    case_equals(page->action, "delete") ||
	    case_equals(page->action, "move")) {

		if (!board) {
			HTTP_STATUS_HTML("404 Not Found");
			HTTP_WRITE_SESSION();
			HTTP_BODY();
			write_dashboard_header(http);
			HTTP_WRITE("<span class='error'>Brett existiert nicht.</span>");
			write_dashboard_footer(http);
			return 0;
		}
	}

	if (page->submitted) {
		if (case_equals(page->action, "edit") ||
		    case_equals(page->action, "add")) {

			if (str_equal(page->board_name, "")) {
				HTTP_STATUS_HTML("400 Bad Request");
				HTTP_WRITE_SESSION();
				HTTP_BODY();
				write_dashboard_header(http);
				HTTP_WRITE("<p class='error'>Bitte Brett-Namen eingeben</p>");
				edit_board_page_print_form(http);
				write_dashboard_footer(http);
				HTTP_EOF();
				return 0;
			}

			if (find_board_by_name(page->board_name) != board) {
				HTTP_STATUS_HTML("400 Bad Request");
				HTTP_WRITE_SESSION();
				HTTP_BODY();
				write_dashboard_header(http);
				HTTP_WRITE("<p class='error'>Ein Brett mit dem Namen '");
				HTTP_WRITE_ESCAPED(page->board_name);
				HTTP_WRITE("' existiert bereits. Bitte einen anderen Namen eingeben.</p>");
				edit_board_page_print_form(http);
				write_dashboard_footer(http);
				HTTP_EOF();
				return 0;
			}
		}
	}

	if (case_equals(page->action, "delete")) {
		if (!page->confirmed) {
			HTTP_STATUS_HTML("200 OK");
			HTTP_WRITE_SESSION();
			HTTP_BODY();
			write_dashboard_header(http);
			edit_board_page_print_confirmation(http);
			write_dashboard_footer(http);
			HTTP_EOF();
			return 0;
		}
	}

	if (case_equals(page->action, "add") ||
	    case_equals(page->action, "edit")) {
		if (!page->submitted) {
			HTTP_STATUS_HTML("200 OK");
			HTTP_WRITE_SESSION();
			HTTP_BODY();
			write_dashboard_header(http);
			edit_board_page_print_form(http);
			write_dashboard_footer(http);
			HTTP_EOF();
			return 0;
		}
	}

	// Execute


	if (case_equals(page->action, "add")) {

		begin_transaction();
		board = board_new();
		struct board *prev = master_last_board(master);
		board_set_prev_board(board, prev);
		if (prev)
			board_set_next_board(prev, board);
		master_set_last_board(master, board);

		uint64 bid = master_board_counter(master) + 1;
		master_set_board_counter(master, bid);
		board_set_id(board, bid);

		board_set_name(board, page->board_name);
		board_set_title(board, page->board_title);
		commit();

	} else if (case_equals(page->action, "edit")) {

		begin_transaction();
		board_set_name(board, page->board_name);
		board_set_title(board, page->board_title);
		commit();

	} else if (case_equals(page->action, "move")) {

		begin_transaction();
		struct board *next = board_next_board(board);
		struct board *prev = board_prev_board(board);
		if (page->move > 0 && next) {
			board_set_prev_board(next, prev);
			board_set_prev_board(board, next);
			board_set_next_board(board, board_next_board(next));
		} else if (page->move < 0 && prev) {
			board_set_next_board(prev, next);
			board_set_prev_board(board, board_prev_board(prev));
			board_set_next_board(board, prev);
		}

		next = board_next_board(board);
		prev = board_prev_board(board);
		if (!prev)
			master_set_first_board(master, board);
		else if (master_first_board(master) == board && prev)
			master_set_first_board(master, prev);
		if (!next)
			master_set_last_board(master, board);
		else if (master_last_board(master) == board && next)
			master_set_last_board(master, next);

		if (next)
			board_set_prev_board(next, board);
		if (prev)
			board_set_next_board(prev, board);
		commit();

	} else if (case_equals(page->action, "delete")) {
		begin_transaction();
		delete_board(board);
		commit();
	}


	HTTP_REDIRECT("302 Found", PREFIX "/dashboard");
}

static void edit_board_page_finalize(http_context *http)
{
	struct edit_board_page *page = (struct edit_board_page*)http->info;
	if (page->action)                free(page->action);
	if (page->board_name)            free(page->board_name);
	if (page->board_title)           free(page->board_title);
	if (page->board_banners)         free(page->board_banners);
}
