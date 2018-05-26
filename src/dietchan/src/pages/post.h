#ifndef POST_H
#define POST_H

#include <libowfat/uint64.h>
#include <libowfat/array.h>

#include "../config.h"

#include "../persistence.h"
#include "../http.h"
#include "../job.h"


struct post_page {
	int64  thread;
	int64  board;

	struct session *session;
	struct user *user;

	int64  sage;
	char  *subject;
	char  *username;
	char  *role;
	char  *text;
	char  *password;

	char  *captcha;
	uint64 captcha_id;
	uint64 captcha_token;

	struct ip ip;
	struct ip x_real_ip;
	array x_forwarded_for;
	char  *user_agent;
	int64  mod_pin;
	int64  mod_close;

	int64  is_bot;

	array  upload_jobs;
	struct upload_job *current_upload_job;
	int    pending;
	int    aborted;
	int    success;
};

void post_page_init(http_context *context);

#endif // POST_H
