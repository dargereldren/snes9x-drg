/*****************************************************************************\
	 Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
				This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#pragma once
#include "gtk_s9x.h"
#include "gtk_display_driver.h"

enum {
	NTSC_COMPOSITE = 0,
	NTSC_SVIDEO = 1,
	NTSC_RGB = 2
};

struct S9xRect {
	int x;
	int y;
	int w;
	int h;
};

void S9xRegisterYUVTables(uint8 *y, uint8 *u, uint8 *v);
double S9xGetAspect();
S9xRect S9xApplyAspect(int, int, int, int);
void S9xConvertYUV(void *src_buffer, void *dst_buffer, int src_pitch, int dst_pitch, int width, int height);
void S9xConvert(void *src, void *dst, int src_pitch, int dst_pitch, int width, int height, int bpp);
void S9xConvertMask(void *src, void *dst, int src_pitch, int dst_pitch, int width, int height, int rshift, int gshift, int bshift, int bpp);
void S9xDisplayRefresh();
void S9xReinitDisplay();
void S9xDisplayReconfigure();
void S9xQueryDrivers();
S9xDisplayDriver *S9xDisplayGetDriver();
bool S9xDisplayDriverIsReady();