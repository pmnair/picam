
#include <stdio.h>
#include "common.h"

int
open_next_file(struct picam_ctx *ctx, char *last_fname, int len)
{
	char fname[1024];

	if (ctx->fp) {
		if (last_fname)
			snprintf(last_fname, len, "%s-%d.h264", ctx->fname, ctx->fname_idx);
		fclose(ctx->fp);
	}

	ctx->fname_idx++;
	snprintf(fname, sizeof(fname), "%s-%d.h264", ctx->fname, ctx->fname_idx);
	ctx->fp = fopen(fname, "w");

	return (ctx->fp == NULL);
}

void
h264_write_frames(struct picam_ctx *ctx)
{
	struct picam_frame *frame = ctx->frame_buffers.frames[ctx->frame_buffers.last];
	int ii = 0;
	int key_frame = -1;
	time_t last_frame_tm = frame->tm;

	/* find last key frame pre-cap seconds before current */
	LOG_DBG("lfi=%d, cfi=%d", ctx->frame_buffers.last, ctx->frame_buffers.curr);
	for (ii = ctx->frame_buffers.last;
		ii != ctx->frame_buffers.curr; ii--)
	{
		frame = ctx->frame_buffers.frames[ii];
		if (frame->keyframe) {
			LOG_DBG("ii=%d - kf=%d, ft=%lu delta=%lu", ii, frame->keyframe,
					frame->tm, last_frame_tm - ctx->nsec_pre_cap);
			if ((key_frame == -1) ||
				(frame->tm <= (last_frame_tm - ctx->nsec_pre_cap)))
			{
				LOG_DBG("lft=%lu, ft=%lu, kfi=%d", last_frame_tm,
						frame->tm, key_frame);
				key_frame = ii;
				break;
			}
		}
		if (ii == 0)
			ii = ctx->frame_buffers.nalloc-1;
	}
	/* write file header */
	fwrite(ctx->h264_hdr, 1, ctx->h264_hdr_pos, ctx->fp);

	LOG_DBG("Final lft=%lu, kfi=%d", last_frame_tm, key_frame);

	/* write all frames from the key frame to last frame */
	for (ii = key_frame; ii != ctx->frame_buffers.last; ii++) {
		if (ii == ctx->frame_buffers.nalloc)
			ii = 0;
		frame = ctx->frame_buffers.frames[ii];
		fwrite(frame->data, 1, frame->len, ctx->fp);
	}

	frame = ctx->frame_buffers.frames[ctx->frame_buffers.last];
	fwrite(frame->data, 1, frame->len, ctx->fp);
}

void
h264_write_frame(struct picam_ctx *ctx, int frame_idx)
{
	struct picam_frame *frame = ctx->frame_buffers.frames[frame_idx];
	fwrite(frame->data, 1, frame->len, ctx->fp);
}
