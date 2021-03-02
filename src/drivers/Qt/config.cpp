/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2020 mjbudd77
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <QDir>

#include "Qt/main.h"
#include "Qt/throttle.h"
#include "Qt/config.h"

#include "../common/cheat.h"

#include "Qt/input.h"
#include "Qt/dface.h"

#include "Qt/sdl.h"
#include "Qt/sdl-video.h"
#include "Qt/unix-netplay.h"

#ifdef WIN32
#include <windows.h>
#endif

//#include <unistd.h>

#include <csignal>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#ifdef WIN32
#include <direct.h>
#else
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

static const char* HotkeyStrings[HK_MAX] = {
		"CheatMenu",
		"BindState",
		"LoadLua",
		"ToggleBG",
		"SaveState",
		"FDSSelect",
		"LoadState",
		"FDSEject",
		"VSInsertCoin",
		"VSToggleDip",
		"MovieToggleFrameDisplay",
		"SubtitleDisplay",
		"Reset",
		"Screenshot",
		"Pause",
		"DecreaseSpeed",
		"IncreaseSpeed",
		"FrameAdvance",
		"Turbo",
		"ToggleInputDisplay",
		"ToggleMovieRW",
		"MuteCapture",
		"Quit",
		"FrameAdvanceLagSkip",
		"LagCounterDisplay",
		"SelectState0", "SelectState1", "SelectState2", "SelectState3",
		"SelectState4", "SelectState5", "SelectState6", "SelectState7", 
		"SelectState8", "SelectState9", "SelectStateNext", "SelectStatePrev",
		"VolumeDown", "VolumeUp", "FKB_Enable" };

const char *getHotkeyString( int i )
{
   if ( (i>=0) && (i<HK_MAX) )
   {
      return HotkeyStrings[i];
   }
   return NULL;
}

/**
 * Read a custom pallete from a file and load it into the core.
 */
int
LoadCPalette(const std::string &file)
{
	uint8 tmpp[192];
	FILE *fp;

	if(!(fp = FCEUD_UTF8fopen(file.c_str(), "rb"))) {
		char errorMsg[256];
		strcpy(errorMsg, "Error loading custom palette from file: ");
		strcat(errorMsg, file.c_str());
		FCEUD_PrintError(errorMsg);
		return 0;
	}
	size_t result = fread(tmpp, 1, 192, fp);
	if(result != 192) {
		char errorMsg[256];
		strcpy(errorMsg, "Error loading custom palette from file: ");
		strcat(errorMsg, file.c_str());
		FCEUD_PrintError(errorMsg);
		return 0;
	}
	FCEUI_SetUserPalette(tmpp, result/3);
	fclose(fp);
	return 1;
}

/**
 * Creates the subdirectories used for saving snapshots, movies, game
 * saves, etc.  Hopefully obsolete with new configuration system.
 */
static void
CreateDirs(const std::string &dir)
{
	const char *subs[9]={"fcs","snaps","gameinfo","sav","cheats","movies","input"};
	std::string subdir;
	int x;

#if defined(WIN32) || defined(NEED_MINGW_HACKS)
	mkdir(dir.c_str());
	chmod(dir.c_str(), 755);
	for(x = 0; x < 7; x++) {
		subdir = dir + PSS + subs[x];
		mkdir(subdir.c_str());
	}
#else
	mkdir(dir.c_str(), S_IRWXU);
	for(x = 0; x < 7; x++) {
		subdir = dir + PSS + subs[x];
		mkdir(subdir.c_str(), S_IRWXU);
	}
#endif
}

/**
 * Attempts to locate FCEU's application directory.  This will
 * hopefully become obsolete once the new configuration system is in
 * place.
 */
static void
GetBaseDirectory(std::string &dir)
{
	char *home = getenv("FCEUX_HOME");

#ifdef WIN32
	// Windows users want base directory to be where executable resides.
	// Only way to override this behavior is to set an FCEUX_HOME 
	// environment variable prior to starting the application.
	//if ( home == NULL )
	//{
	//	home = getenv("USERPROFILE");
	//}
	//if ( home == NULL )
	//{
	//	home = getenv("HOMEPATH");
	//}
#else
	if ( home == NULL )
	{
		home = getenv("HOME");
	}
#endif

	if (home) 
	{
		dir = std::string(home) + "/.fceux";
	} else {
#ifdef WIN32
		home = new char[MAX_PATH + 1];
		GetModuleFileName(NULL, home, MAX_PATH + 1);

		char *lastBS = strrchr(home,'\\');
		if(lastBS) {
			*lastBS = 0;
		}

		dir = std::string(home);
		delete[] home;
#else
		dir = "";
#endif
	}
}

