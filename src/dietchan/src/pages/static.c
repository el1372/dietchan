#include "static.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>


#include <libowfat/str.h>
#include <libowfat/case.h>
#include <libowfat/scan.h>
#include <libowfat/fmt.h>
#include <libowfat/open.h>

#include "../page.h"

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
}

static int static_page_request (http_context *http, http_method method, char *path, char *query)
{
	struct static_page *page = (struct static_page*)http->info;

	if (method != HTTP_GET)
		HTTP_FAIL(METHOD_NOT_ALLOWED);

	if (!str_start(path, "/bbs/"))
		HTTP_FAIL(FORBIDDEN);

	char *rel_path = &path[strlen("/bbs/")];
	char *docroot = "/srv/http/bbs/";
	char *abs_path = alloca(strlen(docroot) + strlen(rel_path) + 1);
	strcpy(abs_path, docroot);
	strcat(abs_path, rel_path);

	// Resolve ./, ../, symlinks etc.

	//page->real_path = realpath(abs_path, NULL);
	// Urgh... non-cvs versions of dietlibc don't conform to POSIX.1-2008
	{
		uint64 path_max;
		#ifdef PATH_MAX
			path_max = PATH_MAX;
		#else
			path_max = pathconf(path, _PC_PATH_MAX);
			if (path_max <= 0)
				path_max = 4096;
		#endif
		page->real_path = malloc(path_max);
		if (!realpath(abs_path, page->real_path)) {
			free(page->real_path);
			page->real_path = NULL;
		}
	}

	if (!page->real_path) {
		// realpath fails if a file doesn't exist
		HTTP_FAIL(NOT_FOUND);
	}

	// Check that we are still in the docroot
	if (!str_start(page->real_path, docroot)) {
		HTTP_FAIL(NOT_FOUND);
	}

	// Hide any hidden files (staring with a .)
	if (strstr(page->real_path, "/.") != NULL) {
		HTTP_FAIL(NOT_FOUND);
	}
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
		HTTP_STATUS("304 Not changed");
		HTTP_WRITE("Cache-Control: private, max-age=31536000\r\n"); // 1 year
		HTTP_BODY();
		HTTP_EOF();
		return 0;
	}

	int fd = open(page->real_path, O_RDONLY | O_NOFOLLOW);
	io_closeonexec(fd);
	if (fd == -1)
		HTTP_FAIL(FORBIDDEN);

	HTTP_STATUS("200 OK");
	HTTP_WRITE("Last-Modified: ");
	HTTP_WRITE_HTTP_DATE(st.st_mtime);
	HTTP_WRITE("\r\n"
	           "Cache-Control: private, max-age=31536000\r\n" // 1 year
	           "Content-Length: ");
	HTTP_WRITE_ULONG(st.st_size);
	HTTP_WRITE("\r\n");
	// Todo: mime type
	HTTP_BODY();
	HTTP_WRITE_FILE(fd, 0, st.st_size);

	HTTP_EOF();
}

static void static_page_finalize (http_context *http)
{
	struct static_page *page = (struct static_page*)http->info;
	if (page->real_path) free(page->real_path);
}
