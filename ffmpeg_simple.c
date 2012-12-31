#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include "ffmpeg_simple.h"

struct ffmpeg_simple_data {
    AVFormatContext *pFormatCtx;
    AVStream* videoStream;
    int videoStreamIndex;
    AVCodec* pCodec;
    AVPacket packet;
    AVFrame* frame;
    AVFrame* frameRGB;
    struct SwsContext *img_convert_ctx;
};

void init_ffmpeg_simple_data (struct ffmpeg_simple_data *d) {
    d->pFormatCtx = NULL;
    d->videoStream = NULL;
    d->videoStreamIndex = -1;
    d->pCodec = NULL;
    d->frame = NULL;
    d->frameRGB = NULL;
    d->img_convert_ctx = NULL;
    av_init_packet(&d->packet);
}

void cleanup_ffmpeg_simple_data (struct ffmpeg_simple_data *d) {
    sws_freeContext(d->img_convert_ctx);
    av_free_packet(&d->packet);
    avcodec_free_frame(&d->frame);  
    avcodec_free_frame(&d->frameRGB);
    if (d->videoStream) {
        avcodec_close(d->videoStream->codec);
    }
    if (d->pFormatCtx) {
        avformat_close_input(&d->pFormatCtx);
    }
}

unsigned long* ffmpegsimple_readfirstframe_impl(
        struct ffmpeg_simple_data *d
        ,const char* filename
        ,int *width
        ,int *height
        ) {
            
    
    if (avformat_open_input(&d->pFormatCtx, filename, NULL, NULL)!=0) {
        fprintf(stderr, "av_open_input_file failed\n");
        return NULL;
    }

    if(avformat_find_stream_info(d->pFormatCtx, NULL)<0) {
        fprintf(stderr, "avformat_find_stream_info failed\n");
        return NULL;
    }    
    
    int i;
    for(i=0; i < d->pFormatCtx->nb_streams; i++) {
        AVStream *r = d->pFormatCtx->streams[i];
        if(r->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (!d->videoStream) {
                d->videoStream  = r;
                d->videoStreamIndex = i;
            } else {
                fprintf(stderr, "Warning: ignoring other video streams\n");
            }
        }
    }
    if (!d->videoStream) {
        fprintf(stderr, "No video streams found\n");
        return NULL;
    }
    
    d->pCodec = avcodec_find_decoder(d->videoStream->codec->codec_id);
    if (d->pCodec==NULL) {
        fprintf(stderr, "Can't find the codec\n");
        return NULL;
    }

    if(avcodec_open2(d->videoStream->codec, d->pCodec, NULL)<0){
        fprintf(stderr, "Can't open the codec\n");
        return NULL;
    }
    
    int packet_limit = 256;
    int no_more_frames_flag = 0;
    int read_at_least_one_packet = 0;
    int read_at_least_one_our_packet = 0;
    
    for(;;) {
        if (!no_more_frames_flag) {
            for(;;) {
                int ret = av_read_frame(d->pFormatCtx, &d->packet);
                if (ret<0) {
                    no_more_frames_flag = 1;
                    break;
                }
                read_at_least_one_packet = 1;
                if (d->packet.stream_index == d->videoStream->index) {
                    read_at_least_one_our_packet = 1;
                    break; // OK, it's our packet;
                }
                if (!--packet_limit) {
                    fprintf(stderr, "Packet limit expired while reading more packets\n");
                    return NULL;
                }
            }
        }
        
        if (no_more_frames_flag && !read_at_least_one_our_packet) {
            if (read_at_least_one_packet) {
                fprintf(stderr, "Can't read any packet for our track\n");
            } else {                
                fprintf(stderr, "Can't read any packets\n");
            }
            return NULL;
        }
        
        d->frame = avcodec_alloc_frame();
        d->frameRGB = avcodec_alloc_frame();
        
        int isFrameAvailable;
        int ret = avcodec_decode_video2(d->videoStream->codec, d->frame, &isFrameAvailable, &d->packet);
        if (ret < 0) {
            fprintf(stderr, "avcodec_decode_video2 failed\n");
            return NULL;
        }

        if (isFrameAvailable) {
            break;
        }
        
        if (!--packet_limit) {
            fprintf(stderr, "Decoding tries limit expired\n");
            return NULL;
        }
    }
    
    int w = d->frame->width;
    int h = d->frame->height;
    
    if (width ) *width  = w;
    if (height) *height = h;
        
    
    int numBytes=avpicture_get_size(PIX_FMT_BGRA, w, h);
 
    
    d->img_convert_ctx = sws_alloc_context();
    d->img_convert_ctx = sws_getCachedContext(
        d->img_convert_ctx, 
        w, h, d->videoStream->codec->pix_fmt, 
        w, h, PIX_FMT_BGRA, 
        SWS_BICUBIC, NULL, NULL, NULL);
    
    if (d->img_convert_ctx == NULL) {
        fprintf(stderr, "Cannot initialize the conversion context\n");  
        return NULL;
    }
    
    
    unsigned char* buffer = malloc(numBytes);
    
    if (!buffer) {
        fprintf(stderr, "Cannot allocate picture buffer\n");
        return NULL;
    }
    
    // Assign appropriate parts of buffer to image planes in pFrameRGB
    avpicture_fill((AVPicture *)d->frameRGB, buffer, PIX_FMT_BGRA, w, h);
    
    
    sws_scale(d->img_convert_ctx, 
        (const unsigned char*const*)d->frame->data, 
        d->frame->linesize, 0, d->videoStream->codec->height,
        d->frameRGB->data, d->frameRGB->linesize);
    
    return (unsigned long*) buffer;
}

unsigned long* ffmpegsimple_readfirstframe(const char* filename, int *width, int *height) {
    struct ffmpeg_simple_data data;
    
    avcodec_register_all();
    av_register_all();
    
    init_ffmpeg_simple_data(&data);
    
    unsigned long* ptr = ffmpegsimple_readfirstframe_impl(
            &data
            ,filename
            ,width
            ,height);
    
    cleanup_ffmpeg_simple_data(&data);
    
    return ptr;
}