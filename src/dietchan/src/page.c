#include "page.h"

#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <libowfat/ip4.h>
#include <libowfat/ip6.h>
#include <libowfat/fmt.h>
#include <libowfat/byte.h>
#include <libowfat/str.h>
#include <libowfat/case.h>
#include <libowfat/scan.h>
#include "util.h"
#include "captcha.h"
#include "bbcode.h"
#include "tpl.h"

void print_reply_form(http_context *http, int board, int thread, struct captcha *captcha)
{
	PRINT(S("<form class='reply' action='"), S(PREFIX), S("/post' method='post' enctype='multipart/form-data' novalidate autocomplete='on'>"), S(
	        // Bot trap
	        "<div class='falle'>"
	          "<del>"
	          "<label for='username'>Username</label>"
	          "<label for='comment'>Comment</label>"
	          "<label for='text'>Text</label>"
	          "<label for='website'>Website</label>"
	          "<input name='username' type='text' size='1' autocomplete='nein' tabindex='-1'>"
	          "<textarea name='comment' rows='1' cols='1' tabindex='-1'></textarea>"
	          "<textarea name='text' rows='1' cols='1' tabindex='-1'></textarea>"
	          "<input name='website' type='text' size='1' autocomplete='nein' tabindex='-1'>"
	          "</del>"
	        "</div>"
	          // Actual form
	        "<table>"
	          "<tr>"
	            "<th colspan='3'>"
	            "<h3>"), (thread == -1)?S("Antwort erstellen"):S("Neuen Faden erstellen"),S("</h3>"
	          "</th>"
	          "</tr>"
	          "<tr>"
	            "<th><label for='sage'>Säge</label></th>"
	            "<td colspan='2'><input type='checkbox' name='sage' value='1'></td>"
	          "</tr>"
	          "<tr>"
	            "<th><label for='subject'>Betreff</label></th>"
	            "<td><input name='subject' type='text'></td>"
	            "<td width='1'>"
	             "<input type='submit' value='"), (thread!=-1)?S("Antwort erstellen"):S("Faden erstellen"), S("'>"
	            "</td>"
	          "</tr>"
	          "<tr>"
	            "<th><label for='username2'>Name</label></th>"
	            "<td colspan='2'><input name='username2' type='text' autocomplete='nein'></td>"
	          "</tr>"
	          "<tr>"
	            "<th><label for='text2'>Kommentar</label></th>"
	            "<td colspan='2'><textarea name='text2'></textarea></td>"
	          "</tr>"
	          "<tr>"
	            "<th><label for='text'>Dateien</label>"
	                 "<span class='sub-label'> (≤ 4)</span>"
	            "</th>"
	            "<td colspan='2'><input type='file' name='file' multiple required><br>"
	                "<input type='file' name='file' multiple required><br>"
	                "<input type='file' name='file' multiple required><br>"
	                "<input type='file' name='file' multiple required><br>"
	            "</td>"
	          "</tr>"
	          "<tr>"
	            "<th><label for='password'>Passwort</label></th>"
	            "<td colspan='2'>"
	              "<input type='text' name='dummy' autocomplete='username' value='-' "
	                "size='1' maxlength='1' tabindex='-1' style='width: 0; height:0; "
	                "padding: 0; margin:0; position:absolute; left: -10000px; '>"
	              "<input type='password' name='password' autocomplete='password'>"
	            "</td>"
	          "</tr>"));
	if (captcha) {
		PRINT(S("<tr>"
		          "<th></th>"
		          "<td colspan='2'><center>"
		            "<img class='captcha' src='"), S(PREFIX), S("/captchas/"), X(captcha_id(captcha)), S(".png'></center>"
		          "</td>"
		        "</tr>"
		        "<tr>"
		          "<th><label for='captcha'>Captcha</label></th>"
		          "<td colspan='2'><input type='text' name='captcha' autocomplete='off'></td>"
		        "</tr>"));
	}
	PRINT(S("</table>"));
	if (captcha) {
		PRINT(S("<input name='captcha_id' value='"), X(captcha_id(captcha)), S("' type='hidden'>"
		        "<input name='captcha_token' value='"), X(captcha_token(captcha)), S("' type='hidden'>"));
	}
	if (thread != -1)
		PRINT(S("<input name='thread' value='"), UL(thread), S("' type='hidden'>"));
	else
		PRINT(S("<input name='board' value='"), UL(board), S("' type='hidden'>"));
	PRINT(S("</form>"));
}

