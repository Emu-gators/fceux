/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2002 Ben Parnell
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

#include "common.h"
#include "cheat.h"
#include "memview.h"
#include "memwatch.h"
#include "debugger.h"
#include "ramwatch.h"
#include "../../fceu.h"
#include "../../cart.h"
#include "../../cheat.h" // For FCEU_LoadGameCheats()
#include <map>

static HWND pwindow = 0;	    //Handle to Cheats dialog
HWND hCheat = 0;			    //mbg merge 7/19/06 had to add
static HMENU hCheatcontext;     //Handle to cheat context menu

bool pauseWhileActive = false;	//For checkbox "Pause while active"
extern bool wasPausedByCheats;

int CheatWindow;
int CheatStyle = 1;

#define GGLISTSIZE 128 //hopefully this is enough for all cases


int selcheat;
int selcheatcount;
int ChtPosX,ChtPosY;
int GGConv_wndx=0, GGConv_wndy=0;
static HFONT hFont,hNewFont;

// static int scrollindex;
// static int scrollnum;
// static int scrollmax;
std::map<int, SEARCHPOSSIBLE> possiList;

int possiStart = 0;
int possiItemCount = 0;
int possiTotalCount = 0;
bool possibleUpdate = false;

int lbfocus = 0;
int searchdone;
static int knownvalue = 0;

int GGaddr, GGcomp, GGval;
char GGcode[10];
int GGlist[GGLISTSIZE];
static int dontupdateGG; //this eliminates recursive crashing

bool dodecode;

HWND hGGConv;

void EncodeGG(char *str, int a, int v, int c);
void ListGGAddresses();

uint16 StrToU16(char *s)
{
	unsigned int ret=0;
	sscanf(s,"%4x",&ret);
	return ret;
}

uint8 StrToU8(char *s)
{
	unsigned int ret=0;
	sscanf(s,"%2x",&ret);
	return ret;
}

char *U16ToStr(uint16 a)
{
	static char str[5];
	sprintf(str,"%04X",a);
	return str;
}

char *U8ToStr(uint8 a)
{
	static char str[3];
	sprintf(str,"%02X",a);
	return str;
}

//int RedoCheatsCallB(char *name, uint32 a, uint8 v, int s) { //bbit edited: this commented out line was changed to the below for the new fceud
int RedoCheatsCallB(char *name, uint32 a, uint8 v, int c, int s, int type, void* data)
{
	char str[256] = { 0 };

	if(a >= 0x8000)
		EncodeGG(str, a, v, c);
	else {
		if(c == -1)
			sprintf(str, "%04X:%02X", (int)a, (int)v);
		else
			sprintf(str, "%04X?%02X:%02X", (int)a, (int)c, (int)v);
	}

	LVITEM lvi = { 0 };
	lvi.mask = LVIF_TEXT;
	lvi.iItem = data ? *((int*)data) : GGLISTSIZE;
	lvi.pszText = str;

	if (data)
		SendDlgItemMessage(hCheat, IDC_LIST_CHEATS, LVM_SETITEM, 0, (LPARAM)&lvi);
	else
		lvi.iItem = SendDlgItemMessage(hCheat, IDC_LIST_CHEATS, LVM_INSERTITEM, 0, (LPARAM)&lvi);
	lvi.iSubItem = 1;
	lvi.pszText = name;
	SendDlgItemMessage(hCheat, IDC_LIST_CHEATS, LVM_SETITEM, 0, (LPARAM)&lvi);

	lvi.mask = LVIF_STATE;
	lvi.stateMask = LVIS_STATEIMAGEMASK;
	lvi.state = INDEXTOSTATEIMAGEMASK(s ? 2 : 1);
	SendDlgItemMessage(hCheat, IDC_LIST_CHEATS, LVM_SETITEMSTATE, lvi.iItem, (LPARAM)&lvi);


	return 1;
}

void RedoCheatsLB(HWND hwndDlg)
{
	SendDlgItemMessage(hwndDlg, IDC_LIST_CHEATS, LVM_DELETEALLITEMS, 0, 0);
	FCEUI_ListCheats(RedoCheatsCallB, 0);

	if (selcheat >= 0)
	{
		EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_CHEAT_DEL), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_CHEAT_UPD), TRUE);
	} else
	{
		EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_CHEAT_DEL), FALSE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_CHEAT_UPD), FALSE);
	}
}

