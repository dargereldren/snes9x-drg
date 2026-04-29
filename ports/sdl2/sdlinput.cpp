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

#include "sdl_snes9x.h"

#include "snes9x.h"
#include "port.h"
#include "controls.h"

extern struct GUIData GUI;

using namespace std;
std::map<string, int> name_sdlkeysym;

int mynumid;

ConfigFile::secvec_t keymaps;

s9xcommand_t S9xGetDisplayCommandT(const char *n) {
	s9xcommand_t cmd;

	cmd.type = S9xBadMapping;
	cmd.multi_press = 0;
	cmd.button_norpt = 0;
	cmd.port[0] = 0xff;
	cmd.port[1] = 0;
	cmd.port[2] = 0;
	cmd.port[3] = 0;

	return (cmd);
}

char *S9xGetDisplayCommandName(s9xcommand_t cmd) {
	return (strdup("None"));
}

void S9xHandleDisplayCommand(s9xcommand_t cmd, int16 data1, int16 data2) {
	return;
}

// markus
extern void S9xSDL_toggleFullscreen(void);
extern void S9xReset(void);
extern void SNES9X_Reset_DROP(char *drop);
//

// domaemon: 2) here we send the keymapping request to the SNES9X
// domaemon: MapInput (J, K, M)
bool8 S9xMapInput(const char *n, s9xcommand_t *cmd) {
	int i, j, d;
	char *c;

	// domaemon: linking PseudoPointer# and command
	if (!strncmp(n, "PseudoPointer", 13)) {
		if (n[13] >= '1' && n[13] <= '8' && n[14] == '\0') {
			return (S9xMapPointer(PseudoPointerBase + (n[13] - '1'), *cmd, false));
		} else {
			goto unrecog;
		}
	}

	// domaemon: linking PseudoButton# and command
	if (!strncmp(n, "PseudoButton", 12)) {
		if (isdigit(n[12]) && (j = strtol(n + 12, &c, 10)) < 256 && (c == NULL || *c == '\0')) {
			return (S9xMapButton(PseudoButtonBase + j, *cmd, false));
		} else {
			goto unrecog;
		}
	}

	if (!(isdigit(n[1]) && isdigit(n[2]) && n[3] == ':')) {
		goto unrecog;
	}

	switch (n[0]) {
	case 'J': // domaemon: joysticks input mapping
	{

		d = ((n[1] - '0') * 10 + (n[2] - '0')) << 24;
		d |= 0x80000000;
		i = 4;

		if (!strncmp(n + i, "Axis", 4)) // domaemon: joystick axis
		{
			d |= 0x8000; // Axis mode
			i += 4;
		} else if (n[i] == 'B') // domaemon: joystick button
		{
			i++;
		} else {
			goto unrecog;
		}

		d |= j = strtol(n + i, &c, 10); // Axis or Button id
		if ((c != NULL && *c != '\0') || j > 0x3fff) {
			goto unrecog;
		}

		if (d & 0x8000) {
			return (S9xMapAxis(d, *cmd, false));
		}

		return (S9xMapButton(d, *cmd, false));
	}

	case 'K': {
		d = 0x00000000;

		for (i = 4; n[i] != '\0' && n[i] != '+'; i++)
			;

		if (n[i] == '\0' || i == 4) {
			// domaemon: if no mod keys are found.
			i = 4;
		} else {
			// domaemon: mod keys are not supported now.
			goto unrecog;
		}

		string keyname(n + i); // domaemon: SDL_keysym in string format.

		d |= name_sdlkeysym[keyname];
		return (S9xMapButton(d, *cmd, false));
	}

	case 'M': {
		d = 0x40000000;

		if (!strncmp(n + 4, "Pointer", 7)) {
			d |= 0x8000;

			if (n[11] == '\0') {
				return (S9xMapPointer(d, *cmd, true));
			}

			i = 11;
		} else if (n[4] == 'B') {
			i = 5;
		} else {
			goto unrecog;
		}

		d |= j = strtol(n + i, &c, 10);

		if ((c != NULL && *c != '\0') || j > 0x7fff) {
			goto unrecog;
		}

		if (d & 0x8000) {
			return (S9xMapPointer(d, *cmd, true));
		}

		return (S9xMapButton(d, *cmd, false));
	}

	default:
		break;
	}

unrecog:
	char *err = new char[strlen(n) + 34];

	sprintf(err, "Unrecognized input device name '%s'", n);
	perror(err);
	delete[] err;

	return (false);
}

