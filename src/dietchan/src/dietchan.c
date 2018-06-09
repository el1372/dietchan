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
#include "tpl.h"

#include "export.h"
#include "import.h"

#include "pages/static.h"
#include "pages/post.h"
#include "pages/thread.h"
#include "pages/board.h"
#include "pages/mod.h"
#include "pages/login.h"
#include "pages/dashboard.h"
#include "pages/edit_user.h"
#include "pages/edit_board.h"
#include "pages/banned.h"

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

	if (str_equal(path, PREFIX "/banned")) {
		banned_page_init(http);
		goto found;
	}

	if (str_start(path, PREFIX "/") && strchr(path+strlen(PREFIX "/"), '/') && path[strlen(path)-1] != '/') {
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
	PRINT(S("HTTP/1.1 "),I64(http->error_status),S(" "),S(http->error_message),S("\r\n"
	        "Connection: close\r\n"
	        "Content-Type: text/html; charset=utf-8\r\n"
	        "\r\n"
	        "<h1>"), S(http->error_message), S("</h1>"));
	PRINT_EOF();
}

struct listener {
	int64 socket;
	struct ip ip;
};
// 64 listeners ought to be enough for anyone
struct listener listeners[64];
size_t listener_count;

static char buf[8192];

void accept_connections(int64 s, struct listener *listener, int limit)
{
	for (int i=0; i<limit; ++i) {
		char ip[16] = {0};
		uint16 port;
		int64 a;
		uint32 scope;
		switch(listener->ip.version) {
			case IP_V4: a = socket_accept4(s, ip, &port); break;
			case IP_V6: a = socket_accept6(s, ip, &port, &scope); break;
		}
		if (a==-1)
			io_eagain_read(s);
		if (a<0) return;

		io_nonblock(a);

		http_context *http = http_new(a);

		http->ip.version = listener->ip.version;
		byte_copy(&http->ip.bytes, sizeof(ip), ip);
		http->port = port;

		http->request = request;
		http->error   = error;
	}
}

void read_data(int64 s, context *ctx, int64 bytes_limit)
{
	int64 bytes_read = 0;

	// I'm not sure if we can actually read only part of the available bytes or if this may cause us
	// to miss events. Because of this, the bytes_limit parameter is currently ignored.

	while (/*bytes_read < bytes_limit*/1) {
		int64 ret=io_tryread(s, buf, sizeof(buf));
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
			io_dontwantread(s);

			context_unref(ctx);
			return;
		}
	}
}

int is_listener(void *cookie)
{
	return ((char*)cookie >= (char*)listeners &&
	        (char*)cookie < (char*)(listeners + sizeof(listeners)));
}

int handle_read_events(int limit)
{
	for (int i=0; i<limit; ++i) {
		int64 s=io_canread();
		if (s == -1)
			return 0;
		void *cookie = io_getcookie(s);
		if (is_listener(cookie)) {
			accept_connections(s, cookie, 1000);
		} else {
			read_data(s, cookie, 1);
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

		context *ctx = io_getcookie(s);
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

void add_listener(struct ip ip, uint16 port)
{
	char buf[256];
	buf[fmt_ip(buf,&ip)] = '\0';
	fprintf(stderr, "Creating listener on port %d for address %s\n", (int)port, buf);

	struct listener *listener = listeners + listener_count;
	listener->ip = ip;

	int64 s=-1;
	int ret=0;
	switch (ip.version) {
	case IP_V4:
		s = socket_tcp4();
		if (s == -1)
			perror("socket_tcp4");
		ret = socket_bind4_reuse(s, &ip.bytes[0], port);
		if (ret == -1)
			perror("socket_bind4");
		break;
	case IP_V6:
		s = socket_tcp6();
		if (s == -1)
			perror("socket_tcp6");
		ret = socket_bind6_reuse(s, &ip.bytes[0], port, 0);
		if (ret == -1)
			perror("socket_bind6");
		break;
	}

	ret = socket_listen(s, 10000);
	if (ret == -1)
		perror("socket_listen");

	io_nonblock(s);
	io_fd(s);
	io_wantread(s);
	io_setcookie(s, listener);

	listener->socket = s;

	++listener_count;
}

const char *usage =
	"Usage:\n"
	"  dietchan [options]\n"
	"\n"
	"Options:\n"
	"  -l ip,port Listen on the specified ip address and port.\n"
    "\n"
    "Examples:\n"
    "  dietchan -l 127.0.0.1,4000 -l ::1,4001\n"
    "  Accept IPv4 requests from 127.0.0.1 on port 4000 and accept IPv6 requests from ::1 on port 4001.\n"
    "\n"
    "  dietchan -l 0.0.0.0,4000\n"
    "  Accept IPv4 requests from any address on port 4000.\n"
    "\n"
    "  dietchan -l ::0,4000\n"
    "  Accept IPv6 and IPv4 requests from any address on port 4000.\n";

int main(int argc, char* argv[])
{
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	signal(SIGPIPE, SIG_IGN);

	// Parse options
	int c;
	struct ip ip;
	uint16 port;
	while ((c = getopt(argc, argv, "l:")) != -1) {
		switch(c) {
		case 'l':
			if (!scan_ip(optarg, &ip))
				goto print_usage;

			char *comma = strchr(optarg, ',');
			if (!comma)
				goto print_usage;

			if (!scan_short(comma+1, &port))
				goto print_usage;

			add_listener(ip, port);
			break;

		case '?':
		print_usage:
			write(2, usage, strlen(usage));
			return -1;
		}
	}

	if (optind < argc) {
		if (case_equals(argv[optind], "import")) {
			if (db_init("imported_db", 0) < 0)
				return -1;

			return import();
		} else if (case_equals(argv[optind], "export")) {
			if (db_init("dietchan_db", 1) < 0)
				return -1;
			export();
			return 0;
		}
	}

	// Add default listener if no listener specified
	if (!listener_count)
		add_listener((struct ip){IP_V4, {127,0,0,1}}, 4000);


	// Open database
	if (db_init("dietchan_db", 1) < 0)
		return -1;

	// Create some required directories if they don't exist
	mkdir(DOC_ROOT, 0755);
	mkdir(DOC_ROOT "/uploads", 0755);
	mkdir(DOC_ROOT "/captchas", 0755);
	// Start generating captchas
	generate_captchas();

	// Main loop
	while (1) {
		io_wait();

		int loop=1;
		while (loop) {
			loop = 0;

			// "speed hack"
			// See gatling source. tl;dr without this, kernel drops connections under heavy load
			for (size_t i=0; i<listener_count; ++i)
				accept_connections(listeners[i].socket, &listeners[i], 10000);

			loop |= handle_read_events(100);
			loop |= handle_write_events(10);
		}
	}

	return 0;
}
