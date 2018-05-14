#include "context.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <libowfat/byte.h>
#include <libowfat/io.h>

void context_init(context *ctx, int fd)
{
	byte_zero(ctx, sizeof(context));
	ctx->refcount = 1;
	ctx->fd = fd;
	#ifdef BUFFER_WRITES
	ctx->batch = iob_new(10);
	#else
	ctx->batch = iob_new(1000);
	#endif
}

void context_addref(context *ctx)
{
	++(ctx->refcount);
}

void context_unref(context *ctx)
{
	--(ctx->refcount);
	if (ctx->refcount == 0) {
		if (ctx->finalize)
			ctx->finalize(ctx);

		#ifdef BUFFER_WRITES
		if (ctx->buf)
			free(ctx->buf);
		#endif

		iob_free(ctx->batch);
		io_close(ctx->fd);
		free(ctx);
	}
}

int  context_read(context *ctx, char *buf, int length)
{
	int ret = 0;
	if (ctx->read && !ctx->error)
		ret = ctx->read(ctx, buf, length);
	else
		ret = 0;

	if (ret == ERROR)
		ctx->error = 1;

	return ret;
}

void context_flush(context *ctx)
{
	#ifdef BUFFER_WRITES
	if (ctx->buf) {
		iob_addbuf_free(ctx->batch, ctx->buf, ctx->buf_offset);
		ctx->buf = 0;
		ctx->buf_size = 0;
		ctx->buf_offset = 0;
	}
	#endif


	int64 ret;

	do {
		ret = iob_send(ctx->fd, ctx->batch);
	} while (ret > 0);

	if (ret == -3) {
		//perror("iob_send");
		ctx->error = 1;
		io_dontwantwrite(ctx->fd);
		shutdown(ctx->fd, SHUT_RDWR);
		return;
	}

	io_wantwrite(ctx->fd);

	if (ret == 0 && ctx->eof) {
		// HTTP:
		//
		// Theoretically, it would be sufficient (and more 'gentle') to close just the write end and
		// wait for the other side to be closed by the client, but browsers are dumb and keep sending
		// the whole request body even though we sent a 413 (details below).
		//
		// This violates the standard, but browsers vendors don't care. Allegedly the lack of interest is
		// because HTTP/1.1 is now considered legacy and the issue is supposedly fixed in HTTP/2.0,
		// but at least in the case of Chrome this is a lie.
		//
		// What should happen (according to the HTTP/1.1 standard):
		// - We send the error response and close the write end. The client detects this and closes its end.
		//   Everybody is happy.
		// - Alternatively, we are allowed to close both ends directly, which should have more or less
		//   the same result.
		//
		// What really happens:
		//
		// (a) When we close only the write direction:
		// - Firefox keeps sending the whole request body until it finally shows our error message.
		// - Chrome keeps sending the whole request body until it finally shows our error message.
		//
		// (b) When we close both directions:
		// - Firefox, in HTTP/1.1 mode, shows a "Connection reset by peer" message instead of our response.
		// - Firefox, in HTTP/2.0 mode, shows our response immediately.
		// - Chrome, in HTTP/1.1 mode, shows our response immediately (but, according to reports on the internet,
		//   only on Linux, while Windows users will get "Connection reset by peer")
		// - Chrome, in HTTP/2.0 mode, keeps sending the whole body, which is a step backwards even from the
		//   already broken HTTP/1.1 behavior.
		// - Additionaly, in HTTP/2.0 mode, Chrome seems to get stuck when uploading very large files (2GB+).
		//   I'm not sure if this is a bug in Chrome or just nginx giving Chrome the middle finger
		//   and cutting off the connection due to some timeout.
		//
		// Aaaaaand the award for the most broken behavior goes to Chrome. But Firefox is a close second contender.
		// (I don't even want to know about IE.)
		//
		// All in all, this seems to be the best compromise right now.
		//
		// Further reading:
		// [0] https://stackoverflow.com/questions/18367824/how-to-cancel-http-upload-from-data-events
		// [1] https://code.google.com/p/chromium/issues/detail?id=174906
		// [2] https://bugzilla.mozilla.org/show_bug.cgi?id=839078
		// [3] http://www.w3.org/Protocols/rfc2616/rfc2616-sec8.html#sec8.2.2
		//
		#if 0
			shutdown(ctx->fd, SHUT_WR);
			io_dontwantwrite(ctx->fd);
		#else
			shutdown(ctx->fd, SHUT_RDWR);
			io_dontwantwrite(ctx->fd);
		#endif
	}
}

void context_eof(context *ctx)
{
	assert(!ctx->eof);
	ctx->eof = 1;
	context_flush(ctx);
}

#ifdef BUFFER_WRITES
void context_write_data(context *ctx, const void *buf, size_t length)
{
	static const size_t chunk_size = 10*4096; // 10 pages
	for (;;) {
		if (!ctx->buf) {
			if (length > chunk_size) {
				void *tmp = malloc(length);
				iob_addbuf_free(ctx->batch, tmp, length);
				return;
			} else {
				ctx->buf_size = chunk_size;
				ctx->buf = malloc(ctx->buf_size);
				memcpy(ctx->buf, buf, length);
				ctx->buf_offset = length;
				return;
			}
		} else {
			size_t l = length;
			if (ctx->buf_offset + l > ctx->buf_size)
				l = ctx->buf_size - ctx->buf_offset;
			memcpy(ctx->buf + ctx->buf_offset, buf, l);
			ctx->buf_offset += l;
			assert (ctx->buf_offset <= ctx->buf_size);
			if (ctx->buf_offset == ctx->buf_size) {
				iob_addbuf_free(ctx->batch, ctx->buf, ctx->buf_offset);
				ctx->buf = 0;
				ctx->buf_size = 0;
				ctx->buf_offset = 0;
			}
			if (l < length) {
				buf += l;
				length -= l;
			} else {
				return;
			}
		}
	}
}

void context_write_file(context *ctx, int64 fd, uint64 offset, uint64 length)
{
	if (ctx->buf) {
		iob_addbuf_free(ctx->batch, ctx->buf, ctx->buf_offset);
		ctx->buf = 0;
		ctx->buf_size = 0;
		ctx->buf_offset = 0;
	}

	iob_addfile_close(ctx->batch, fd, offset, length);
}
#endif