HWND InitializeResultsList(HWND hwnd)
{
	HWND hwndResults = GetDlgItem(hwnd, IDC_CHEAT_LIST_POSSIBILITIES);

	// prepare columns
	LVCOLUMN lv = { 0 };
	lv.mask = LVCF_TEXT | LVCF_WIDTH;

	lv.pszText = "Addr";
	lv.cx = 50;
	SendMessage(hwndResults, LVM_INSERTCOLUMN, 0, (LPARAM)&lv);

	lv.pszText = "Pre";
	lv.mask |= LVCF_FMT;
	lv.fmt = LVCFMT_RIGHT;
	lv.cx = 36;
	SendMessage(hwndResults, LVM_INSERTCOLUMN, 1, (LPARAM)&lv);

	lv.pszText = "Cur";
	SendMessage(hwndResults, LVM_INSERTCOLUMN, 2, (LPARAM)&lv);


	// set style to full row select
	SendMessage(hwndResults, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
	

	return hwndResults;
}

int ShowResultsCallB(uint32 a, uint8 last, uint8 current)
{
	if (hCheat)
	{
		if (possiList[possiItemCount].update =
			(possiList[possiItemCount].addr != a ||
				possiList[possiItemCount].previous != last ||
				possiList[possiItemCount].current != current))
		{
			possiList[possiItemCount].addr = a;
			possiList[possiItemCount].previous = last;
			possiList[possiItemCount].current = current;
			possibleUpdate |= possiList[possiItemCount].update;
		}
		++possiItemCount;
	}

	return 1;
}

int ShowResults(HWND hwndDlg, bool supressUpdate = false)
{

	if (possiList.size() > 64)
		possiList.clear();

	int count = FCEUI_CheatSearchGetCount();

	if (count != possiTotalCount)
	{
		char str[20];
		sprintf(str, "%d Possibilit%s", count, count == 1 ? "y" : "ies");
		SetDlgItemText(hwndDlg, IDC_CHEAT_BOX_POSSIBILITIES, str);
		SendDlgItemMessage(hwndDlg, IDC_CHEAT_LIST_POSSIBILITIES, LVM_SETITEMCOUNT, count, 0);
		possiTotalCount = count;
	}

	if (count)
	{
		int first = SendDlgItemMessage(hwndDlg, IDC_CHEAT_LIST_POSSIBILITIES, LVM_GETTOPINDEX, 0, 0);
		if (first < 0)
			first = 0;
		int last = first + possiItemCount + 1;
		if (last > count)
			last = count;

		int tmpPossiItemCount = possiItemCount;
		possiItemCount = first;
		FCEUI_CheatSearchGetRange(first, last, ShowResultsCallB);
		possiItemCount = tmpPossiItemCount;
		if (possibleUpdate && !supressUpdate)
		{
			int start = -1, end = -1;
			for (int i = first; i < last; ++i)
				if (possiList[i - first].update)
				{
					if (start == -1)
						start = i;
					end = i;
				}

			SendDlgItemMessage(hwndDlg, IDC_CHEAT_LIST_POSSIBILITIES, LVM_REDRAWITEMS, start, end);
		}
		possibleUpdate = false;
		possiStart = first;
	}

	return 1;
}

void EnableCheatButtons(HWND hwndDlg, int enable)
{
	EnableWindow(GetDlgItem(hwndDlg,IDC_CHEAT_VAL_KNOWN),enable);
	EnableWindow(GetDlgItem(hwndDlg,IDC_BTN_CHEAT_KNOWN),enable);
	EnableWindow(GetDlgItem(hwndDlg,IDC_BTN_CHEAT_EQ),enable);
	EnableWindow(GetDlgItem(hwndDlg,IDC_BTN_CHEAT_NE),enable);
	EnableWindow(GetDlgItem(hwndDlg,IDC_BTN_CHEAT_GT),enable);
	EnableWindow(GetDlgItem(hwndDlg,IDC_BTN_CHEAT_LT),enable);
}

HWND InitializeCheatList(HWND hwnd)
{
	HWND hwndChtList = GetDlgItem(hwnd, IDC_LIST_CHEATS);

	// prepare the columns
	LVCOLUMN lv = { 0 };
	lv.mask = LVCF_TEXT | LVCF_WIDTH;

	lv.pszText = "Code";
	lv.cx = 100;
	SendMessage(hwndChtList, LVM_INSERTCOLUMN, 0, (LPARAM)&lv);

	lv.pszText = "Name";
	lv.cx = 132;
	SendMessage(hwndChtList, LVM_INSERTCOLUMN, 1, (LPARAM)&lv);

	// Add a checkbox to indicate if the cheat is activated
	SendMessage(hwndChtList, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);

	return hwndChtList;
}

BOOL CALLBACK CheatConsoleCallB(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			if (ChtPosX == -32000) ChtPosX = 0; //Just in case
			if (ChtPosY == -32000) ChtPosY = 0;
			SetWindowPos(hwndDlg, 0, ChtPosX, ChtPosY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);

			CheckDlgButton(hwndDlg, IDC_CHEAT_PAUSEWHENACTIVE, pauseWhileActive ? MF_CHECKED : MF_UNCHECKED);

			//setup font
			hFont = (HFONT)SendMessage(hwndDlg, WM_GETFONT, 0, 0);
			LOGFONT lf;
			GetObject(hFont, sizeof(LOGFONT), &lf);
			strcpy(lf.lfFaceName, "Courier New");
			hNewFont = CreateFontIndirect(&lf);

			SendDlgItemMessage(hwndDlg, IDC_CHEAT_ADDR, WM_SETFONT, (WPARAM)hNewFont, FALSE);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_VAL, WM_SETFONT, (WPARAM)hNewFont, FALSE);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_COM, WM_SETFONT, (WPARAM)hNewFont, FALSE);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_VAL_KNOWN, WM_SETFONT, (WPARAM)hNewFont, FALSE);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_LIST_POSSIBILITIES, WM_SETFONT, (WPARAM)hNewFont, FALSE);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_VAL_NE_BY, WM_SETFONT, (WPARAM)hNewFont, FALSE);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_VAL_GT_BY, WM_SETFONT, (WPARAM)hNewFont, FALSE);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_VAL_LT_BY, WM_SETFONT, (WPARAM)hNewFont, FALSE);

			//text limits
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_ADDR, EM_SETLIMITTEXT, 4, 0);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_VAL, EM_SETLIMITTEXT, 2, 0);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_COM, EM_SETLIMITTEXT, 2, 0);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_NAME, EM_SETLIMITTEXT, 256, 0);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_VAL_KNOWN, EM_SETLIMITTEXT, 2, 0);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_VAL_NE_BY, EM_SETLIMITTEXT, 2, 0);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_VAL_GT_BY, EM_SETLIMITTEXT, 2, 0);
			SendDlgItemMessage(hwndDlg, IDC_CHEAT_VAL_LT_BY, EM_SETLIMITTEXT, 2, 0);

			//disable or enable buttons
			EnableWindow(GetDlgItem(hwndDlg, IDC_CHEAT_VAL_KNOWN), FALSE);

			possiTotalCount = 0;
			possiItemCount = SendDlgItemMessage(hwndDlg, IDC_CHEAT_LIST_POSSIBILITIES, LVM_GETCOUNTPERPAGE, 0, 0);

			EnableCheatButtons(hwndDlg, possiTotalCount != 0);
			ShowResults(hwndDlg);

			//add header for cheat list and possibilities
			InitializeCheatList(hwndDlg);
			InitializeResultsList(hwndDlg);

			//misc setup
			searchdone = 0;
			SetDlgItemText(hwndDlg, IDC_CHEAT_VAL_KNOWN, (LPTSTR)U8ToStr(knownvalue));

			// Enable Context Sub-Menus
			hCheatcontext = LoadMenu(fceu_hInstance, "CHEATCONTEXTMENUS");

			break;
		}
		case WM_KILLFOCUS:
			break;
		case WM_NCACTIVATE:
			if (pauseWhileActive) 
			{
				if (EmulationPaused == 0) 
				{
					EmulationPaused = 1;
					wasPausedByCheats = true;
					FCEU_printf("Emulation paused: %d\n", EmulationPaused);
				}
			
			}
			if (CheatStyle && possiTotalCount) {
				if ((!wParam) && searchdone) {
					searchdone = 0;
					FCEUI_CheatSearchSetCurrentAsOriginal();
				}
				ShowResults(hwndDlg);   
			}
			break;
		case WM_CLOSE:
			if (CheatStyle)
				DestroyWindow(hwndDlg);
			else
				EndDialog(hwndDlg, 0);
			break;
		case WM_DESTROY:
			CheatWindow = 0;
			hCheat = NULL;
			DeleteObject(hFont);
			DeleteObject(hNewFont);
			if (searchdone)
				FCEUI_CheatSearchSetCurrentAsOriginal();
			possiList.clear();
			break;
		case WM_MOVE:
			if (!IsIconic(hwndDlg)) {
				RECT wrect;
				GetWindowRect(hwndDlg, &wrect);
				ChtPosX = wrect.left;
				ChtPosY = wrect.top;

				#ifdef WIN32
				WindowBoundsCheckNoResize(ChtPosX, ChtPosY, wrect.right);
				#endif
			}
			break;
		case WM_CONTEXTMENU:
		{
			// Handle certain subborn context menus for nearly incapable controls.
			HWND itemHwnd = (HWND)wParam;
			int dlgId = GetDlgCtrlID(itemHwnd);
			int sel = SendMessage(itemHwnd, LVM_GETSELECTIONMARK, 0, 0);
			HMENU hCheatcontextsub = NULL;
			switch (dlgId) {
				case IDC_LIST_CHEATS:
					// Only open the menu if a cheat is selected
					if (selcheat >= 0)
						// Open IDC_LIST_CHEATS Context Menu
						hCheatcontextsub = GetSubMenu(hCheatcontext, 0);
				break;
				case IDC_CHEAT_LIST_POSSIBILITIES:
					if (sel != -1) {
						hCheatcontextsub = GetSubMenu(hCheatcontext, 1);
						SetMenuDefaultItem(hCheatcontextsub, CHEAT_CONTEXT_POSSI_ADDTOMEMORYWATCH, false);
					}
			}

			if (hCheatcontextsub)
			{
				POINT point;
				if (lParam != -1)
				{
					point.x = LOWORD(lParam);
					point.y = HIWORD(lParam);
				} else {
					// Handle the context menu keyboard key
					RECT wrect;
					wrect.left = LVIR_BOUNDS;
					SendMessage(itemHwnd, LVM_GETITEMRECT, sel, (LPARAM)&wrect);
					POINT point;
					point.x = wrect.left + (wrect.right - wrect.left) / 2;
					point.y = wrect.top + (wrect.bottom - wrect.top) / 2;
					ClientToScreen(itemHwnd, &point);
				}
				TrackPopupMenu(hCheatcontextsub, TPM_RIGHTBUTTON, point.x, point.y, 0, hwndDlg, 0);	//Create menu

			}
		}
		break;
		case WM_COMMAND:
			switch (HIWORD(wParam)) {
				case BN_CLICKED:
					switch (LOWORD(wParam)) {
						case CHEAT_CONTEXT_LIST_TOGGLECHEAT:
						{
							LVITEM lvi;
							lvi.mask = LVIF_STATE;
							lvi.stateMask = LVIS_STATEIMAGEMASK;
							int tmpsel = SendDlgItemMessage(hCheat, IDC_LIST_CHEATS, LVM_GETNEXTITEM, -1, LVNI_ALL | LVNI_SELECTED);

							char* name = ""; int s;
							while (tmpsel != -1)
							{
								FCEUI_GetCheat(tmpsel, &name, NULL, NULL, NULL, &s, NULL);
								FCEUI_SetCheat(tmpsel, name, -1, -1, -2, s ^= 1, 1);

								lvi.iItem = tmpsel;
								lvi.state = INDEXTOSTATEIMAGEMASK(s ? 2 : 1);
								SendDlgItemMessage(hCheat, IDC_LIST_CHEATS, LVM_SETITEMSTATE, tmpsel, (LPARAM)&lvi);

								tmpsel = SendDlgItemMessage(hwndDlg, IDC_LIST_CHEATS, LVM_GETNEXTITEM, tmpsel, LVNI_ALL | LVNI_SELECTED);
							}

							UpdateCheatRelatedWindow();
							UpdateCheatListGroupBoxUI();
						}
						break;
						case CHEAT_CONTEXT_LIST_POKECHEATVALUE:
						{
							uint32 a; uint8 v;
							FCEUI_GetCheat(selcheat, NULL, &a, &v, NULL, NULL, NULL);
							BWrite[a](a, v);
						}
						break;
						case CHEAT_CONTEXT_LIST_GOTOINHEXEDITOR:
						{
							uint32 a;
							FCEUI_GetCheat(selcheat, NULL, &a, NULL, NULL, NULL, NULL);
							DoMemView();
							SetHexEditorAddress(a);
						}
						break;
						case CHEAT_CONTEXT_POSSI_ADDCHEAT:
						{
							char str[256] = { 0 };
							int sel = SendDlgItemMessage(hwndDlg, IDC_CHEAT_LIST_POSSIBILITIES, LVM_GETSELECTIONMARK, 0, 0);
							if (sel != -1)
							{
								char str[256] = { 0 };
								GetDlgItemText(hwndDlg, IDC_CHEAT_NAME, str, 256);
								SEARCHPOSSIBLE& possible = possiList[sel];
								if (FCEUI_AddCheat(str, possible.addr, possible.current, -1, 1))
								{
									RedoCheatsCallB(str, possible.addr, possible.current, -1, 1, 1, NULL);

									int newselcheat = SendDlgItemMessage(hwndDlg, IDC_LIST_CHEATS, LVM_GETITEMCOUNT, 0, 0) - 1;
									ListView_MoveSelectionMark(GetDlgItem(hwndDlg, IDC_LIST_CHEATS), selcheat, newselcheat);
									selcheat = newselcheat;
								}

								UpdateCheatRelatedWindow();
								UpdateCheatListGroupBoxUI();
							}
						}
						break;
						case CHEAT_CONTEXT_POSSI_ADDTOMEMORYWATCH:
						{
							char addr[16] = { 0 };
							int sel = SendDlgItemMessage(hwndDlg, IDC_CHEAT_LIST_POSSIBILITIES, LVM_GETSELECTIONMARK, 0, 0);
							if (sel != -1)
							{
								sprintf(addr, "%04X", possiList[sel].addr);
								AddMemWatch(addr);
							}
						}
						break;
						case CHEAT_CONTEXT_POSSI_ADDTORAMWATCH:
						{
							int sel = SendDlgItemMessage(hwndDlg, IDC_CHEAT_LIST_POSSIBILITIES, LVM_GETSELECTIONMARK, 0, 0);
							if (sel != -1)
							{
								AddressWatcher tempWatch;
								tempWatch.Size = 'b';
								tempWatch.Type = 'h';
								tempWatch.Address = possiList[sel].addr;
								tempWatch.WrongEndian = false;
								if(InsertWatch(tempWatch, hwndDlg) && !RamWatchHWnd)
									SendMessage(hAppWnd, WM_COMMAND, ID_RAM_WATCH, 0);
								SetForegroundWindow(RamWatchHWnd);
							}
						}
						break;
						case CHEAT_CONTEXT_POSSI_GOTOINHEXEDITOR:
						{
							int sel = SendDlgItemMessage(hwndDlg, IDC_CHEAT_LIST_POSSIBILITIES, LVM_GETSELECTIONMARK, 0, 0);
							if (sel != -1)
							{
								DoMemView();
								SetHexEditorAddress(possiList[sel].addr);
							}
						}
						break;
						case IDC_CHEAT_PAUSEWHENACTIVE:
							pauseWhileActive ^= 1;
							if ((EmulationPaused == 1 ? true : false) != pauseWhileActive) 
							{
								EmulationPaused = (pauseWhileActive ? 1 : 0);
								wasPausedByCheats = pauseWhileActive;
								if (EmulationPaused)
									FCEU_printf("Emulation paused: %d\n", EmulationPaused);
							}
						break;
						case IDC_BTN_CHEAT_ADD:
						{
							char str[256] = { 0 };
							uint32 a = 0;
							uint8 v = 0;
							int c = 0;
							dodecode = true;
							
							GetDlgItemText(hwndDlg, IDC_CHEAT_ADDR, str, 5);
							if(str[0] != 0)
								dodecode = false;
							a = StrToU16(str);
							GetDlgItemText(hwndDlg, IDC_CHEAT_VAL, str, 3);
							if(str[0] != 0)
								dodecode = false;
							v = StrToU8(str);
							GetDlgItemText(hwndDlg,IDC_CHEAT_COM, str, 3);
							if(str[0] != 0)
								dodecode = false;
							c = (str[0] == 0) ? -1 : StrToU8(str);
							GetDlgItemText(hwndDlg, IDC_CHEAT_NAME, str, 256);
							if(dodecode && (strlen(str) == 6 || strlen(str) == 8))
								if(FCEUI_DecodeGG(str, &GGaddr, &GGval, &GGcomp)) {
									a = GGaddr;
									v = GGval;
									c = GGcomp;
								}
							if (FCEUI_AddCheat(str, a, v, c, 1)) {
								RedoCheatsCallB(str, a, v, c, 1, 1, NULL);

								int newselcheat = SendDlgItemMessage(hwndDlg, IDC_LIST_CHEATS, LVM_GETITEMCOUNT, 0, 0) - 1;
								ListView_MoveSelectionMark(GetDlgItem(hwndDlg, IDC_LIST_CHEATS), selcheat, newselcheat);
								selcheat = newselcheat;

								ClearCheatListText(hwndDlg);
							}
							UpdateCheatRelatedWindow();
							UpdateCheatListGroupBoxUI();
							break;
						}
						case CHEAT_CONTEXT_LIST_DELETESELECTEDCHEATS:
						case IDC_BTN_CHEAT_DEL:
							if (selcheatcount > 1)
							{
								if (IDYES == MessageBox(hwndDlg, "Multiple cheats selected. Continue with delete?", "Delete multiple cheats?", MB_ICONQUESTION | MB_YESNO)) { //Get message box
									selcheat = -1;

									int selcheattemp = SendDlgItemMessage(hwndDlg, IDC_LIST_CHEATS, LVM_GETNEXTITEM, -1, LVNI_ALL | LVNI_SELECTED);
									LVITEM lvi;
									lvi.mask = LVIF_STATE;
									lvi.stateMask = LVIS_SELECTED;
									lvi.state = 0;
									while (selcheattemp != -1)
									{
										FCEUI_DelCheat(selcheattemp);
										SendDlgItemMessage(hwndDlg, IDC_LIST_CHEATS, LVM_DELETEITEM, selcheattemp, 0);
										selcheattemp = SendDlgItemMessage(hwndDlg, IDC_LIST_CHEATS, LVM_GETNEXTITEM, -1, LVNI_ALL | LVNI_SELECTED);
									}

									ClearCheatListText(hwndDlg);

									UpdateCheatRelatedWindow();
									UpdateCheatListGroupBoxUI();
								}
							} else {
								if (selcheat >= 0) {
									FCEUI_DelCheat(selcheat);
									SendDlgItemMessage(hwndDlg, IDC_LIST_CHEATS, LVM_DELETEITEM, selcheat, 0);
									selcheat = -1;
									ClearCheatListText(hwndDlg);
								}
								UpdateCheatRelatedWindow();
								UpdateCheatListGroupBoxUI();
							}
							break;
						case IDC_BTN_CHEAT_UPD:
						{
							selcheat = SendDlgItemMessage(hwndDlg, IDC_LIST_CHEATS, LVM_GETSELECTIONMARK, 0, 0);

							dodecode = true;
							char str[256] = { 0 };
							char* name = ""; uint32 a; uint8 v; int s; int c;

							if (selcheat < 0)
								break;
							GetDlgItemText(hwndDlg, IDC_CHEAT_ADDR, str, 5);
							if (str[0] != 0)
								dodecode = false;
							a = StrToU16(str);
							GetDlgItemText(hwndDlg, IDC_CHEAT_VAL, str, 3);
							if (str[0] != 0)
								dodecode = false;
							v = StrToU8(str);
							GetDlgItemText(hwndDlg, IDC_CHEAT_COM, str, 3);
							if (str[0] != 0)
								dodecode = false;
							c = (str[0] == 0) ? -1 : StrToU8(str);
							GetDlgItemText(hwndDlg, IDC_CHEAT_NAME, str, 256);
							if (dodecode && (strlen(str) == 6 || strlen(str) == 8)) {
								if (FCEUI_DecodeGG(str, &GGaddr, &GGval, &GGcomp)) {
									a = GGaddr;
									v = GGval;
									c = GGcomp;
								}
							}
							FCEUI_SetCheat(selcheat, str, a, v, c, -1, 1);
							FCEUI_GetCheat(selcheat, &name, &a, &v, &c, &s, NULL);
							RedoCheatsCallB(name, a, v, c, s, 1, &selcheat);
							SendDlgItemMessage(hwndDlg, IDC_LIST_CHEATS, LVM_SETSELECTIONMARK, 0, selcheat);

							SetDlgItemText(hwndDlg, IDC_CHEAT_ADDR, (LPTSTR)U16ToStr(a));
							SetDlgItemText(hwndDlg, IDC_CHEAT_VAL, (LPTSTR)U8ToStr(v));
							if (c == -1)
								SetDlgItemText(hwndDlg, IDC_CHEAT_COM, (LPTSTR)"");
							else
								SetDlgItemText(hwndDlg, IDC_CHEAT_COM, (LPTSTR)U8ToStr(c));
							UpdateCheatRelatedWindow();
							UpdateCheatListGroupBoxUI();
							// UpdateCheatAdded();
							break;
						}
						case IDC_BTN_CHEAT_ADDFROMFILE:
						{
							OPENFILENAME ofn;
							memset(&ofn, 0, sizeof(ofn));
							ofn.lStructSize = sizeof(ofn);
							ofn.hwndOwner = hwndDlg;
							ofn.hInstance = fceu_hInstance;
							ofn.lpstrTitle = "Open Cheats file";
							const char filter[] = "Cheat files (*.cht)\0*.cht\0All Files (*.*)\0*.*\0\0";
							ofn.lpstrFilter = filter;

							char nameo[2048] = {0};
							ofn.lpstrFile = nameo;							
							ofn.nMaxFile = 2048;
							ofn.Flags = OFN_EXPLORER|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_FILEMUSTEXIST;
							std::string initdir = FCEU_GetPath(FCEUMKF_CHEAT);
							ofn.lpstrInitialDir = initdir.c_str();

							if (GetOpenFileName(&ofn))
							{
								FILE* file = FCEUD_UTF8fopen(nameo, "rb");
								if (file)
								{
									FCEU_LoadGameCheats(file, 0);
									UpdateCheatsAdded();
									UpdateCheatRelatedWindow();
									savecheats = 1;
								}
							}
							break;
						}
						case IDC_BTN_CHEAT_RESET:
							FCEUI_CheatSearchBegin();
							ShowResults(hwndDlg);
							EnableCheatButtons(hwndDlg,TRUE);
							break;
						case IDC_BTN_CHEAT_KNOWN:
						{
							char str[256] = { 0 };
							searchdone = 1;
							GetDlgItemText(hwndDlg, IDC_CHEAT_VAL_KNOWN, str, 3);
							knownvalue = StrToU8(str);
							FCEUI_CheatSearchEnd(FCEU_SEARCH_NEWVAL_KNOWN, knownvalue, 0);
							ShowResults(hwndDlg);
							break;
						}
						case IDC_BTN_CHEAT_EQ:
							searchdone = 1;
							FCEUI_CheatSearchEnd(FCEU_SEARCH_PUERLY_RELATIVE_CHANGE,0,0);
							ShowResults(hwndDlg);
							break;
						case IDC_BTN_CHEAT_NE:
						{
							char str[256] = { 0 };
							searchdone = 1;
							if (IsDlgButtonChecked(hwndDlg, IDC_CHEAT_CHECK_NE_BY) == BST_CHECKED) {
								GetDlgItemText(hwndDlg, IDC_CHEAT_VAL_NE_BY, str, 3);
								FCEUI_CheatSearchEnd(FCEU_SEARCH_PUERLY_RELATIVE_CHANGE, 0, StrToU8(str));
							}
							else FCEUI_CheatSearchEnd(FCEU_SEARCH_ANY_CHANGE, 0, 0);
							ShowResults(hwndDlg);
							break;
						}
						case IDC_BTN_CHEAT_GT:
						{
							char str[256] = { 0 };
							searchdone = 1;
							if (IsDlgButtonChecked(hwndDlg, IDC_CHEAT_CHECK_GT_BY) == BST_CHECKED) {
								GetDlgItemText(hwndDlg, IDC_CHEAT_VAL_GT_BY, str, 3);
								FCEUI_CheatSearchEnd(FCEU_SEARCH_NEWVAL_GT_KNOWN, 0, StrToU8(str));
							}
							else FCEUI_CheatSearchEnd(FCEU_SEARCH_NEWVAL_GT, 0, 0);
							ShowResults(hwndDlg);
							break;
						}
						case IDC_BTN_CHEAT_LT:
						{
							char str[256] = { 0 };
							searchdone = 1;
							if (IsDlgButtonChecked(hwndDlg, IDC_CHEAT_CHECK_LT_BY) == BST_CHECKED) {
								GetDlgItemText(hwndDlg, IDC_CHEAT_VAL_LT_BY, str, 3);
								FCEUI_CheatSearchEnd(FCEU_SEARCH_NEWVAL_LT_KNOWN, 0, StrToU8(str));
							}
							else FCEUI_CheatSearchEnd(FCEU_SEARCH_NEWVAL_LT, 0, 0);
							ShowResults(hwndDlg);
							break;
						}
					}
					break;
			}
			break;
			case WM_NOTIFY:
			{
				switch (wParam)
				{
					case IDC_LIST_CHEATS:
					{
						NMHDR* lP = (NMHDR*)lParam;
						switch (lP->code) {
							case LVN_ITEMCHANGED:
							{
								NMLISTVIEW* pNMListView = (NMLISTVIEW*)lP;

//								selcheat = pNMListView->iItem;
								selcheatcount = SendDlgItemMessage(hwndDlg, IDC_LIST_CHEATS, LVM_GETSELECTEDCOUNT, 0, 0);
								if (pNMListView->uNewState & LVIS_FOCUSED ||
									!(pNMListView->uOldState & LVIS_SELECTED) && pNMListView->uNewState & LVIS_SELECTED)
								{
									selcheat = pNMListView->iItem;
									if (selcheat >= 0)
									{
										char* name = ""; uint32 a; uint8 v; int s; int c;
										FCEUI_GetCheat(selcheat, &name, &a, &v, &c, &s, NULL);
										SetDlgItemText(hwndDlg, IDC_CHEAT_NAME, (LPTSTR)name);
										SetDlgItemText(hwndDlg, IDC_CHEAT_ADDR, (LPTSTR)U16ToStr(a));
										SetDlgItemText(hwndDlg, IDC_CHEAT_VAL, (LPTSTR)U8ToStr(v));
										SetDlgItemText(hwndDlg, IDC_CHEAT_COM, (LPTSTR)(c == -1 ? "" : U8ToStr(c)));
									}

									EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_CHEAT_DEL), selcheatcount > 0);
									EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_CHEAT_UPD), selcheatcount > 0);
								}

								if (pNMListView->uChanged & LVIF_STATE)
									// uncheck -> check
									if (pNMListView->uOldState & INDEXTOSTATEIMAGEMASK(1) &&
										pNMListView->uNewState & INDEXTOSTATEIMAGEMASK(2))
									{
										int tmpsel = pNMListView->iItem;
										char* name = ""; int s;
										FCEUI_GetCheat(tmpsel, &name, NULL, NULL, NULL, &s, NULL);
										if (!s)
										{
											FCEUI_SetCheat(tmpsel, name, -1, -1, -2, s ^= 1, 1);

											UpdateCheatRelatedWindow();
											UpdateCheatListGroupBoxUI();
										}
									}
									// check -> uncheck
									else if (pNMListView->uOldState & INDEXTOSTATEIMAGEMASK(2) &&
										pNMListView->uNewState & INDEXTOSTATEIMAGEMASK(1))
									{
										int tmpsel = pNMListView->iItem;
										char* name = ""; int s;
										FCEUI_GetCheat(tmpsel, &name, NULL, NULL, NULL, &s, NULL);
										if (s)
										{
											FCEUI_SetCheat(tmpsel, name, -1, -1, -2, s ^= 1, 1);

											UpdateCheatRelatedWindow();
											UpdateCheatListGroupBoxUI();
										}
									}
							}
						}
					}
					break;
					case IDC_CHEAT_LIST_POSSIBILITIES:
					{
						LPNMHDR lP = (LPNMHDR)lParam;
						switch (lP->code)
						{
							case LVN_ITEMCHANGED:
							{
								NMLISTVIEW* pNMListView = (NMLISTVIEW*)lP;
								if (pNMListView->uNewState & LVIS_FOCUSED ||
									!(pNMListView->uOldState & LVIS_SELECTED) && pNMListView->uNewState & LVIS_SELECTED)
								{

									SEARCHPOSSIBLE& possible = possiList[pNMListView->iItem];
									char str[16];
									sprintf(str, "%04X", possible.addr);
									SetDlgItemText(hwndDlg, IDC_CHEAT_ADDR, (LPCTSTR)str);
									sprintf(str, "%02X", possible.current);
									SetDlgItemText(hwndDlg, IDC_CHEAT_VAL, (LPCTSTR)str);
									SetDlgItemText(hwndDlg, IDC_CHEAT_COM, (LPTSTR)"");
								}
							}
							break;
							case LVN_GETDISPINFO:
							{
								NMLVDISPINFO* info = (NMLVDISPINFO*)lParam;
								
								if (!possiList.count(info->item.iItem))
									ShowResults(hwndDlg, true);

								static char num[32];
								switch (info->item.iSubItem)
								{
									case 0:
										sprintf(num, "$%04X", possiList[info->item.iItem].addr);
									break;
									case 1:
										sprintf(num, "%02X", possiList[info->item.iItem].previous);
									break;
									case 2:
										sprintf(num, "%02X", possiList[info->item.iItem].current);
									break;
								}
								info->item.pszText = num;
							}
							break;
							case NM_DBLCLK:
							{
								char addr[16];
								sprintf(addr, "%04X", possiList[((NMITEMACTIVATE*)lParam)->iItem].addr);
								AddMemWatch(addr);
							}
							break;
						}
					}
					break;
				}
			}
	}
	return 0;
}


