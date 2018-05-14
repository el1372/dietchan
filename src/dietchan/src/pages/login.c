#include "login.h"

#include <time.h>
#include <libowfat/str.h>
#include <libowfat/case.h>

#include "../page.h"
#include "../util.h"

static int  login_page_get_param (http_context *http, char *key, char *val);
static int  login_page_post_param (http_context *http, char *key, char *val);
static int  login_page_cookie (http_context *http, char *key, char *val);
static int  login_page_finish (http_context *http);
static void login_page_finalize(http_context *http);

void login_page_init(http_context *http)
{
	struct login_page *page = malloc(sizeof(struct login_page));
	byte_zero(page, sizeof(struct login_page));
	http->info = page;

	http->get_param    = login_page_get_param;
	http->post_param   = login_page_post_param;
	http->cookie       = login_page_cookie;
	http->finish       = login_page_finish;
	http->finalize     = login_page_finalize;

	page->redirect = strdup(PREFIX "/dashboard");
	memcpy(&page->ip, &http->ip, sizeof(struct ip));
}

static int  login_page_get_param (http_context *http, char *key, char *val)
{
	struct login_page *page = (struct login_page*)http->info;
	if (case_equals(key, "logout")) {
		page->logout = 1;
		return 0;
	}
	PARAM_REDIRECT("redirect", page->redirect);
	HTTP_FAIL(BAD_REQUEST);
}

static int  login_page_post_param (http_context *http, char *key, char *val)
{
	struct login_page *page = (struct login_page*)http->info;

	PARAM_STR("username", page->username);
	PARAM_STR("password", page->password);

	if (case_equals(key, "redirect"))
		return login_page_get_param(http, key, val);

	HTTP_FAIL(BAD_REQUEST);
}

static int login_page_cookie (http_context *http, char *key, char *val)
{
	struct login_page *page = (struct login_page*)http->info;
	PARAM_SESSION();
	return 0;
}

static int  login_page_finish (http_context *http)
{
	struct login_page *page = (struct login_page*)http->info;

	if (page->logout) {
		session_destroy(page->session);
		page->session = 0;
		page->user = 0;

		HTTP_REDIRECT("302 Success", page->redirect);
		return 0;
	}

	if (!page->username || str_equal(page->username, "")) {
		HTTP_STATUS_HTML("200 OK");
		HTTP_WRITE_SESSION();
		HTTP_BODY();
		write_page_header(http);
		HTTP_WRITE("<div class='top-bar'>");
		write_board_bar(http);
		HTTP_WRITE("</div>");
		HTTP_WRITE("<p><form action='" PREFIX "/login' method='post'>"
		             "<input type='hidden' name='redirect' value='");
		HTTP_WRITE_ESCAPED(page->redirect);
		HTTP_WRITE(  "'>"
		             "<label for='username'>Name</label>"
		             "<input type='text' name='username'><br>"
		             "<label for='password'>Password</label>"
		             "<input type='password' name='password'><br>"
		             "<input type='submit' value='Einloggen'>"
		           "</form></p>");
		write_bottom_bar(http);
		write_page_footer(http);
		HTTP_EOF();
		return 0;
	}
	if (!page->password || str_equal(page->password, "")) {
		HTTP_STATUS_HTML("400 Bad Request");
		HTTP_BODY();
		HTTP_WRITE("<h1>Du musst ein Passwort eingeben.</h1>");
		HTTP_EOF();
		return 0;
	}

	struct user *user = find_user_by_name(page->username);
	if (!user || !check_password(user_password(user), page->password)) {
		HTTP_STATUS_HTML("403 Du kommst hier nid rein");
		HTTP_BODY();
		HTTP_WRITE("<h1>Benutzername oder Passwort falsch.</h1>");
		HTTP_EOF();
		return 0;
	}

	// Do some cleanup every time before we add a new session
	purge_expired_sessions();

	begin_transaction();

	struct session *session = session_new();
	struct session *next_session = master_first_session(master);
	session_set_next_session(session, next_session);
	if (next_session)
		session_set_prev_session(next_session, session);
	master_set_first_session(master, session);

	uint64 timestamp = time(0);
	char sid[33];
	generate_random_string(sid, sizeof(sid)-1, "0123456789abcdefghijklmnopqrstuvwxyz");
	sid[sizeof(sid)-1] = '\0';

	session_set_login_time(session, timestamp);
	session_set_last_seen(session, timestamp);
	session_set_timeout(session, 60*60*24); // 24 hours

	session_set_user(session, user_id(user));
	session_set_login_ip(session, page->ip);
	session_set_last_ip(session, page->ip);
	session_set_sid(session, sid);

	commit();

	// Todo: Expire old session

	HTTP_STATUS_REDIRECT("302 Success", page->redirect);
	HTTP_WRITE("Set-Cookie:session=");
	HTTP_WRITE_DYNAMIC(sid);
	HTTP_WRITE(";path=" PREFIX "/;\r\n");
	HTTP_BODY();
	HTTP_EOF();
}

static void login_page_finalize(http_context *http)
{
	struct login_page *page = (struct login_page*)http->info;

	if (page->username) free(page->username);
	if (page->password) free(page->password);
	if (page->redirect) free(page->redirect);
}