// domaemon: SetupDefaultKeymap -> MapInput (JS) -> MapDisplayInput (KB)

char *S9xGetPortCommandName(s9xcommand_t cmd) {
	std::string x;

	switch (cmd.type) {
	case S9xButtonPort:
		if (cmd.port[0] != 0) {
			break;
		}

		switch (cmd.port[1]) {
		case 0:
			x = "JS";
			x += (int)cmd.port[2];
			x += " Meta";
			x += (int)cmd.port[3];
			return (strdup(x.c_str()));

		case 1:
			x = "JS";
			x += (int)cmd.port[2];
			x += " ToggleMeta";
			x += (int)cmd.port[3];
			return (strdup(x.c_str()));

		case 2:
			return (strdup("Rewind"));

		case 3:
			return (strdup("Advance"));
		}

		break;

	case S9xAxisPort:
		break;

	case S9xPointerPort:
		break;
	}

	return (S9xGetDisplayCommandName(cmd));
}

s9xcommand_t S9xGetPortCommandT(const char *n) {
	s9xcommand_t cmd;

	cmd.type = S9xBadMapping;
	cmd.multi_press = 0;
	cmd.button_norpt = 0;
	cmd.port[0] = 0;
	cmd.port[1] = 0;
	cmd.port[2] = 0;
	cmd.port[3] = 0;

	if (!strncmp(n, "JS", 2) && n[2] >= '1' && n[2] <= '8') {
		if (!strncmp(n + 3, " Meta", 5) && n[8] >= '1' && n[8] <= '8' && n[9] == '\0') {
			cmd.type = S9xButtonPort;
			cmd.port[1] = 0;
			cmd.port[2] = n[2] - '1';
			cmd.port[3] = 1 << (n[8] - '1');

			return (cmd);
		} else if (!strncmp(n + 3, " ToggleMeta", 11) && n[14] >= '1' && n[14] <= '8' && n[15] == '\0') {
			cmd.type = S9xButtonPort;
			cmd.port[1] = 1;
			cmd.port[2] = n[2] - '1';
			cmd.port[3] = 1 << (n[13] - '1');

			return (cmd);
		}
	} else if (!strcmp(n, "Rewind")) {
		cmd.type = S9xButtonPort;
		cmd.port[1] = 2;

		return (cmd);
	} else if (!strcmp(n, "Advance")) {
		cmd.type = S9xButtonPort;
		cmd.port[1] = 3;
		return (cmd);
	}

	return (S9xGetDisplayCommandT(n));
}

void S9xSetupDefaultKeymap(void) {
	s9xcommand_t cmd;

	S9xUnmapAllControls();

	for (ConfigFile::secvec_t::iterator i = keymaps.begin(); i != keymaps.end(); i++) {
		cmd = S9xGetPortCommandT(i->second.c_str());

		if (cmd.type == S9xBadMapping) {
			cmd = S9xGetCommandT(i->second.c_str());
			if (cmd.type == S9xBadMapping) {
				/*std::string	s("Unrecognized command '");
				s += i->second + "'";
				perror(s.c_str());*/
				continue;
			}
		}

		if (!S9xMapInput(i->first.c_str(), &cmd)) {
			/*std::string	s("Could not map '");
			s += i->second + "' to '" + i->first + "'";
			perror(s.c_str());*/
			continue;
		}
	}

	keymaps.clear();
}