void ConfigCheats(HWND hParent)
{
	if (!GameInfo)
	{
		FCEUD_PrintError("You must have a game loaded before you can manipulate cheats.");
		return;
	}
	//if (GameInfo->type==GIT_NSF) {
	//	FCEUD_PrintError("Sorry, you can't cheat with NSFs.");
	//	return;
	//}

	if (!CheatWindow) 
	{
		selcheat = -1;
		CheatWindow = 1;
		if (CheatStyle)
			pwindow = hCheat = CreateDialog(fceu_hInstance, "CHEATCONSOLE", hParent, CheatConsoleCallB);
		else
			DialogBox(fceu_hInstance, "CHEATCONSOLE", hParent, CheatConsoleCallB);
		UpdateCheatsAdded();
		UpdateCheatRelatedWindow();
	} else
	{
		ShowWindow(hCheat, SW_SHOWNORMAL);
		SetForegroundWindow(hCheat);
	}
}

void UpdateCheatList()
{
	if (!pwindow)
		return;
	else
		ShowResults(pwindow);
}

void UpdateCheatListGroupBoxUI()
{
	char temp[64];
	if (FrozenAddressCount < 256)
	{
		sprintf(temp, "Active Cheats %d", FrozenAddressCount);
		EnableWindow(GetDlgItem(hCheat, IDC_BTN_CHEAT_ADD), TRUE);
		EnableWindow(GetDlgItem(hCheat, IDC_BTN_CHEAT_ADDFROMFILE), TRUE);
	}
	else if (FrozenAddressCount == 256)
	{
		sprintf(temp, "Active Cheats %d (Max Limit)", FrozenAddressCount);
		EnableWindow(GetDlgItem(hCheat, IDC_BTN_CHEAT_ADD), FALSE);
		EnableWindow(GetDlgItem(hCheat, IDC_BTN_CHEAT_ADDFROMFILE), FALSE);
	}
	else
	{
		sprintf(temp, "%d Error: Too many cheats loaded!", FrozenAddressCount);
		EnableWindow(GetDlgItem(hCheat, IDC_BTN_CHEAT_ADD), FALSE);
		EnableWindow(GetDlgItem(hCheat, IDC_BTN_CHEAT_ADDFROMFILE), FALSE);
	}

	SetDlgItemText(hCheat, IDC_GROUPBOX_CHEATLIST, temp);
}

