#include "job.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libowfat/io.h>
#include <libowfat/byte.h>

static void    job_finalize(context *ctx);
static void    job_free(context *ctx);
static int     job_read(context *ctx, char *buf, int length);


job_context* job_new(const char *command)
{
	int64 pid;
	int64 p[2];

	job_context *job;
	context *ctx;

	if (!io_socketpair(&p[0]))
		return 0;

	switch (pid = fork()) {
		case -1: /* error */
			io_close(p[0]);
			io_close(p[1]);
			return 0;
		case 0: /* child */
			if (p[0] != 0) {
				dup2(p[0], 0);
				dup2(p[0], 1);
				close(p[1]);
			}
			execl("/bin/sh", "sh", "-c", command, 0);
			exit(127);
			/* not reached */
		default: /* parent */
			close(p[0]);

			job = malloc(sizeof(job_context));
			byte_zero(job, sizeof(job_context));
			ctx = (context*)job;

			context_init(ctx, p[1]);
			job->pid = pid;
			printf("Started job %d (%s)\n", (int)job->pid, command);
			ctx->read = job_read;
			ctx->finalize = job_finalize;
			ctx->free = job_free;

			io_nonblock(ctx->fd);
			io_fd(ctx->fd);
			io_wantread(ctx->fd);
			io_setcookie(ctx->fd, job);
			return job;
	}
}

static void job_finalize(context *ctx)
{
	job_context *job = (job_context*)ctx;
	printf("Finalized job %d\n", (int)job->pid);
	// waitpid?
}

void job_free(context *ctx)
{
	free(ctx);
}

static int job_read(context *ctx, char *buf, int length)
{
	job_context *job = (job_context*)ctx;
	if (length > 0) {
		return job->read(job, buf, length);
	} else {
		int status;
		// For some reason WNOHANG sometimes returns 0, even though length is 0 which
		// indicates that the process has terminated. Race condition in the kernel?
		int ret = waitpid(job->pid, &status, /*WNOHANG*/0);
		if (ret <= 0)
			return ERROR;

		printf("Exited job %d\n", (int)job->pid);

		if (WIFEXITED(status)) {
			job->finish(job, WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			// Dirty
			job->finish(job, -1);
		}
	}
	return 0;
}
