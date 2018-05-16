#include "upload_job.h"

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <libowfat/byte.h>
#include <libowfat/buffer.h>
#include <libowfat/case.h>
#include <libowfat/open.h>
#include <libowfat/scan.h>

#include "util.h"
#include "mime_types.h"

static void start_mime_check_job(struct upload_job *upload_job);
static void finished_mime_check_job(struct upload_job *upload_job);
static void start_extract_meta_job(struct upload_job *upload_job);
static void finished_extract_meta_job(struct upload_job *upload_job);
static void start_thumbnail_job(struct upload_job *upload_job);
static void finished_thumbnail_job(struct upload_job *upload_job);

static int  upload_job_job_read(job_context *job, char *buf, size_t length);
static void upload_job_job_finish(job_context *job, int status);
static void upload_job_error(struct upload_job *upload_job, int status, char *message);

static void extract_meta_command(const char *file, const char *mime_type, char *command);
static void thumbnail_command(const char *file, const char *mime_type, const char *thumbnail_base,
                              const char **ext, char *command);

void upload_job_init(struct upload_job *upload_job, char *upload_dir)
{
	byte_zero(upload_job, sizeof(struct upload_job));

	upload_job->ok = 1;

	upload_job->width = -1;
	upload_job->height = -1;
	upload_job->duration = -1;
	upload_job->upload_dir = strdup(upload_dir);
	upload_job->file_ext = "";
	upload_job->thumb_ext = "";

	upload_job->file_path = malloc(strlen(upload_dir) + 15);
	byte_zero(upload_job->file_path, strlen(upload_dir) + 15);
	strcpy(upload_job->file_path, upload_dir);
	strcat(upload_job->file_path, ".tmp");
	generate_random_string(&upload_job->file_path[strlen(upload_job->file_path)], 10, "0123456789");

	// For performance reasons, we only create the fd when we actually write something.
	upload_job->fd = -1;
}

void upload_job_finalize(struct upload_job *upload_job)
{
	if (!upload_job->ok) {
		// Aborted or error occured, clean up
		if (upload_job->file_path)
			unlink(upload_job->file_path);
		if (upload_job->thumb_path)
			unlink(upload_job->thumb_path);
	}
	if (upload_job->upload_dir) free(upload_job->upload_dir);
	if (upload_job->file_path)  free(upload_job->file_path);
	if (upload_job->thumb_path) free(upload_job->thumb_path);
	if (upload_job->mime_type)  free(upload_job->mime_type);
	if (upload_job->fd >= 0)    close(upload_job->fd);
	array_reset(&upload_job->job_output);
}

void upload_job_write_content(struct upload_job *upload_job, char *buf, size_t length)
{
	if (upload_job->fd < 0) {
		upload_job->fd = open_trunc(upload_job->file_path);
		io_closeonexec(upload_job->fd);
	}
	write(upload_job->fd, buf, length);
	upload_job->size += length;
}

void upload_job_write_eof(struct upload_job *upload_job)
{
	assert(upload_job->state == UPLOAD_JOB_UPLOADING);
	upload_job->state = UPLOAD_JOB_UPLOADED;

	if (upload_job->fd < 0) {
		upload_job->fd = open_trunc(upload_job->file_path);
		io_closeonexec(upload_job->fd);
	}

	close(upload_job->fd);
	start_mime_check_job(upload_job);
}

void upload_job_abort(struct upload_job *upload_job)
{
	upload_job->ok = 0;
	// Kill current job?
}

void upload_job_check_mime(struct upload_job *upload_job)
{
	start_mime_check_job(upload_job);
}

// --- Internal ---

static int upload_job_job_read(job_context *job, char *buf, size_t length)
{
	struct upload_job *upload_job = (struct upload_job*)job->info;
	array_catb(&upload_job->job_output, buf, length);
	return 0;
}

