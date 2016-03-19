//
//  converter.c
//  picam
//
//  Created by Praveen Nair on 3/18/16.
//  Copyright © 2016 Praveen Nair. All rights reserved.
//

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"
#include "converter.h"

#define POLL_INTERVAL   5000
#define EVENT_SIZE      ( sizeof( struct inotify_event ) )
#define EVENT_BUF_LEN   ( 2 * ( EVENT_SIZE + 32 ) )

struct converter_job {
	/* msgq msgs need to have mtype as the
	 * first element; it should be set to > 1
	 * before sending the message.
	 */
	long mtype;
	/* modify from here down */
	unsigned long flags;
	char *fname;
};

void *
capture_converter_fn(void *arg)
{
	struct converter_ctx *conv = (struct converter_ctx *)arg;

	LOG_INF("msgqid=%d", conv->qid);
	while(1) {
		struct converter_job job;
		ssize_t sz;

		sz = msgrcv(conv->qid, &job, sizeof(struct converter_job), 1, 0);
		if (sz == -1) {
			perror("msgrcv");
			goto out;
		}
		LOG_VDBG("msgrcv; sz=%d, %p, %s", sz, job.fname, job.fname);
	}
out:
	LOG_INF("Exiting now");
	return NULL;
}

int init_converter(struct picam_ctx *ctx)
{
	int rc = -1;
	struct converter_ctx *conv = &ctx->conv;

	conv->qid = msgget(0xAABBCCAA, IPC_CREAT | IPC_EXCL | 0666);
	if (conv->qid == -1) {
		LOG_ERROR("msgq create failed!");
		goto out;
	}

	if (pthread_create(&conv->tid, NULL, capture_converter_fn, (void *)conv)) {
		LOG_ERROR("pthread_create failed!\n");
		goto out;
	}
	LOG_INF("capture converter started q=%u pid=%d", conv->qid, getpid());

	rc = 0;
out:
	return rc;
}

void cleanup_converter(struct converter_ctx *conv)
{
	msgctl(conv->qid, IPC_RMID, NULL);
	pthread_join(conv->tid, NULL);
}

int convert_capture(struct converter_ctx *conv, const char *name)
{
	struct converter_job job;
	int rc;

	job.mtype = 1;
	job.flags = 0;
	job.fname = strdup(name);

	rc = msgsnd(conv->qid, &job, sizeof(struct converter_job), 0);
	if (-1 == rc) {
		int err = errno;
		LOG_ERROR("msgsnd(%d) sz=%d failed pid=%d, errno=%d", conv->qid,
			  sizeof(struct converter_job), getpid(), err);
	}

	return rc;
}
