#ifndef JOB_H
#define JOB_H

#include "context.h"

typedef struct job_context {
	context parent_instance;
	int64 pid;

	// Data for callbacks
	void  *info;
	int  (*read)(struct job_context *job, char *buf, size_t length);
	void (*finish)(struct job_context *job, int status);
} job_context;

job_context* job_new(const char *command);

#endif // JOB_H