// returns a config structure with default options
// also creates config base directory (ie: /home/user/.fceux as well as subdirs
Config *
InitConfig()
{
	std::string dir, prefix, savPath, movPath;
	Config *config;

	GetBaseDirectory(dir);

	FCEUI_SetBaseDirectory(dir.c_str());
	CreateDirs(dir);

	config = new Config(dir);

	// sound options
	config->addOption('s', "sound", "SDL.Sound", 1);
	config->addOption("volume", "SDL.Sound.Volume", 150);
	config->addOption("trianglevol", "SDL.Sound.TriangleVolume", 256);
	config->addOption("square1vol", "SDL.Sound.Square1Volume", 256);
	config->addOption("square2vol", "SDL.Sound.Square2Volume", 256);
	config->addOption("noisevol", "SDL.Sound.NoiseVolume", 256);
	config->addOption("pcmvol", "SDL.Sound.PCMVolume", 256);
	config->addOption("soundrate", "SDL.Sound.Rate", 44100);
	config->addOption("soundq", "SDL.Sound.Quality", 1);
	config->addOption("soundrecord", "SDL.Sound.RecordFile", "");
	config->addOption("soundbufsize", "SDL.Sound.BufSize", 128);
	config->addOption("lowpass", "SDL.Sound.LowPass", 0);
    
	config->addOption('g', "gamegenie", "SDL.GameGenie", 0);
	config->addOption("pal", "SDL.PAL", 0);
	config->addOption("autoPal", "SDL.AutoDetectPAL", 1);
	config->addOption("frameskip", "SDL.Frameskip", 0);
	config->addOption("clipsides", "SDL.ClipSides", 0);
	config->addOption("nospritelim", "SDL.DisableSpriteLimit", 1);
	config->addOption("swapduty", "SDL.SwapDuty", 0);
	config->addOption("ramInit", "SDL.RamInitMethod", 0);

	// color control
	config->addOption('p', "palette", "SDL.Palette", "");
	config->addOption("tint", "SDL.Tint", 56);
	config->addOption("hue", "SDL.Hue", 72);
	config->addOption("ntsccolor", "SDL.NTSCpalette", 0);

	// scanline settings
	config->addOption("SDL.ScanLineStartNTSC", 0+8);
	config->addOption("SDL.ScanLineEndNTSC", 239-8);
	config->addOption("SDL.ScanLineStartPAL", 0);
	config->addOption("SDL.ScanLineEndPAL", 239);

	// video controls
	config->addOption('f', "fullscreen", "SDL.Fullscreen", 0);
	config->addOption("videoDriver", "SDL.VideoDriver", 0);

	// set x/y res to 0 for automatic fullscreen resolution detection (no change)
	config->addOption('x', "xres", "SDL.XResolution", 0);
	config->addOption('y', "yres", "SDL.YResolution", 0);
	config->addOption("SDL.LastXRes", 0);
	config->addOption("SDL.LastYRes", 0);
	config->addOption("SDL.WinSizeX", 0);
	config->addOption("SDL.WinSizeY", 0);
	config->addOption("doublebuf", "SDL.DoubleBuffering", 1);
	config->addOption("autoscale", "SDL.AutoScale", 1);
	config->addOption("keepratio", "SDL.KeepRatio", 1);
	config->addOption("xscale", "SDL.XScale", 2.000);
	config->addOption("yscale", "SDL.YScale", 2.000);
	config->addOption("xstretch", "SDL.XStretch", 0);
	config->addOption("ystretch", "SDL.YStretch", 0);
	config->addOption("noframe", "SDL.NoFrame", 0);
	config->addOption("special", "SDL.SpecialFilter", 0);
	config->addOption("showfps", "SDL.ShowFPS", 0);
	config->addOption("togglemenu", "SDL.ToggleMenu", 0);

	// OpenGL options
	config->addOption("opengl", "SDL.OpenGL", 1);
	config->addOption("openglip", "SDL.OpenGLip", 0);
	config->addOption("SDL.SpecialFilter", 0);
	config->addOption("SDL.SpecialFX", 0);
	config->addOption("SDL.Vsync", 1);

	// network play options - netplay is broken
	config->addOption("server", "SDL.NetworkIsServer", 0);
	config->addOption('n', "net", "SDL.NetworkIP", "");
	config->addOption('u', "user", "SDL.NetworkUsername", "");
	config->addOption('w', "pass", "SDL.NetworkPassword", "");
	config->addOption('k', "netkey", "SDL.NetworkGameKey", "");
	config->addOption("port", "SDL.NetworkPort", 4046);
	config->addOption("players", "SDL.NetworkPlayers", 1);
     
	// input configuration options
	config->addOption("input1", "SDL.Input.0", "GamePad.0");
	config->addOption("input2", "SDL.Input.1", "GamePad.1");
	config->addOption("input3", "SDL.Input.2", "Gamepad.2");
	config->addOption("input4", "SDL.Input.3", "Gamepad.3");

	config->addOption("autoInputPreset", "SDL.AutoInputPreset", 0);

	// display input
	config->addOption("inputdisplay", "SDL.InputDisplay", 0);

	// enable / disable opposite directionals (left + right or up + down simultaneously)
	config->addOption("opposite-directionals", "SDL.Input.EnableOppositeDirectionals", 1);
    
	// pause movie playback at frame x
	config->addOption("pauseframe", "SDL.PauseFrame", 0);
	config->addOption("recordhud", "SDL.RecordHUD", 1);
	config->addOption("moviemsg", "SDL.MovieMsg", 1);

	// Hex Editor Options
	config->addOption("hexEditBgColor", "SDL.HexEditBgColor", "#000000");
	config->addOption("hexEditFgColor", "SDL.HexEditFgColor", "#FFFFFF");
    
	// Debugger Options
	config->addOption("autoLoadDebugFiles"     , "SDL.AutoLoadDebugFiles", 1);
	config->addOption("autoOpenDebugger"       , "SDL.AutoOpenDebugger"  , 0);
	config->addOption("debuggerPCPlacementMode", "SDL.DebuggerPCPlacement"  , 0);
	config->addOption("debuggerPCDLineOffset"  , "SDL.DebuggerPCLineOffset" , 0);

	// Code Data Logger Options
	config->addOption("autoSaveCDL"  , "SDL.AutoSaveCDL", 1);
	config->addOption("autoLoadCDL"  , "SDL.AutoLoadCDL", 1);
	config->addOption("autoResumeCDL", "SDL.AutoResumeCDL", 0);
	
	// overwrite the config file?
	config->addOption("no-config", "SDL.NoConfig", 0);

	config->addOption("autoresume", "SDL.AutoResume", 0);
    
	// video playback
	config->addOption("playmov", "SDL.Movie", "");
	config->addOption("subtitles", "SDL.SubtitleDisplay", 1);
	config->addOption("movielength", "SDL.MovieLength", 0);
	
	config->addOption("fourscore", "SDL.FourScore", 0);

	config->addOption("nofscursor", "SDL.NoFullscreenCursor", 1);
    
    #ifdef _S9XLUA_H
	// load lua script
	config->addOption("loadlua", "SDL.LuaScript", "");
    #endif
    
    #ifdef CREATE_AVI
	config->addOption("videolog",  "SDL.VideoLog",  "");
	config->addOption("mute", "SDL.MuteCapture", 0);
    #endif
    
    // auto load/save on gameload/close
	config->addOption("loadstate", "SDL.AutoLoadState", INVALID_STATE);
	config->addOption("savestate", "SDL.AutoSaveState", INVALID_STATE);

	//TODO implement this
	config->addOption("periodicsaves", "SDL.PeriodicSaves", 0);

	savPath = dir + "/sav";
	movPath = dir + "/movies";

	// prefixed with _ because they are internal (not cli options)
	config->addOption("_lastopenfile", "SDL.LastOpenFile", dir);
	config->addOption("_laststatefrom", "SDL.LastLoadStateFrom", savPath );
	config->addOption("_lastopennsf", "SDL.LastOpenNSF", dir);
	config->addOption("_lastsavestateas", "SDL.LastSaveStateAs", savPath );
	config->addOption("_lastopenmovie", "SDL.LastOpenMovie", movPath);
	config->addOption("_lastloadlua", "SDL.LastLoadLua", "");

	for (unsigned int i=0; i<10; i++)
	{
		char buf[128];
		sprintf(buf, "SDL.RecentRom%02u", i);

		config->addOption( buf, "");
	}

	config->addOption("_useNativeFileDialog", "SDL.UseNativeFileDialog", false);
	config->addOption("_useNativeMenuBar"   , "SDL.UseNativeMenuBar", false);
	config->addOption("SDL.GuiStyle", "");
	config->addOption("SDL.QtStyleSheet", "");
	config->addOption("SDL.UseCustomQss", 0);

	config->addOption("_setSchedParam"      , "SDL.SetSchedParam" , 0);
	config->addOption("_emuSchedPolicy"     , "SDL.EmuSchedPolicy", 0);
	config->addOption("_emuSchedNice"       , "SDL.EmuSchedNice"  , 0);
	config->addOption("_emuSchedPrioRt"     , "SDL.EmuSchedPrioRt", 40);
	config->addOption("_guiSchedPolicy"     , "SDL.GuiSchedPolicy", 0);
	config->addOption("_guiSchedNice"       , "SDL.GuiSchedNice"  , 0);
	config->addOption("_guiSchedPrioRt"     , "SDL.GuiSchedPrioRt", 40);
	config->addOption("_emuTimingMech"      , "SDL.EmuTimingMech" , 0);

	// fcm -> fm2 conversion
	config->addOption("fcmconvert", "SDL.FCMConvert", "");
    
	// fm2 -> srt conversion
	config->addOption("ripsubs", "SDL.RipSubs", "");
	
	// enable new PPU core
	config->addOption("newppu", "SDL.NewPPU", 0);

	// quit when a+b+select+start is pressed
	config->addOption("4buttonexit", "SDL.ABStartSelectExit", 0);

	// GamePad 0 - 3
	for(unsigned int i = 0; i < GAMEPAD_NUM_DEVICES; i++) 
	{
		char buf[64];
		snprintf(buf, sizeof(buf)-1, "SDL.Input.GamePad.%u.", i);
		prefix = buf;

		config->addOption(prefix + "DeviceType", DefaultGamePadDevice[i]);
		config->addOption(prefix + "DeviceGUID", "");
		config->addOption(prefix + "Profile"   , "");
	}
    
	// PowerPad 0 - 1
	for(unsigned int i = 0; i < POWERPAD_NUM_DEVICES; i++) {
		char buf[64];
		snprintf(buf, sizeof(buf)-1, "SDL.Input.PowerPad.%u.", i);
		prefix = buf;

		config->addOption(prefix + "DeviceType", DefaultPowerPadDevice[i]);
		config->addOption(prefix + "DeviceNum",  0);
		for(unsigned int j = 0; j < POWERPAD_NUM_BUTTONS; j++) {
			config->addOption(prefix +PowerPadNames[j], DefaultPowerPad[i][j]);
		}
	}

	// QuizKing
	prefix = "SDL.Input.QuizKing.";
	config->addOption(prefix + "DeviceType", DefaultQuizKingDevice);
	config->addOption(prefix + "DeviceNum", 0);
	for(unsigned int j = 0; j < QUIZKING_NUM_BUTTONS; j++) {
		config->addOption(prefix + QuizKingNames[j], DefaultQuizKing[j]);
	}

	// HyperShot
	prefix = "SDL.Input.HyperShot.";
	config->addOption(prefix + "DeviceType", DefaultHyperShotDevice);
	config->addOption(prefix + "DeviceNum", 0);
	for(unsigned int j = 0; j < HYPERSHOT_NUM_BUTTONS; j++) {
		config->addOption(prefix + HyperShotNames[j], DefaultHyperShot[j]);
	}

	// Mahjong
	prefix = "SDL.Input.Mahjong.";
	config->addOption(prefix + "DeviceType", DefaultMahjongDevice);
	config->addOption(prefix + "DeviceNum", 0);
	for(unsigned int j = 0; j < MAHJONG_NUM_BUTTONS; j++) {
		config->addOption(prefix + MahjongNames[j], DefaultMahjong[j]);
	}

	// TopRider
	prefix = "SDL.Input.TopRider.";
	config->addOption(prefix + "DeviceType", DefaultTopRiderDevice);
	config->addOption(prefix + "DeviceNum", 0);
	for(unsigned int j = 0; j < TOPRIDER_NUM_BUTTONS; j++) {
		config->addOption(prefix + TopRiderNames[j], DefaultTopRider[j]);
	}

	// FTrainer
	prefix = "SDL.Input.FTrainer.";
	config->addOption(prefix + "DeviceType", DefaultFTrainerDevice);
	config->addOption(prefix + "DeviceNum", 0);
	for(unsigned int j = 0; j < FTRAINER_NUM_BUTTONS; j++) {
		config->addOption(prefix + FTrainerNames[j], DefaultFTrainer[j]);
	}

	// FamilyKeyBoard
	prefix = "SDL.Input.FamilyKeyBoard.";
	config->addOption(prefix + "DeviceType", DefaultFamilyKeyBoardDevice);
	config->addOption(prefix + "DeviceNum", 0);
	for(unsigned int j = 0; j < FAMILYKEYBOARD_NUM_BUTTONS; j++) {
		config->addOption(prefix + FamilyKeyBoardNames[j],
						DefaultFamilyKeyBoard[j]);
	}

	// for FAMICOM microphone in pad 2 pad 1 didn't have it
	// Takeshi no Chousenjou uses it for example.
	prefix = "SDL.Input.FamicomPad2.";
	config->addOption("rp2mic", prefix + "EnableMic", 0);

	// TODO: use a better data structure to store the hotkeys or something
	//			improve this code overall in the future to make it
	//			easier to maintain
	const int Hotkeys[HK_MAX] = {
		SDLK_F1, // cheat menu
		SDLK_F2, // bind state
		SDLK_F3, // load lua
		SDLK_F4, // toggleBG
		SDLK_F5, // save state
		SDLK_F6, // fds select
		SDLK_F7, // load state
		SDLK_F8, // fds eject
		SDLK_F6, // VS insert coin
		SDLK_F8, // VS toggle dipswitch
		SDLK_PERIOD, // toggle frame display
		SDLK_F10, // toggle subtitle
		SDLK_F11, // reset
		SDLK_F12, // screenshot
		SDLK_PAUSE, // pause
		SDLK_MINUS, // speed++
		SDLK_EQUALS, // speed--
		SDLK_BACKSLASH, //frame advnace
		SDLK_TAB, // turbo
		SDLK_COMMA, // toggle input display
		SDLK_q, // toggle movie RW
		SDLK_QUOTE, // toggle mute capture
		0, // quit // edit 10/11/11 - don't map to escape, it causes ugly things to happen to sdl.  can be manually appended to config
		SDLK_DELETE, // frame advance lag skip
		SDLK_SLASH, // lag counter display
		SDLK_0, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5,
		SDLK_6, SDLK_7, SDLK_8, SDLK_9,
		SDLK_PAGEUP, // select state next
		SDLK_PAGEDOWN, // select state prev
		0, // Volume Down Internal 
		0, // Volume Up Internal 
		SDLK_SCROLLLOCK }; // FKB Enable Toggle

	prefix = "SDL.Hotkeys.";
	for(int i=0; i < HK_MAX; i++)
	{
		char buf[256];
		std::string keyText;

		keyText.assign(" mod=");

		sprintf( buf, "  key=%s", SDL_GetKeyName( Hotkeys[i] ) );

		keyText.append( buf );

		config->addOption(prefix + HotkeyStrings[i], keyText);
	}
	// All mouse devices
	config->addOption("SDL.OekaKids.0.DeviceType", "Mouse");
	config->addOption("SDL.OekaKids.0.DeviceNum", 0);

	config->addOption("SDL.Arkanoid.0.DeviceType", "Mouse");
	config->addOption("SDL.Arkanoid.0.DeviceNum", 0);

	config->addOption("SDL.Shadow.0.DeviceType", "Mouse");
	config->addOption("SDL.Shadow.0.DeviceNum", 0);

	config->addOption("SDL.Zapper.0.DeviceType", "Mouse");
	config->addOption("SDL.Zapper.0.DeviceNum", 0);

	return config;
}

