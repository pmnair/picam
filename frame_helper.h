//
//  frame_helper.h
//  pimotioncam
//
//  Created by Praveen Nair on 3/12/16.
//  Copyright © 2016 Praveen Nair. All rights reserved.
//

#ifndef frame_helper_h
#define frame_helper_h

#define MAX_MOTION_VECTS 5

struct picam_ctx;
struct motion_macro_block {
	int8_t x;
	int8_t y;
	uint16_t sad;
};

struct motion_map {
	int rows;
	int cols;
	int nblocks;
	uint16_t *buff;
};

struct motion_vector {
	struct motion_macro_block *data;
	int size;
};

struct motion_vector_arr {
	struct motion_vector vect[MAX_MOTION_VECTS];
	int curr;
};

struct frame_helper {
	pthread_t tid;
	int	  qid;
	struct motion_map *map;
};

int init_frame_helper(struct picam_ctx *ctx);

void cleanup_frame_helper(struct frame_helper *helper);

int allocate_motion_map(struct picam_ctx *ctx);

int analyze_motion_frame(struct frame_helper *helper, int frame_idx);

int write_data_frames(struct frame_helper *helper);

#endif /* frame_helper_h */
