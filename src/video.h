#ifndef _VIDEO_H_
#define _VIDEO_H_
int FCEU_InitVirtualVideo(void);
void FCEU_KillVirtualVideo(void);
int SaveSnapshot(void);
int SaveSnapshot(char[]);
void ResetScreenshotsCounter();
uint32 GetScreenPixel(int x, int y, bool usebackup);
int GetScreenPixelPalette(int x, int y, bool usebackup);
void FCEU_DrawLuaGui(void);


//in case we need more flags in the future we can change the size here
//bit0 : monochrome bit
//bit5 : emph red
//bit6 : emph green
//bit7 : emph blue
typedef uint8 xfbuf_t;

extern uint8 *XBuf;
extern uint8 *XBackBuf;
extern uint8 *XDBuf;
extern uint8 *XDBackBuf;
extern xfbuf_t *XFBuf;

extern int ClipSidesOffset;

struct GUIMESSAGE
{
	//countdown for gui messages
	int howlong;

	//the current gui message
	char errmsg[110];

	//indicates that the movie should be drawn even on top of movies
	bool isMovieMessage;

	//in case of multiple lines, allow one to move the message
	int linesFromBottom;

	// constructor
	GUIMESSAGE(void)
	{
		howlong = 0;
		linesFromBottom = 0;
		isMovieMessage = false;
		errmsg[0] = 0;
	}
};

extern GUIMESSAGE guiMessage;
extern GUIMESSAGE subtitleMessage;

extern bool vidGuiMsgEna;

void FCEU_DrawNumberRow(uint8 *XBuf, int *nstatus, int cur);

std::string FCEUI_GetSnapshotAsName();
void FCEUI_SetSnapshotAsName(std::string name);
bool FCEUI_ShowFPS();
void FCEUI_SetShowFPS(bool showFPS);
void FCEUI_ToggleShowFPS();
void ShowFPS(void);
void ResetFPS(void);
void snapAVI(void);
#endif