void print_board_bar(http_context *http)
{
	struct board *board = master_first_board(master);
	PRINT(S("<div class='boards'>"));
	while (board) {
		PRINT(S("<span class='board'>"
		          "<a href='"), S(PREFIX), S("/"), E(board_name(board)), S("/' "
		             "title='"), E(board_title(board)), S("'>["), E(board_name(board)), S("]"
		          "</a>"
		        "</span><span class='space'> </span>"));
		board = board_next_board(board);
	}

	PRINT(S("</div>"));
}

void print_top_bar(http_context *http, struct user *user, const char *url)
{
	PRINT(S("<div class='top-bar'>"
	        "<div class='top-bar-right'>"));
	if (user) {
		if (user_type(user) == USER_ADMIN || user_type(user) == USER_MOD)
			PRINT(S("<a href='"), S(PREFIX), S("/dashboard'>Kontrollzentrum</a><span class='space'> </span>"));
		PRINT(S("<a href='"), S(PREFIX), S("/login?logout&amp;redirect="), E(url), S("'>Ausloggen</a>"));
	} else {
		PRINT(S("<a href='"), S(PREFIX), S("/login?redirect="), E(url), S("'>Einloggen</a>"));
	}
	PRINT(S("</div>"));
	print_board_bar(http);
	PRINT(S("</div>"));
}

void print_bottom_bar(http_context *http)
{
	PRINT(S("<div class='bottom-bar'>"));
	print_board_bar(http);
	PRINT(S("</div>"));
}

void print_mod_bar(http_context *http, int ismod)
{
	if (!ismod) {
		PRINT(S("<div class='mod-bar'>"
		          "<input type='text' name='dummy' autocomplete='username' value='-' size='1' "
		            "maxlength='1' style='display:none'>"
		          "<span class='segment'>"
		            "<label for='password'>Passwort:</label>"
		            "<span class='space'> </span>"
		            "<input type='password' name='password' autocomplete='password'>"
		          "</span><span class='space'> </span>"
		          "<span class='segment'>"
		            "<button name='action' value='delete'>Löschen</button>"
		          "</span><span class='space'> </span>"
		          "<span class='segment'>"
		            "<button name='action' value='report'>Petzen</button>"
		          "</span>"
		        "</div>"));
	} else {
		PRINT(S("<div class='mod-bar'>"
		          "<span class='segment'>"
		            "<label for='action'>Aktion:</label>"
		            "<span class='space'> </span>"
		            "<select name='action'>"
		              "<option value='delete'>Löschen</option>"
		              "<option value='ban'>Bannen</option>"
		              "<option value='delete_and_ban'>Löschen + Bannen</option>"
		              "<option value='close'>Schließen</option>"
		              "<option value='pin'>Anpinnen</option>"
		            "</select>"
		          "</span><span class='space'> </span>"
		          "<span class='segment'>"
		            "<input type='submit' value='Ausführen'>"
		          "</span>"
		        "</div>"));
	}
}

