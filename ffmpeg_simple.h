#pragma once

/* 
 * Open the the file with FFmpeg, read the first video frame, convert it to RGBA image.
 * 
 * returns malloc'ed buffer with the RGBA image on success and NULL on failure
 */
unsigned long* ffmpegsimple_readfirstframe(const char* filename, int *width, int *height);