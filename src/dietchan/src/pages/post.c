#define _BSD_SOURCE 1
#define _GNU_SOURCE 1
#define _XOPEN_SOURCE 1
#include "post.h"

#include "malloc.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

#include <libowfat/byte.h>
#include <libowfat/case.h>
#include <libowfat/str.h>
#include <libowfat/scan.h>
#include <libowfat/fmt.h>
#include <libowfat/open.h>
#include <libowfat/textcode.h>
#include <libowfat/ip4.h>
#include <libowfat/ip6.h>

#include "../arc4random.h"
#include "../db.h"
#include "../upload_job.h"
#include "../util.h"
#include "../captcha.h"
#include "../bans.h"


#include "../tpl.h"
#include "../mime_types.h"


static int  post_page_header (http_context *http, char *key, char *val);
static int  post_page_post_param (http_context *http, char *key, char *val);
static int  post_page_cookie (http_context *http, char *key, char *val);
static int  post_page_file_begin (http_context *http, char *name, char *filename, char *content_type);
static int  post_page_file_content (http_context *http, char *buf, size_t length);
static int  post_page_file_end (http_context *http);
static int  post_page_finish (http_context *http);
static void post_page_finalize(http_context *http);

static void post_page_upload_job_mime(struct upload_job *upload_job, char *mime_type);
static void post_page_upload_job_finish(struct upload_job *upload_job);
static void post_page_upload_job_error(struct upload_job *upload_job, int status, char *message);

void post_page_init(http_context *http)
{
	struct post_page *page = malloc(sizeof(struct post_page));
	byte_zero(page, sizeof(struct post_page));

	page->board = -1;
	page->thread = -1;
	page->pending = 0;

	http->info = page;

	http->header       = post_page_header;
	http->post_param   = post_page_post_param;
	http->cookie       = post_page_cookie;
	http->file_begin   = post_page_file_begin;
	http->file_content = post_page_file_content;
	http->file_end     = post_page_file_end;
	http->finish       = post_page_finish;
	http->finalize     = post_page_finalize;

	// Optional parameters
	page->username = strdup("");
	page->password = strdup("");
	page->subject = strdup("");

	byte_copy(&page->ip, sizeof(struct ip), &http->ip);

}

static int post_page_header (http_context *http, char *key, char *val)
{
	struct post_page *page = (struct post_page*)http->info;

	PARAM_STR("User-Agent", page->user_agent);

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

static int post_page_post_param (http_context *http, char *key, char *val)
{
	struct post_page *page = (struct post_page*)http->info;

	PARAM_I64("thread",        page->thread);
	PARAM_I64("board",         page->board);
	PARAM_STR("subject",       page->subject);
	PARAM_STR("username2",     page->username);
	PARAM_STR("text2",         page->text);
	PARAM_STR("password",      page->password);
	PARAM_I64("sage",          page->sage);

	PARAM_STR("captcha",       page->captcha);
	PARAM_X64("captcha_id",    page->captcha_id);
	PARAM_X64("captcha_token", page->captcha_token);

	// Bot trap
	if (case_equals(key, "username") ||
	    case_equals(key, "text") ||
	    case_equals(key, "comment") ||
	    case_equals(key, "website")) {
		if (val[0] != '\0')
			page->is_bot = 1;
		return 0;
	}

	if (case_equals(key, "dummy")) {
		return 0;
	}

	HTTP_FAIL(BAD_REQUEST);
}

static int post_page_cookie (http_context *http, char *key, char *val)
{
	struct post_page *page = (struct post_page*)http->info;
	PARAM_SESSION();
	return 0;
}

static int post_page_file_begin (http_context *http, char *name, char *filename, char *content_type)
{
	struct context* ctx = (context*)http;
	struct post_page *page = (struct post_page*)http->info;

	if (page->aborted)
		return ERROR;

	if (page->is_bot) // Don't waste any resources if it's a bot
		return 0;

	if (!case_equals(name, "file"))
		HTTP_FAIL(BAD_REQUEST);

	// We ignore the client-sent mime type since we cannot trust the information anyway.
	(void) content_type;

	size_t count = array_length(&page->upload_jobs, sizeof(struct upload_job));
	struct upload_job *upload_job = array_allocate(&page->upload_jobs, sizeof(struct upload_job), count);
	upload_job_init(upload_job, DOC_ROOT "/uploads/");
	upload_job->original_name = strdup(filename);
	upload_job->info = http;
	upload_job->mime = post_page_upload_job_mime;
	upload_job->finished = post_page_upload_job_finish;
	upload_job->error = post_page_upload_job_error;
	page->current_upload_job = upload_job;

	++page->pending;

	// Since uploads are handled asynchronously, we must increase the reference count of the
	// http_context. The reason is that the connection could already be closed by the client
	// when the asynchronous job completes. If we didn't increment the reference count, the closing
	// of the connection would cause the http_context to be destroyed, leading to a crash later.
	context_addref(ctx);

	return 0;
}

static int post_page_file_content (http_context *http, char *buf, size_t length)
{
	struct post_page *page = (struct post_page*)http->info;
	context *ctx = (context*)http;

	if (page->aborted)
		return ERROR;

	if (page->is_bot) // Don't waste any resources if it's a bot
		return 0;

	if (array_length(&page->upload_jobs, sizeof(struct upload_job)) > MAX_FILES_PER_POST) {
		// This check must be in file_content instead of file_begin because in file_begin
		// we don't know whether the field is actually empty or not.

		PRINT_STATUS_HTML("413 Too many files");
		PRINT_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>Error</h1>"
		        "<p>You may only attach up to "), L(MAX_FILES_PER_POST), S(" files.</p>"));
		PRINT_EOF();

		upload_job_abort(page->current_upload_job);
		page->aborted = 1;

		--page->pending;
		context_unref(ctx);
		return ERROR;
	}

	if (page->current_upload_job->size+length > MAX_UPLOAD_SIZE) {
		PRINT_STATUS_HTML("413 File too large");
		PRINT_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>Error</h1>"
		        "<p>The file "), E(page->current_upload_job->original_name), S(
		        " is larger than the allowed maximum of "), HK(MAX_UPLOAD_SIZE), S("B."));

		PRINT_EOF();

		upload_job_abort(page->current_upload_job);
		page->aborted = 1;

		--page->pending;
		context_unref(ctx);
		return ERROR;
	}


	upload_job_write_content(page->current_upload_job, buf, length);

	return 0;
}


