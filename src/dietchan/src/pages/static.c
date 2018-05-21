#include "static.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#include <libowfat/str.h>
#include <libowfat/case.h>
#include <libowfat/scan.h>
#include <libowfat/fmt.h>
#include <libowfat/open.h>

#include "../tpl.h"
#include "../mime_types.h"

static int static_page_request (http_context *http, http_method method, char *path, char *query);
static int static_page_header (http_context *http, char *key, char *val);
static int static_page_finish (http_context *http);
static void static_page_finalize (http_context *http);

void static_page_init(http_context *http)
{
	struct static_page *page = malloc(sizeof(struct static_page));
	byte_zero(page, sizeof(struct static_page));

	http->info = page;

	http->request      = static_page_request;
	http->header       = static_page_header;
	http->finish       = static_page_finish;
	http->finalize     = static_page_finalize;

	page->doc_root = realpath(DOC_ROOT "/", 0);
}

static int static_page_request (http_context *http, http_method method, char *path, char *query)
{
	struct static_page *page = (struct static_page*)http->info;

	if (method != HTTP_GET)
		HTTP_FAIL(METHOD_NOT_ALLOWED);

	if (!str_start(path, PREFIX "/"))
		HTTP_FAIL(FORBIDDEN);

	char *rel_path = &path[strlen(PREFIX)];
	char *abs_path = alloca(strlen(page->doc_root) + strlen(rel_path) + 1);
	strcpy(abs_path, page->doc_root);
	strcat(abs_path, rel_path);

	// Resolve ./, ../, symlinks etc.
	page->real_path = realpath(abs_path, 0);

	// realpath fails if a file doesn't exist
	if (!page->real_path)
		HTTP_FAIL(NOT_FOUND);

	// Check that we are still in the docroot
	if (!str_start(page->real_path, page->doc_root))
		HTTP_FAIL(NOT_FOUND);

	// Hide any hidden files (staring with a .)
	if (strstr(page->real_path, "/.") != NULL)
		HTTP_FAIL(NOT_FOUND);
}

static int static_page_header (http_context *http, char *key, char *val)
{
	struct static_page *page = (struct static_page*)http->info;

	if (case_equals(key, "If-Modified-Since")) {
		if (scan_httpdate(val, &page->if_modified_since) != strlen(val))
			HTTP_FAIL(BAD_REQUEST);
	}

	return 0;
}

static int static_page_finish (http_context *http)
{
	struct static_page *page = (struct static_page*)http->info;

	struct stat st;
	if (lstat(page->real_path, &st) == -1)
		HTTP_FAIL(NOT_FOUND); // We could be more specific here, but for now let's just pretend it does not exist.

	if (st.st_mtime <= page->if_modified_since) {
		PRINT_STATUS("304 Not changed");
		PRINT(S("Cache-Control: private, max-age=31536000\r\n")); // 1 year
		PRINT_BODY();
		PRINT_EOF();
		return 0;
	}

	int fd = open(page->real_path, O_RDONLY | O_NOFOLLOW);
	io_closeonexec(fd);
	if (fd == -1)
		HTTP_FAIL(FORBIDDEN);

	const char *ext = strrchr(page->real_path, '.');
	const char *mime = get_mime_type_for_extension(ext);

	PRINT_STATUS("200 OK");
	PRINT(S("Last-Modified: "),HTTP_DATE(st.st_mtime), S("\r\n"
	        "Cache-Control: private, max-age=31536000\r\n" // 1 year
	        "Content-Type: "), S(mime), S("\r\n"
	        "Content-Length: "), UL(st.st_size), S("\r\n"));
	PRINT_BODY();
	context_write_file((context*)http, fd, 0, st.st_size);

	PRINT_EOF();
}

static void static_page_finalize (http_context *http)
{
	struct static_page *page = (struct static_page*)http->info;
	if (page->real_path) free(page->real_path);
	if (page->doc_root)  free(page->doc_root);
	free(page);
}