// domaemon: FIXME, just collecting the essentials.
// domaemon: *) here we define the keymapping.
void S9xParseInputConfig(ConfigFile &conf, int pass) {
	keymaps.clear();
	if (!conf.GetBool("Unix::ClearAllControls", false)) {
		// Using 'Joypad# Axis'
		keymaps.push_back(strpair_t("J00:Axis0", "Joypad1 Axis Left/Right T=50%"));
		keymaps.push_back(strpair_t("J00:Axis1", "Joypad1 Axis Up/Down T=50%"));

		keymaps.push_back(strpair_t("J00:B0", "Joypad1 X"));
		keymaps.push_back(strpair_t("J00:B1", "Joypad1 A"));
		keymaps.push_back(strpair_t("J00:B2", "Joypad1 B"));
		keymaps.push_back(strpair_t("J00:B3", "Joypad1 Y"));

		keymaps.push_back(strpair_t("J00:B6", "Joypad1 L"));
		keymaps.push_back(strpair_t("J00:B7", "Joypad1 R"));
		keymaps.push_back(strpair_t("J00:B8", "Joypad1 Select"));
		keymaps.push_back(strpair_t("J00:B11", "Joypad1 Start"));

		keymaps.push_back(strpair_t("J01:Axis0", "Joypad2 Axis Left/Right T=50%"));
		keymaps.push_back(strpair_t("J01:Axis1", "Joypad2 Axis Up/Down T=50%"));

		keymaps.push_back(strpair_t("J01:B0", "Joypad2 X"));
		keymaps.push_back(strpair_t("J01:B1", "Joypad2 A"));
		keymaps.push_back(strpair_t("J01:B2", "Joypad2 B"));
		keymaps.push_back(strpair_t("J01:B3", "Joypad2 Y"));

		keymaps.push_back(strpair_t("J01:B6", "Joypad2 L"));
		keymaps.push_back(strpair_t("J01:B7", "Joypad2 R"));
		keymaps.push_back(strpair_t("J01:B8", "Joypad2 Select"));
		keymaps.push_back(strpair_t("J01:B11", "Joypad2 Start"));

		keymaps.push_back(strpair_t("K00:SDLK_RIGHT", "Joypad1 Right"));
		keymaps.push_back(strpair_t("K00:SDLK_LEFT", "Joypad1 Left"));
		keymaps.push_back(strpair_t("K00:SDLK_DOWN", "Joypad1 Down"));
		keymaps.push_back(strpair_t("K00:SDLK_UP", "Joypad1 Up"));
		keymaps.push_back(strpair_t("K00:SDLK_RETURN", "Joypad1 Start"));
		keymaps.push_back(strpair_t("K00:SDLK_SPACE", "Joypad1 Select"));
		keymaps.push_back(strpair_t("K00:SDLK_d", "Joypad1 A"));
		keymaps.push_back(strpair_t("K00:SDLK_c", "Joypad1 B"));
		keymaps.push_back(strpair_t("K00:SDLK_s", "Joypad1 X"));
		keymaps.push_back(strpair_t("K00:SDLK_x", "Joypad1 Y"));
		keymaps.push_back(strpair_t("K00:SDLK_a", "Joypad1 L"));
		keymaps.push_back(strpair_t("K00:SDLK_z", "Joypad1 R"));

		// New key support by BeWorld
		keymaps.push_back(strpair_t("K00:SDLK_DELETE", "Screenshot"));
		keymaps.push_back(strpair_t("K00:SDLK_TAB", "EmuTurbo"));
		keymaps.push_back(strpair_t("K00:SDLK_ESCAPE", "ExitEmu"));
		keymaps.push_back(strpair_t("K00:SDLK_q", "ExitEmu"));

		keymaps.push_back(strpair_t("K00:SDLK_1", "ToggleBG0"));
		keymaps.push_back(strpair_t("K00:SDLK_2", "ToggleBG1"));
		keymaps.push_back(strpair_t("K00:SDLK_3", "ToggleBG2"));
		keymaps.push_back(strpair_t("K00:SDLK_4", "ToggleBG3"));
		keymaps.push_back(strpair_t("K00:SDLK_5", "ToggleSprites"));
		keymaps.push_back(strpair_t("K00:SDLK_9", "ToggleTransparency"));
		keymaps.push_back(strpair_t("K00:SDLK_BACKSPACE", "ClipWindows"));

		keymaps.push_back(strpair_t("K00:SDLK_F1", "QuickLoad000"));
		keymaps.push_back(strpair_t("K00:SDLK_F2", "QuickSave000"));

		// domaemon: *) GetSDLKeyFromName().
		name_sdlkeysym["SDLK_s"] = SDLK_s;
		name_sdlkeysym["SDLK_d"] = SDLK_d;
		name_sdlkeysym["SDLK_x"] = SDLK_x;
		name_sdlkeysym["SDLK_c"] = SDLK_c;
		name_sdlkeysym["SDLK_a"] = SDLK_a;
		name_sdlkeysym["SDLK_z"] = SDLK_z;
		name_sdlkeysym["SDLK_UP"] = SDLK_UP;
		name_sdlkeysym["SDLK_DOWN"] = SDLK_DOWN;
		name_sdlkeysym["SDLK_RIGHT"] = SDLK_RIGHT;
		name_sdlkeysym["SDLK_LEFT"] = SDLK_LEFT;
		name_sdlkeysym["SDLK_RETURN"] = SDLK_RETURN;
		name_sdlkeysym["SDLK_SPACE"] = SDLK_SPACE;
		name_sdlkeysym["SDLK_TAB"] = SDLK_TAB;
		name_sdlkeysym["SDLK_DELETE"] = SDLK_DELETE;
		name_sdlkeysym["SDLK_ESCAPE"] = SDLK_ESCAPE;
		name_sdlkeysym["SDLK_q"] = SDLK_s;

		name_sdlkeysym["SDLK_1"] = SDLK_1;
		name_sdlkeysym["SDLK_2"] = SDLK_2;
		name_sdlkeysym["SDLK_3"] = SDLK_3;
		name_sdlkeysym["SDLK_4"] = SDLK_4;
		name_sdlkeysym["SDLK_5"] = SDLK_5;
		name_sdlkeysym["SDLK_9"] = SDLK_9;
		name_sdlkeysym["SDLK_BACKSPACE"] = SDLK_BACKSPACE;

		name_sdlkeysym["SDLK_F1"] = SDLK_F1;
		name_sdlkeysym["SDLK_F2"] = SDLK_F2;
	}

	return;
}