//Used by cheats and external dialogs such as hex editor to update items in the cheat search dialog
void UpdateCheatsAdded()
{
	RedoCheatsLB(hCheat);
	UpdateCheatListGroupBoxUI();
}

BOOL CALLBACK GGConvCallB(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char str[100];
	int i;

	switch(uMsg) {
		case WM_MOVE: {
			if (!IsIconic(hwndDlg)) {
			RECT wrect;
			GetWindowRect(hwndDlg,&wrect);
			GGConv_wndx = wrect.left;
			GGConv_wndy = wrect.top;

			#ifdef WIN32
			WindowBoundsCheckNoResize(GGConv_wndx,GGConv_wndy,wrect.right);
			#endif
			}
			break;
		};
		case WM_INITDIALOG:
			//todo: set text limits
			if (GGConv_wndx==-32000) GGConv_wndx=0; //Just in case
			if (GGConv_wndy==-32000) GGConv_wndy=0;
			SetWindowPos(hwndDlg,0,GGConv_wndx,GGConv_wndy,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER);
			break;
		case WM_CREATE:

			break;
		case WM_PAINT:
			break;
		case WM_CLOSE:
		case WM_QUIT:
			DestroyWindow(hGGConv);
			hGGConv = 0;
			break;

		case WM_COMMAND:
				switch(HIWORD(wParam)) {
					case EN_UPDATE:
						if(dontupdateGG)break;
						dontupdateGG = 1;
						switch(LOWORD(wParam)){ //lets find out what edit control got changed
							case IDC_GAME_GENIE_CODE: //The Game Genie Code - in this case decode it.
								GetDlgItemText(hGGConv,IDC_GAME_GENIE_CODE,GGcode,9);
								if((strlen(GGcode) != 8) && (strlen(GGcode) != 6))break;

								FCEUI_DecodeGG(GGcode, &GGaddr, &GGval, &GGcomp);

								sprintf(str,"%04X",GGaddr);
								SetDlgItemText(hGGConv,IDC_GAME_GENIE_ADDR,str);

								if(GGcomp != -1)
									sprintf(str,"%02X",GGcomp);
								else str[0] = 0;
								SetDlgItemText(hGGConv,IDC_GAME_GENIE_COMP,str);

								sprintf(str,"%02X",GGval);
								SetDlgItemText(hGGConv,IDC_GAME_GENIE_VAL,str);
								//ListGGAddresses();
							break;

							case IDC_GAME_GENIE_ADDR:
							case IDC_GAME_GENIE_COMP:
							case IDC_GAME_GENIE_VAL:

								GetDlgItemText(hGGConv,IDC_GAME_GENIE_ADDR,str,5);
								if(strlen(str) != 4) break;

								GetDlgItemText(hGGConv,IDC_GAME_GENIE_VAL,str,5);
								if(strlen(str) != 2) {GGval = -1; break;}

								GGaddr = GetEditHex(hGGConv,IDC_GAME_GENIE_ADDR);
								GGval = GetEditHex(hGGConv,IDC_GAME_GENIE_VAL);

								GetDlgItemText(hGGConv,IDC_GAME_GENIE_COMP,str,5);
								if(strlen(str) != 2) GGcomp = -1;
								else GGcomp = GetEditHex(hGGConv,IDC_GAME_GENIE_COMP);

								EncodeGG(GGcode, GGaddr, GGval, GGcomp);
								SetDlgItemText(hGGConv,IDC_GAME_GENIE_CODE,GGcode);
								//ListGGAddresses();
							break;
						}
						ListGGAddresses();
						dontupdateGG = 0;
					break;
					case BN_CLICKED:
						switch (LOWORD(wParam)) {
							case IDC_BTN_ADD_TO_CHEATS:
								//ConfigCheats(fceu_hInstance);

								if(GGaddr < 0x8000)GGaddr += 0x8000;

								if (FCEUI_AddCheat(GGcode, GGaddr, GGval, GGcomp, 1) && hCheat) {
									RedoCheatsCallB(GGcode, GGaddr, GGval, GGcomp, 1, 1, NULL);
									int newselcheat = SendDlgItemMessage(hCheat, IDC_LIST_CHEATS, LVM_GETITEMCOUNT, 0, 0) - 1;
									ListView_MoveSelectionMark(GetDlgItem(hCheat, IDC_LIST_CHEATS), selcheat, newselcheat);
									selcheat = newselcheat;

									SetDlgItemText(hCheat, IDC_CHEAT_ADDR, (LPTSTR)U16ToStr(GGaddr));
									SetDlgItemText(hCheat, IDC_CHEAT_VAL, (LPTSTR)U8ToStr(GGval));
									if(GGcomp == -1)
										SetDlgItemText(hwndDlg, IDC_CHEAT_COM, (LPTSTR)"");
									else
										SetDlgItemText(hwndDlg, IDC_CHEAT_COM, (LPTSTR)U8ToStr(GGcomp));

									EnableWindow(GetDlgItem(hCheat, IDC_BTN_CHEAT_DEL), TRUE);
									EnableWindow(GetDlgItem(hCheat, IDC_BTN_CHEAT_UPD), TRUE);

									UpdateCheatRelatedWindow();
									UpdateCheatListGroupBoxUI();
								}
						}
					break;

					case LBN_DBLCLK:
					switch (LOWORD(wParam)) {
						case IDC_LIST_GGADDRESSES:
							i = SendDlgItemMessage(hwndDlg,IDC_LIST_GGADDRESSES,LB_GETCURSEL,0,0);
							ChangeMemViewFocus(3,GGlist[i],-1);
						break;
					}
					break;
				}
			break;
		case WM_MOUSEMOVE:
			break;
		case WM_HSCROLL:
			break;
	}
	return FALSE;
}