static int post_page_file_end (http_context *http)
{
	struct post_page *page = (struct post_page*)http->info;
	context *ctx = (context*)http;

	if (page->is_bot) // Don't waste any resources if it's a bot
		return 0;

	if (page->current_upload_job->size == 0) {
		// Empty form field, ignore
		--page->pending;
		context_unref(ctx);

		upload_job_finalize(page->current_upload_job);
		page->current_upload_job = 0;
		ssize_t upload_job_count = array_length(&page->upload_jobs, sizeof(struct upload_job));
		array_truncate(&page->upload_jobs, sizeof(struct upload_job), upload_job_count - 1);
	} else {
		upload_job_write_eof(page->current_upload_job);
	}

	return 0;
}


static void post_page_upload_job_mime(struct upload_job *upload_job, char *mime_type)
{
	// Validate mime type
	http_context *http = (http_context*)upload_job->info;
	struct post_page *page = (struct post_page*)http->info;

	if (!is_mime_allowed(mime_type)) {
	    PRINT_STATUS_HTML("415 Unsupported media type");
		PRINT_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>Error</h1>"
		        "<p>Unsupported mime type: "), E(mime_type), S("<br>"),
		        E(upload_job->original_name), S("</p>"));

		PRINT_EOF();
		upload_job_abort(upload_job);
		page->aborted = 1;
	}

	const char *original_ext = strrchr(upload_job->original_name, '.');
	if (!is_valid_extension(mime_type, original_ext)) {
	    PRINT_STATUS_HTML("415 Unsupported media type");
		PRINT_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>Error</h1>"
		        "<p>Invalid file extension '"),original_ext?E(original_ext):S(""),
		      S("' for mime type '"), E(mime_type), S("'<br>"),
		      E(upload_job->original_name), S("</p>"));

		PRINT_EOF();
		upload_job_abort(upload_job);
		page->aborted = 1;
	}
}

static void post_page_upload_job_finish(struct upload_job *upload_job)
{
	http_context *http = (http_context*)upload_job->info;
	context *ctx = (context*)http;
	struct post_page *page = (struct post_page*)http->info;

	--page->pending;
	post_page_finish(http);
	context_unref(ctx);
}

