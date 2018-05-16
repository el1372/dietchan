#include "dashboard.h"

#include <libowfat/byte.h>
#include <libowfat/fmt.h>
#include <libowfat/ip4.h>
#include <libowfat/ip6.h>
#include <assert.h>

#include "../page.h"
#include "../util.h"

static int  dashboard_page_param (http_context *http, char *key, char *val);
static int  dashboard_page_cookie (http_context *http, char *key, char *val);
static int  dashboard_page_file_begin (http_context *http, char *name, char *filename, char *content_type);
static int  dashboard_page_file_content (http_context *http, char *buf, size_t length);
static int  dashboard_page_file_end (http_context *http);
static int  dashboard_page_finish (http_context *http);
static void dashboard_page_finalize(http_context *http);

void dashboard_page_init(http_context *http)
{
	struct dashboard_page *page = malloc(sizeof(struct dashboard_page));
	byte_zero(page, sizeof(struct dashboard_page));
	http->info = page;

	http->get_param    = dashboard_page_param;
	http->post_param   = dashboard_page_param;
	http->cookie       = dashboard_page_cookie;
	http->finish       = dashboard_page_finish;
	http->finalize     = dashboard_page_finalize;
}

static int  dashboard_page_param (http_context *http, char *key, char *val)
{
	struct dashboard_page *page = (struct dashboard_page*)http->info;

	PARAM_STR("boards_order", page->boards_order);

	HTTP_FAIL(BAD_REQUEST);
}

static int dashboard_page_cookie (http_context *http, char *key, char *val)
{
	struct dashboard_page *page = (struct dashboard_page*)http->info;
	PARAM_SESSION();
	return 0;
}

void write_dashboard_header(http_context *http, uint64 user_id)
{
	HTTP_WRITE("<!DOCTYPE html>"
	           "<html>"
	             "<head>"
	               "<title>Kontrollzentrum</title>"
	               "<style>"
	                 "body {"
	                   "background: #fff;"
	                   "color: #000;"
	                 "}"
	                 "th {"
	                   "text-align:left;"
	                 "}"
	                 "th, td {"
	                   "padding-left: 8px;"
	                   "padding-right: 8px;"
	                   "vertical-align: top;"
	                 "}"
	                 "table {"
	                   "border-spacing: 0;"
	                   "margin-left: -6px;"
	                   "margin-right: -6px;"
	                 "}"
	                 ".button, button, input[type='submit'] {"
	                 	"padding-left: 6px;"
	                 	"padding-right: 6px;"
	                 	"border-radius: 5px;"
	                 	"border: 1px solid #aaa;"
	                 	"background: #eeeef8;"
	                 	"display: inline-block;"
	                 	"text-decoration: none;"
	                 	"color: #000;"
	                 	"font-size: 90%;"
	                 	"cursor: pointer;"
	                 "}"
	                 ".button:hover, button:hover, input[type='submit']:hover {"
	                   "border-color: #66c;"
	                 	"color: #008;"
	                 "}"
	                 "label {"
	                   "font-weight: bold;"
	                 "}"
	                 ".post ul {"
	                   "margin:0;"
	                   "padding:0;"
	                   "list-style: none; "
	                   "font-size:small"
	                 "}"
	                 ".post ul input[type=checkbox] {"
	                   "display: none;"
	                 "}"
                     "div.files {"
                       "float: left;"
                     "}"
                     "div.files.multiple {"
                       "float: none;"
                     "}"
                     "div.file {"
	                   "margin-right: 0.5em;"
                       "display: inline-block;"
	                   "vertical-align: top;"
                     "}"
                     ".file-header {"
                       "display: none;"
                     "}"
                     ".file-subheader {"
                       "display: none;"
	                 "}"
                     ".file-thumbnail-img {"
                       "vertical-align: bottom;"
                       "max-width: 60px;"
                       "max-height: 60px;"
                     "}"
                     ".post {"
                       "max-width: 600px;"
                       "max-height: 80px;"
                       "overflow-y: auto;"
                     "}"
                     ".post-header{"
                       "display:none;"
                     "}"
                     ".text {"
                       "overflow-wrap: break-word;"
                     "}"
                     "span.quote {"
                       "color: #090;"
                     "}"
                     "span.spoiler {"
                       "color: #000;"
                       "background: #000;"
                     "}"
                     "span.spoiler:hover {"
                       "color: #fff;"
                     "}"
                     ".banned {"
                       "color: #f00;"
                     "}"
                     "td input[type=checkbox] {"
                       "margin: 0;"
                       "position: relative;"
                       "top: 1px;"
                     "}"
	               "</style>"
	             "</head>"
	             "<body>"
	               "<div style='float:right'>"
	                 "<a href='" PREFIX "/edit_user?action=edit&user_id=");
	HTTP_WRITE_ULONG(user_id);
	HTTP_WRITE(      "'>Konto bearbeiten</a> "
	                 "<a href='" PREFIX "/login?logout&redirect=" PREFIX "/login'>Ausloggen</a>"
	               "</div>"
	               "<h1>Kontrollzentrum</h1>");
}