SDL_Joystick *joystick[4] = {NULL, NULL, NULL, NULL};
SDL_GameController *gamecontroller[4] = {NULL, NULL, NULL, NULL};

void S9xInitInputDevices(void) {

	// domaemon: 1) initializing the joystic subsystem
	SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);

	/*
	 * domaemon: 2) check how may joysticks are connected
	 * domaemon: 3) relate paddev1 to SDL_Joystick[0], paddev2 to SDL_Joystick[1]...
	 * domaemon: 4) print out the joystick name and capabilities
	 */

	int num_joysticks = SDL_NumJoysticks();

	if (num_joysticks > 0) {
		SDL_JoystickEventState(SDL_ENABLE);

		for (int i = 0; i < num_joysticks; i++) {
			if (num_joysticks > 3) {
				break;
			}

			if (SDL_IsGameController(i)) {

				// This is a game controller - try opening that way
				gamecontroller[i] = SDL_GameControllerOpen(i);
				if (gamecontroller[i]) {
					printf("[%d] game controller %s\n", i, SDL_GameControllerName(gamecontroller[i]));
					// joystick[i] = SDL_GameControllerGetJoystick(gamecontroller[i]);
				} else {
					joystick[i] = SDL_JoystickOpen(i);
					// printf ("SDL use joystick %d-axis %d-buttons %d-balls %d-hats \n",
					SDL_JoystickNumAxes(joystick[i]),
						SDL_JoystickNumButtons(joystick[i]),
						SDL_JoystickNumBalls(joystick[i]),
						SDL_JoystickNumHats(joystick[i]);
				}
			} else {
				joystick[i] = SDL_JoystickOpen(i);
				// printf ("SDL use joystick %d-axis %d-buttons %d-balls %d-hats \n",
				SDL_JoystickNumAxes(joystick[i]),
					SDL_JoystickNumButtons(joystick[i]),
					SDL_JoystickNumBalls(joystick[i]),
					SDL_JoystickNumHats(joystick[i]);
			}
			S9xSetController(i, CTL_JOYPAD, i, 0, 0, 0);
		}
	}
}