void write_page_css(http_context *http)
{
	PRINT(S("h1, h3 {"
	          "text-align: center;"
	        "}"
	        "th {"
	          "text-align: left;"
	          "vertical-align: baseline;"
	        "}"
	        "td {"
	          "vertical-align: baseline;"
	          "position: relative;"
	        "}"
	        ".footer {"
	          "text-align: right;"
	        "}"
	        "form.reply textarea,"
	        "form.reply input[type='text'],"
	        "form.reply input[type='file'],"
	        "form.reply input[type='password'] {"
	          "width: 100%;"
	          "box-sizing: border-box;"
	        "}"
	        "form.reply input[type='file'] {"
	          "margin-bottom: 4px;"
	          "border: none;"
	        "}"
	        "form.reply input[type='file']:invalid + br,"
	        "form.reply input[type='file']:invalid + br + input[type='file'] {"
	          "display:none;"
	        "}"
	        "input[type='checkbox'] {"
	          "margin: 0;"
	        "}"
	        "form.reply {"
	          "margin-bottom: 2em;"
	        "}"
	        "form.reply > table {"
	          "margin: 0 auto;"
	        "}"
	        "form.reply textarea {"
	          "vertical-align: baseline;"
	          "min-width:300px;"
	          "min-height:1.5em;"
	        "}"
	        "form.reply input[type=checkbox] {"
	          "position: relative;"
	          "top: 2.5px;"
	        "}"
	        "form.reply td label {"
	          "font-size: small;"
	        "}"
	        ".sub-label {"
	          "font-weight: normal;"
	        "}"
	        ".falle {"
	          "position: absolute;"
	          "left: -10000px;"
	        "}"
	        "img.captcha {"
	          "display: block;"
	          "width: 140px;"
	          "height: 50px;"
	        "}"
	        "li.reply {"
	          "list-style-type: disc;"
	          "position: relative;"
	          "left: 1.5em;"
	        "}"
	        "ul.replies {"
	          "padding-left: 0;"
	        "}"
	        ".thread{"
	          "clear:both;"
	        "}"
	        ".thread-stats {"
	          "margin-left: 2em;"
	          "margin-top: 1em;"
	          "margin-bottom: 0.75em;"
	          "opacity: 0.5;"
	        "}"
	        ".post-header {"
	          "margin-top: .25em;"
	          "margin-bottom: .5em;"
	        "}"
	        ".post-header > .sage {"
	          "color: red;"
	          "font-weight: bold;"
	        "}"
	        ".post-header > .sticky,"
	        ".post-header > .closed {"
	          "font-weight: bold;"
	        "}"
	        ".post-header input[type='checkbox'] {"
	          "position: relative;"
	          "top: 2.5px;"
	          "height: 1em;"
	        "}"
	        ".post.reply {"
	          "overflow:hidden;"
	          "display: inline-block;"
	          "max-width: 100%;"
	        "}"
	        ".post ul {"
	          "padding-left: 2em;"
	          "padding-right: 2em;"
	        "}"
	        ".post.reply ul {"
	          "margin-bottom: .25em;"
	          "margin-top: .25em;"
	        "}"
	        ".replies {"
	          "margin-left: 2em;"
	        "}"
	        ".post:target {"
	          "outline: 1px dashed #00f;"
	          "outline-offset: -1px;"
	        //   "background: #ffc;"
	        "}"
	        ".content {"
	          "margin-bottom: .25em;"
	        "}"
	        ".text {"
	          "margin-top: .5em;"
	          "overflow-wrap: break-word;"
	        "}"
	        "div.files {"
	          "margin-bottom: .25em;"
	        "}"
	        "ul.thread > li,"
	        "ul.replies > li,"
	        " {"
	          "margin-bottom: .5em;"
	        "}"
	        "div.files {"
	          "float: left;"
	          "margin-top: -1.5em;"
	        "}"
	        "div.files.multiple {"
	          "float: none;"
	        "}"
	        "div.file {"
	          "margin-right: 1.5em;"
	          "margin-top: 1.5em;"
	          "display: inline-block;"
	          "vertical-align: top;"
	        "}"
	        ".file-header {"
	          "font-size: small;"
	        "}"
	        ".file-subheader {"
	          "font-size: x-small;"
	        "}"
	        ".file-thumbnail-img {"
	          "vertical-align: bottom;"
	          "max-width: 200px;"
	          "max-height: 200px;"
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
	          "font-weight: bold;"
	          "margin-top: 1em;"
	        "}"
	        ".mod-bar {"
	          "text-align: right;"
	        "}"
	        ".clear {"
	          "clear: both;"
	        "}"
	        "span.ip {"
	          "color: #35f;"
	        "}"
	        ".top-bar-right {"
	          "float: right;"
	        "}"
	        ".bottom-bar {"
	          "margin-top: 1em;"
	        "}"
	        ));
}

void print_page_header(http_context *http)
{
	PRINT(S("<!DOCTYPE html>"
	        "<html>"
	          "<head>"
	            "<style>"));
	write_page_css(http);
	PRINT(S(    "</style>"
	          "</head>"
	          "<body>"));
}

void print_page_footer(http_context *http)
{
	PRINT(S(    "<div class='footer'>"
	              "Proudly made without PHP, Java, Perl, MySQL, Postgres, MongoDB and Node.js.<br>"
	            "</div>"
	          "</body>"
	        "</html>"));
}

void print_post_url2(http_context *http, struct board *board, struct thread *thread, struct post *post, int absolute)
{
	if (absolute) {
		struct post *first_post = thread_first_post(thread);

		PRINT(S(PREFIX), S("/"), E(board_name(board)), S("/"), UL(post_id(first_post)));

		if (first_post == post)
			return;
	}
	PRINT(S("#"), UL(post_id(post)));
}

void print_post_url(http_context *http, struct post *post, int absolute)
{
	struct thread *thread = 0;
	struct board *board = 0;

	if (absolute) {
		thread = post_thread(post);
		board = thread_board(thread);
	}
	print_post_url2(http, board, thread, post, absolute);
}