//The code in this function is a modified version
//of Chris Covell's work - I'd just like to point that out
void EncodeGG(char *str, int a, int v, int c)
{
	uint8 num[8];
	static char lets[16]={'A','P','Z','L','G','I','T','Y','E','O','X','U','K','S','V','N'};
	int i;
	
	a&=0x7fff;

	num[0]=(v&7)+((v>>4)&8);
	num[1]=((v>>4)&7)+((a>>4)&8);
	num[2]=((a>>4)&7);
	num[3]=(a>>12)+(a&8);
	num[4]=(a&7)+((a>>8)&8);
	num[5]=((a>>8)&7);

	if (c == -1){
		num[5]+=v&8;
		for(i = 0;i < 6;i++)str[i] = lets[num[i]];
		str[6] = 0;
	} else {
		num[2]+=8;
		num[5]+=c&8;
		num[6]=(c&7)+((c>>4)&8);
		num[7]=((c>>4)&7)+(v&8);
		for(i = 0;i < 8;i++)str[i] = lets[num[i]];
		str[8] = 0;
	}
	return;
}

void ListGGAddresses()
{
	uint32 i, j = 0; //mbg merge 7/18/06 changed from int
	char str[20];
	SendDlgItemMessage(hGGConv,IDC_LIST_GGADDRESSES,LB_RESETCONTENT,0,0);

	//also enable/disable the add GG button here
	GetDlgItemText(hGGConv,IDC_GAME_GENIE_CODE,GGcode,9);

	if((GGaddr < 0) || ((strlen(GGcode) != 8) && (strlen(GGcode) != 6)))EnableWindow(GetDlgItem(hGGConv,IDC_BTN_ADD_TO_CHEATS),FALSE);
	else EnableWindow(GetDlgItem(hGGConv,IDC_BTN_ADD_TO_CHEATS),TRUE);

	for(i = 0;i < PRGsize[0];i+=0x2000){
		if((PRGptr[0][i+(GGaddr&0x1FFF)] == GGcomp) || (GGcomp == -1)){
			GGlist[j] = i+(GGaddr&0x1FFF)+0x10;
			if(++j > GGLISTSIZE)return;
			sprintf(str,"%06X",i+(GGaddr&0x1FFF)+0x10);
			SendDlgItemMessage(hGGConv,IDC_LIST_GGADDRESSES,LB_ADDSTRING,0,(LPARAM)(LPSTR)str);
		}
	}
}

