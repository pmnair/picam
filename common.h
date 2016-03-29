//
//  common.h
//  pimotioncam
//
//  Created by Praveen Nair on 3/12/16.
//  Copyright © 2016 Praveen Nair. All rights reserved.
//

#ifndef common_h
#define common_h

#include <pthread.h>
#include <syslog.h>

#include "interface/vcos/vcos.h"
#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

#include "frame_helper.h"
#include "converter.h"

#define CAMERA_PREVIEW_PORT 0
#define CAMERA_VIDEO_PORT   1
#define CAMERA_STILL_PORT   2

#ifndef MAX
#define MAX(a,b)    (((a) > (b)) ? (a) : (b))
#define MIN(a,b)    (((a) < (b)) ? (a) : (b))
#endif

#if _DEBUG_
#define LOG_DBG(fmt, ...)	printf("*DBG* %s:"fmt"\n", __FUNCTION__, ## __VA_ARGS__ );
#else
#define LOG_DBG(fmt, ...)
#endif

#if _VERBOSE_
#define LOG_VDBG(fmt, ...)	printf("*DBG* %s:"fmt"\n", __FUNCTION__, ## __VA_ARGS__ );
#else
#define LOG_VDBG(fmt, ...)
#endif

#define LOG_INF(fmt, ...)	{ printf("*INF* %s: "fmt"\n", __FUNCTION__, ## __VA_ARGS__ ); \
				  syslog(LOG_INFO, "*INF* %s: "fmt"\n", __FUNCTION__, ## __VA_ARGS__ ); }
#define LOG_ERROR(fmt, ...)	{ printf("*ERR* %s: "fmt"\n", __FUNCTION__, ## __VA_ARGS__ ); \
				  syslog(LOG_ERR, "*ERR* %s: "fmt"\n", __FUNCTION__, ## __VA_ARGS__ ); }

enum {
	PICAM_WAITING         = 0,
	PICAM_RECORDING_START = 1,
	PICAM_RECORDING	      = 2,
};

struct picam_frame {
	uint8_t *data;
	uint32_t len;
	uint8_t keyframe;
	time_t  tm;
};

struct picam_buffers {
	struct picam_frame **frames;
	int nalloc;
	int last;
	int curr;
	pthread_mutex_t lock;
};

struct picam_component {
	const char          *name;

	MMAL_COMPONENT_T    *comp;
	MMAL_CONNECTION_T   *conn;
	MMAL_POOL_T         *out_pool;

	struct picam_ctx    *ctx;
};

struct picam_ctx {
	int width;
	int height;
	int fps;
	int nsec_pre_cap;
	int nsec_cap_len;
	int sensitivity;
	int threshold;
	
	struct picam_component camera;
	struct picam_component video_encoder;
	struct picam_component stream_resizer;
	struct picam_component image_encoder;
	FILE *video_fp;
	FILE *image_fp;
	int   key_frame_cnt;
	int   frame_cnt;
	struct timespec start_tm;
	
	uint8_t	h264_hdr[32];
	int	h264_hdr_pos;
	
	struct frame_helper helper;
	struct motion_vector_arr vect_arr;
	struct motion_map map;
	struct picam_buffers frame_buffers;
	
	pthread_mutex_t lock;
	int state;
	time_t  rec_start;
	char fname[PATH_MAX];
	char *path;
	char *curr_dir;

	struct converter_ctx conv;

	time_t  startup_time;
	int     motion_check_delay;
};

static inline void init_picam_state(struct picam_ctx *ctx)
{
	pthread_mutex_init(&ctx->lock, NULL);
	ctx->state = PICAM_WAITING;
}

static inline int get_picam_state(struct picam_ctx *ctx)
{
	int state;
	pthread_mutex_lock(&ctx->lock);
	state = ctx->state;
	pthread_mutex_unlock(&ctx->lock);

	return state;
}

static inline void set_picam_state(struct picam_ctx *ctx, int state)
{
	pthread_mutex_lock(&ctx->lock);
	ctx->state = state;
	pthread_mutex_unlock(&ctx->lock);
}

int
open_next_file(struct picam_ctx *ctx, char *last_fname, int len);

void
h264_write_frames(struct picam_ctx *ctx);

void
h264_write_frame(struct picam_ctx *ctx, int frame_idx);

#endif /* common_h */
