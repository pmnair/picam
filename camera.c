//
//  camera.c
//  pimotioncam
//
//  Created by Praveen Nair on 3/12/16.
//  Copyright © 2016 Praveen Nair. All rights reserved.
//
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include "camera.h"
#include "common.h"

#define _WRITE_FILE_ 0

static void
camera_control_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	LOG_VDBG("event");
	if (buffer->cmd != MMAL_EVENT_PARAMETER_CHANGED)
		LOG_VDBG("event 0x%x", buffer->cmd);

	/* Not doing anything with control buffer headers so just send
	 * them back to the owner.
	 */
	mmal_buffer_header_release(buffer);
}

int
h264_save_frame(struct picam_ctx *ctx, MMAL_BUFFER_HEADER_T *buffer)
{
	struct picam_frame *frame = ctx->frame_buffers.frames[ctx->frame_buffers.curr];
	int frame_idx;

	pthread_mutex_lock(&ctx->frame_buffers.lock);

	mmal_buffer_header_mem_lock(buffer);
	memcpy(frame->data, buffer->data, buffer->length);
	frame->len = buffer->length;
	frame->keyframe = (buffer->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME);
	mmal_buffer_header_mem_unlock(buffer);

	frame->tm = time(NULL);
	LOG_VDBG("curr frame = %d", ctx->frame_buffers.curr);
	ctx->frame_buffers.last = ctx->frame_buffers.curr;
	ctx->frame_buffers.curr = (ctx->frame_buffers.curr+1)%ctx->frame_buffers.nalloc;
	frame_idx = ctx->frame_buffers.last;
	pthread_mutex_unlock(&ctx->frame_buffers.lock);
	return frame_idx;
}

void
h264_encoder_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	MMAL_BUFFER_HEADER_T *free_buffer;
	MMAL_STATUS_T ret = MMAL_SUCCESS;
	struct picam_component *component = (struct picam_component *)port->userdata;
	struct picam_ctx *ctx = component->ctx;
	int state;

	/* h264 header; this is received at the start of recording cycle */
	if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG) {
		mmal_buffer_header_mem_lock(buffer);
		memcpy(ctx->h264_hdr + ctx->h264_hdr_pos, buffer->data, buffer->length);
		ctx->h264_hdr_pos += buffer->length;
#if _WRITE_FILE_
		fwrite(buffer->data, 1, buffer->length, ctx->fp);
#endif
		mmal_buffer_header_mem_unlock(buffer);
		LOG_VDBG("h264 header length(%d) total=%d",
				buffer->length, ctx->h264_hdr_pos);
	}
	/* this frame has codec info that can be used for motion detection */
	else if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) {
		state = get_picam_state(ctx);
		if ((state == PICAM_WAITING) &&
		    	(ctx->startup_time + ctx->motion_check_delay) < time(NULL))
		{
#if !_WRITE_FILE_
			mmal_buffer_header_mem_lock(buffer);
			memcpy(ctx->vect_arr.vect[ctx->vect_arr.curr].data, buffer->data, buffer->length);
			mmal_buffer_header_mem_unlock(buffer);
			LOG_VDBG("sending motion frame %d for analysis", ctx->vect_arr.curr);
			analyze_motion_frame(&ctx->helper, ctx->vect_arr.curr);
			ctx->vect_arr.curr = (ctx->vect_arr.curr+1)%MAX_MOTION_VECTS;
#endif
		}
	}
	else {
		int frame_idx;
		/* data contains key frame; this is received once every second or so */
		if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME) {
			LOG_VDBG("h264 key frame length(%d)", buffer->length);
			component->ctx->key_frame_cnt++;
		}
		/* data contains frame; multiple frames are received per second */
		if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) {
			LOG_VDBG("h264 frame length(%d)", buffer->length);
			component->ctx->frame_cnt++;
		}
#if _WRITE_FILE_
		mmal_buffer_header_mem_lock(buffer);
		fwrite(buffer->data, 1, buffer->length, ctx->fp);
		mmal_buffer_header_mem_unlock(buffer);
#else
		frame_idx = h264_save_frame(ctx, buffer);
		write_data_frames(&ctx->helper, frame_idx);
#endif
	}

	mmal_buffer_header_release(buffer);
	if (port->is_enabled)
	{
		free_buffer = mmal_queue_get(component->out_pool->queue);
		if (free_buffer)
			ret = mmal_port_send_buffer(port, free_buffer);

		if (!free_buffer || (ret != MMAL_SUCCESS))
			LOG_ERROR("failed to send buffer to port");
	}
}

MMAL_STATUS_T setup_port_format(MMAL_PORT_T *port, int enc, int enc_var, int width, int height, int fps)
{
	MMAL_ES_FORMAT_T *fmt = port->format;

	fmt->encoding = enc;
	fmt->encoding_variant = enc_var;
	fmt->bitrate = 2000000;
	fmt->es->video.width = width;
	fmt->es->video.height = height;
	fmt->es->video.crop.x = 0;
	fmt->es->video.crop.y = 0;
	fmt->es->video.crop.width = width;
	fmt->es->video.crop.height = height;

	if (fps) {
		fmt->es->video.frame_rate.num = fps;
		fmt->es->video.frame_rate.den = 1;
	}

	return mmal_port_format_commit(port);
}