static void post_page_upload_job_error(struct upload_job *upload_job, int status, char *message)
{
	http_context *http = (http_context*)upload_job->info;
	context *ctx = (context*)http;
	struct post_page *page = (struct post_page*)http->info;

	// We could have more than one error, but we can only handle the first one.
	if (!page->aborted) {
		PRINT_STATUS_HTML("500 Internal Server Error");
		PRINT_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>Error</h1>"
		        "<p>Could not process file: "), E(upload_job->original_name), S("<br>Corrupt file?</p>"));
		PRINT_EOF();
	}
	upload_job_abort(upload_job);
	page->aborted = 1;

	--page->pending;
	post_page_finish(http);
	context_unref(ctx);
}

static int post_page_finish (http_context *http)
{
	struct post_page *page = (struct post_page*)http->info;

	// We aborted due to an error and already sent a response
	if (page->aborted)
		return ERROR;

	// We are still waiting for uploads to be processed
	if (page->pending > 0)
		return 0;

	// Fake successful error code for bots in case they evaluate it
	if (page->is_bot) {
		// If any uploads happened, mark for deletion
		for (int i=0; i<array_length(&page->upload_jobs, sizeof(struct upload_job)); ++i) {
			struct upload_job *upload_job = array_get(&page->upload_jobs, sizeof(struct upload_job), i);
			upload_job_abort(upload_job);
		}
		PRINT_STATUS_HTML("200 OK");
		PRINT_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>Hello Robot :)</h1>"));
		PRINT_EOF();
		return 0;
	}

	struct board  *board;
	struct thread *thread;
	struct post   *post;

	if (page->board == -1 && page->thread == -1)
		HTTP_FAIL(BAD_REQUEST);

	if (page->thread == -1) {
		board = find_board_by_id(page->board);
		if (!board) {
			PRINT_STATUS_HTML("404 Gibbet nich");
			PRINT_SESSION();
			PRINT_BODY();
			PRINT(S("<h1>Brett existiert nicht :(</h1>"));
			PRINT_EOF();
			return ERROR;
		}
	} else {
		thread = find_thread_by_id(page->thread);
		if (!thread) {
			PRINT_STATUS_HTML("404 Gibbet nich");
			PRINT_SESSION();
			PRINT_BODY();
			PRINT(S("<h1>Faden existiert nicht :(</h1>"));
			PRINT_EOF();
			return ERROR;
		}

		if (thread_closed(thread)) {
			PRINT_STATUS_HTML("402 Verboten");
			PRINT_SESSION();
			PRINT_BODY();
			PRINT(S("<h1>Faden geschlossen.</h1>"));
			PRINT_EOF();
			return ERROR;

		}

		board = thread_board(thread);
	}

	// Check if user is banned

	int64 banned = any_ip_affected(&page->ip, &page->x_real_ip, &page->x_forwarded_for,
	                               board, BAN_TARGET_POST, is_banned);

	if (banned) {
		PRINT_REDIRECT("302 Found",
		               S(PREFIX), S("/banned"));
		return ERROR;
	}

	// New threads must contain text.
	// Posts without text are okay, as long as they contain at least one file.
	if ((!page->text || page->text[0] == '\0') &&
	    !(page->thread != -1 && array_length(&page->upload_jobs, sizeof(struct upload_job)) > 0)) {
		PRINT_STATUS_HTML("400 Not okay");
		PRINT_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>Beitrag muss einen Text enthalten!</h1>"));
		PRINT_EOF();
		return ERROR;
	}

	// New threads must contain an image
	if (page->thread == -1 && array_length(&page->upload_jobs, sizeof(struct upload_job)) <= 0) {
		PRINT_STATUS_HTML("400 Not okay");
		PRINT_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>Neuer Faden muss ein Bild enthalten.</h1>"));
		PRINT_EOF();
		return ERROR;
	}

	// Length checks
	if (strlen(page->text) > POST_MAX_BODY_LENGTH) {
		PRINT_STATUS_HTML("400 Not okay");
		PRINT_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>Beitrag ist zu lang! (maximal "), L(POST_MAX_BODY_LENGTH), S(" Zeichen)</h1>"));
		PRINT_EOF();
		return ERROR;
	}

	if (strlen(page->subject) > POST_MAX_SUBJECT_LENGTH) {
		PRINT_STATUS_HTML("400 Not okay");
		PRINT_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>Betreff ist zu lang! (maximal "), L(POST_MAX_SUBJECT_LENGTH), S(" Zeichen)</h1>"));
		PRINT_EOF();
		return ERROR;
	}

	if (strlen(page->username) > POST_MAX_NAME_LENGTH) {
		PRINT_STATUS_HTML("400 Not okay");
		PRINT_SESSION();
		PRINT_BODY();
		PRINT(S("<h1>Name ist zu lang! (maximal "), L(POST_MAX_NAME_LENGTH), S(" Zeichen)</h1>"));
		PRINT_EOF();
		return ERROR;
	}

	// Check if user is flood-limited

	int64 flood = any_ip_affected(&page->ip, &page->x_real_ip, &page->x_forwarded_for,
	                              board, BAN_TARGET_POST, is_flood_limited);

	if (flood) {
		uint64 now = time(0);
		PRINT_STATUS_HTML("403 Verboten");
		PRINT_BODY();
		PRINT(S("<p>Flood protection: Du kannst den nächsten Beitrag erst in "), UL(flood - now), S(" Sekunden erstellen.</p>"));
		PRINT_EOF();
		return ERROR;
	}

	// Check captcha
	if (any_ip_affected(&page->ip, &page->x_real_ip, &page->x_forwarded_for,
	                    board, BAN_TARGET_POST, is_captcha_required)) {
		if (!page->captcha || str_equal(page->captcha, "")) {
			PRINT_STATUS_HTML("403 Verboten");
			PRINT_BODY();
			PRINT(S("<p>Du hast das Captcha nicht eingegeben.</p>"));
			PRINT_EOF();
			return ERROR;
		}
		struct captcha *captcha = find_captcha_by_id(page->captcha_id);
		if (!captcha || captcha_token(captcha) != page->captcha_token) {
			PRINT_STATUS_HTML("403 Verboten");
			PRINT_BODY();
			PRINT(S("<p>Captcha abgelaufen :(</p>"));
			PRINT_EOF();
			return ERROR;
		}
		int valid = case_equals(captcha_solution(captcha), page->captcha);
		if (valid)
			replace_captcha(captcha);
		else {
			invalidate_captcha(captcha);
			PRINT_STATUS_HTML("403 Verboten");
			PRINT_BODY();
			PRINT(S("<p>Dein eingegebenes Captcha stimmt leider nicht :(</p>"));
			PRINT_EOF();
			return ERROR;
		}
	}


	begin_transaction();

	if (page->thread == -1) {
		// Create new thread
		thread = thread_new();
		thread_set_board(thread, board);

		bump_thread(thread);

		uint64 thread_count = board_thread_count(board);
		++thread_count;
		board_set_thread_count(board, thread_count);

		post = post_new();
		thread_set_first_post(thread, post);
		thread_set_last_post(thread, post);

		// Prune oldest thread
		if (thread_count > MAX_PAGES*THREADS_PER_PAGE)
			delete_thread(board_last_thread(board));
	} else {
		// Create reply
		post = post_new();
		struct post *prev = thread_last_post(thread);
		post_set_next_post(prev, post);
		post_set_prev_post(post, prev);
		thread_set_last_post(thread, post);


		// Bump thread unless post was saged or whole thread is saged.
		if (!page->sage && !thread_saged(thread))
			bump_thread(thread);
	}

	uint64 post_count = thread_post_count(thread);
	++post_count;
	thread_set_post_count(thread, post_count);

	// Autosage
	if (thread_post_count(thread) == BUMP_LIMIT-1)
		thread_set_saged(thread,1);

	// Autoclose
	if (thread_post_count(thread) == POST_LIMIT-1)
		thread_set_closed(thread,1);

	post_set_id(post, master_post_counter(master)+1);
	master_set_post_counter(master, post_id(post));
	db_hashmap_insert(&post_tbl, &post_id(post), post);

	uint64 timestamp = time(NULL);

	const char *password = "";
	if (page->password[0] != '\0')
		password = crypt_password(page->password);

	// We don't support tripcodes at the moment, strip everything after # for security.
	page->username[str_chr(page->username, '#')] = '\0';

	post_set_thread(post, thread);
	post_set_timestamp(post, timestamp);
	post_set_subject(post, page->subject);
	post_set_username(post, page->username);
	post_set_text(post, page->text);
	post_set_password(post, password);
	post_set_ip(post, page->ip);
	post_set_x_real_ip(post, page->x_real_ip);
	if (array_bytes(&page->x_forwarded_for) > 0) {
		size_t len = array_length(&page->x_forwarded_for, sizeof(struct ip));
		struct ip *ips = db_alloc(db, sizeof(struct ip)*len);
		byte_copy(ips, sizeof(struct ip)*len, array_start(&page->x_forwarded_for));
		db_invalidate_region(db, ips, sizeof(struct ip)*len);
		post_set_x_forwarded_for_count(post, len);
		post_set_x_forwarded_for(post, ips);
	}

	post_set_sage(post, page->sage);

	for (int i=0; i<array_length(&page->upload_jobs, sizeof(struct upload_job)); ++i) {
		struct upload_job *upload_job = array_get(&page->upload_jobs, sizeof(struct upload_job), i);
		if (!upload_job->ok)
			continue;
		if (upload_job->size == 0)
			continue;

		struct upload *up = upload_new();

		// Use timestamp to generate file name
		uint64 upload_id = timestamp*1000 + 1;
		uint64 last_upload_id = master_last_upload(master);
		if (upload_id <= last_upload_id)
			upload_id = last_upload_id+1;

		master_set_last_upload(master, upload_id);

		char filename[32];
		byte_zero(filename, sizeof(filename));
		fmt_ulong(filename, upload_id);
		strcat(filename, upload_job->file_ext);

		char thumb_filename[32];
		byte_zero(thumb_filename, sizeof(filename));
		fmt_ulong(thumb_filename, upload_id);
		strcat(thumb_filename, "s");
		strcat(thumb_filename, upload_job->thumb_ext);

		char file_path[256];
		strcpy(file_path, DOC_ROOT "/uploads/");
		strcat(file_path, filename);

		char thumb_path[256];
		strcpy(thumb_path, DOC_ROOT "/uploads/");
		strcat(thumb_path, thumb_filename);

		// Move temporary files to their final locations
		// Todo: handle errors
		rename(upload_job->file_path, file_path);
		rename(upload_job->thumb_path, thumb_path);

		upload_set_file(up, filename);
		upload_set_thumbnail(up, thumb_filename);
		upload_set_original_name(up, upload_job->original_name);
		upload_set_mime_type(up, upload_job->mime_type);
		upload_set_size(up, upload_job->size);
		upload_set_width(up, upload_job->width);
		upload_set_height(up, upload_job->height);
		upload_set_duration(up, upload_job->duration);
		upload_set_state(up, UPLOAD_NORMAL);

		if (i==0) {
			post_set_first_upload(post, up);
			post_set_last_upload(post, up);
		} else {
			struct upload *prev = post_last_upload(post);
			upload_set_prev_upload(up, prev);
			upload_set_next_upload(prev, up);
			post_set_last_upload(post, up);
		}
	}

	purge_expired_bans();

	struct ban *ban = ban_new();
	uint64 ban_counter = master_ban_counter(master)+1;
	master_set_ban_counter(master, ban_counter);
	ban_set_id(ban, ban_counter);
	ban_set_enabled(ban, 1);
	ban_set_type(ban, BAN_FLOOD);
	ban_set_target(ban, BAN_TARGET_POST);
	ban_set_timestamp(ban, timestamp);
	ban_set_duration(ban, FLOOD_LIMIT);
	struct ip_range range;
	range.ip = page->ip;
	range.range = 32; // FIXME: support IPV6
	ban_set_range(ban, range);
	ban_set_post(ban, post_id(post));

	insert_ban(ban);

	commit();

	// 302 apparently confuses Chrome's password manager
	//HTTP_STATUS("302 Success");
	//HTTP_WRITE("Location: ");
	PRINT_STATUS("200 Success");
	PRINT(S("Refresh: 0;"));
	print_post_url(http, post, 1);
	PRINT(S("\r\n"));
	PRINT_SESSION();
	PRINT_BODY();
	PRINT_EOF();

	return 0;
}

static void post_page_finalize (http_context *http)
{
	struct post_page *page = (struct post_page*)http->info;

	assert(page->pending == 0);

	if (page->subject)    free(page->subject);
	if (page->username)   free(page->username);
	if (page->text)       free(page->text);
	if (page->password)   free(page->password);
	if (page->user_agent) free(page->user_agent);
	array_reset(&page->x_forwarded_for);

	ssize_t upload_job_count = array_length(&page->upload_jobs, sizeof(struct upload_job));
	for (ssize_t i=0; i<upload_job_count; ++i) {
		struct upload_job *upload_job = array_get(&page->upload_jobs, sizeof(struct upload_job), i);
		upload_job_finalize(upload_job);
	}
	array_reset(&page->upload_jobs);

	free(page);
}

