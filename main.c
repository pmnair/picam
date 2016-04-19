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
#include <getopt.h>
#include <execinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "bcm_host.h"

#include "camera.h"
#include "common.h"
#include "frame_helper.h"

struct picam_ctx ctx;

void signal_handler(int signum, siginfo_t *siginfo, void *secret)
{
	LOG_INF("Signal caught %d; cleaning up", signum);
	if (signum == SIGSEGV) {
		void *array[10];
		size_t size;
		char err_filename[64];
		int fd = 0;

		snprintf(err_filename, 64, "/var/log/picam-crash-%d-%d.log", getpid(), signum);

		fd = open(err_filename, O_WRONLY | O_CREAT, 0644);

		/* dump the backtrace */
		size = backtrace(array, 10);
		backtrace_symbols_fd(array, size, fd);

		fsync( fd );
		close( fd );
	}
	cleanup_converter(&ctx.conv);
}

static char short_opts[] = "W:H:F:R:L:P:D:S:T:f:p:n:h";
static const struct option long_opts[] = {
	{ "width",       1, 0, 'W' },
	{ "height",      1, 0, 'H' },
	{ "fps",         1, 0, 'F' },
	{ "rot",         1, 0, 'R' },
	{ "length",      1, 0, 'L' },
	{ "pre",         1, 0, 'P' },
	{ "delay",       1, 0, 'D' },
	{ "sensitivity", 1, 0, 'S' },
	{ "threshold",   1, 0, 'T' },
	{ "file",        1, 0, 'f' },
	{ "path",        1, 0, 'p' },
	{ "num",         1, 0, 'n' },
	{ "help",        0, 0, 'h' },
	{ NULL, 0, NULL, 0 }
};

static char *usage_txt =
"Call: picam [options] -p|--path <filepath>\n"
"\n"
"-p|--path <filepath>     : path to store the files in\n\n"
"[OPTIONS]\n"
"-W|--width <value>       : resolution width\n"
"-H|--height <value>      : resolution height\n"
"-R|--rot <value>         : rotation, valid values are 90, 180, 270\n"
"-F|--fps <value>         : frames per second\n"
"-L|--length <value>      : length of each capture in seconds; default is 15 seconds\n"
"-P|--pre <value>         : length of pre-capture in seconds; default is 5 seconds\n"
"-D|--delay <value>       : startup delay in seconds; default is 5\n"
"-S|--sensitivity <value> : sensitivity of motion detection; default is 50\n"
"-T|--threshold <value>   : threshold for motion detection; default is 10\n"
"-h|--help                : print this help\n\n";