void destroy_comp(struct picam_component *component)
{
	if (!component->comp)
		return;

	if (component->conn) {
		mmal_connection_disable(component->conn);
		mmal_connection_destroy(component->conn);
		component->conn = NULL;
	}
	mmal_component_disable(component->comp);
	mmal_component_destroy(component->comp);
	component->comp = NULL;
}

int start_camera(struct picam_component *cam)
{
	MMAL_STATUS_T ret;

	ret = mmal_port_parameter_set_boolean(cam->comp->output[CAMERA_VIDEO_PORT],
					      MMAL_PARAMETER_CAPTURE,
					      MMAL_TRUE);
	return (ret == MMAL_SUCCESS)?0:1;
}

int allocate_frame_buffers(struct picam_ctx *ctx)
{
	int rc = 0;
	int ii;

	ctx->frame_buffers.nalloc = ctx->fps * (ctx->nsec_pre_cap + 3);
	ctx->frame_buffers.frames = calloc(ctx->frame_buffers.nalloc,
					   sizeof(struct picam_frame *));

	for (ii = 0; ii < ctx->frame_buffers.nalloc; ii++) {
		ctx->frame_buffers.frames[ii] = calloc(1, sizeof(struct picam_frame));
		ctx->frame_buffers.frames[ii]->data = calloc(65536,
							     sizeof(uint8_t));
	}

	ctx->frame_buffers.curr = 0;
	ctx->frame_buffers.last = -1;
	pthread_mutex_init(&ctx->frame_buffers.lock, NULL);
	
	return rc;
}

void free_frame_buffers(struct picam_ctx *ctx)
{
	int ii;

	for (ii = 0; ii < ctx->frame_buffers.nalloc; ii++) {
		if (ctx->frame_buffers.frames[ii]->data)
			free(ctx->frame_buffers.frames[ii]->data);
		if (ctx->frame_buffers.frames[ii])
			free(ctx->frame_buffers.frames[ii]);
	}
	if (ctx->frame_buffers.frames)
		free(ctx->frame_buffers.frames);
}

int create_camera(struct picam_component *cam, int width, int height, int fps)
{
	MMAL_STATUS_T ret;
	MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
		{ MMAL_PARAMETER_CAMERA_CONFIG, sizeof (cam_config)},
		.max_stills_w = width,
		.max_stills_h = height,
		.stills_yuv422 = 0,
		.one_shot_stills = 1,
		.max_preview_video_w = width,
		.max_preview_video_h = height,
		.num_preview_video_frames = 3,
		.stills_capture_circular_buffer_height = 0,
		.fast_preview_resume = 0,
		.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
	};
	int rc = -1;

	/* create camera component */
	cam->name = "camera";
	ret = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &cam->comp);
	if (ret != MMAL_SUCCESS) {
		LOG_ERROR("Failed to create camera component!");
		goto out;
	}

	/* register camera control callback */
	ret = mmal_port_enable(cam->comp->control, camera_control_cb);
	if (ret != MMAL_SUCCESS) {
		LOG_ERROR("Failed to setup camera control!");
		goto error;
	}

	/* configure camera */
	mmal_port_parameter_set(cam->comp->control, &cam_config.hdr);

	/* setup preview port format */
	ret = setup_port_format(cam->comp->output[CAMERA_PREVIEW_PORT],
				MMAL_ENCODING_OPAQUE, MMAL_ENCODING_I420, width, height, fps);
	if (ret != MMAL_SUCCESS) {
		LOG_ERROR("Failed to setup preview port format!");
		goto error;
	}
	LOG_INF("camera preview port nbuffs=%d, buff_sz=%d",
				cam->comp->output[CAMERA_PREVIEW_PORT]->buffer_num,
				cam->comp->output[CAMERA_PREVIEW_PORT]->buffer_size);

	/* setup video port format */
	ret = setup_port_format(cam->comp->output[CAMERA_VIDEO_PORT],
				MMAL_ENCODING_OPAQUE, MMAL_ENCODING_I420, width, height, fps);
	if (ret != MMAL_SUCCESS) {
		LOG_ERROR("Failed to setup video port format!");
		goto error;
	}
	LOG_INF("camera video port nbuffs=%d, buff_sz=%d",
				cam->comp->output[CAMERA_VIDEO_PORT]->buffer_num,
				cam->comp->output[CAMERA_VIDEO_PORT]->buffer_size);

	/* setup still port format */
	ret = setup_port_format(cam->comp->output[CAMERA_STILL_PORT],
				MMAL_ENCODING_OPAQUE, MMAL_ENCODING_I420, width, height, 0);
	if (ret != MMAL_SUCCESS) {
		LOG_ERROR("Failed to setup still port format!");
		goto error;
	}
	LOG_INF("camera still port nbuffs=%d, buff_sz=%d",
				cam->comp->output[CAMERA_STILL_PORT]->buffer_num,
				cam->comp->output[CAMERA_STILL_PORT]->buffer_size);

	ret = mmal_component_enable(cam->comp);
	if (ret != MMAL_SUCCESS) {
		LOG_ERROR("Failed to enable camera component!");
		goto error;
	}

	rc = 0;