//A different model for this could be to have everything
//set in the INITDIALOG message based on the internal
//variables, and have this simply call that.
void SetGGConvFocus(int address,int compare)
{
	char str[10];
	if(!hGGConv)DoGGConv();
	GGaddr = address;
	GGcomp = compare;

	dontupdateGG = 1; //little hack to fix a nasty bug

	sprintf(str,"%04X",address);
	SetDlgItemText(hGGConv,IDC_GAME_GENIE_ADDR,str);

	dontupdateGG = 0;

	sprintf(str,"%02X",GGcomp);
	SetDlgItemText(hGGConv,IDC_GAME_GENIE_COMP,str);


	if(GGval < 0)SetDlgItemText(hGGConv,IDC_GAME_GENIE_CODE,"");
	else {
		EncodeGG(GGcode, GGaddr, GGval, GGcomp);
		SetDlgItemText(hGGConv,IDC_GAME_GENIE_CODE,GGcode);
	}

	SetFocus(GetDlgItem(hGGConv,IDC_GAME_GENIE_VAL));

	return;
}

void DoGGConv()
{
	if (hGGConv)
	{
		ShowWindow(hGGConv, SW_SHOWNORMAL);
		SetForegroundWindow(hGGConv);
	} else
	{
		hGGConv = CreateDialog(fceu_hInstance,"GGCONV",NULL,GGConvCallB);
	}
	return;
}

