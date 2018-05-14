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

void write_reply_form(http_context *http, int board, int thread, struct captcha *captcha)
{
	HTTP_WRITE("<form class='reply' action='" PREFIX "/post' method='post' enctype='multipart/form-data' novalidate autocomplete='on'>"
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
	                 "<th colspan='3'>");
	if (thread != -1)
		HTTP_WRITE( "<h3>Antwort erstellen</h3>");
	else
		HTTP_WRITE( "<h3>Neuen Faden erstellen</h3>");
	HTTP_WRITE(      "</th>"
	               "</tr>"
	               "<tr>"
	                 "<th><label for='sage'>Säge</label></th>"
	                 "<td colspan='2'><input type='checkbox' name='sage' value='1'></td>"
	               "</tr>"
	               "<tr>"
	                 "<th><label for='subject'>Betreff</label></th>"
	                 "<td><input name='subject' type='text'></td>"
	                 "<td width='1'>");
	if (thread != -1)
		HTTP_WRITE(      "<input type='submit' value='Antwort erstellen'>");
	else
		HTTP_WRITE(      "<input type='submit' value='Absenden'>");

	HTTP_WRITE(      "</td>"
	               "</tr>"
	               "<tr>"
	                 "<th><label for='username2'>Name</label></th>"
	                 "<td colspan='2'><input name='username2' type='text' autocomplete='nein'></td>"
	                 //"<td colspan='2'><input name='username2' type='text' ></td>"
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
	               "</tr>");
	if (captcha) {
		HTTP_WRITE("<tr>"
		             "<th></th>"
		             "<td colspan='2'><center>"
		               "<img class='captcha' src='" PREFIX "/captchas/");
		HTTP_WRITE_XLONG(captcha_id(captcha));
		HTTP_WRITE(    ".png'></center>"
		             "</td>"
		           "</tr>"
		           "<tr>"
		             "<th><label for='captcha'>Captcha</label></th>"
		             "<td colspan='2'><input type='text' name='captcha' autocomplete='off'></td>"
		           "</tr>");
	}
	HTTP_WRITE(  "</table>");

	if (captcha) {
		HTTP_WRITE("<input name='captcha_id' value='");
		HTTP_WRITE_XLONG(captcha_id(captcha));
		HTTP_WRITE("' type='hidden'>");
		HTTP_WRITE("<input name='captcha_token' value='");
		HTTP_WRITE_XLONG(captcha_token(captcha));
		HTTP_WRITE("' type='hidden'>");
	}

	if (thread != -1) {
		HTTP_WRITE("<input name='thread' value='");
		HTTP_WRITE_ULONG(thread);
		HTTP_WRITE("' type='hidden'>");
	} else {
		HTTP_WRITE("<input name='board' value='");
		HTTP_WRITE_ULONG(board);
		HTTP_WRITE("' type='hidden'>");
	}

	HTTP_WRITE("</form>");
}

void write_board_bar(http_context *http)
{
	struct board *board = master_first_board(master);
	HTTP_WRITE("<div class='boards'>");
	while (board) {
		HTTP_WRITE("<span class='board'><a href='" PREFIX "/");
		HTTP_WRITE_ESCAPED(board_name(board));
		HTTP_WRITE("/' title='");
		HTTP_WRITE_ESCAPED(board_title(board));
		HTTP_WRITE("'>[");
		HTTP_WRITE_ESCAPED(board_name(board));
		HTTP_WRITE("]</a></span>"
		           "<span class='space'> </span>");
		board = board_next_board(board);
	}

	HTTP_WRITE("</div>");
}

void write_top_bar(http_context *http, struct user *user, const char *url)
{
	HTTP_WRITE("<div class='top-bar'>");
	HTTP_WRITE("<div class='top-bar-right'>");
	if (user) {
		if (user_type(user) == USER_ADMIN || user_type(user) == USER_MOD)
			HTTP_WRITE("<a href='" PREFIX "/dashboard'>Kontrollzentrum</a><span class='space'> </span>");
		HTTP_WRITE("<a href='" PREFIX "/login?logout&amp;redirect=");
		HTTP_WRITE_ESCAPED(url);
		HTTP_WRITE("'>Ausloggen</a>");
	} else {
		HTTP_WRITE("<a href='" PREFIX "/login?redirect=");
		HTTP_WRITE_ESCAPED(url);
		HTTP_WRITE("'>Einloggen</a>");
	}
	HTTP_WRITE("</div>");
	write_board_bar(http);
	HTTP_WRITE("</div>");
}