void pretty_print_mime(http_context *http, const char *mime_type)
{
	if (case_equals(mime_type, "image/jpeg")) {
		// Print JPG instead of JPEG
		PRINT(S("JPG"));
		return;
	}
	char *buf = alloca(strlen(mime_type)+1);
	const char *c = &mime_type[str_chr(mime_type, '/')];
	assert (*c != '\0');
	++c;
	char *o = buf;
	for (; *c != '\0'; ++c) {
		*o = toupper(*c);
		++o;
	}
	*o = '\0';
	PRINT(S(buf));
}


static void calculate_thumbnail_size(uint64 *w, uint64 *h, uint64 max_size)
{
	if (*w <= max_size & *h <= max_size)
		return;

	double ww = *w;
	double hh = *h;
	if (ww > hh) {
		*w = max_size;
		*h = hh/ww*max_size;
	} else {
		*h = max_size;
		*w = ww/hh*max_size;
	}
}

void print_upload(http_context *http, struct upload *upload)
{
	uint64 w = upload_width(upload);
	uint64 h = upload_height(upload);
	calculate_thumbnail_size(&w,&h,200);

	char buf[256];
	strcpy(buf, upload_original_name(upload));
	abbreviate_filename(buf, 0.15*w*strlen(buf)/estimate_width(buf));

	const char *mime = upload_mime_type(upload);

	PRINT(S("<div class='file'>"
	          "<div class='file-header'>"
	            "<span class='file-name'>"
	              "<a href='"),S(PREFIX), S("/uploads/"), E(upload_file(upload)), S("' "
	                 "download='"), E(upload_original_name(upload)), S("'"));
	if (strlen(buf) < strlen(upload_original_name(upload)))
		PRINT(S(     " title='"), E(upload_original_name(upload)), S("'"));
	PRINT(S(      ">"),
	                E(buf),  S(
	              "</a>"
	            "</span>"
	          "</div>"
	          "<div class='file-subheader'>"
	            "<span class='file-type'>"));
	               pretty_print_mime(http, mime);
	PRINT(S(    "</span>"));

	if (case_starts(mime, "image/") || case_starts(mime, "video/"))
		PRINT(S(" <span class='file-dimensions'>"),
		            I(upload_width(upload)), S("×"), I(upload_height(upload)),
		      S("</span>"));

	if (case_starts(mime, "video/"))
		PRINT(S(" <span class='file-duration'>"), TIME_MS(upload_duration(upload)), S("</span>"));

	PRINT(S(    " <span class='file-size'>"), HK(upload_size(upload)), S("</span>"
	          "</div>"
	          "<div class='file-thumbnail'>"
	            "<a href='"), S(PREFIX), S("/uploads/"), E(upload_file(upload)), S("'>"
	              "<img class='file-thumbnail-img' "
	                   "src='"), S(PREFIX), S("/uploads/"), S(upload_thumbnail(upload)), S("'>"
	            "</a>"
	          "</div>"
	        "</div>"));
}

