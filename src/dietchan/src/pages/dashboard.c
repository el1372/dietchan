#include "dashboard.h"

#include <libowfat/byte.h>
#include <libowfat/fmt.h>
#include <libowfat/ip4.h>
#include <libowfat/ip6.h>
#include <assert.h>

#include "../tpl.h"
#include "../permissions.h"
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
	PRINT(S("<!DOCTYPE html>"
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
	              "<a href='"), S(PREFIX), S("/edit_user?action=edit&user_id="), U64(user_id), S("'>"
	                "Konto bearbeiten"
	              "</a> "
	            "<a href='"),S(PREFIX), S("/login?logout&redirect="), S(PREFIX), S("/login'>"
	              "Ausloggen"
	            "</a>"
	          "</div>"
	          "<h1>Kontrollzentrum</h1>"));
}

void write_dashboard_footer(http_context *http)
{
	PRINT(S("</body></html>"));
}

static int  dashboard_page_finish (http_context *http)
{
	struct dashboard_page *page = (struct dashboard_page*)http->info;

	if (!page->user) {
		PRINT_REDIRECT("307 Temporary Redirect",
		              S(PREFIX), S("/login?redirect="), S(PREFIX), S("/dashboard"));
		return ERROR;
	}

	PRINT_STATUS_HTML("200 OK");
	PRINT_SESSION();
	PRINT_BODY();

	write_dashboard_header(http, user_id(page->user));

	// Board list
	if (user_type(page->user) == USER_ADMIN) {
		PRINT(S("<h2>Bretter</h2>"
		           "<p>"
		           "<table>"
		             "<tr><th>Name (URL)</th><th>Title</th><th></th></tr>"));
		for (struct board *board = master_first_board(master); board; board=board_next_board(board)) {
			PRINT(S("<tr>"
			          "<td>"
			            "<a href='"), S(PREFIX), S("/"), E(board_name(board)), S("/'>/"), E(board_name(board)), S("/</a>"
			          "</td>"
			          "<td>"), E(board_title(board)), S("</td>"
			          "<td><a class='button' href='"), S(PREFIX), S("/edit_board?action=move&amp;move=-1&amp;board_id="), U64(board_id(board)), S("'>▲</a>"
			              "<span class='space'> </span>"
			              "<a class='button' href='"), S(PREFIX), S("/edit_board?action=move&amp;move=1&amp;board_id="), U64(board_id(board)), S("'>▼</a>"
			              "<span class='space'> </span>"
			              "<a class='button' href='"), S(PREFIX), S("/edit_board?action=edit&amp;board_id="), U64(board_id(board)), S("'>Bearbeiten</a>"
			              "<span class='space'> </span>"
			              "<a class='button' href='"), S(PREFIX), S("/edit_board?action=delete&amp;board_id="), U64(board_id(board)), S("'>Löschen</a>"
			          "</td>"
			        "</tr>"));
		}
		PRINT(S("</table></p>"
		        "<p><a class='button' href='"), S(PREFIX), S("/edit_board?action=add'>Neues Brett hinzufügen</a></p>"
		        "<h2>Benutzer</h2>"
		        "<p>"
		        "<table>"
		          "<tr><th>Name</th><th>Rolle</th><th>Bretter</th><td></td></tr>"));
	}

	// User list
	if (user_type(page->user) == USER_ADMIN) {
		for (struct user *user = master_first_user(master); user; user=user_next_user(user)) {
			PRINT(S("<tr>"
			          "<td>"), E(user_name(user)), S("</td>"
			          "<td>"));
			switch (user_type(user)) {
				case USER_ADMIN: PRINT(S("<span class='admin'>Admin</span>"));    break;
				case USER_MOD:   PRINT(S("<span class='mod'>Mod</span>"));        break;
				default:         PRINT(S("<span class='other'>Sonstige</span>")); break;
			}
			PRINT(S(  "</td>"
			          "<td>"));
			if (user_type(user) == USER_ADMIN || user_type(user) == USER_MOD) {
				if (!user_boards(user)) {
					PRINT(S("<span class='global'>Global</span>"));
				} else {
					const int64 *bid = user_boards(user);
					int comma=0;
					while (*bid != -1) {
						struct board *b = find_board_by_id(*bid);
						if (b) {
							PRINT(comma?S("<span class='comma'>, </span>"):S(""),
							      S("<span class='board'>/"), E(board_name(b)), S("/</span>"));
							comma = 1;
						}
						++bid;
					}
				}
			}
			PRINT(S(  "</td>"
			          "<td>"
			            "<a class='button' href='"), S(PREFIX), S("/edit_user?action=edit&amp;user_id="), U64(user_id(user)), S("'>Bearbeiten</a>"
			            "<span class='space'> </span>"
			            "<a class='button' href='"), S(PREFIX), S("/edit_user?action=delete&amp;user_id="), U64(user_id(user)), S("'>Löschen</a>"
			          "</td>"
			        "</tr>"));
		}
		PRINT(S(  "</table></p>"
		          "<p><a class='button' href='"), S(PREFIX), S("/edit_user?action=add'>Neuen Benutzer hinzufügen</a></p>"));
	}

	// Reports
	if (user_type(page->user) == USER_ADMIN || user_type(page->user) == USER_MOD) {
		PRINT(S("<h2>Meldungen</h2>"));

		int any_report=0;
		for (struct report *report = master_last_report(master); report; report=report_prev_report(report)) {
			struct board *board = find_board_by_id(report_board_id(report));
			if (!is_mod_for_board(page->user, board))
				continue;
			if (!any_report) {
				any_report = 1;
		    	PRINT(S("<form action='"), S(PREFIX), S("/mod' method='post'>"
		    	           "<input type='hidden' name='redirect' value='"), S(PREFIX), S("/dashboard'>"
				           "<p>"
				           "<table>"
				             "<tr><th></th><th>Datum</th><th>Brett</th><th>Beitrag</th><th>Vorschau</th><th>Grund</th><th>Kommentar</th></tr>"));
		    }
			struct thread *thread = find_thread_by_id(report_thread_id(report));
			struct post *post = find_post_by_id(report_post_id(report));
			PRINT(S("<tr>"
			          "<td><input type='checkbox' name='report' value='"), U64(report_id(report)), S("'></td>"
			          "<td>"), HTTP_DATE(report_timestamp(report)), S("</td>"
			          "<td>/"), board?E(board_name(board)):S(""), S("/</td>"
			          "<td><a href='"));
			print_post_url2(http, board, thread, post, 1);
			PRINT(S(       "'>&gt;&gt;"), U64(report_post_id(report)), S("</a></td><td>"));
			if (post)
				print_post(http, post, 1, WRITE_POST_IP | WRITE_POST_USER_AGENT);
			else
				PRINT(S("<i>gelöscht</i>"));

			PRINT(S(  "</td>"
			          "<td>"));
			switch (report_type(report)) {
				case REPORT_SPAM: PRINT(S("Spam")); break;
				case REPORT_ILLEGAL: PRINT(S("Illegal Inhalte")); break;
				case REPORT_OTHER: PRINT(S("Sonstiges")); break;
			}
			PRINT(S(  "</td>"
			          "<td>"), report_comment(report)?E(report_comment(report)):S(""), S(
			          "</td>"
			        "</tr>"));
		}
		if (any_report) {
			PRINT(S("</table></p>"
			        "<p>"
			        "<select name='action'>"
			          "<option value='delete'>Post löschen</option>"
			          "<option value='ban'>Bannen</option>"
			          "<option value='delete_and_ban'>Post löschen + Bannen</option>"
			          "<option value='delete_report'>Meldung löschen</option>"
			        "</select> "
			        "<input type='submit' value='Ausführen'>"
			        "</p>"
			        "</form>"));
		} else {
			PRINT(S("<p><i>Keine Meldungen</i></p>"));
		}
	}

	// Ban list
	if (user_type(page->user) == USER_ADMIN || user_type(page->user) == USER_MOD) {
		PRINT(S("<h2>Banne</h2>"));
		int any_ban = 0;
		for (struct ban *ban = master_last_ban(master); ban; ban=ban_prev_ban(ban)) {
			if (!can_see_ban(page->user, ban))
				continue;

			if (ban_type(ban) == BAN_FLOOD)
				continue;

			if (ban_duration(ban) == 0)
				continue;

			if (!any_ban) {
				any_ban = 1;
				PRINT(S("<p>"
				        "<table>"
				          "<tr><th>IP-Bereich</th><th>Art</th><th>Bretter</th><th>Gesperrt seit</th><th>Gesperrt bis</th><th>Grund</th><td></td></tr>"));
			}

			PRINT(S("<tr>"
			          "<td>"),
			            IP(ban_range(ban).ip), S("/"), U64(ban_range(ban).range), S(
			          "</td>"
			          "<td>"));
			switch (ban_type(ban)) {
				case BAN_BLACKLIST:         PRINT(S("Bann"));        break;
				case BAN_FLOOD:             PRINT(S("Anti-Flood"));  break;
				case BAN_CAPTCHA_ONCE:      PRINT(S("Captcha (1)")); break;
				case BAN_CAPTCHA_PERMANENT: PRINT(S("Captcha")); break;
			}
			PRINT(S(  "</td>"
			          "<td>"));
			int64 *boards = ban_boards(ban);
			if (boards) {
				int64 *bid = boards;
				int comma = 0;
				while (*bid != -1) {
					struct board *b = find_board_by_id(*bid);
					if (b) {
						PRINT(comma?S("<span class='comma'>, </span>"):S(""),
						      S("/"), E(board_name(b)), S("/"));
						comma = 1;
					}
					++bid;
				}
			} else {
				PRINT(S("Global"));
			}
			PRINT(S(  "</td>"
			          "<td>"), HTTP_DATE(ban_timestamp(ban)), S("</td>"
			          "<td>"),
			            (ban_duration(ban) >= 0)?HTTP_DATE(ban_timestamp(ban) + ban_duration(ban)):S("Unbegrenzt"), S(
			          "</td>"
			          "<td>"),
			            ban_reason(ban)?E(ban_reason(ban)):S(""), S(
			          "</td>"
			          "<td>"
			            "<a class='button' href='"), S(PREFIX), S("/mod?action=edit_ban&amp;redirect="), S(PREFIX), S("/dashboard&amp;ban_id="), U64(ban_id(ban)), S("'>Bearbeiten</a>"
			            "<span class='space'> </span>"
			            "<a class='button' href='"), S(PREFIX), S("/mod?action=delete_ban&amp;redirect="), S(PREFIX), S("/dashboard&amp;ban_id="), U64(ban_id(ban)), S("'>Löschen</a>"
			          "</td>"
			        "</tr>"));
		}
		PRINT(any_ban?S("</table></p>"):S("<p><i>Keine Banne</i></p>"),
		      S("<p><a class='button' href='"), S(PREFIX), S("/mod?action=ban&amp;redirect="), S(PREFIX), S("/dashboard'>Neuen Bann hinzufügen</a></p>"));
	}

	write_dashboard_footer(http);

	PRINT_EOF();
	return 0;
}

static void dashboard_page_finalize(http_context *http)
{
	struct dashboard_page *page = (struct dashboard_page*)http->info;
	if (page->boards_order)          free(page->boards_order);
	free(page);
}
