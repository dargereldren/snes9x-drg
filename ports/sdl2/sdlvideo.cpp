/***********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  See CREDITS file to find the copyright owners of this file.

  SDL Input/Audio/Video code (many lines of code come from snes9x & drnoksnes)
  The code conversion to SDL2 was done by https://github.com/darkxex/
  (c) Copyright 2011         Makoto Sugano (makoto.sugano@gmail.com)

  Snes9x homepage: http://www.snes9x.com/

  Permission to use, copy, modify and/or distribute Snes9x in both binary
  and source form, for non-commercial purposes, is hereby granted without
  fee, providing that this license information and copyright notice appear
  with all copies and any derived work.

  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software or it's derivatives.

  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes,
  but is not limited to, charging money for Snes9x or software derived from
  Snes9x, including Snes9x or derivatives in commercial game bundles, and/or
  using Snes9x as a promotion for your commercial product.

  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.

  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
 ***********************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "snes9x.h"
#include "memmap.h"
#include "ppu.h"
#include "controls.h"
#include "movie.h"
#include "conffile.h"
#include "display.h"
#include "iostream"
#include "sdl_snes9x.h"

// markus

struct GUIData GUI;

// markus
#ifdef __MORPHOS__
#define SDL_FULLSCREEN SDL_WINDOW_FULLSCREEN
#else
#define SDL_FULLSCREEN SDL_WINDOW_FULLSCREEN_DESKTOP
#endif
uint32 videoFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
SDL_Rect sdl_sizescreen;
char *dropped_filedir = NULL; // Pointer for directory of dropped file
//

typedef void (*Blitter)(uint8 *, int, uint8 *, int, int, int);

#ifdef __linux
// Select seems to be broken in 2.x.x kernels - if a signal interrupts a
// select system call with a zero timeout, the select call is restarted but
// with an infinite timeout! The call will block until data arrives on the
// selected fd(s).
//
// The workaround is to stop the X library calling select in the first
// place! Replace XPending - which polls for data from the X server using
// select - with an ioctl call to poll for data and then only call the blocking
// XNextEvent if data is waiting.
#define SELECT_BROKEN_FOR_SIGNALS
#endif

static void SetupImage(void);
static void TakedownImage(void);

void S9xExtraDisplayUsage(void) {
#ifndef SDL_DROP
#ifdef AOS4QEMU
	S9xMessage(S9X_INFO, S9X_USAGE, "-fullscreen                     fullscreen mode (without scaling)");
#else
	S9xMessage(S9X_INFO, S9X_USAGE, "-fullscreen                     fullscreen mode");
#endif
	S9xMessage(S9X_INFO, S9X_USAGE, "");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v1                             Video mode: Blocky (default)");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v2                             Video mode: TV");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v3                             Video mode: Smooth");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v4                             Video mode: SuperEagle");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v5                             Video mode: 2xSaI");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v6                             Video mode: Super2xSaI");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v7                             Video mode: EPX");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v8                             Video mode: hq2x");
	S9xMessage(S9X_INFO, S9X_USAGE, "");
#endif
}

void S9xParseDisplayArg(char **argv, int &i, int argc) {
	if (!strncasecmp(argv[i], "-fullscreen", 11)) {
		GUI.fullscreen = TRUE;
		printf("Entering fullscreen mode (without scaling).\n");
	} else if (!strncasecmp(argv[i], "-v", 2)) {
		switch (argv[i][2]) {
		case '1': GUI.video_mode = VIDEOMODE_BLOCKY; break;
		case '2': GUI.video_mode = VIDEOMODE_TV; break;
		case '3': GUI.video_mode = VIDEOMODE_SMOOTH; break;
		case '4': GUI.video_mode = VIDEOMODE_SUPEREAGLE; break;
		case '5': GUI.video_mode = VIDEOMODE_2XSAI; break;
		case '6': GUI.video_mode = VIDEOMODE_SUPER2XSAI; break;
		case '7': GUI.video_mode = VIDEOMODE_EPX; break;
		case '8': GUI.video_mode = VIDEOMODE_HQ2X; break;
		}
	} else {
		S9xUsage();
	}
}

const char *S9xParseDisplayConfig(ConfigFile &conf, int pass) {
	if (pass != 1) {
		return ("Unix/SDL");
	}

	if (conf.Exists("Unix/SDL::VideoMode")) {
		GUI.video_mode = conf.GetUInt("Unix/SDL::VideoMode", VIDEOMODE_BLOCKY);
		if (GUI.video_mode < 1 || GUI.video_mode > 8) {
			GUI.video_mode = VIDEOMODE_BLOCKY;
		}
	} else {
		GUI.video_mode = VIDEOMODE_BLOCKY;
	}

	return ("Unix/SDL");
}

static void FatalError(const char *str) {
#ifndef SDL_DROP
	fprintf(stderr, "%s\n", str);
#endif
	S9xExit();
}

void S9xInitDisplay(int argc, char **argv) {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		printf("Unable to initialize SDL: %s\n", SDL_GetError());
	}

	atexit(SDL_Quit);

	/*
	 * domaemon
	 *
	 * FIXME: The secreen size should be flexible
	 * FIXME: Check if the SDL screen is really in RGB565 mode. screen->fmt
	 */
	if (GUI.fullscreen == TRUE) {
		videoFlags |= SDL_FULLSCREEN;
		SDL_ShowCursor(SDL_FALSE);
	}

	sdl_sizescreen.w = (SNES_WIDTH * 2);
	sdl_sizescreen.h = (SNES_HEIGHT_EXTENDED * 2);
	sdl_sizescreen.y = 0;
	sdl_sizescreen.x = 0;

	GUI.pixels = (uint32 *)calloc(1, (sdl_sizescreen.w * sdl_sizescreen.h) * sizeof(uint32));

	GUI.sdl_window = SDL_CreateWindow("Snes9x for SDL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, sdl_sizescreen.w, sdl_sizescreen.h, videoFlags);
	GUI.sdl_renderer = SDL_CreateRenderer(GUI.sdl_window, -1, SDL_RENDERER_ACCELERATED);
	if (!GUI.sdl_renderer) {
		GUI.sdl_renderer = SDL_CreateRenderer(GUI.sdl_window, -1, SDL_RENDERER_SOFTWARE);
	}

	// scaling hint - before texture creation
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

	GUI.sdl_texture = SDL_CreateTexture(GUI.sdl_renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, sdl_sizescreen.w, sdl_sizescreen.h);

	SDL_RenderSetLogicalSize(GUI.sdl_renderer, sdl_sizescreen.w, SNES_HEIGHT * 2);
	SDL_RenderClear(GUI.sdl_renderer);
	/*
	 * domaemon
	 *
	 * buffer allocation, quite important
	 */
	SetupImage();
}

