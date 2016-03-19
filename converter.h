//
//  converter.h
//  picam
//
//  Created by Praveen Nair on 3/18/16.
//  Copyright © 2016 Praveen Nair. All rights reserved.
//

#ifndef converter_h
#define converter_h

#include <linux/limits.h>
#include <pthread.h>

struct picam_ctx;
struct converter_ctx {
	pthread_t tid;
	int	  qid;
	char	  capture_path[PATH_MAX];
	char	  storage_path[PATH_MAX];
};

int init_converter(struct picam_ctx *ctx);

void cleanup_converter(struct converter_ctx *conv);

int convert_capture(struct converter_ctx *conv, const char *name);

#endif /* converter_h */
