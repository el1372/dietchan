#ifndef CONTEXT_H
#define CONTEXT_H

#include <libowfat/iob.h>

#define AGAIN -1
#define ERROR -3

typedef struct context {
	int  refcount;
	int  fd;

	struct chunk *chunk;
	size_t buf_offset;
	size_t buf_size;

	io_batch *batch;
	int  error;
	int  eof;
	int  (*read)(struct context *ctx, char *buf, int length);
	void (*finalize)(struct context *ctx);
	void (*free)(struct context *ctx);
} context;

void context_init(context *ctx, int fd);
void context_addref(context *ctx);
void context_unref(context *ctx);
int  context_read(context *ctx, char *buf, int length);
void context_flush(context *ctx);
void context_eof(context *ctx);

size_t context_get_buffer(context *ctx, void **buf);
void context_consume_buffer(context *ctx, size_t bytes_written);
void context_write_data(context *ctx, const void *buf, size_t length);
void context_write_file(context *ctx, int64 fd, uint64 offset, uint64 length);

#endif // CONTEXT_H