void write_bottom_bar(http_context *http)
{
	HTTP_WRITE("<div class='bottom-bar'>");
	write_board_bar(http);
	HTTP_WRITE("</div>");
}

void write_mod_bar(http_context *http, int ismod)
{
	if (!ismod) {
		HTTP_WRITE("<div class='mod-bar'>"
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
		           "</div>");
	} else {
		HTTP_WRITE("<div class='mod-bar'>"
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
		           "</div>");
	}
}

void write_page_css(http_context *http)
{
	HTTP_WRITE("h1, h3 {"
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
              // ".post:target {"
              //   "background: #ffc;"
              // "}"
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
               ".mod-bar {"
                 "text-align: right;"
               "}"
               ".clear {"
                 "clear: both;"
               "}"
               "span.ip {"
                 "color: #00f;"
               "}"
               ".top-bar-right {"
                 "float: right;"
               "}"
               ".bottom-bar {"
                 "margin-top: 1em;"
               "}"
               );
}

void write_page_header(http_context *http)
{
	HTTP_WRITE("<!DOCTYPE html>"
	           "<html>"
	             "<head>"
	               "<style>");
	write_page_css(http);
	HTTP_WRITE(    "</style>"
	             "</head>"
	             "<body>");
}

void write_page_footer(http_context *http)
{
	HTTP_WRITE(    "<div class='footer'>"
	                 "Proudly made without PHP, Java, Perl, MySQL, Postgres, MongoDB and Node.js.<br>"
	               "</div>"
	             "</body>"
	           "</html>");
}

void write_post_url2(http_context *http, struct board *board, struct thread *thread, struct post *post, int absolute)
{
	if (absolute) {
		struct post *first_post = thread_first_post(thread);

		HTTP_WRITE(PREFIX "/");
		HTTP_WRITE_DYNAMIC(board_name(board));
		HTTP_WRITE("/");
		HTTP_WRITE_ULONG(post_id(first_post));

		if (first_post == post)
			return;
	}
	HTTP_WRITE("#");
	HTTP_WRITE_ULONG(post_id(post));
}

void write_post_url(http_context *http, struct post *post, int absolute)
{
	struct thread *thread = 0;
	struct board *board = 0;

	if (absolute) {
		thread = post_thread(post);
		board = thread_board(thread);
	}
	write_post_url2(http, board, thread, post, absolute);
}

