//
//  main.c
//  pimotioncam
//
//  Created by Praveen Nair on 3/12/16.
//  Copyright © 2016 Praveen Nair. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "bcm_host.h"

#include "camera.h"
#include "common.h"
#include "frame_helper.h"

int main(int argc, const char * argv[]) {
	int rc = 0;
	struct picam_ctx ctx;
	struct timespec end_tm;
	int width = 1280;
	int height = 720;
	int fps = 30;
	
	if (argc < 2) {
		printf("Usage: %s <fname>.h264\n", argv[0]);
		exit(1);
	}
	bcm_host_init();
	
	memset(&ctx, 0, sizeof(struct picam_ctx));
	ctx.width = width;
	ctx.height = height;
	ctx.fps = fps;
	ctx.nsec_pre_cap = 5;
	ctx.nsec_cap_len = 5;
	ctx.sensitivity = 10;
	ctx.threshold = 5;
	ctx.motion_check_delay = 3;
	
	init_picam_state(&ctx);
	ctx.fname_idx = 1;
	ctx.fname = strdup(argv[1]);
	ctx.fp = NULL;
	
	ctx.camera.ctx = &ctx;
	ctx.encoder.ctx = &ctx;
	
	/* allocate frame buffers */
	rc = allocate_frame_buffers(&ctx);
	if (rc) {
		LOG_ERROR("Failed to allocate frame buffers!");
		goto out;
	}
	
	/* allocate frame buffers */
	rc = allocate_motion_map(&ctx);
	if (rc) {
		LOG_ERROR("Failed to allocate motion map!");
		goto out;
	}

	/* initialize capture converter context */
	rc = init_converter(&ctx);
	if (rc) {
		LOG_ERROR("Failed to initialize capture converter!");
		goto out;
	}

	/* initialize motion analyzer */
	rc = init_frame_helper(&ctx);
	if (rc) {
		LOG_ERROR("Failed to initialize motion analyzer!");
		goto out;
	}

	/* create camera component */
	rc = create_camera(&ctx.camera, width, height, fps);
	if (rc) {
		LOG_ERROR("Failed to create camera component!");
		goto cleanup;
	}
	
	/* create encoder component */
	rc = create_encoder(&ctx.encoder, fps);
	if (rc) {
		LOG_ERROR("Failed to create encoder component!");
		goto cleanup;
	}
	
	/* open file to record in */
	open_next_file(&ctx);
	
	/* connect video output port of camera to input of encoder */
	connect_components(&ctx.camera, CAMERA_VIDEO_PORT, &ctx.encoder);
	
	/* setup callback for encoder output */
	connect_output_callback(&ctx.encoder, 0, h264_encoder_cb);
	
	/* start recording */
	clock_gettime(CLOCK_MONOTONIC_RAW, &ctx.start_tm);
	ctx.startup_time = time(NULL);
	start_camera(&ctx.camera);
	
	/* run the capture converter loop */
	run_capture_converter(&ctx);
	
cleanup:
	destroy_comp(&ctx.encoder);
	destroy_comp(&ctx.camera);
	clock_gettime(CLOCK_MONOTONIC_RAW, &end_tm);
	fclose(ctx.fp);
	LOG_INF("Key Frame Count=%d, Frame Count=%d, nsec=%lu",
		ctx.key_frame_cnt, ctx.frame_cnt,
		end_tm.tv_sec - ctx.start_tm.tv_sec);
	cleanup_frame_helper(&ctx.helper);
out:
	free_frame_buffers(&ctx);
	if (ctx.fname) free(ctx.fname);
	if (ctx.map.buff) free(ctx.map.buff);

	return rc;
}
