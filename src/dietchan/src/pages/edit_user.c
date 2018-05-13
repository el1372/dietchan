#include "edit_user.h"

#include <libowfat/byte.h>
#include <libowfat/fmt.h>
#include <assert.h>

#include "../page.h"
#include "../util.h"
#include "dashboard.h"

static int  edit_user_page_get_param (http_context *http, char *key, char *val);
static int  edit_user_page_post_param (http_context *http, char *key, char *val);
static int  edit_user_page_cookie (http_context *http, char *key, char *val);
static int  edit_user_page_finish (http_context *http);
static void edit_user_page_finalize(http_context *http);

static int can_edit_everything(struct edit_user_page *page);

void edit_user_page_init(http_context *http)
{
	struct edit_user_page *page = malloc(sizeof(struct edit_user_page));
	byte_zero(page, sizeof(struct edit_user_page));
	http->info = page;

	http->get_param          = edit_user_page_get_param;
	http->post_param         = edit_user_page_post_param;
	http->cookie             = edit_user_page_cookie;
	http->finish             = edit_user_page_finish;
	http->finalize           = edit_user_page_finalize;

	page->user_id = -1L;
	page->action = strdup("");
}

static int  edit_user_page_get_param (http_context *http, char *key, char *val)
{
	struct edit_user_page *page = (struct edit_user_page*)http->info;

	PARAM_STR("action", page->action);
	PARAM_I64("user_id", page->user_id);

	HTTP_FAIL(BAD_REQUEST);
}

static int  edit_user_page_post_param (http_context *http, char *key, char *val)
{
	struct edit_user_page *page = (struct edit_user_page*)http->info;

	PARAM_STR("action", page->action);
	PARAM_I64("submitted", page->submitted);
	PARAM_I64("confirmed", page->confirmed);

	PARAM_I64("user_id", page->user_id);
	PARAM_STR("user_name", page->user_name);

	if (case_equals(key, "user_type")) {
		if (case_equals(val, "admin"))
			page->user_type = USER_ADMIN;
		else if (case_equals(val, "mod"))
			page->user_type = USER_MOD;
		else
			page->user_type = USER_REGULAR;

		return 0;
	}

	PARAM_STR("user_email", page->user_email);
	PARAM_STR("user_password", page->user_password);
	PARAM_STR("user_password_confirm", page->user_password_confirm);
	PARAM_STR("boards", page->boards);

	HTTP_FAIL(BAD_REQUEST);
}

static int  edit_user_page_cookie (http_context *http, char *key, char *val)
{
	struct edit_user_page *page = (struct edit_user_page*)http->info;
	PARAM_SESSION();
	return 0;
}

static int can_edit_everything(struct edit_user_page *page)
{
	// Only admins are allowed to change usernames and roles.
	// Other users can only change their password and email.

	if (!page->user) return 0;
	if (user_type(page->user) == USER_ADMIN)
		return 1;

	return 0;
}

static void edit_user_page_print_form(http_context *http)
{
	struct edit_user_page *page = (struct edit_user_page*)http->info;

	if (case_equals(page->action, "add"))
		HTTP_WRITE("<h2>Benutzer hinzufügen</h2>");
	else
		HTTP_WRITE("<h2>Benutzer bearbeiten</h2>");

	if (user_id(page->user) == page->user_id)
		HTTP_WRITE("<p><span class='warning'>Achtung! Du bearbeitest dein eigenes Benutzerkonto</span></p>");

	HTTP_WRITE("<form method='post'>"
	           "<input type='hidden' name='action' value='");
	HTTP_WRITE_ESCAPED(page->action);
	HTTP_WRITE("'>"
	           "<input type='hidden' name='submitted' value='1'>"
	           "<input type='hidden' name='user_id' value='");
	HTTP_WRITE_LONG(page->user_id);
	HTTP_WRITE("'>"
	           "<p><label for='user_name'>Name: </label>"
	           "<input type='text' name='user_name' value='");
	HTTP_WRITE_ESCAPED(page->user_name);
	HTTP_WRITE("' required");
	if (!can_edit_everything(page))
		HTTP_WRITE(" disabled");
	HTTP_WRITE("></p>"
	           "<p><label for='user_type'>Rolle: </label>"
	           "<select name='user_type' required");
	if (!can_edit_everything(page))
		HTTP_WRITE(" disabled");
	HTTP_WRITE(">"
	             "<option value='mod'>Mod</option>"
	             "<option value='admin'");
	if (page->user_type == USER_ADMIN) HTTP_WRITE(" selected");
	HTTP_WRITE(  ">Admin</option>"
	           "</select></p>"
	           "<p><label for='boards'>Bretter: </label>"
	           "<input type='text' name='boards' value='");
	if (page->boards) {
		HTTP_WRITE_ESCAPED(page->boards);
	} else {
		struct user *edited_user = find_user_by_id(page->user_id);
		if (edited_user) {
			int64 *bids = user_boards(edited_user);
			if (bids) {
				for (int i=0; bids[i] != -1; ++i) {
					struct board *b = find_board_by_id(bids[i]);
					if (b) {
						HTTP_WRITE_ESCAPED(board_name(b));
						HTTP_WRITE(" ");
					}
				}
			}
		}
	}
	HTTP_WRITE("'");
	if (!can_edit_everything(page))
		HTTP_WRITE(" disabled");
	HTTP_WRITE("></p>"
	           "<p><label for='user_email'>E-Mail: </label>"
	           "<input type='text' name='user_email' value='");
	HTTP_WRITE_ESCAPED(page->user_email);
	HTTP_WRITE("' optional></p>"
	           "<p><label for='user_password'>Passwort: </label>"
	           "<input type='password' name='user_password' value='");
	HTTP_WRITE_ESCAPED(page->user_password);
	HTTP_WRITE("'");
	if (case_equals(page->action, "add"))
		HTTP_WRITE(" required");
	HTTP_WRITE("></p>"
	           "<p><label for='user_password_confirm'>Bestätigen: </label>"
	           "<input type='password' name='user_password_confirm' value='");
	HTTP_WRITE_ESCAPED(page->user_password_confirm);
	HTTP_WRITE("'");
	if (case_equals(page->action, "add"))
		HTTP_WRITE(" required");
	HTTP_WRITE("></p><input type='submit' value='Übernehmen'></form>");
}

