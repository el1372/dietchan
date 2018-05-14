#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <libowfat/socket.h>
#include <libowfat/ip4.h>
#include <libowfat/io.h>
#include <libowfat/iob.h>
#include <libowfat/stralloc.h>
#include <libowfat/byte.h>
#include <libowfat/array.h>
#include <libowfat/buffer.h>
#include <libowfat/scan.h>
#include <libowfat/str.h>
#include <libowfat/case.h>
#include <libowfat/fmt.h>
#include "http.h"
#include "db.h"
#include "db_hashmap.h"
#include "persistence.h"
#include "captcha.h"

#include "pages/static.h"
#include "pages/post.h"
#include "pages/thread.h"
#include "pages/board.h"
#include "pages/mod.h"
#include "pages/login.h"
#include "pages/dashboard.h"
#include "pages/edit_user.h"
#include "pages/edit_board.h"

static int  default_get_param (http_context *http, char *key, char *val)
{
	HTTP_FAIL(BAD_REQUEST);
}
static int  default_post_param (http_context *http, char *key, char *val)
{
	HTTP_FAIL(METHOD_NOT_ALLOWED);
}
static int  default_file_begin (http_context *http, char *name, char *filename, char *content_type)
{
	HTTP_FAIL(METHOD_NOT_ALLOWED);
}
static int  default_file_content (http_context *http, char *buf, size_t length)
{
	HTTP_FAIL(METHOD_NOT_ALLOWED);
}
static int  default_file_end (http_context *http)
{
	HTTP_FAIL(METHOD_NOT_ALLOWED);
}
static int  default_finish (http_context *http)
{
	HTTP_FAIL(INTERNAL_SERVER_ERROR);
}

int request (http_context *http, http_method method, char *path, char *query)
{
	http->get_param    = default_get_param;
	http->post_param   = default_post_param;
	http->file_begin   = default_file_begin;
	http->file_content = default_file_content;
	http->file_end     = default_file_end;
	http->finish       = default_finish;

	if (str_equal(path, PREFIX "/post")) {
		post_page_init(http);
		goto found;
	}

	if (str_equal(path, PREFIX "/mod")) {
		mod_page_init(http);
		goto found;
	}

	if (str_equal(path, PREFIX "/login")) {
		login_page_init(http);
		goto found;
	}

	if (str_start(path, PREFIX "/dashboard")) {
		dashboard_page_init(http);
		goto found;
	}

	if (str_start(path, PREFIX "/edit_user")) {
		edit_user_page_init(http);
		goto found;
	}

	if (str_start(path, PREFIX "/edit_board")) {
		edit_board_page_init(http);
		goto found;
	}

	if (str_start(path, PREFIX "/uploads/") ||
	    str_start(path, PREFIX "/captchas/")) {
		static_page_init(http);
		goto found;
	}

	if (str_start(path, PREFIX "/") && path[strlen(path)-1] != '/') {
		thread_page_init(http);
		goto found;
	}

	if (str_start(path, PREFIX "/")) {
		board_page_init(http);
		goto found;
	}

	http->error_status = 404;
	http->error_message = "Not Found";

	return ERROR;

found:
	if (http->request != request) // Page set its own request handler
		return http->request(http, method, path, query);
	else
		return 0;
}

static void error (http_context *http)
{
	char err_status[FMT_LONG];
	byte_zero(err_status, sizeof(err_status));
	fmt_int(err_status, http->error_status);

	context *ctx = (context*)http;

	iob_adds(ctx->batch, "HTTP/1.1 ");
	iob_adds_free(ctx->batch, strdup(err_status));
	iob_adds(ctx->batch, " ");
	iob_adds(ctx->batch, http->error_message);
	iob_adds(ctx->batch, "\r\n");
	iob_adds(ctx->batch, "Connection: close\r\n");
	iob_adds(ctx->batch, "Content-Type: text/html; charset=utf-8\r\n");
	iob_adds(ctx->batch, "\r\n");
	iob_adds(ctx->batch, "<h1>");
	iob_adds_free(ctx->batch, strdup(err_status));
	iob_adds(ctx->batch, " ");
	iob_adds(ctx->batch, http->error_message);
	iob_adds(ctx->batch, "</h1>");

	context_eof(ctx);
}

//char ip[4] = {127,0,0,1};
char ip[4] = {0,0,0,0};
uint16 port;
int listener;
static char buf[8192];
http_context *http;
context *ctx;

void accept_connections(int limit)
{
	for (int i=0; i<limit; ++i) {
		int s=socket_accept4(listener, ip, &port);
		if (s==-1) {
			io_eagain_read(s);
		}
		if (s<0) return;

		http = http_new(s);

		http->ip.version = IP_V4;
		byte_copy(&http->ip.bytes, 4, ip);
		http->port = port;

		http->request = request;
		http->error   = error;
	}
}

void read_data(int connection, int64 bytes_limit)
{
	ctx = io_getcookie(connection);

	int64 bytes_read = 0;

	// I'm not sure if we can actually read only part of the available bytes or if this may cause us
	// to miss events. Because of this, the bytes_limit parameter is currently ignored.

	while (/*bytes_read < bytes_limit*/1) {
		int ret=io_tryread(connection, buf, sizeof(buf));
		if (ret == -1)
			return;

		if (ret >= 0) {
			context_read(ctx, buf, ret);
			bytes_read += ret;
		}

		if (ret <= 0) {
			// Important:
			// Without the call to io_dontwantread, we get the close notification again and
			// again. This leads to the reference count being decremented multiple times
			// when it should only be decremented once. The result is a crash.
			io_dontwantread(connection);

			context_unref(ctx);
			return;
		}
	}
}

int handle_read_events(int limit)
{
	for (int i=0; i<limit; ++i) {
		int s=io_canread();
		if (s == -1)
			return 0;
		if (s==listener) {
			accept_connections(1000);
		} else {
			read_data(s, 1);
		}
	}
	return 1;
}

int handle_write_events(int limit)
{
	for (int i=0; i<limit; ++i) {
		int s=io_canwrite();
		if (s == -1)
			return 0;

		ctx = io_getcookie(s);
		if (!ctx) {
			io_dontwantwrite(s);
			io_close(s);
			printf("Warning: cookie is null\n");
			continue;
		}
		context_flush(ctx);
	}
	return 1;
}

int main(int argc, char* argv[])
{
	(void)argc; (void)argv;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	int ret;

	signal(SIGPIPE, SIG_IGN);

	if (db_init("dietchan_db") < 0)
		return -1;

	mkdir(DOC_ROOT, 0755);
	mkdir(DOC_ROOT "/uploads", 0755);
	mkdir(DOC_ROOT "/captchas", 0755);

	generate_captchas();

	listener = socket_tcp4();

	if (listener == -1) {
		perror("socket_tcp4");
	}
	ret = socket_bind4_reuse(listener, ip, 4000);
	if (ret == -1) {
		perror("socket_bind4");
	}
	ret = socket_listen(listener, 10000);
	if (ret == -1) {
		perror("socket_listen");
	}

	io_nonblock(listener);
	ret = io_fd(listener);
	if (!ret) {
		printf("io_fd failed\n");
		return -1;
	}

	io_wantread(listener);

	while (1) {
		io_wait();

		int loop=1;
		while (loop) {
			loop = 0;

			// "speed hack"
			// See gatling source. tl;dr without this, kernel drops connections under heavy load
			accept_connections(1000);

			loop |= handle_read_events(10);
			loop |= handle_write_events(10);
		}
	}

	return 0;
}