/*
void ListBox::OnRButtonDown(UINT nFlags, CPoint point)
{
CPoint test = point;
} */

void DisableAllCheats()
{
	if(FCEU_DisableAllCheats() && hCheat){
		LVITEM lvi;
		lvi.mask = LVIF_STATE;
		lvi.stateMask = LVIS_STATEIMAGEMASK;
		lvi.state = INDEXTOSTATEIMAGEMASK(1);
		for (int current = SendDlgItemMessage(hCheat, IDC_LIST_CHEATS, LB_GETCOUNT, 0, 0) - 1; current >= 0; --current)
			SendDlgItemMessage(hCheat, IDC_LIST_CHEATS, LVM_GETITEMSTATE, current, (LPARAM)&lvi);
		UpdateCheatListGroupBoxUI();
	}	
}

void UpdateCheatRelatedWindow()
{
	// hex editor
	if (hMemView)
		UpdateColorTable(); //if the memory viewer is open then update any blue freeze locations in it as well

	// ram search
	extern HWND RamSearchHWnd;
	if (RamSearchHWnd)
	{
		// if ram search is open then update the ram list.
		SendDlgItemMessage(RamSearchHWnd, IDC_RAMLIST, LVM_REDRAWITEMS, 
			SendDlgItemMessage(RamSearchHWnd, IDC_RAMLIST, LVM_GETTOPINDEX, 0, 0),
			SendDlgItemMessage(RamSearchHWnd, IDC_RAMLIST, LVM_GETCOUNTPERPAGE, 0, 0) + 1);
	}

	// ram watch
	extern void UpdateWatchCheats();
	UpdateWatchCheats();
	extern HWND RamWatchHWnd;
	if (RamWatchHWnd)
	{
		// if ram watch is open then update the ram list.
		SendDlgItemMessage(RamWatchHWnd, IDC_WATCHLIST, LVM_REDRAWITEMS,
			SendDlgItemMessage(RamWatchHWnd, IDC_WATCHLIST, LVM_GETTOPINDEX, 0, 0),
			SendDlgItemMessage(RamWatchHWnd, IDC_WATCHLIST, LVM_GETCOUNTPERPAGE, 0, 0) + 1);
	}

}