void S9xProcessEvents(bool8 block) {
	SDL_Event event;
	bool8 quit_state = FALSE;

	// markus drop
	char *dropped_filedir;
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	//
	while ((block) || (SDL_PollEvent(&event) != 0)) {
		switch (event.type) {
		case SDL_DROPFILE: {

			const char *extensions[] = {
				".smc",
				".SMC",
				".fig",
				".FIG",
				".sfc",
				".SFC",
				".jma",
				".JMA",
				".zip",
				".ZIP",
				".gd3",
				".GD3",
				".swc",
				".SWC",
				".gz",
				".GZ",
				".bs",
				".BS",
				NULL};
			char *ext;
			SDL_bool bingo = SDL_FALSE;

			// check extension
			ext = strrchr(event.drop.file, '.');
			// printf("Dropfile %s\n", event.drop.file);
			if (ext) {
				for (int i = 0; extensions[i]; i++) {
					if (strcmp(extensions[i], ext) == 0) {
						bingo = SDL_TRUE;
						break;
					}
				}
			}
			if (bingo) {
				dropped_filedir = event.drop.file;
				SNES9X_Reset_DROP(dropped_filedir);
				S9xReset();
			}
			break;
		}
			//
		case SDL_KEYDOWN:
		case SDL_KEYUP: {
			if (event.key.keysym.sym == SDLK_f) {
				if (event.type == SDL_KEYDOWN) {
					S9xSDL_toggleFullscreen();
				}
			} else if (event.key.keysym.sym == SDLK_F3) {
				if (event.type == SDL_KEYDOWN) {
					GUI.video_mode = VIDEOMODE_BLOCKY;
				}
			} else if (event.key.keysym.sym == SDLK_F4) {
				if (event.type == SDL_KEYDOWN) {
					GUI.video_mode = VIDEOMODE_TV;
				}
			} else if (event.key.keysym.sym == SDLK_F5) {
				if (event.type == SDL_KEYDOWN) {
					GUI.video_mode = VIDEOMODE_SMOOTH;
				}
			} else if (event.key.keysym.sym == SDLK_F6) {
				if (event.type == SDL_KEYDOWN) {
					GUI.video_mode = VIDEOMODE_SUPEREAGLE;
				}
			} else if (event.key.keysym.sym == SDLK_F7) {
				if (event.type == SDL_KEYDOWN) {
					GUI.video_mode = VIDEOMODE_2XSAI;
				}
			} else if (event.key.keysym.sym == SDLK_F8) {
				if (event.type == SDL_KEYDOWN) {
					GUI.video_mode = VIDEOMODE_SUPER2XSAI;
				}
			} else if (event.key.keysym.sym == SDLK_F9) {
				if (event.type == SDL_KEYDOWN) {
					GUI.video_mode = VIDEOMODE_EPX;
				}
			} else if (event.key.keysym.sym == SDLK_F9) {
				if (event.type == SDL_KEYDOWN) {
					GUI.video_mode = VIDEOMODE_HQ2X;
				}
			} else {

				S9xReportButton(event.key.keysym.mod << 16 | // keyboard mod
									event.key.keysym.sym,	 // keyboard ksym
								event.type == SDL_KEYDOWN);	 // press or release
			}

		} break;

		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP: {

			if (!gamecontroller[event.jbutton.which]) {
				S9xReportButton(0x80000000 |					  // joystick button
									(event.jbutton.which << 24) | // joystick index
									event.jbutton.button,		  // joystick button code
								event.type == SDL_JOYBUTTONDOWN); // press or release
			}
		} break;
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP: {
			if (gamecontroller[event.cbutton.which]) {
				S9xReportButton(0x80000000 |							 // joystick button
									(event.cbutton.which << 24) |		 // joystick index
									event.cbutton.button,				 // joystick button code
								event.type == SDL_CONTROLLERBUTTONDOWN); // press or release
			}

		} break;
		case SDL_JOYAXISMOTION:
		case SDL_CONTROLLERAXISMOTION: {
			if (gamecontroller[event.cbutton.which]) {
				S9xReportAxis(0x80008000 |					  // joystick axis
								  (event.caxis.which << 24) | // joystick index
								  event.caxis.axis,			  // joystick axis
							  event.caxis.value);			  // axis value
			} else {
				S9xReportAxis(0x80008000 |					  // joystick axis
								  (event.jaxis.which << 24) | // joystick index
								  event.jaxis.axis,			  // joystick axis
							  event.jaxis.value);			  // axis value
			}
		} break;

		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				SDL_RenderClear(GUI.sdl_renderer);
			}
			break;

		case SDL_QUIT:
			quit_state = TRUE;
			break;
		}
	}

	if (quit_state == TRUE) {
		S9xExit();
	}
}

bool S9xPollButton(uint32 id, bool *pressed) {
	return (false);
}

bool S9xPollAxis(uint32 id, int16 *value) {
	return (false);
}

bool S9xPollPointer(uint32 id, int16 *x, int16 *y) {
	return (false);
}

// domaemon: needed by SNES9X
void S9xHandlePortCommand(s9xcommand_t cmd, int16 data1, int16 data2) {
	return;
}
