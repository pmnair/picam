//
//  camera.h
//  pimotioncam
//
//  Created by Praveen Nair on 3/12/16.
//  Copyright © 2016 Praveen Nair. All rights reserved.
//

#ifndef camera_h
#define camera_h

#include "common.h"
#include "frame_helper.h"

#define MAX_ENCODER_BUFFERS 3


void destroy_comp(struct picam_component *component);

int start_camera(struct picam_component *cam);
int allocate_frame_buffers(struct picam_ctx *ctx);
void free_frame_buffers(struct picam_ctx *ctx);
int create_camera(struct picam_component *cam, int width, int height, int fps, int rot);
int create_video_encoder(struct picam_component *enc, int fps);
int create_resizer(struct picam_component *resizer, struct picam_component *src);
int create_image_encoder(struct picam_component *enc, struct picam_component *src);
int connect_components(struct picam_component *out, int pnum, struct picam_component *in);
int connect_output_callback(struct picam_component *component,
			    int pnum,
			    void (*cb)(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *mmalbuf));

void
h264_encoder_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

void
jpeg_encoder_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

#endif /* camera_h */