out:
	return rc;

error:
	destroy_comp(cam);
	return -1;
}

int create_encoder(struct picam_component *enc, int fps)
{
	MMAL_STATUS_T ret;
	MMAL_PORT_T	*in_port, *out_port;
	int rc = -1;

	/* create camera component */
	enc->name = "h264-encoder";
	ret = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &enc->comp);
	if (ret != MMAL_SUCCESS) {
		LOG_ERROR("Failed to create encoder component!");
		goto out;
	}

	in_port = enc->comp->input[0];
	out_port = enc->comp->output[0];
	out_port->buffer_num = MAX_ENCODER_BUFFERS;
	out_port->buffer_size =
			MAX(out_port->buffer_size_recommended,
			    out_port->buffer_size_min);

	LOG_INF("encoder output port nbuffs=%d, buff_sz=%d",
			out_port->buffer_num, out_port->buffer_size);
	mmal_format_copy(out_port->format, in_port->format);
	out_port->format->encoding = MMAL_ENCODING_H264;
	out_port->format->bitrate = 2000000;
	/* We need to set the frame rate on output to 0, to ensure
	 * it gets updated correctly from the input framerate when
	 * port connected.
	 */
	out_port->format->es->video.frame_rate.num = 0;
	out_port->format->es->video.frame_rate.den = 1;

	/* Enable inline motion vectors */
	mmal_port_parameter_set_boolean(out_port,
					MMAL_PARAMETER_VIDEO_ENCODE_INLINE_VECTORS, MMAL_TRUE);

	ret = mmal_port_format_commit(out_port);
	if (ret != MMAL_SUCCESS) {
		LOG_ERROR("Failed to commit encoder format!");
		goto error;
	}

	ret = mmal_component_enable(enc->comp);
	if (ret != MMAL_SUCCESS) {
		LOG_ERROR("Failed to enable encoder component!");
		goto error;
	}

	rc = 0;
out:
	return rc;

error:
	destroy_comp(enc);
	return -1;
}

int connect_components(struct picam_component *out, int pnum, struct picam_component *in)
{
	MMAL_PORT_T	*out_port, *in_port;
	MMAL_CONNECTION_T *conn = NULL;
	MMAL_STATUS_T ret;
	int rc = -1;

	if (!out->comp || !in->comp)
		goto out;
	out_port = out->comp->output[pnum];
	in_port =  in->comp->input[0];

	ret = mmal_connection_create(&conn, out_port, in_port,
				     MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT
				     | MMAL_CONNECTION_FLAG_TUNNELLING);
	if (ret != MMAL_SUCCESS) {
		LOG_ERROR("Failed to create connection!");
		goto out;
	}

	ret = mmal_connection_enable(conn);
	if (ret != MMAL_SUCCESS) {
		LOG_ERROR("Failed to enable connection!");
		goto error;
	}

	in->conn = conn;
	rc = 0;
out:
	return rc;

error:
	mmal_connection_destroy(conn);
	return -1;
}

int connect_output_callback(struct picam_component *component,
			    int pnum,
			    void (*cb)(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *mmalbuf))
{
	MMAL_PORT_T *port;
	MMAL_STATUS_T ret;
	int	i, n;
	int rc = -1;

	if (!component->comp)
		goto out;

	port = component->comp->output[pnum];
	/* Create an initial queue of buffers for the output port */
	LOG_INF("creating pool nbuffs=%d, buff_sz=%d",
			port->buffer_num, port->buffer_size);
	component->out_pool = mmal_port_pool_create(port,
						    port->buffer_num,
						    port->buffer_size);
	if (!component->out_pool) {
		LOG_ERROR("Failed to create output pool!");
		goto out;
	}

	/* Connect the callback and initialize buffer pool of data
	 * that will be sent to the callback.
	 */
	ret = mmal_port_enable(port, cb);
	if (ret != MMAL_SUCCESS) {
		LOG_ERROR("Failed to enable output callback!");
		goto out;
	}

	port->userdata = (struct MMAL_PORT_USERDATA_T *)component;

	/* Send all buffers in the created queue to the GPU output port.
	 * These buffers will then be delivered back to the ARM with filled
	 * GPU data via the above callback where we can process the data
	 * and then resend the buffer back to the port to be refilled.
	 */
	n = mmal_queue_length(component->out_pool->queue);
	LOG_INF("pool queue length=%d", n);
	for (i = 0; i < n; ++i) {
		ret = mmal_port_send_buffer(port,
					    mmal_queue_get(component->out_pool->queue));
		if (ret != MMAL_SUCCESS)
			break;
	}

	rc = 0;
out:
	return rc;
}
