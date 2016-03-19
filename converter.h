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

struct picam_ctx;
struct converter_ctx {
	int	  qid;
	char	  capture_path[PATH_MAX];
	char	  storage_path[PATH_MAX];
};

int init_converter(struct picam_ctx *ctx);

void cleanup_converter(struct converter_ctx *conv);

int convert_capture(struct converter_ctx *conv, const char *name);

void run_capture_converter(struct picam_ctx *ctx);

#endif /* converter_h */
