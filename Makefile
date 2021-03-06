
MMAL_INCLUDE ?= -I/opt/vc/include \
                                -I/opt/vc/include/interface/vcos/pthreads \
                                -I/opt/vc/include/interface/vmcs_host/linux

MMAL_LIB ?= -L/opt/vc/lib -lbcm_host -lvcos -lmmal -lmmal_core -lmmal_util \
                                -lmmal_vc_client


FLAGS = -O2 -Wall $(MMAL_INCLUDE) $(INCLUDES)
LIBS = $(MMAL_LIB) -lm -lpthread

SRCS := main.c camera.c frame_helper.c common.c converter.c
SRCS_CTL := picam_controller.c

%.o:%.c
	$(CC) $(CXXFLAGS) $(FLAGS) -c $< -o $@

OBJS := $(patsubst %.c, %.o, $(SRCS))
OBJS_CTL := $(patsubst %.c, %.o, $(SRCS_CTL))

all: $(OBJS) $(OBJS_CTL)
	$(CC) -o picam $(OBJS) $(LIBS)
	$(CC) -o pictl $(OBJS_CTL) $(LIBS)

clean:
	rm -f $(OBJS) $(OBJS_CTL) picam pictl

