/* File: loader_webp.c
   Time-stamp: <2012-12-09 21:19:30 gawen>

   Copyright (c) 2011 David Hauweele <david@hauweele.net>
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the University nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE. */

#define _BSD_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <Imlib2.h>

#include "imlib2_common.h"
#include "loader.h"


static int ffmpeg_already_initialized = 0;

char load(ImlibImage * im, ImlibProgressFunction progress,
          char progress_granularity, char immediate_load)
{
  
  int w,h;
  int has_alpha = 1;
  char retret = 0;

  //fprintf(stderr, "Qqq %s\n", im->real_file);
  
  
  if(im->data)
    return 0;

  if (!ffmpeg_already_initialized) {
    avcodec_register_all();
    av_register_all();
    ffmpeg_already_initialized = 1;
  }
  
  AVFormatContext *pFormatCtx = NULL;
  if (avformat_open_input(&pFormatCtx, im->real_file, NULL, NULL)!=0) {
    perror("av_open_input_file");
    return 0;
  }

  if(avformat_find_stream_info(pFormatCtx, NULL)<0) {
    perror("avformat_find_stream_info");
    return 0;
  }
  
  int i;
  AVStream* videoStream = NULL;
  int videoStreamIndex = 0;
  for(i=0; i<pFormatCtx->nb_streams; i++) {
    AVStream *r = pFormatCtx->streams[i];
    if(r->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (!videoStream) {
        videoStream = r;
        videoStreamIndex = i;
      } else {
        fprintf(stderr, "Warning: ignoring other video streams\n");
      }
    }
  }
  if (!videoStream) {
    avformat_close_input(&pFormatCtx);
    return 0;
  }
  
  // Find the decoder for the video stream
  AVCodec* pCodec = avcodec_find_decoder(videoStream->codec->codec_id);
  if(pCodec==NULL) {
    goto EXIT;
  }

   // Open codec
  if(avcodec_open2(videoStream->codec, pCodec, NULL)<0){
    goto EXIT;
  }
    
  
  w = videoStream->codec->width;
  h = videoStream->codec->height;

  fprintf(stderr, "w=%d h=%d\n", w, h);
  
  if(!im->loader && !im->data) {
    im->w = w;
    im->h = h;

    if(!IMAGE_DIMENSIONS_OK(w, h))
      goto EXIT;

    if(!has_alpha)
      UNSET_FLAGS(im->flags, F_HAS_ALPHA);
    else
      SET_FLAGS(im->flags, F_HAS_ALPHA);
    
    im->format = strdup("ffmpeg");
  }

  if((!im->data && im->loader) || immediate_load || progress) {
    AVPacket packet;
    AVPacket packet_to_send;
    av_init_packet(&packet);
    packet.size=0;
    packet_to_send.size=0;
    AVFrame* frame = avcodec_alloc_frame();
    AVFrame* frameRGB = avcodec_alloc_frame();
    
    
    int limit=256;
    int got_frame = 0;
    int no_more_frames=0;
    for(;;) {
      int ret;
        
      if (!no_more_frames) {
        read_next_packet:
        ret = av_read_frame(pFormatCtx, &packet);
        if(ret<0) {
          fprintf(stderr, "No more frames\n");
          no_more_frames=1;
          //break;
        }
      }
      fprintf(stderr, "ret %p %p %d\n", videoStream->codec, packet.data, packet.size);
      if (packet.stream_index != videoStream->index) {
        goto read_next_packet;
      }
        
      
      int isFrameAvailable;
      ret = avcodec_decode_video2(videoStream->codec, frame, &isFrameAvailable, &packet);
      if (ret < 0) {
        fprintf(stderr, "avcodec_decode_video2 failed\n");
        break; // fail
      }
      
      if (isFrameAvailable) {
        got_frame = 1;
        break;
      }
      
      --limit;
      if(!limit) {
        fprintf(stderr, "Limit reached\n");
        break;
      }
    }
    av_free_packet(&packet);
    
    if (got_frame) {
    
      fprintf(stderr, "Got frame %dx%d\n", frame->width, frame->height);
    
      int numBytes=avpicture_get_size(PIX_FMT_RGBA, w, h);
      unsigned char* buffer = malloc(numBytes);

      // Assign appropriate parts of buffer to image planes in pFrameRGB
      avpicture_fill((AVPicture *)frameRGB, buffer, PIX_FMT_RGBA, w, h);
    
      struct SwsContext *img_convert_ctx = sws_alloc_context();
      img_convert_ctx = sws_getCachedContext(
        img_convert_ctx, 
        w, h, videoStream->codec->pix_fmt, 
        w, h, PIX_FMT_RGBA, 
        SWS_BICUBIC, NULL, NULL, NULL);

       if (img_convert_ctx == NULL) {
        fprintf(stderr, "Cannot initialize the conversion context\n");  
        avcodec_free_frame(&frame);  
        avcodec_free_frame(&frameRGB);
        goto EXIT;
       }
       
      sws_scale(img_convert_ctx, 
       (const unsigned char*const*)frame->data, frame->linesize, 0, videoStream->codec->height,
       frameRGB->data, frameRGB->linesize);
    
      im->data = malloc(4*w*h);
      int j;
      for (i=0; i<w; ++i) {
        for (j=0; j<h; ++j) {
          int index = 4*w*j+4*i;
          im->data[w*j+i] = 
            (buffer[index+3]<<24)+
            (buffer[index]<<16) + 
            (buffer[index+1]<<8) + 
            (buffer[index+2]<<0);
        }
      }
      
      free(buffer);
      
      retret = 1;
      if(progress)
        progress(im, 100, 0, 0, 0, 0);
    }
    
    avcodec_free_frame(&frame); 
    avcodec_free_frame(&frameRGB);
    
  }

  

EXIT:
  avformat_close_input(&pFormatCtx);
  if(!retret) {
    fprintf(stderr, "R\n");
    im->w = 0;
    im->h = 0;
  }
  return retret;
}

char save(ImlibImage *im, ImlibProgressFunction progress,
          char progress_granularity)
{
  return 0;
}

void formats(ImlibLoader *l)
{
  int i;
  char *list_formats[] = { "ffmpeg" };

  l->num_formats = (sizeof(list_formats) / sizeof(char *));
  l->formats     = malloc(sizeof(char *) * l->num_formats);
  for(i = 0 ; i < l->num_formats ; i++)
    l->formats[i] = strdup(list_formats[i]);
}
