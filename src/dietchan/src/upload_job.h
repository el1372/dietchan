#ifndef UPLOAD_JOB_H
#define UPLOAD_JOB_H

#include <libowfat/uint64.h>
#include <libowfat/array.h>
#include "job.h"

typedef enum upload_job_state {
	UPLOAD_JOB_UPLOADING,
	UPLOAD_JOB_UPLOADED,
	UPLOAD_JOB_MIMECHECKING,
	UPLOAD_JOB_MIMECHECKED,
	UPLOAD_JOB_EXTRACTING_META,
	UPLOAD_JOB_EXTRACTED_META,
	UPLOAD_JOB_THUMBNAILING,
	UPLOAD_JOB_THUMBNAILED
} upload_job_state;

struct upload_job {
	upload_job_state state;
	job_context *current_job;
	array job_output;
	int ok;

	char *upload_dir;
	int fd;
	char *original_name;
	char *file_path;
	const char *file_ext;
	char *thumb_path;
	const char *thumb_ext;
	char *mime_type;
	uint64 size;
	int64 width;
	int64 height;
	double duration;

	// Info for callbacks
	void *info;
	// Called when mime type is known. Return 0 if accepted, ERROR if not accepted.
	void (*mime)(struct upload_job *upload_job, char *mime_type);
	// Called when meta information is known.
	void (*meta)(struct upload_job *upload_job, int64 width, int64 height, double duration);
	// Called when everything is done, i.e. mime type checked and thumbnail generated.
	void (*finished)(struct upload_job *upload_job);
	// Called when an internal error occurs.
	void (*error)(struct upload_job *upload_job, int status, char *message);
};

void upload_job_init(struct upload_job *upload_job, char *upload_dir);
void upload_job_finalize(struct upload_job *upload_job);
// Write a chunk of data.
void upload_job_write_content(struct upload_job *upload_job, char *buf, size_t length);
// Signal end of data stream. Starts further processing of uploaded file.
void upload_job_write_eof(struct upload_job *upload_job);
// Abort upload, pretend it never happened (delete temp files etc.)
void upload_job_abort(struct upload_job *upload_job);

#endif // UPLOAD_JOB_H
