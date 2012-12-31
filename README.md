Suppose you have a one-frame video file

    ffmpeg -i picture.ppm -vcodec libx264 -preset placebo -tune stillimage -crf 25 -y picture.mkv
    
and want to view it.

Here is a imlib2 plugin that uses ffmpeg to 
decode the first frame of the given video.


[imlib2-webp](http://www.hauweele.net/~gawen/imlib2-webp.html) was used as a imlib2 loader sample.


Little bonus: simple ffmpeg usage function:

    /* 
     * Open the the file with FFmpeg, read the first video frame, convert it to RGBA image.
     * 
     * returns malloc'ed buffer with the RGBA image on success and NULL on failure
     */
    unsigned long* ffmpegsimple_readfirstframe(const char* filename, int *width, int *height);