void print_post(http_context *http, struct post *post, int absolute_url, int flags)
{
	struct thread *thread = post_thread(post);
	int is_first = thread && thread_first_post(thread) == post;

	PRINT(S("<div class='post-wrapper'>"
	          "<div class='post "), is_first?S("first"):S("reply"), S("'"
	              " id='"), UL(post_id(post)), S("'>"
	            "<ul>"
	              "<li>"
	                "<div class='post-header'>"
	                  "<span class='delete'>"
	                    "<input type='checkbox' name='post' value='"), UL(post_id(post)), S("'>"
	                  "</span><span class='space'> </span>"
	                  "<span class='link'>"
	                    "<a href='"));
	                      print_post_url(http, post, absolute_url);
	PRINT(S(            "'>[l]</a>"
	                  "</span><span class='space'> </span>"
	                  "<span class='subject'>"
	                    "<b>"), E(post_subject(post)), S("</b>"
	                  "</span>"),
	                  (post_subject(post)[0] != '\0')?S("<span class='space'> </span>"):S(""),S(
	                  "<span class='username'>"),
	                  (post_username(post)[0] == '\0')?E(DEFAULT_NAME):E(post_username(post)),S(
	                  "</span><span class='space'> </span>"));
	if (flags & WRITE_POST_IP) {
		PRINT(S(      "<span class='ip client-ip' title='Client IP'>"), IP(post_ip(post)), S("</span>"));
		if (post_x_real_ip(post).version)
			PRINT(S(  "<span class='space'> / </span>"
			          "<span class='ip x-real-ip' title='X-Real-IP'>"), IP(post_x_real_ip(post)), S("</span>"));
		if (post_x_forwarded_for_count(post)) {
			PRINT(S(  "<span class='space'> / </span>"
			          "<span class='ip x-forwarded-for' title='X-Forwarded-For'>"));
			struct ip *ips = post_x_forwarded_for(post);
			int comma = 0;
			for (size_t i=0; i<post_x_forwarded_for_count(post); ++i) {
				PRINT(comma?S("<span class='comma'>, </span>"):S(""), S("<span>"), IP(ips[i]), S("</span>"));
				comma = 1;
			}
			PRINT(S(  "</span>"));
		}
	}
	PRINT(S(          "<span class='space'> </span>"
	                  "<span class='time'>"), HTTP_DATE(post_timestamp(post)), S("</span>"
	                  "<span class='space'> </span>"
	                  "<span class='number'>Nr. "), UL(post_id(post)), S("</span>"
	                  "<span class='space'> </span>"),
	                  (post_sage(post))?
	                    S("<span class='sage'>SÄGE</span>"):S(""),
	                  (thread_pinned(thread) && thread_first_post(thread) == post)?
	                    S("<span class='sticky'>Klebrig</span><span class='space'> </span>"):S(""),
	                  (thread_closed(thread) && thread_first_post(thread) == post)?
	                    S("<span class='closed'>Geschlossen</span><span class='space'> </span>"):S(""), S(
	                "</div>"
	                "<div class='content'>"));
	struct upload *up = post_first_upload(post);
	int multi_upload = 0;
	if (up) {
		PRINT(S("<div class='files"), upload_next_upload(up)?S(" multiple"):S(""), S("'>"));
		while (up) {
			print_upload(http, up);
			up = upload_next_upload(up);
		}
		PRINT(S("</div>"));
	}
	PRINT(S(        "<div class='text'>"));
	write_bbcode(http, post_text(post), absolute_url?0:thread);
	PRINT(S(        "</div>"));
	if (post_banned(post) && post_ban_message(post))
		PRINT(S(    "<div class='banned'>("), E(post_ban_message(post)), S(")</div>"));
	PRINT(S(        "</div>"
	              "</li>"
	            "</ul>"
	          "</div>"
	        "</div>"));
}

size_t estimate_width(const char *buffer)
{
	// Try to guesstimate the length of the text in a proportional font, measured in "average"
	// characters.
	const char *c = buffer;
	size_t w = 0;
	for (; *c != '\0'; ++c) {
		switch (*c) {
			// Thin character
			case 'i': case 'I': case 'l': case '(': case ')': case '|': case '.': case ',':
			case '-': case '\'': case '"': case ' ': case 'r': case 't': case 'f':
				w += 7; break;
			// Wide character
			case '_': case 'w': case 'W': case 'M': case 'm':
				w += 15; break;
			// Medium character
			default:
				w += 12;
		}
	}
	return w/10;
}

void abbreviate_filename(char *buffer, size_t max_length)
{
	const char *ellipsis = "[…]";
	size_t len = strlen(buffer);

	if (len <= max_length)
		return;

	size_t diff = len-max_length + strlen(ellipsis);

	size_t ext = str_rchr(buffer, '.');

	size_t end = ext;

	ssize_t start = end - diff;
	if (start < 1)
		start = 1;

	memmove(&buffer[start+strlen(ellipsis)], &buffer[end], len - end + 1);
	memmove(&buffer[start], ellipsis, strlen(ellipsis));
}


void print_session(http_context *http, struct session *session)
{
	if (!session)
		PRINT(S("Set-Cookie: session=; path="), S(PREFIX), S("/; expires=Thu, 01 Jan 1970 00:00:00 GMT;\r\n"));
}

void find_bans(struct ip *ip, struct board *board, find_bans_callback callback, void *extra)
{
	uint64 now = time(NULL);

	struct ip_range range = {0};
	range.ip = *ip;

	switch (ip->version) {
		case IP_V4: range.range =  32; break;
		case IP_V6: range.range = 128; break;
	}

	while (range.range >= 0) {
		for (struct ban *ban = db_hashmap_get(&ban_tbl, &range); ban; ban=ban_next_in_bucket(ban)) {
			if (ban_enabled(ban) &&
			    ban_target(ban) == BAN_TARGET_POST &&
			    ((ban_duration(ban) < 0) || (now <= ban_timestamp(ban) + ban_duration(ban))) &&
			    ban_matches_ip(ban, ip) &&
			    ban_matches_board(ban, board_id(board))) {

			    callback(ban, extra);
			}
		}
		--(range.range);
	}
}