static void edit_user_page_print_confirmation(http_context *http)
{
	struct edit_user_page *page = (struct edit_user_page*)http->info;
	struct user *user = find_user_by_id(page->user_id);
	HTTP_WRITE("<form method='post'>"
	           "<input type='hidden' name='user_id' value='");
	HTTP_WRITE_LONG(page->user_id);
	HTTP_WRITE("'><p><label><input type='checkbox' name='confirmed' value='1'>Benutzer '");
	HTTP_WRITE_ESCAPED(user_name(user));
	HTTP_WRITE("' wirklich löschen.</label></p>"
	           "<input type='submit' value='Löschen'></form>");
}

static int edit_user_page_finish (http_context *http)
{
	struct edit_user_page *page = (struct edit_user_page*)http->info;

	// Check permission

	if (!page->user ||
	    !(user_type(page->user) == USER_ADMIN ||
		  user_id(page->user) == page->user_id)) {
		HTTP_STATUS_HTML("403 Verboten");
		HTTP_WRITE_SESSION();
		HTTP_WRITE("<h1>403 Verboten</h1>"
		           "Du kommst hier nid rein.");
		HTTP_EOF();
		return 0;
	}

	// Initialize

	struct user	*user = (case_equals(page->action, "add"))?0:find_user_by_id(page->user_id);
	array boards = {0};
	size_t boards_count = 0;

	if (!page->user_name)
		page->user_name = strdup(user?user_name(user):"");
	if (!page->user_email)
		page->user_email = strdup(user?user_email(user):"");
	if (!page->user_password)
		page->user_password = strdup("");
	if (!page->user_password_confirm)
		page->user_password_confirm = strdup("");
	if (!page->submitted)
		page->user_type = user?user_type(user):USER_MOD;

	// Validate

	if (case_equals(page->action, "edit") ||
	    case_equals(page->action, "delete")) {

	    if (!user) {
			HTTP_STATUS_HTML("404 Not Found");
			HTTP_WRITE_SESSION();
			HTTP_BODY();
			write_dashboard_header(http, 0);
			HTTP_WRITE("<span class='error'>Benutzer existiert nicht.</span>");
			write_dashboard_footer(http);
			return 0;
	    }
	}

	if (case_equals(page->action, "delete")) {
		if (user == page->user) {
			HTTP_STATUS_HTML("403 Verboten");
			HTTP_WRITE_SESSION();
			HTTP_BODY();
			HTTP_WRITE("<p>Du kannst dein eigenes Benutzerkonto nicht löschen.</p>");
			HTTP_EOF();
			return 0;
		}
	}

	if (page->submitted) {
		if (case_equals(page->action, "add") ||
		    case_equals(page->action, "edit")) {

			int error_header_sent = 0;

			#define ERROR_HEADER() \
				if (!error_header_sent) { \
					HTTP_STATUS_HTML("400 Bad Request"); \
					HTTP_WRITE_SESSION(); \
					HTTP_BODY(); \
					write_dashboard_header(http, 0); \
					error_header_sent = 1; \
				}

			if (str_equal(page->user_name, "")) {
				ERROR_HEADER();
				HTTP_WRITE("<p class='error'>Bitte User-Namen eingeben</p>");
			}

			if (case_equals(page->action, "add")) {
				if (str_equal(page->user_password, "")) {
					ERROR_HEADER();
					HTTP_WRITE("<p class='error'>Bitte Passwort eingeben</p>");
				}
			}

			if (!str_equal(page->user_password, page->user_password_confirm)) {
				ERROR_HEADER();
				HTTP_WRITE("<p class='error'>Passwörter stimmen nicht überein.</p>");
			}

			if (find_user_by_name(page->user_name) != user) {
				ERROR_HEADER();
				HTTP_WRITE("<p class='error'>Ein Benutzer mit dem Namen '");
				HTTP_WRITE_ESCAPED(page->user_name);
				HTTP_WRITE("' existiert bereits. Bitte einen anderen Namen eingeben.</p>");
			}

			if (page->boards) {
				int ok;
				boards_count = parse_boards(0, page->boards, &boards, &ok);
				if (!ok) {
					ERROR_HEADER();
					parse_boards(http, page->boards, &boards, &ok);
				}
			}

			if (error_header_sent) {
				edit_user_page_print_form(http);
				write_dashboard_footer(http);
				HTTP_EOF();
				return 0;
			}

			#undef ERROR_HEADER
		}
	}

	if (case_equals(page->action, "delete")) {
		if (!page->confirmed) {
			HTTP_STATUS_HTML("200 OK");
			HTTP_WRITE_SESSION();
			HTTP_BODY();
			write_dashboard_header(http, 0);
			edit_user_page_print_confirmation(http);
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
			write_dashboard_header(http, 0);
			edit_user_page_print_form(http);
			write_dashboard_footer(http);
			HTTP_EOF();
			return 0;
		}
	}

	// Execute

	if (case_equals(page->action, "add")) {

		begin_transaction();
		user = user_new();
		struct user *prev = master_last_user(master);
		user_set_prev_user(user, prev);
		if (prev)
			user_set_next_user(prev, user);
		master_set_last_user(master, user);

		uint64 uid = master_user_counter(master) + 1;
		master_set_user_counter(master, uid);
		user_set_id(user, uid);

		if (boards_count > 0) {
			uint64 *bids = db_alloc(db, sizeof(int64)*(boards_count + 1));
			for (int i=0; i<boards_count; ++i) {
				struct board **board = array_get(&boards, sizeof(struct board*), i);
				bids[i] = board_id(*board);
			}
			bids[boards_count] = -1;
			db_invalidate_region(db, bids, sizeof(int64)*(boards_count + 1));
			user_set_boards(user, bids);
		}

		user_set_name(user, page->user_name);
		user_set_type(user, page->user_type);
		user_set_email(user, page->user_email);
		user_set_password(user, crypt_password(page->user_password));
		commit();

	} else if (case_equals(page->action, "edit")) {

		begin_transaction();
		if (can_edit_everything(page)) {
			user_set_name(user, page->user_name);
			user_set_type(user, page->user_type);

			db_free(db, user_boards(user));
			if (boards_count > 0) {
				uint64 *bids = db_alloc(db, sizeof(int64)*(boards_count + 1));
				for (int i=0; i<boards_count; ++i) {
					struct board **board = array_get(&boards, sizeof(struct board*), i);
					bids[i] = board_id(*board);
				}
				bids[boards_count] = -1;
				db_invalidate_region(db, bids, sizeof(int64)*(boards_count + 1));
				user_set_boards(user, bids);
			} else {
				user_set_boards(user, 0);
			}
		}
		user_set_email(user, page->user_email);
		if (!str_equal(page->user_password, ""))
			user_set_password(user, crypt_password(page->user_password));
		commit();

	} else if (case_equals(page->action, "delete")) {
		if (!can_edit_everything(page))
			HTTP_FAIL(FORBIDDEN);

		begin_transaction();
		delete_user(user);
		commit();
	}


	HTTP_REDIRECT("302 Found", "/bbs/dashboard");

}

static void edit_user_page_finalize(http_context *http)
{
	struct edit_user_page *page = (struct edit_user_page*)http->info;
	if (page->action)                free(page->action);
	if (page->user_name)             free(page->user_name);
	if (page->user_email)            free(page->user_email);
	if (page->user_password)         free(page->user_password);
	if (page->user_password_confirm) free(page->user_password_confirm);
	if (page->boards)                free(page->boards);
}