void write_dashboard_footer(http_context *http)
{
	HTTP_WRITE("</body></html>");
}

static int  dashboard_page_finish (http_context *http)
{
	struct dashboard_page *page = (struct dashboard_page*)http->info;

	if (!page->user) {
		HTTP_REDIRECT("307 Temporary Redirect", PREFIX "/login?redirect=" PREFIX "/dashboard");
		return ERROR;
	}

	HTTP_STATUS_HTML("200 OK");
	HTTP_WRITE_SESSION();
	HTTP_BODY();

	write_dashboard_header(http, user_id(page->user));

	// Board list
	if (user_type(page->user) == USER_ADMIN) {
		HTTP_WRITE("<h2>Bretter</h2>"
		           "<p>"
		           "<table>"
		             "<tr><th>Name (URL)</th><th>Title</th><th></th></tr>");
		for (struct board *board = master_first_board(master); board; board=board_next_board(board)) {
			HTTP_WRITE("<tr><td><a href='" PREFIX "/");
			HTTP_WRITE_ESCAPED(board_name(board));
			HTTP_WRITE("/'>/");
			HTTP_WRITE_ESCAPED(board_name(board));
			HTTP_WRITE("/</a></td><td>");
			HTTP_WRITE_ESCAPED(board_title(board));
			HTTP_WRITE("</td><td>"
			           "<a class='button' href='" PREFIX "/edit_board?action=move&amp;move=-1&amp;board_id=");
			HTTP_WRITE_ULONG(board_id(board));
			HTTP_WRITE("'>▲</a><span class='space'> </span>"
			           "<a class='button' href='" PREFIX "/edit_board?action=move&amp;move=1&amp;board_id=");
			HTTP_WRITE_ULONG(board_id(board));
			HTTP_WRITE("'>▼</a><span class='space'> </span>"
			           "<a class='button' href='" PREFIX "/edit_board?action=edit&amp;board_id=");
			HTTP_WRITE_ULONG(board_id(board));
			HTTP_WRITE("'>Bearbeiten</a><span class='space'> </span>"
			           "<a class='button' href='" PREFIX "/edit_board?action=delete&amp;board_id=");
			HTTP_WRITE_ULONG(board_id(board));
			HTTP_WRITE("'>Löschen</a></td></tr>");
		}
		HTTP_WRITE("</table></p>"
		           "<p><a class='button' href='" PREFIX "/edit_board?action=add'>Neues Brett hinzufügen</a></p>"
		           "<h2>Benutzer</h2>"
		           "<p>"
		           "<table>"
		             "<tr><th>Name</th><th>Rolle</th><th>Bretter</th><td></td></tr>");
	}

	// User list
	if (user_type(page->user) == USER_ADMIN) {
		for (struct user *user = master_first_user(master); user; user=user_next_user(user)) {
			HTTP_WRITE("<tr><td>");
			HTTP_WRITE_ESCAPED(user_name(user));
			HTTP_WRITE("</td><td>");
			switch (user_type(user)) {
				case USER_ADMIN: HTTP_WRITE("<span class='admin'>Admin</span>");    break;
				case USER_MOD:   HTTP_WRITE("<span class='mod'>Mod</span>");        break;
				default:         HTTP_WRITE("<span class='other'>Sonstige</span>"); break;
			}
			HTTP_WRITE("</td><td>");
			if (user_type(user) == USER_ADMIN || user_type(user) == USER_MOD) {
				if (!user_boards(user)) {
					HTTP_WRITE("<span class='global'>Global</span>");
				} else {
					const int64 *bid = user_boards(user);
					int comma=0;
					while (*bid != -1) {
						struct board *b = find_board_by_id(*bid);
						if (b) {
							if (comma)
								HTTP_WRITE("<span class='comma'>, </span>");
							comma = 1;
							HTTP_WRITE("<span class='board'>/");
							HTTP_WRITE_ESCAPED(board_name(b));
							HTTP_WRITE("/</span>");
						}
						++bid;
					}
				}
			}
			HTTP_WRITE("</td><td><a class='button' href='" PREFIX "/edit_user?action=edit&amp;user_id=");
			HTTP_WRITE_ULONG(user_id(user));
			HTTP_WRITE("'>Bearbeiten</a><span class='space'> </span>"
			           "<a class='button' href='" PREFIX "/edit_user?action=delete&amp;user_id=");
			HTTP_WRITE_ULONG(user_id(user));
			HTTP_WRITE("'>Löschen</a></td></tr>");
		}
		HTTP_WRITE("</table>"
		           "<p><a class='button' href='" PREFIX "/edit_user?action=add'>Neuen Benutzer hinzufügen</a></p>");
	}

	// Reports
	if (user_type(page->user) == USER_ADMIN || user_type(page->user) == USER_MOD) {
		HTTP_WRITE("<h2>Meldungen</h2>");

		int any_report=0;
		for (struct report *report = master_last_report(master); report; report=report_prev_report(report)) {
			struct board *board = find_board_by_id(report_board_id(report));
			if (!is_mod_for_board(page->user, board))
				continue;
			if (!any_report) {
				any_report = 1;
		    	HTTP_WRITE("<form action='" PREFIX "/mod' method='post'>"
		    	           "<input type='hidden' name='redirect' value='" PREFIX "/dashboard'>"
				           "<p>"
				           "<table>"
				             "<tr><th></th><th>Datum</th><th>Brett</th><th>Beitrag</th><th>Vorschau</th><th>Grund</th><th>Kommentar</th></tr>");
		    }
			struct thread *thread = find_thread_by_id(report_thread_id(report));
			struct post *post = find_post_by_id(report_post_id(report));
			HTTP_WRITE("<tr>"
			           "<td><input type='checkbox' name='report' value='");
			HTTP_WRITE_ULONG(report_id(report));
			HTTP_WRITE("'></td>"
			           "<td>");
			HTTP_WRITE_HTTP_DATE(report_timestamp(report));
			HTTP_WRITE("</td><td>/");
			if (board)
				HTTP_WRITE_ESCAPED(board_name(board));
			HTTP_WRITE("/</td><td><a href='");
			write_post_url2(http, board, thread, post, 1);
			HTTP_WRITE("'>&gt;&gt;");
			HTTP_WRITE_ULONG(report_post_id(report));
			HTTP_WRITE("</a></td><td>");
			if (post)
				write_post(http, post, 1, WRITE_POST_IP | WRITE_POST_USER_AGENT);
			else
				HTTP_WRITE("<i>gelöscht</i>");

			HTTP_WRITE("</td><td>");
			switch (report_type(report)) {
				case REPORT_SPAM: HTTP_WRITE("Spam"); break;
				case REPORT_ILLEGAL: HTTP_WRITE("Illegal Inhalte"); break;
				case REPORT_OTHER: HTTP_WRITE("Sonstiges"); break;
			}
			HTTP_WRITE("</td><td>");
			if (report_comment(report))
				HTTP_WRITE_ESCAPED(report_comment(report));
			HTTP_WRITE("</td></tr>");
		}
		if (any_report) {
			HTTP_WRITE("</table></p>"
			           "<p>"
			           "<select name='action'>"
			             "<option value='delete'>Post löschen</option>"
			             "<option value='ban'>Bannen</option>"
			             "<option value='delete_and_ban'>Post löschen + Bannen</option>"
			             "<option value='delete_report'>Meldung löschen</option>"
			           "</select> "
			           "<input type='submit' value='Ausführen'>"
			           "</p>"
			           "</form>");
		} else {
			HTTP_WRITE("<p><i>Keine Meldungen</i></p>");
		}
	}

	// Ban list
	if (user_type(page->user) == USER_ADMIN || user_type(page->user) == USER_MOD) {
		HTTP_WRITE("<h2>Banne</h2>");
		int any_ban = 0;
		for (struct ban *ban = master_last_ban(master); ban; ban=ban_prev_ban(ban)) {
			if (!can_see_ban(page->user, ban))
				continue;

			if (ban_type(ban) == BAN_FLOOD)
				continue;

			if (!any_ban) {
				any_ban = 1;
				HTTP_WRITE("<p>"
				           "<table>"
				             "<tr><th>IP-Bereich</th><th>Art</th><th>Bretter</th><th>Gesperrt seit</th><th>Gesperrt bis</th><th>Grund</th><td></td></tr>");
			}

			HTTP_WRITE("<tr><td>");
			HTTP_WRITE_IP(ban_range(ban).ip);
			HTTP_WRITE("/");
			HTTP_WRITE_ULONG(ban_range(ban).range);
			HTTP_WRITE("</td><td>");
			switch (ban_type(ban)) {
				case BAN_BLACKLIST:         HTTP_WRITE("Bann");        break;
				case BAN_FLOOD:             HTTP_WRITE("Anti-Flood");  break;
				case BAN_CAPTCHA_ONCE:      HTTP_WRITE("Captcha (1)"); break;
				case BAN_CAPTCHA_PERMANENT: HTTP_WRITE("Captcha"); break;
			}
			HTTP_WRITE("</td><td>");
			int64 *boards = ban_boards(ban);
			if (boards) {
				int64 *bid = boards;
				int comma = 0;
				while (*bid != -1) {
					struct board *b = find_board_by_id(*bid);
					if (b) {
						if (comma) HTTP_WRITE("<span class='comma'>, </span>");
						HTTP_WRITE("/");
						HTTP_WRITE(board_name(b));
						HTTP_WRITE("/");
						comma = 1;
					}
					++bid;
				}
			} else {
				HTTP_WRITE("Global");
			}
			HTTP_WRITE("</td><td>");
			HTTP_WRITE_HTTP_DATE(ban_timestamp(ban));
			HTTP_WRITE("</td><td>");
			if (ban_duration(ban) > 0) {
				HTTP_WRITE_HTTP_DATE(ban_timestamp(ban) + ban_duration(ban));
			} else {
				HTTP_WRITE("Unbegrenzt");
			}
			HTTP_WRITE("</td><td>");
			if (ban_reason(ban))
				HTTP_WRITE_ESCAPED(ban_reason(ban));
			HTTP_WRITE("</td><td><a class='button' href='" PREFIX "/mod?action=edit_ban&amp;redirect=" PREFIX "/dashboard&amp;ban_id=");
			HTTP_WRITE_ULONG(ban_id(ban));
			HTTP_WRITE("'>Bearbeiten</a><span class='space'> </span>"
			           "<a class='button' href='" PREFIX "/mod?action=delete_ban&amp;redirect=" PREFIX "/dashboard&amp;ban_id=");
			HTTP_WRITE_ULONG(ban_id(ban));
			HTTP_WRITE("'>Löschen</a></tr>");
		}
		if (any_ban) {
			HTTP_WRITE("</table></p>");
		} else {
			HTTP_WRITE("<p><i>Keine Banne</i></p>");
		}
		HTTP_WRITE("<p><a class='button' href='" PREFIX "/mod?action=ban&amp;redirect=" PREFIX "/dashboard'>Neuen Bann hinzufügen</a></p>");
	}

	write_dashboard_footer(http);

	HTTP_EOF();
	return 0;
}

static void dashboard_page_finalize(http_context *http)
{
	struct dashboard_page *page = (struct dashboard_page*)http->info;
	if (page->boards_order)          free(page->boards_order);

}
