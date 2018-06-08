#include "captcha.h"

#include "config.h"
#include <assert.h>
#include <time.h>
#include <libowfat/str.h>
#include <libowfat/fmt.h>
#include <libowfat/byte.h>
#include "arc4random.h"
#include "util.h"
#include "job.h"

struct captcha *random_captcha()
{
	uint64 count = master_captcha_count(master);
	if (count <= 0)
		return 0;

	uint64 *captchas = master_captchas(master);
	uint64 idx = arc4random_uniform(count);
	uint64 id = captchas[idx];

	struct captcha *captcha = find_captcha_by_id(id);
	assert(captcha);
	return captcha;
}

void invalidate_captcha(struct captcha *captcha)
{
	uint64 token;
	arc4random_buf(&token, sizeof(uint64));
	captcha_set_token(captcha, token);
}

void replace_captcha(struct captcha *captcha)
{
	begin_transaction();

	// Answer is valid -> remove captcha and replace with new one
	db_hashmap_remove(&captcha_tbl, &captcha_id(captcha));

	uint64 idx = captcha_idx(captcha);
	uint64 count = master_captcha_count(master);
	uint64 *captchas = master_captchas(master);
	captchas[idx] = captchas[count-1];
	db_invalidate_region(db, &captchas[idx], sizeof(uint64));

	struct captcha *other = find_captcha_by_id(captchas[idx]);
	captcha_set_idx(other, idx);

	master_set_captcha_count(master, count - 1);

	captcha_free(captcha);

	commit();

	generate_captchas();
}

struct captcha_info {
	uint64 id;
	char solution[32];
};

static int in_flight = 0;

static void captcha_job_finish(job_context *job, int status);

static void captcha_job_start()
{
	struct captcha_info *info = malloc(sizeof(struct captcha_info));
	byte_zero(info, sizeof(struct captcha_info));
	arc4random_buf(&info->id, sizeof(uint64));

	static const char *allowed = "ABDEFGHMNQRTabdefghijmbqrt34678";
	uint64 length = 5 + arc4random_uniform(3);

	generate_random_string(&info->solution[0], length, allowed);

	int seed = 0;
	arc4random_buf(&seed, sizeof(int));

	char command[4096];
	byte_zero(command, sizeof(command));
	strcpy(command, "./captcha -t ");
	strcat(command, info->solution);
	strcat(command, " -r ");
	fmt_int(&command[strlen(command)], seed);
	strcat(command, " -d 140x50 -s 4 -q");
	strcat(command, " | convert tga:- -resize 25% png8:" DOC_ROOT "/captchas/");
	fmt_xint64(&command[strlen(command)], info->id);
	strcat(command, ".png");

	job_context *job = job_new(command);
	job->info = info;
	job->finish = captcha_job_finish;

	++in_flight;
}

static void captcha_job_finish(job_context *job, int status)
{
	struct captcha_info *info = (struct captcha_info*)job->info;
	if (status == 0) {
		--in_flight;
		begin_transaction();

		struct captcha *captcha = captcha_new();
		captcha_set_id(captcha, info->id);

		uint64 count = master_captcha_count(master)+1;
		master_set_captcha_count(master, count);

		uint64 *captchas = master_captchas(master);
		captchas = db_realloc(db, captchas, sizeof(uint64)*count);
		master_set_captchas(master, captchas);

		uint64 idx = count-1;
		captchas[idx] = info->id;
		db_invalidate_region(db, &captchas[idx], sizeof(uint64));
		captcha_set_idx(captcha, idx);

		captcha_set_solution(captcha, info->solution);

		uint64 now = time(0);
		captcha_set_timestamp(captcha, now);

		db_hashmap_insert(&captcha_tbl, &captcha_id(captcha), captcha);

		invalidate_captcha(captcha);

		commit();
	}
	free(info);

	// Keep generating captchas
	if (master_captcha_count(master) + in_flight < 1000)
		captcha_job_start();
}

void generate_captchas()
{
	// Run up to 10 jobs in parallel
	for (int i=0; i<10; ++i) {
		// Generate up to 1000 captchas
		if (master_captcha_count(master) + in_flight >= 1000)
			return;
		captcha_job_start();

	}
}
