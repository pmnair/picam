
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "common.h"

int
open_next_file(struct picam_ctx *ctx, char *last_fname, int len)
{
	char dir[1024];
	struct tm now, *pnow;
	time_t tm_now;

	/* create directory based on todays date */
	tm_now = time(NULL);
	pnow = localtime_r(&tm_now, &now);
	snprintf(dir, sizeof(dir), "%s/%02d-%02d-%04d",
			ctx->path, pnow->tm_mon+1, pnow->tm_mday, pnow->tm_year+1900);
	if (access(dir, 0) != 0) {
		LOG_INF("creating dir %s", dir);
		mkdir(dir, 0777);
	}

	/* save the current path if its new or different */
	if (!ctx->curr_dir || (0 != strncmp(dir, ctx->curr_dir, strlen(dir)))) {
		if (ctx->curr_dir)
			free(ctx->curr_dir);
		ctx->curr_dir = strdup(dir);
	}

	/* if we have a file open; close it */
	if (ctx->video_fp) {
		if (last_fname)
			snprintf(last_fname, len, "%s", ctx->fname);
		fclose(ctx->video_fp);
	}

	/* open the file with next index */
	snprintf(ctx->fname, PATH_MAX, "%s/mov-%02d%02d%02d.h264",
				ctx->curr_dir, pnow->tm_hour,
				pnow->tm_min, pnow->tm_sec);
	LOG_INF("Opening file: %s", ctx->fname);
	ctx->video_fp = fopen(ctx->fname, "w");

	return (ctx->video_fp == NULL);
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
	fwrite(ctx->h264_hdr, 1, ctx->h264_hdr_pos, ctx->video_fp);

	LOG_DBG("Final lft=%lu, kfi=%d", last_frame_tm, key_frame);

	/* write all frames from the key frame to last frame */
	for (ii = key_frame; ii != ctx->frame_buffers.last; ii++) {
		if (ii == ctx->frame_buffers.nalloc)
			ii = 0;
		frame = ctx->frame_buffers.frames[ii];
		fwrite(frame->data, 1, frame->len, ctx->video_fp);
	}

	frame = ctx->frame_buffers.frames[ctx->frame_buffers.last];
	fwrite(frame->data, 1, frame->len, ctx->video_fp);
}

void
h264_write_frame(struct picam_ctx *ctx, int frame_idx)
{
	struct picam_frame *frame = ctx->frame_buffers.frames[frame_idx];
	fwrite(frame->data, 1, frame->len, ctx->video_fp);
}
