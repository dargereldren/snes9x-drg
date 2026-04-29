#include <SDL2/SDL.h>
#include "port.h"
#include "conffile.h"
typedef std::pair<std::string, std::string> strpair_t;
extern ConfigFile::secvec_t keymaps;

struct GUIData {
	SDL_Renderer *sdl_renderer;
	SDL_Texture *sdl_texture;
	SDL_Window *sdl_window;
	uint32 *pixels;
	uint8 *snes_buffer;
	uint8 *blit_screen;
	uint32 blit_screen_pitch;
	int video_mode;
	bool8 fullscreen;
};

enum {
	VIDEOMODE_BLOCKY = 1,
	VIDEOMODE_TV,
	VIDEOMODE_SMOOTH,
	VIDEOMODE_SUPEREAGLE,
	VIDEOMODE_2XSAI,
	VIDEOMODE_SUPER2XSAI,
	VIDEOMODE_EPX,
	VIDEOMODE_HQ2X
};

extern bool8 S9xCloseSoundDevice();