static void upload_job_job_finish(job_context *job, int status)
{
	struct upload_job *upload_job = (struct upload_job*)job->info;

	if (status != 0) {
		upload_job->ok = 0;
		upload_job_error(upload_job, 500, "Internal Server Error");
		return;
	}

	array_cat0(&upload_job->job_output);

	switch (upload_job->state) {
	case UPLOAD_JOB_MIMECHECKING:
		finished_mime_check_job(upload_job);
		break;
	case UPLOAD_JOB_EXTRACTING_META:
		finished_extract_meta_job(upload_job);
		break;
	case UPLOAD_JOB_THUMBNAILING:
		finished_thumbnail_job(upload_job);
		break;
	}
}

static void upload_job_error(struct upload_job *upload_job, int status, char *message)
{
	if (upload_job->error)
		upload_job->error(upload_job, status, message);
}

// --- MIME Checking ---

static void start_mime_check_job(struct upload_job *upload_job)
{
	assert(upload_job->state == UPLOAD_JOB_UPLOADED);

	char command[512];
	strcpy(command, "/bin/file --mime-type --brief ");
	strcat(command, upload_job->file_path);

	job_context *job = job_new(command);
	job->info = upload_job;
	job->read = upload_job_job_read;
	job->finish = upload_job_job_finish;

	upload_job->current_job = job;
	array_trunc(&upload_job->job_output);
	upload_job->state = UPLOAD_JOB_MIMECHECKING;
}

static void remove_trailing_space(char *s)
{
	ssize_t length = strlen(s);
	for (ssize_t i = length-1; isspace(s[i]) && i>=0; --i)
		s[i] = '\0';
}

static void finished_mime_check_job(struct upload_job *upload_job)
{
	assert(upload_job->state == UPLOAD_JOB_MIMECHECKING);

	upload_job->mime_type = strdup(array_start(&upload_job->job_output));
	remove_trailing_space(upload_job->mime_type);

	upload_job->file_ext = get_extension_for_mime_type(upload_job->mime_type);

	if (upload_job->mime)
		upload_job->mime(upload_job, upload_job->mime_type);

	upload_job->state = UPLOAD_JOB_MIMECHECKED;

	if (upload_job->ok)
		start_extract_meta_job(upload_job);
}

// --- Meta extraction ---

static void start_extract_meta_job(struct upload_job *upload_job)
{
	assert(upload_job->state == UPLOAD_JOB_MIMECHECKED);

	char buf[4096];
	extract_meta_command(upload_job->file_path, upload_job->mime_type, buf);

	job_context *job = job_new(buf);
	job->info = upload_job;
	job->read = upload_job_job_read;
	job->finish = upload_job_job_finish;

	upload_job->current_job = job;
	array_trunc(&upload_job->job_output);

	upload_job->state = UPLOAD_JOB_EXTRACTING_META;
}

static void finished_extract_meta_job(struct upload_job *upload_job)
{
	assert(upload_job->state == UPLOAD_JOB_EXTRACTING_META);

	buffer buf;
	buffer_fromarray(&buf, &upload_job->job_output);

	char   line[512];
	ssize_t line_length;
	int    int_val;
	double dbl_val;

	while ((line_length = buffer_getline(&buf, line, sizeof(line)-1)) > 0) {
		line[line_length] = '\0';
		if (case_starts(line, "width=")) {
			if (scan_int(&line[strlen("width=")], &int_val) > 0)
				upload_job->width = int_val;
		} else if (case_starts(line, "height=")) {
			if (scan_int(&line[strlen("height=")], &int_val) > 0)
				upload_job->height = int_val;
		} else if (case_starts(line, "duration=")) {
			if (scan_double(&line[strlen("duration=")], &dbl_val) > 0)
				upload_job->duration = dbl_val*1000L;
		}
	}

	upload_job->state = UPLOAD_JOB_EXTRACTED_META;

	if (upload_job->meta)
		upload_job->meta(upload_job, upload_job->width, upload_job->height, upload_job->duration);

	if (upload_job->ok)
		start_thumbnail_job(upload_job);
}

// --- Thumbnailing ---