struct is_banned_info {
	enum ban_target target;
	enum ban_type type;
	int64 expires;
};

static void is_banned_callback(struct ban *ban, void *extra)
{
	struct is_banned_info *info = (struct is_banned_info*)extra;
	if (ban_type(ban) == info->type &&
	    ban_target(ban) == info->target) {
		if (ban_duration(ban) > 0) {
			int64 expires = ban_timestamp(ban) + ban_duration(ban);
			if (expires > info->expires)
				info->expires = expires;
		} else {
			info->expires = -1;
		}
	}
}

int64 is_banned(struct ip *ip, struct board *board, enum ban_target target)
{
	struct is_banned_info info = {0};
	info.type = BAN_BLACKLIST;
	info.target = target;
	find_bans(ip, board, is_banned_callback, &info);
	return info.expires;
}

int64 is_flood_limited(struct ip *ip, struct board *board, enum ban_target target)
{
	struct is_banned_info info = {0};
	info.type = BAN_FLOOD;
	info.target = target;
	find_bans(ip, board, is_banned_callback, &info);
	return info.expires;
}

int64 is_captcha_required(struct ip *ip, struct board *board, enum ban_target target)
{
	struct is_banned_info info = {0};
	info.type = BAN_CAPTCHA_PERMANENT;
	info.target = target;
	find_bans(ip, board, is_banned_callback, &info);
	if (!info.expires) {
		info.type = BAN_CAPTCHA_ONCE;
		info.target = target;
		find_bans(ip, board, is_banned_callback, &info);
	}
	return info.expires;
}

int64 any_ip_affected(struct ip *ip, struct ip *x_real_ip, array *x_forwarded_for,
                      struct board *board, enum ban_target target,
                      int64 (*predicate)(struct ip *ip, struct board *board, enum ban_target target))
{
	int64 affected = 0;
	affected = predicate(ip, board, BAN_TARGET_POST);
	if (!affected)
		affected = predicate(x_real_ip, board, BAN_TARGET_POST);
	if (!affected) {
		size_t count = array_length(x_forwarded_for, sizeof(struct ip));
		for (size_t i=0; i<count; ++i) {
			struct ip *x = array_get(x_forwarded_for, sizeof(struct ip), i);
			if (affected = predicate(x, board, BAN_TARGET_POST))
				break;
		}
	}
	return affected;
}

size_t parse_boards(http_context *http, char *s, array *boards, int *ok)
{
	int i = 0;
	size_t boards_count = 0;
	*ok = 1;
	while (1) {
		i += scan_whiteskip(&s[i]);
		int d = scan_nonwhiteskip(&s[i]);
		if (!d) break;

		char tmp = s[i+d];
		s[i+d] = '\0';
		struct board *board = find_board_by_name(&s[i]);
		if (!board) {
			if (http)
				PRINT(S("<p class='error'>Brett existiert nicht: /"), E(&s[i]), S("/</p>"));
			*ok = 0;
			i += d;
			continue;
		}
		s[i+d] = tmp;
		i += d;
		int dup = 0;
		for (int j=0; j<boards_count; ++j) {
			struct board **member = array_get(boards, sizeof(struct board*), j);
			if (*member == board) {
				dup = 1;
				break;
			}
		}
		if (!dup) {
			struct board **member = array_allocate(boards, sizeof(struct board*), boards_count);
			*member = board;
			++boards_count;
		}
	}

	return boards_count;
}

void parse_x_forwarded_for(array *ips, const char *header)
{
	size_t offset = 0;
	struct ip ip;
	while (1) {
		offset += scan_whiteskip(&header[offset]);
		size_t delta=0;
		delta = scan_ip(&header[offset], &ip);
		if (!delta)
			break;
		offset += delta;
		offset += scan_whiteskip(&header[offset]);
		if (header[offset] == ',')
			++offset;

		size_t count = array_length(ips, sizeof(struct ip));
		struct ip *member = array_allocate(ips, sizeof(struct ip), count);
		*member = ip;
	}
}

void print_escaped(context *ctx, const char *unescaped)
{
	size_t len = fmt_escape(FMT_LEN, unescaped);
	char *buf = alloca(len+1);
	buf[fmt_escape(buf, unescaped)] = '\0';
	context_write_data(ctx, buf, len);
}