void
UpdateEMUCore(Config *config)
{
	int ntsccol, ntsctint, ntschue, flag, region;
	int startNTSC, endNTSC, startPAL, endPAL;
	std::string cpalette;

	config->getOption("SDL.NTSCpalette", &ntsccol);
	config->getOption("SDL.Tint", &ntsctint);
	config->getOption("SDL.Hue", &ntschue);
	FCEUI_SetNTSCTH(ntsccol, ntsctint, ntschue);

	config->getOption("SDL.Palette", &cpalette);
	if(cpalette.size()) {
		LoadCPalette(cpalette);
	}

	config->getOption("SDL.PAL", &region);
	FCEUI_SetRegion(region);

	config->getOption("SDL.GameGenie", &flag);
	FCEUI_SetGameGenie(flag ? 1 : 0);

	config->getOption("SDL.Sound.LowPass", &flag);
	FCEUI_SetLowPass(flag ? 1 : 0);

	config->getOption("SDL.DisableSpriteLimit", &flag);
	FCEUI_DisableSpriteLimitation(flag ? 1 : 0);

	config->getOption("SDL.ScanLineStartNTSC", &startNTSC);
	config->getOption("SDL.ScanLineEndNTSC", &endNTSC);
	config->getOption("SDL.ScanLineStartPAL", &startPAL);
	config->getOption("SDL.ScanLineEndPAL", &endPAL);

#if DOING_SCANLINE_CHECKS
	for(int i = 0; i < 2; x++) {
		if(srendlinev[x]<0 || srendlinev[x]>239) srendlinev[x]=0;
		if(erendlinev[x]<srendlinev[x] || erendlinev[x]>239) erendlinev[x]=239;
	}
#endif

	FCEUI_SetRenderedLines(startNTSC, endNTSC, startPAL, endPAL);
}

