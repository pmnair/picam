//
//  frame_helper.h
//  pimotioncam
//
//  Created by Praveen Nair on 3/12/16.
//  Copyright © 2016 Praveen Nair. All rights reserved.
//

#ifndef frame_helper_h
#define frame_helper_h

#include "common.h"

int init_frame_helper(struct picam_ctx *ctx);

void cleanup_frame_helper(struct frame_helper *helper);

int allocate_motion_map(struct picam_ctx *ctx);

int analyze_motion_frame(struct frame_helper *helper, int frame_idx);

int write_data_frames(struct frame_helper *helper);

#endif /* frame_helper_h */