int main(int argc, char * const argv[])
{
	int rc = 0;
	struct timespec end_tm;
	struct sigaction signal_act;
	int c;

	memset(&ctx, 0, sizeof(struct picam_ctx));
	ctx.width = 1280;
	ctx.height = 720;
	ctx.fps = 30;
	ctx.rot = 0;
	ctx.nsec_pre_cap = 5;
	ctx.nsec_cap_len = 15;
	ctx.sensitivity = 50;
	ctx.threshold = 10;
	ctx.motion_check_delay = 5;
	
	init_picam_state(&ctx);
	ctx.path = NULL;
	ctx.video_fp = NULL;
	ctx.image_fp = NULL;

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch( c ) {
			case 'W':
				ctx.width = atoi(optarg);
				break;
			case 'H':
				ctx.height = atoi(optarg);
				break;
			case 'F':
				ctx.fps = atoi(optarg);
				break;
			case 'R':
				ctx.rot = atoi(optarg);
				switch(ctx.rot)
				{
				case 90:
				case 180:
				case 270:
					break;
				default:
					ctx.rot = 0;
				}

				break;
			case 'L':
				ctx.nsec_cap_len = atoi(optarg);
				break;
			case 'P':
				ctx.nsec_pre_cap = atoi(optarg);
				break;
			case 'D':
				ctx.motion_check_delay = atoi(optarg);
				break;
			case 'S':
				ctx.sensitivity = atoi(optarg);
				break;
			case 'T':
				ctx.threshold = atoi(optarg);
				break;
			case 'p':
				ctx.path = strdup(optarg);
				break;
			case '?':
			default:
				fprintf(stderr, "unknown option\n");
				fprintf(stderr, "%s", usage_txt);
				exit(1);
		}
	}

	if (!ctx.path) {
		fprintf(stderr, "unknown option\n");
		fprintf(stderr, "%s", usage_txt);
		exit(1);
	}

	openlog("picam", LOG_PID, LOG_DAEMON);
	bcm_host_init();

	ctx.camera.ctx = &ctx;
	ctx.video_encoder.ctx = &ctx;
	ctx.image_encoder.ctx = &ctx;
	ctx.stream_resizer.ctx = &ctx;

	sigemptyset(&signal_act.sa_mask);
	signal_act.sa_flags = SA_ONESHOT;
	signal_act.sa_sigaction = signal_handler;
	sigaction(SIGHUP, &signal_act, NULL);    /* Hangup (POSIX).  */
	sigaction(SIGINT, &signal_act, NULL);    /* Interrupt (ANSI).  */
	sigaction(SIGQUIT, &signal_act, NULL);   /* Quit (POSIX).  */
	sigaction(SIGILL, &signal_act, NULL);    /* Illegal instruction (ANSI).  */
	sigaction(SIGABRT, &signal_act, NULL);   /* Abort (ANSI).  */
	sigaction(SIGSEGV, &signal_act, NULL);   /* Segmentation violation (ANSI).  */
	sigaction(SIGTERM, &signal_act, NULL);   /* Termination (ANSI).  */
	sigaction(SIGPWR, &signal_act, NULL);    /* Power failure restart (System V).  */
	sigaction(SIGSYS, &signal_act, NULL);    /* Bad system call.  */

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
	rc = create_camera(&ctx.camera, ctx.width, ctx.height, ctx.fps, ctx.rot);
	if (rc) {
		LOG_ERROR("Failed to create camera component!");
		goto cleanup;
	}
	
	/* create video encoder component */
	rc = create_video_encoder(&ctx.video_encoder, ctx.fps);
	if (rc) {
		LOG_ERROR("Failed to create video encoder component!");
		goto cleanup_cam;
	}

	/* create resizer component */
	rc = create_resizer(&ctx.stream_resizer, &ctx.camera);
	if (rc) {
		LOG_ERROR("Failed to create resizer component!");
		goto cleanup_ve;
	}

	/* create image encoder component */
	rc = create_image_encoder(&ctx.image_encoder, &ctx.stream_resizer);
	if (rc) {
		LOG_ERROR("Failed to create image encoder component!");
		goto cleanup_sr;
	}

	/* open file to record in */
	open_next_file(&ctx, NULL, 0);
	
	/* connect video output port of camera to input of video encoder */
	/* camera_video--(tunnel)-->h264 encoder-->video_h264_encoder_callback */
	connect_components(&ctx.camera, CAMERA_VIDEO_PORT, &ctx.video_encoder);

	/* connect image output port of camera to input of jpeg encoder */
	/* preview --(tunnel)--> resizer --> I420_callback --> jpeg_encoder --> mjpeg_callback */
	connect_components(&ctx.camera, CAMERA_PREVIEW_PORT, &ctx.stream_resizer);
	connect_components(&ctx.stream_resizer, 0, &ctx.image_encoder);

	/* setup callback for video encoder output */
	connect_output_callback(&ctx.video_encoder, 0, h264_encoder_cb);

	/* setup callback for image encoder output */
	connect_output_callback(&ctx.image_encoder, 0, jpeg_encoder_cb);

	/* start recording */
	clock_gettime(CLOCK_MONOTONIC_RAW, &ctx.start_tm);
	ctx.startup_time = time(NULL);
	start_camera(&ctx.camera);
	
	/* run the capture converter loop */
	run_capture_converter(&ctx);
	
	destroy_comp(&ctx.image_encoder);
cleanup_sr:
	destroy_comp(&ctx.stream_resizer);
cleanup_ve:
	destroy_comp(&ctx.video_encoder);
cleanup_cam:
	destroy_comp(&ctx.camera);
cleanup:
	clock_gettime(CLOCK_MONOTONIC_RAW, &end_tm);
	fclose(ctx.video_fp);
	LOG_INF("Key Frame Count=%d, Frame Count=%d, nsec=%lu",
		ctx.key_frame_cnt, ctx.frame_cnt,
		end_tm.tv_sec - ctx.start_tm.tv_sec);
	cleanup_frame_helper(&ctx.helper);
out:
	free_frame_buffers(&ctx);
	if (ctx.map.buff) free(ctx.map.buff);

	closelog();
	return rc;
}
