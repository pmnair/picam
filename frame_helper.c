//
//  motion.c
//  pimotioncam
//
//  Created by Praveen Nair on 3/12/16.
//  Copyright © 2016 Praveen Nair. All rights reserved.
//

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <errno.h>

#include "frame_helper.h"
#include "common.h"

#define FLAG_ANALYZE_MOTION	0x0001
#define FLAG_WRITE_DATA_FRAMES	0x0002

struct frame_helper_job {
	/* msgq msgs need to have mtype as the
	 * first element; it should be set to > 1
	 * before sending the message.
	 */
	long mtype;
	/* modify from here down */
	unsigned long flags;
	int frame_idx;
};

int analyze_motion_frame(struct frame_helper *helper, int frame_idx)
{
	struct frame_helper_job job;
	int rc;

	job.mtype = 1;
	job.flags = FLAG_ANALYZE_MOTION;
	job.frame_idx = frame_idx;

	rc = msgsnd(helper->qid, &job, sizeof(struct frame_helper_job), 0);
	if (-1 == rc) {
		int err = errno;
		LOG_ERROR("msgsnd(%d) sz=%d failed pid=%d, errno=%d", helper->qid,
				sizeof(struct frame_helper_job), getpid(), err);
	}

	return rc;
}

int write_data_frames(struct frame_helper *helper)
{
	struct frame_helper_job job;
	int rc;

	job.mtype = 1;
	job.flags = FLAG_WRITE_DATA_FRAMES;
	job.frame_idx = 0;

	rc = msgsnd(helper->qid, &job, sizeof(struct frame_helper_job), 0);
	if (-1 == rc) {
		int err = errno;
		LOG_ERROR("msgsnd(%d) sz=%d failed pid=%d, errno=%d", helper->qid,
				sizeof(struct frame_helper_job), getpid(), err);
	}

	return rc;
}

static int
analyse_motion_vector(struct picam_ctx *ctx, int frame_idx)
{
	int i = 0;
	int t = 0;
	int d = 0;
	struct motion_vector *vect = &ctx->vect_arr.vect[frame_idx];

	for (i = t = d = 0; i < ctx->map.nblocks /*&& t < ctx->threshold*/; i++, d++) {
		if (ctx->map.buff[i] <
		    ((vect->data[i].x * vect->data[i].x) + (vect->data[i].y * vect->data[i].y))) {
			t++;
		}
		if (d == ctx->map.cols)
			d = 0;
	}
	return t;
	if (t)
		LOG_INF("t=%d", t);

	return (t >= ctx->threshold);
}

void *
motion_analyzer_fn(void *arg)
{
	struct picam_ctx *ctx = (struct picam_ctx *)arg;
	struct frame_helper *helper = &ctx->helper;
	
	LOG_INF("msgqid=%d", helper->qid);
	while(1) {
		struct frame_helper_job job;
		ssize_t sz;
		
		sz = msgrcv(helper->qid, &job, sizeof(struct frame_helper_job), 1, 0);
		if (sz == -1) {
			perror("msgrcv");
			goto out;
		}
		LOG_VDBG("msgrcv; sz=%d, mtype=%d, %d", sz, job.mtype, job.frame_idx);
		
		if ((job.flags == FLAG_WRITE_DATA_FRAMES) &&
				(get_picam_state(ctx) == PICAM_RECORDING))
		{
			h264_write_frame(ctx, 0);
			if ((ctx->rec_start + ctx->nsec_cap_len) < time(NULL)) {
				LOG_INF("STOP recording now");
				set_picam_state(ctx, PICAM_WAITING);
				ctx->fname_idx++;
				open_next_file(ctx);
			}
		}

		/* analyze motion in frame */
		if ((job.flags == FLAG_ANALYZE_MOTION) &&
				analyse_motion_vector(ctx, job.frame_idx))
		{
			if (get_picam_state(ctx) == PICAM_WAITING) {
				LOG_INF("Motion detected in frame %d; Start Recording", job.frame_idx);
				h264_write_frame(ctx, 1);
				ctx->rec_start = time(NULL);
				set_picam_state(ctx, PICAM_RECORDING);
			}
		}
	}
out:
	LOG_INF("Exiting now");
	return NULL;
}

int init_frame_helper(struct picam_ctx *ctx)
{
	int rc = -1;
	struct frame_helper *helper = &ctx->helper;
	
	helper->qid = msgget(0xAABBCCDD, IPC_CREAT | IPC_EXCL | 0666);
	if (helper->qid == -1) {
		LOG_ERROR("msgq create failed!");
		goto out;
	}

	if (pthread_create(&helper->tid, NULL, motion_analyzer_fn, (void *)ctx)) {
		LOG_ERROR("pthread_create failed!\n");
		goto out;
	}
	LOG_INF("frame helper analyzer started q=%u pid=%d", helper->qid, getpid());

	rc = 0;
out:
	return rc;
}

void cleanup_frame_helper(struct frame_helper *helper)
{
	msgctl(helper->qid, IPC_RMID, NULL);
	pthread_join(helper->tid, NULL);
}

int allocate_motion_map(struct picam_ctx *ctx)
{
	int x, y;
	uint16_t sens = ctx->sensitivity;
	int i;
	int motion_vect_size;

	ctx->map.rows = (ctx->height + 15) / 16;
	ctx->map.cols = ((ctx->width + 15) / 16) + 1;
	ctx->map.nblocks = ctx->map.rows * ctx->map.cols;
	
	motion_vect_size = (ctx->map.nblocks+1) * sizeof(struct motion_macro_block);
	for (i = 0; i < MAX_MOTION_VECTS; i++) {
		ctx->vect_arr.vect[i].size = motion_vect_size;
		ctx->vect_arr.vect[i].data = malloc(motion_vect_size);
	}
	ctx->vect_arr.curr = 0;

	LOG_INF("rows=%d, cols=%d, nblocks=%d, Size of IMV map=%d\n",
		ctx->map.rows, ctx->map.cols, ctx->map.nblocks,
		(ctx->map.cols+1)*ctx->map.rows);
	ctx->map.buff = calloc((ctx->map.cols+1)*ctx->map.rows, sizeof(uint16_t));
	
	sens = sens * sens;
	if (sens > 65535)
		sens = 65535;
	for (y = 0; y < ctx->map.rows; y++) {
		for (x = 0; x < ctx->map.cols-1; x++)
			ctx->map.buff[y*ctx->map.cols + x] = (uint16_t) sens;
		ctx->map.buff[y*ctx->map.cols + ctx->map.cols] = 65535;
	}
	return (ctx->map.buff == NULL);
}