/*
 * toggles fullscreen.
 */
void S9xSDL_toggleFullscreen(void) {

	if (videoFlags & SDL_FULLSCREEN) {
		videoFlags &= ~SDL_FULLSCREEN;

		SDL_SetWindowFullscreen(GUI.sdl_window, videoFlags);
		SDL_ShowCursor(SDL_ENABLE);
	} else {
		videoFlags |= SDL_FULLSCREEN;
		SDL_SetWindowFullscreen(GUI.sdl_window, videoFlags);
		SDL_ShowCursor(SDL_DISABLE);
	}
#ifdef __MORPHOS__
	SDL_RenderClear(GUI.sdl_renderer);
#endif
}

static void TakedownImage(void) {

	if (GUI.snes_buffer) {
		free(GUI.snes_buffer);
		GUI.snes_buffer = NULL;
	}

	S9xGraphicsDeinit();
}

void S9xDeinitDisplay(void) {
	TakedownImage();
	// markus
	free(GUI.pixels);
	GUI.pixels = NULL;
#ifdef __MORPHOS__
	if (GUI.sdl_texture) {
		SDL_DestroyTexture(GUI.sdl_texture);
	}

	if (GUI.sdl_renderer) {
		SDL_DestroyRenderer(GUI.sdl_renderer);
	}
#endif

	SDL_DestroyWindow(GUI.sdl_window);
	SDL_Quit();
}

static void SetupImage(void) {
	TakedownImage();

	// domaemon: The whole unix code basically assumes output=(original * 2);
	// This way the code can handle the SNES filters, which does the 2X.
	// GFX.Pitch = SNES_WIDTH * 2 * 2;
	GUI.snes_buffer = (uint8 *)calloc(GFX.Pitch * ((SNES_HEIGHT_EXTENDED + 4) * 2), 1);
	if (!GUI.snes_buffer) {
		FatalError("Failed to allocate GUI.snes_buffer.");
	}

	// domaemon: Add 2 lines before drawing.
	GFX.Screen = (uint16 *)(GUI.snes_buffer + (GFX.Pitch * 2 * 2));
	GUI.blit_screen = (uint8 *)GUI.pixels;
	GUI.blit_screen_pitch = SNES_WIDTH * 2 * 2; // window size =(*2); 2 byte pir pixel =(*2)

	S9xGraphicsInit();
}

void S9xPutImage(int width, int height) {
	static int prevWidth = 0, prevHeight = 0;

	// domaemon: does the height change on the fly?
	if (height < prevHeight) {
		int p = GUI.blit_screen_pitch >> 2;
		for (int y = SNES_HEIGHT * 2; y < SNES_HEIGHT_EXTENDED * 2; y++) {
			uint32 *d = (uint32 *)(GUI.blit_screen + y * GUI.blit_screen_pitch);
			for (int x = 0; x < p; x++) {
				*d++ = 0;
			}
		}
	}

	SDL_UpdateTexture(GUI.sdl_texture, NULL, GUI.blit_screen, GUI.blit_screen_pitch);
	SDL_RenderCopy(GUI.sdl_renderer, GUI.sdl_texture, NULL, &sdl_sizescreen);
	SDL_RenderPresent(GUI.sdl_renderer);

	prevWidth = width;
	prevHeight = height;
}

void S9xMessage(int type, int number, const char *message) {
	const int max = 36 * 3;
	static char buffer[max + 1];

	fprintf(stdout, "%s\n", message);

	strncpy(buffer, message, max + 1);
	buffer[max] = 0;
	S9xSetInfoString(buffer);
}

const char *S9xStringInput(const char *message) {
	static char buffer[256];

	printf("%s: ", message);
	fflush(stdout);

	if (fgets(buffer, sizeof(buffer) - 2, stdin)) {
		return (buffer);
	}

	return (NULL);
}

void S9xSetTitle(const char *string) {
	SDL_SetWindowTitle(GUI.sdl_window, string);
}