static void start_thumbnail_job(struct upload_job *upload_job)
{
	assert(upload_job->state == UPLOAD_JOB_EXTRACTED_META);

	char *thumbnail_base = alloca(strlen(upload_job->file_path) + 2);
	strcpy(thumbnail_base, upload_job->file_path);
	strcat(thumbnail_base, "s");

	char buf[4096];
	const char *ext;

	thumbnail_command(upload_job->file_path, upload_job->mime_type, thumbnail_base, &ext, buf);

	upload_job->thumb_path = malloc(strlen(thumbnail_base) + strlen(ext));
	strcat(upload_job->thumb_path, thumbnail_base);
	strcat(upload_job->thumb_path, ext);

	upload_job->thumb_ext = ext;

	job_context *job = job_new(buf);
	job->info = upload_job;
	job->read = upload_job_job_read;
	job->finish = upload_job_job_finish;

	upload_job->current_job = job;
	array_trunc(&upload_job->job_output);

	upload_job->state = UPLOAD_JOB_THUMBNAILING;
}

static void finished_thumbnail_job(struct upload_job *upload_job)
{
	assert(upload_job->state == UPLOAD_JOB_THUMBNAILING);

	upload_job->state = UPLOAD_JOB_THUMBNAILED;
	if (upload_job->finished)
		upload_job->finished(upload_job);
}

// --- Commands ---

static void extract_meta_command(const char *file, const char *mime_type, char *command)
{
	if (case_starts(mime_type, "video/")) {
		strcpy(command, "/bin/ffprobe -v error -show_entries format=duration:stream=index,codec_types,width,height -of default=noprint_wrappers=1 ");
		strcat(command, file);
	} else if (case_starts(mime_type, "image/")) {
		strcpy(command, "/bin/magick identify -format 'width=%[fx:w]\\nheight=%[fx:h]\\n' ");
		strcat(command, file);
	}
}

static void thumbnail_command(const char *file, const char *mime_type, const char *thumbnail_base,
                              const char **ext, char *command)
{
	*ext = "";

	char thumb_file[512];
	strcpy(thumb_file, thumbnail_base);

	int multipage=0;

	if (case_starts(mime_type, "video/")) {
		// Hardcoded at 1 sec right now
		strcpy(command, "/bin/ffmpeg -ss 00:00:01.800 -i ");
		strcat(command, file);
		strcat(command, " -vframes 1 -map 0:v -vf 'thumbnail=5,scale=iw*sar:ih' -f image2pipe -vcodec bmp - ");

		strcat(command, " | ");

		// Call recursively to generate jpg thumbnail from bmp
		thumbnail_command("-", "image/jpeg", thumbnail_base, ext, &command[strlen(command)]);
	} else if (case_equals(mime_type, "image/png") ||
	    case_equals(mime_type, "image/gif") ||
	    case_equals(mime_type, "application/pdf")) {
	    *ext = ".png";
		strcat(thumb_file, *ext);

	    if (case_equals(mime_type, "image/gif") ||
	        case_equals(mime_type, "application/pdf"))
	    	multipage=1;

		strcpy(command, "/bin/magick convert ");
		strcat(command, file);
		if (multipage)
			strcat(command, "[0]");

		if (case_equals(mime_type, "application/pdf"))
			strcat(command, " -flatten");

		strcat(command, " -resize 400x400 -quality 0 -profile /usr/share/color/icc/colord/sRGB.icc -strip ");
		strcat(command, thumb_file);

		strcat(command, " && (");

		// Imagemagick's PNG8 capabilities suck, so use pngquant to further optimize the size (optional)
		strcat(command, "/bin/pngquant -f 32 ");
		strcat(command, thumb_file);
		strcat(command, " -o ");
		strcat(command, thumb_file);

		strcat(command, " || true)");
	} else {
	    *ext = ".jpg";
		strcat(thumb_file, *ext);

		strcpy(command, "/bin/magick convert ");
		strcat(command, " -define jpeg:size=800x800 -define jpeg:extent=20kb ");
		strcat(command, file);
		strcat(command, "'[400x400]' -auto-orient -sharpen 0.1 -quality 50 -sampling-factor 2x2,1x1,1x1 "
		                "-profile /usr/share/color/icc/colord/sRGB.icc -strip ");
		strcat(command, thumb_file);
	}
}
