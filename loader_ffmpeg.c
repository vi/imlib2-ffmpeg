/* File: loader_ffmpeh.c
   Time-stamp: <2012-12-09 21:19:30 gawen>

   Copyright (c) 2011 David Hauweele <david@hauweele.net>
   All rights reserved.
   
   Modified by Vitaly "_Vi" Shukela to use ffmpeg instead of webp.

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

#include "ffmpeg_simple.h"

#include <Imlib2.h>

#include "imlib2_common.h"
#include "loader.h"


char load(ImlibImage * im, ImlibProgressFunction progress,
          char progress_granularity, char immediate_load)
{
  
  int w,h;
  
  if(im->data)
    return 0;

  unsigned long* decoded_image = ffmpegsimple_readfirstframe(
      im->real_file, &w, &h);
  int decoded_image_used = 0;
  
  if (!decoded_image) return 0;
  
  if(!im->loader && !im->data) {
    im->w = w;
    im->h = h;

    if(!IMAGE_DIMENSIONS_OK(w, h))
      goto EXIT;

    SET_FLAGS(im->flags, F_HAS_ALPHA);
    
    im->format = strdup("ffmpeg");
  }

  if((!im->data && im->loader) || immediate_load || progress) {
    im->data = (DATA32*)decoded_image;
    decoded_image_used = 1;
    if(progress)
      progress(im, 100, 0, 0, w, h);
    return 1;
  }

  

EXIT:
  if (!decoded_image_used) {
    free(decoded_image);
  }
  return 0;
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
