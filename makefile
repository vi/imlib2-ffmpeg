include commands.mk

OPTS    := -O2
CFLAGS  := -std=c99 $(OPTS) $(shell imlib2-config --cflags) -fPIC -Wall
LDFLAGS := $(shell imlib2-config --libs) -lavcodec -lavformat -lswscale

SRC = $(wildcard *.c)
OBJ = $(foreach obj, $(SRC:.c=.o), $(notdir $(obj)))
DEP = $(SRC:.c=.d)

LIBDIR    ?= $(shell pkg-config --variable=libdir imlib2)
LOADERDIR ?= $(LIBDIR)/imlib2/loaders/

ifndef DISABLE_DEBUG
	CFLAGS += -ggdb
endif

.PHONY: all clean

all: ffmpeg.so

ffmpeg.so: $(OBJ)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -Wp,-MMD,$*.d -c $(CFLAGS) -o $@ $<

clean:
	$(RM) $(DEP)
	$(RM) $(OBJ)
	$(RM) ffmpeg.so

install:
	$(INSTALL_DIR) $(DESTDIR)$(LOADERDIR)
	$(INSTALL_LIB) ffmpeg.so $(DESTDIR)$(LOADERDIR)

uninstall:
	$(RM) $(PLUGINDIR)/ffmpeg.so

-include $(DEP)


# This is only for my system, don't rely on it

F=/home/vi/src/git/mplayer/ffmpeg
ffmpeg_static.so: ffmpeg_simple.c loader_ffmpeg.c
	ar q libffmpeg.a $F/libavutil/*.o  $F/libavcodec/*.o $F/libavcodec/*/*.o $F/libavutil/*/*.o $F/libavformat/*.o $F/libswscale/*.o $F/libswscale/*/*.o
	echo "void avformat_close_input(struct AVFormatContext **p) {void av_close_input_file(struct AVFormatContext *); av_close_input_file(*p); *p=0; }" > compat.c
	gcc -shared -o ffmpeg_static.so  -lImlib2 -lz -lX11 -lXext -ldl -lbz2 -lm -lpthread  -I $F -Wp,-MMD,ffmpeg_simple.d -std=c99 -O2  -fPIC -Davcodec_free_frame=av_freep compat.c ffmpeg_simple.c loader_ffmpeg.c ./libffmpeg.a
	strip ffmpeg_static.so
