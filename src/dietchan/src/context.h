#ifndef CONTEXT_H
#define CONTEXT_H

#include <libowfat/iob.h>

#define AGAIN -1
#define ERROR -3

// It turns out that writev is very slow when using lots of small buffers.
// It is faster to copy the small buffers into a larger buffer first and then pass it to writev.
#define BUFFER_WRITES

typedef struct context {
	int  refcount;
	int  fd;

	#ifdef BUFFER_WRITES
	void *buf;
	size_t buf_offset;
	size_t buf_size;
	#endif

	io_batch *batch;
	int  error;
	int  eof;
	int  (*read)(struct context *ctx, char *buf, int length);
	void (*finalize)(struct context *ctx);
} context;

void context_init(context *ctx, int fd);
void context_addref(context *ctx);
void context_unref(context *ctx);
int  context_read(context *ctx, char *buf, int length);
void context_flush(context *ctx);
void context_eof(context *ctx);

#ifdef BUFFER_WRITES
void context_write_data(context *ctx, const void *buf, size_t length);
void context_write_file(context *ctx, int64 fd, uint64 offset, uint64 length);
#endif

#endif // CONTEXT_H