void pretty_print_mime(http_context *http, const char *mime_type)
{
	if (case_equals(mime_type, "image/jpeg")) {
		// Print JPG instead of JPEG
		HTTP_WRITE("JPG");
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
	HTTP_WRITE_DYNAMIC(buf);
}

size_t fmt_time(char *out, uint64 ms)
{
	uint64 t = ms;
	/*uint64 msecs  = t % 1000;*/ t /= 1000;
	uint64 secs   = t % 60;   t /= 60;
	uint64 mins   = t % 60;   t /= 60;
	uint64 hours  = t;

	char *o = out;
	if (hours > 0) {
		o += fmt_int(out?o:FMT_LEN, hours);
		if (out) *o = ':';
		++o;
	}
	o += fmt_uint0(out?o:FMT_LEN, mins, (hours>0)?2:1);
	if (out) *o = ':';
	++o;
	o += fmt_uint0(out?o:FMT_LEN, secs, 2);

	return o-out;
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

void write_upload(http_context *http, struct upload *upload)
{
	uint64 w = upload_width(upload);
	uint64 h = upload_height(upload);
	calculate_thumbnail_size(&w,&h,200);

	HTTP_WRITE("<div class='file'>"
	             "<div class='file-header'>"
	               "<span class='file-name'><a href='");
	HTTP_WRITE(PREFIX "/uploads/"); // Todo: Don't hardcode
	HTTP_WRITE_ESCAPED(upload_file(upload));
	HTTP_WRITE(    "' download='");
	HTTP_WRITE_ESCAPED(upload_original_name(upload));
	char buf[256];
	strcpy(buf, upload_original_name(upload));
	abbreviate_filename(buf, 0.15*w*strlen(buf)/estimate_width(buf));
	HTTP_WRITE(    "'");
	if (strlen(buf) < strlen(upload_original_name(upload))) {
		HTTP_WRITE(" title='");
		HTTP_WRITE_ESCAPED(upload_original_name(upload));
		HTTP_WRITE("'");
	}
	HTTP_WRITE(    ">");
	HTTP_WRITE_ESCAPED(buf);
	HTTP_WRITE(    "</a>");
	HTTP_WRITE(    "</span>"
	             "</div>"
	             "<div class='file-subheader'>"
	               "<span class='file-type'>");
	const char *mime = upload_mime_type(upload);
	pretty_print_mime(http, mime);
	HTTP_WRITE(    "</span>");
	if (case_starts(mime, "image/") || case_starts(mime, "video/")) {
		HTTP_WRITE(" <span class='file-dimensions'>");
		HTTP_WRITE_LONG(upload_width(upload));
		HTTP_WRITE("×");
		HTTP_WRITE_LONG(upload_height(upload));
		HTTP_WRITE("</span>");
	}
	if (case_starts(mime, "video/")) {
		HTTP_WRITE(" <span class='file-duration'>");
		HTTP_WRITE_TIME(upload_duration(upload));
		HTTP_WRITE("</span>");
	}
	HTTP_WRITE(    " <span class='file-size'>");
	HTTP_WRITE_HUMANK(upload_size(upload));
	HTTP_WRITE(    "</span>");
	HTTP_WRITE(    "</div>"
	             "<div class='file-thumbnail'>");

	HTTP_WRITE(  "<a href='");

	HTTP_WRITE(PREFIX "/uploads/"); // Todo: Don't hardcode
	HTTP_WRITE_ESCAPED(upload_file(upload));
	HTTP_WRITE(    "'>");
	HTTP_WRITE(      "<img class='file-thumbnail-img' src='");
	HTTP_WRITE(PREFIX "/uploads/"); // Todo: Don't hardcode
	HTTP_WRITE_ESCAPED(upload_thumbnail(upload));
	HTTP_WRITE(      "'>");
	HTTP_WRITE(    "</a>");

	HTTP_WRITE(  "</div>"
	           "</div>");
}

void write_post(http_context *http, struct post *post, int absolute_url, int flags)
{
	struct thread *thread = post_thread(post);
	int is_first = thread && thread_first_post(thread) == post;

	HTTP_WRITE("<div class='post-wrapper'>");

	if (is_first)
		HTTP_WRITE("<div class='post first'");
	else
		HTTP_WRITE("<div class='post reply'");

	HTTP_WRITE(  " id='");
	HTTP_WRITE_ULONG(post_id(post));
	HTTP_WRITE(  "'>"
	               "<ul>"
	                 "<li>"
	                   "<div class='post-header'>"
	                     "<span class='delete'>"
	                       "<input type='checkbox' name='post' value='");
	HTTP_WRITE_ULONG(post_id(post));
	HTTP_WRITE(          "'>"
	                     "</span><span class='space'> </span>"
			             "<span class='link'>"
		                   "<a href='");
	write_post_url(http, post, absolute_url);
	HTTP_WRITE(            "'>[l]</a>"
	                     "</span><span class='space'> </span>"
	                     "<span class='subject'><b>");
	HTTP_WRITE_ESCAPED(post_subject(post));
	HTTP_WRITE(          "</b></span>");
	if (post_subject(post)[0] != '\0') {
		HTTP_WRITE(      "<span class='space'> </span>");
	}
	HTTP_WRITE(          "<span class='username'>");
	if (post_username(post)[0] == '\0')
		HTTP_WRITE(DEFAULT_NAME);
	else
		HTTP_WRITE_ESCAPED(post_username(post));
	HTTP_WRITE(          "</span><span class='space'> </span>");
	if (flags & WRITE_POST_IP) {
		HTTP_WRITE(      "<span class='ip client-ip' title='Client IP'>");
		HTTP_WRITE_IP(post_ip(post));
		HTTP_WRITE(      "</span>");
		if (post_x_real_ip(post).version) {
			HTTP_WRITE(  "<span class='space'> / </span>"
			             "<span class='ip x-real-ip' title='X-Real-IP'>");
			HTTP_WRITE_IP(post_x_real_ip(post));
			HTTP_WRITE(  "</span>");
		}
		if (post_x_forwarded_for_count(post)) {
			HTTP_WRITE(  "<span class='space'> / </span>"
			             "<span class='ip x-forwarded-for' title='X-Forwarded-For'>");
			struct ip *ips = post_x_forwarded_for(post);
			int comma = 0;
			for (size_t i=0; i<post_x_forwarded_for_count(post); ++i) {
				if (comma)
					HTTP_WRITE("<span class='comma'>, </span>");
				HTTP_WRITE("<span>");
				HTTP_WRITE_IP(ips[i]);
				HTTP_WRITE("</span>");
				comma = 1;
			}
			HTTP_WRITE(  "</span>");
		}
	}
	HTTP_WRITE(          "<span class='space'> </span>");
	HTTP_WRITE(          "<span class='time'>");
	HTTP_WRITE_HTTP_DATE(post_timestamp(post));
	HTTP_WRITE(          "</span><span class='space'> </span>");
	HTTP_WRITE(          "<span class='number'>Nr. ");
	HTTP_WRITE_ULONG(post_id(post));
	HTTP_WRITE(          "</span><span class='space'> </span>");
	if (post_sage(post))
		HTTP_WRITE(      "<span class='sage'>SÄGE</span>");
	if (thread_pinned(thread) && thread_first_post(thread) == post)
		HTTP_WRITE(      "<span class='sticky'>Klebrig</span><span class='space'> </span>");
	if (thread_closed(thread) && thread_first_post(thread) == post)
		HTTP_WRITE(      "<span class='closed'>Geschlossen</span><span class='space'> </span>");
	HTTP_WRITE(        "</div>"
	                   "<div class='content'>");
	struct upload *up = post_first_upload(post);
	int multi_upload = 0;
	if (up) {
		if (upload_next_upload(up))
			multi_upload = 1;

		if (multi_upload)
	    	HTTP_WRITE(  "<div class='files multiple'>");
		else
	    	HTTP_WRITE(  "<div class='files'>");

		while (up) {
			write_upload(http, up);
			up = upload_next_upload(up);
		}
		HTTP_WRITE(      "</div>");
	}
	HTTP_WRITE(          "<div class='text'>");
	HTTP_WRITE_ESCAPED(post_text(post));
	HTTP_WRITE(          "</div>");
	HTTP_WRITE(        "</div>");
	HTTP_WRITE(      "</li>"
	               "</ul>");
	HTTP_WRITE(  "</div>");
	HTTP_WRITE("</div>");
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

static size_t html_escape_char(char *output, char character)
{
	char *entity;
	switch (character) {
		case '&':  entity = "&amp;";  break;
 		case '<':  entity = "&lt;";   break;
 		case '>':  entity = "&gt;";   break;
 		case '"':  entity = "&quot;"; break;
 		case '\'': entity = "&#x27;"; break;
 		case '/':  entity = "&#x2F;"; break;
 		default:
 			if (output)
 				*output = character;
 			return 1;
	}
	if (output)
		strncpy(output, entity, strlen(entity));
	return strlen(entity);
}

size_t fmt_escape(char *buf, const char *unescaped)
{
	size_t escaped_length = 0;
	const char *c;
	if (!buf) {
		for (c = unescaped; *c != '\0'; ++c) {
			escaped_length += html_escape_char(FMT_LEN, *c);
		}
	} else {
		char *o = buf;
		for (c = unescaped; *c != '\0'; ++c) {
			size_t d = html_escape_char(o, *c);
			o += d;
			escaped_length += d;
		}
	}
	return escaped_length;
}

void write_escaped(context *ctx, const char *unescaped)
{
	size_t len = fmt_escape(FMT_LEN, unescaped);
	char *buf = alloca(len+1);
	buf[fmt_escape(buf, unescaped)] = '\0';
	context_write_data(ctx, buf, len);
}

void write_session(http_context *http, struct session *session)
{
	if (!session)
		HTTP_WRITE("Set-Cookie: session=; path=" PREFIX "/; expires=Thu, 01 Jan 1970 00:00:00 GMT;\r\n");
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
			if (http) {
				HTTP_WRITE("<p class='error'>Brett existiert nicht: /");
				HTTP_WRITE_DYNAMIC(&s[i]);
				HTTP_WRITE("/</p>");
			}
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
