/* FCEUXD SP - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2005 Sebastian Porst
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

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "memviewsp.h"
#include "memview.h"
#include "common.h"

HexBookmark hexBookmarks[64];
int hexBookmarkShortcut[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
int numHexBookmarkShortcut = 0;
int nextBookmark = 0;

/// Finds the bookmark for a given address
/// @param address The address to find.
/// @param editmode The editing mode of the hex editor (RAM/PPU/OAM/ROM)
/// @return The index of the bookmark at that address or -1 if there's no bookmark at that address.
int findBookmark(unsigned int address, int editmode)
{
	int i;

	if (address > 0xFFFF)
	{
		MessageBox(0, "Error: Invalid address was specified as parameter to findBookmark", "Error", MB_OK | MB_ICONERROR);
		return -1;
	}
	
	for (i=0;i<nextBookmark;i++)
	{
		if (hexBookmarks[i].address == address && hexBookmarks[i].editmode == editmode)
			return i;
	}

	return -1;
}

BOOL CenterWindow(HWND hwndDlg);

/// Callback function for the name bookmark dialog
/*
 TODO: The bookmarks of Debugger and Hex Editor uses the same dialog box,
 but different bookmark systems, either decouple their callback into separate
 functions or unify them to use the same bookmark system.
*/
INT_PTR CALLBACK nameHexBookmarkCallB(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// For Hex Editor
	static HexBookmarkMsg* hexBookmarkMsg;
	int dlgShortcutRadioCheck[10] = { IDC_RADIO_SHORTCUT0, IDC_RADIO_SHORTCUT1, IDC_RADIO_SHORTCUT2, IDC_RADIO_SHORTCUT3, IDC_RADIO_SHORTCUT4, IDC_RADIO_SHORTCUT5, IDC_RADIO_SHORTCUT6, IDC_RADIO_SHORTCUT7, IDC_RADIO_SHORTCUT8, IDC_RADIO_SHORTCUT9 };

	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			// Limit bookmark descriptions to 50 characters
			SendDlgItemMessage(hwndDlg, IDC_BOOKMARK_DESCRIPTION, EM_SETLIMITTEXT, 50, 0);
			
			// Limit the address text
			SendDlgItemMessage(hwndDlg, IDC_BOOKMARK_ADDRESS, EM_SETLIMITTEXT, 6, 0);
			DefaultEditCtrlProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_BOOKMARK_ADDRESS), GWLP_WNDPROC, (LONG_PTR)FilterEditCtrlProc);

			// Add View list box
			for (int i = 0; i < 4; ++i)
				SendDlgItemMessage(hwndDlg, IDC_BOOKMARK_COMBO_VIEW, CB_INSERTSTRING, -1, (LPARAM)EditString[i]);

			hexBookmarkMsg = (HexBookmarkMsg*)lParam;
			HexBookmark* hexBookmark = hexBookmarkMsg->bookmark;

			bool shortcut_assigned = hexBookmarkMsg->shortcut_index != -1;
			if (shortcut_assigned)
			{
				CheckDlgButton(hwndDlg, IDC_CHECK_SHORTCUT, BST_CHECKED);
				CheckDlgButton(hwndDlg, dlgShortcutRadioCheck[hexBookmarkMsg->shortcut_index], BST_CHECKED);
			}
			else
				EnableWindow(GetDlgItem(hwndDlg, IDC_BOOKMARK_SHORTCUT_PREFIX_TEXT), FALSE);

			for (int i = 0; i < 10; ++i)
				if (!shortcut_assigned || hexBookmarkShortcut[i] != -1 && hexBookmarkShortcut[i] != hexBookmarkMsg->bookmark_index)
					// this bookmark doesn't have a shortcut, or the shortcut number is occupied but it doesn't belongs to this bookmark
					EnableWindow(GetDlgItem(hwndDlg, dlgShortcutRadioCheck[i]), FALSE);

			if (!shortcut_assigned && numHexBookmarkShortcut >= 10)
			{
				// all the shortcuts are occupied and this one doesn't have a shortcut, it's impossible to assign a new shortcut
				EnableWindow(GetDlgItem(hwndDlg, IDC_CHECK_SHORTCUT), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BOOKMARK_SHORTCUT_PREFIX_TEXT), FALSE);
			}

			// Set address and description
			char addr[8];
			sprintf(addr, "%04X", hexBookmark->address);
			SetDlgItemText(hwndDlg, IDC_BOOKMARK_ADDRESS, addr);
			SetDlgItemText(hwndDlg, IDC_BOOKMARK_DESCRIPTION, hexBookmark->description);

			// Set the view of the bookmark
			SendDlgItemMessage(hwndDlg, IDC_BOOKMARK_COMBO_VIEW, CB_SETCURSEL, hexBookmarkMsg->bookmark->editmode, 0);

			// and set focus to that edit field.
			SetFocus(GetDlgItem(hwndDlg, IDC_BOOKMARK_DESCRIPTION));
			SendDlgItemMessage(hwndDlg, IDC_BOOKMARK_DESCRIPTION, EM_SETSEL, 0, 52);

			break;
		}
		case WM_CLOSE:
		case WM_QUIT:
			EndDialog(hwndDlg, 0);
			break;
		case WM_COMMAND:
			switch(HIWORD(wParam))
			{
				case BN_CLICKED:
					switch(LOWORD(wParam))
					{
						case IDC_CHECK_SHORTCUT:
						{
							UINT shortcut_assigned = IsDlgButtonChecked(hwndDlg, IDC_CHECK_SHORTCUT);
							EnableWindow(GetDlgItem(hwndDlg, IDC_BOOKMARK_SHORTCUT_PREFIX_TEXT), shortcut_assigned);

							for (int i = 0; i < 10; ++i)
								EnableWindow(GetDlgItem(hwndDlg, dlgShortcutRadioCheck[i]), shortcut_assigned && (hexBookmarkShortcut[i] == -1 || hexBookmarkShortcut[i] == hexBookmarkMsg->bookmark_index));
						}
						break;
						case IDOK:
						{
							int address, editmode;

							// Get the address;
							char addr_str[8];
							GetDlgItemText(hwndDlg, IDC_BOOKMARK_ADDRESS, addr_str, 8);
							sscanf(addr_str, "%X", &address);

							// Get the view which the address in
							editmode = SendDlgItemMessage(hwndDlg, IDC_BOOKMARK_COMBO_VIEW, CB_GETCURSEL, 0, 0);
							extern int GetMaxSize(int editingMode);
							int MaxSize = GetMaxSize(editmode);

							// Update the address
							if (address > MaxSize - 1)
							{
								// if the address is out of range
								char errmsg[64];
								sprintf(errmsg, "The address in %s must be in range of 0-%X", EditString[editmode], MaxSize - 1);
								MessageBox(hwndDlg, errmsg, "Address out of range", MB_OK | MB_ICONERROR);
								SetFocus(GetDlgItem(hwndDlg, IDC_BOOKMARK_ADDRESS));
								return FALSE;
							}

							int found = findBookmark(address, editmode);
							if (found != -1 && found != hexBookmarkMsg->bookmark_index)
							{
								// if the address already have a bookmark and the bookmark is not the one we currently editing
								MessageBox(hwndDlg, "This address already have a bookmark", "Bookmark duplicated", MB_OK | MB_ICONASTERISK);
								SetFocus(GetDlgItem(hwndDlg, IDC_BOOKMARK_ADDRESS));
								return FALSE;
							}

							hexBookmarkMsg->bookmark->address = address;
							hexBookmarkMsg->bookmark->editmode = editmode;

							// Update the bookmark description
							GetDlgItemText(hwndDlg, IDC_BOOKMARK_DESCRIPTION, hexBookmarkMsg->bookmark->description, 50);
								
							// Update the shortcut key
							if (hexBookmarkMsg->shortcut_index != -1 && hexBookmarkShortcut[hexBookmarkMsg->shortcut_index] == hexBookmarkMsg->bookmark_index)
							{

								hexBookmarkShortcut[hexBookmarkMsg->shortcut_index] = -1;
								--numHexBookmarkShortcut;
							}

							if (IsDlgButtonChecked(hwndDlg, IDC_CHECK_SHORTCUT))
								for (int i = 0; i < 10; ++i)
									if (IsDlgButtonChecked(hwndDlg, dlgShortcutRadioCheck[i]))
									{
										// Update the shortcut index
										hexBookmarkShortcut[i] = hexBookmarkMsg->bookmark_index;
										++numHexBookmarkShortcut;
										break;
									}

							EndDialog(hwndDlg, 1);
						}
						break;
						case IDCANCEL:
							EndDialog(hwndDlg, 0);
					}
			}
	}
	
	return FALSE;
}

/// Attempts to add a new bookmark to the bookmark list.
/// @param hwnd HWND of the FCEU window
/// @param address Address of the new bookmark
/// @param editmode The editing mode of the hex editor (RAM/PPU/OAM/ROM)
/// @return Returns 0 if everything's OK, 1 if user canceled and an error flag otherwise.
int addBookmark(HWND hwnd, unsigned int address, int editmode)
{
	// Enforce a maximum of 64 bookmarks
	if (nextBookmark < 64)
	{
		hexBookmarks[nextBookmark].address = address;
		hexBookmarks[nextBookmark].editmode = editmode;
		sprintf(hexBookmarks[nextBookmark].description, "%s %04X", EditString[editmode], address);

		HexBookmarkMsg msg;

		// Pre-define a shortcut if possible
		for (int i = 0; i < 10; ++i)
			if (hexBookmarkShortcut[i] == -1)
			{
				msg.shortcut_index = i;
				break;
			}

		msg.bookmark = &hexBookmarks[nextBookmark];
		msg.bookmark_index = nextBookmark;

		// Show the bookmark name dialog
		if (DialogBoxParam(fceu_hInstance, "NAMEBOOKMARKDLGMEMVIEW", hwnd, nameHexBookmarkCallB, (LPARAM)&msg))
		{
			nextBookmark++;
			return 0;
		}
		else
		{
			if (msg.shortcut_index != -1)
			{
				if (hexBookmarkShortcut[msg.shortcut_index] != -1)
					--numHexBookmarkShortcut;
				hexBookmarkShortcut[msg.shortcut_index] = -1;
			}
			return 1;
		}
	}
	else
		return -1;
}

/// Edit a bookmark in the bookmark list
/// @param hwnd HWND of the FCEU window
/// @param index Index of the bookmark to edit
/// @return Returns 0 if everything's OK, 1 if user canceled and an error flag otherwise.
int editBookmark(HWND hwnd, unsigned int index)
{
	if (index >= 64) return -1;

	HexBookmarkMsg msg;
	msg.bookmark = &hexBookmarks[index];
	msg.bookmark_index = index;

	// find its shortcut index
	for (int i = 0; i < 10; ++i)
		if (index == hexBookmarkShortcut[i])
		{
			msg.shortcut_index = i;
			break;
		}

	// Show the bookmark name dialog
	if (DialogBoxParam(fceu_hInstance, "NAMEBOOKMARKDLGMEMVIEW", hwnd, nameHexBookmarkCallB, (LPARAM)&msg))
		return 0;
	else
		return 1;

	return -1;
}

/// Removes a bookmark from the bookmark list
/// @param index Index of the bookmark to remove
/// @return Returns 0 if everything's OK, 1 if user canceled and an error flag otherwise.
int removeBookmark(unsigned int index)
{
	if (index >= 64) return -1;

	// remove its related shortcut
	for (int i = 0; i < 10; ++i)
	{
		// all the indexes after the deleted one sould decrease by 1
		if (hexBookmarkShortcut[i] != -1 && hexBookmarkShortcut[i] > index)
			--hexBookmarkShortcut[i];
		else if (hexBookmarkShortcut[i] == index)
		{
			// delete the shortcut index itself
			hexBookmarkShortcut[i] = -1;
			--numHexBookmarkShortcut;
		}
	}

	// At this point it's necessary to move the content of the bookmark list
	for (int i=index;i<nextBookmark - 1;i++)
	{
		hexBookmarks[i] = hexBookmarks[i+1];
	}
	
	--nextBookmark;

	return 0;
}
/*
/// Adds or removes a bookmark from a given address
/// @param hwnd HWND of the emu window
/// @param address Address of the bookmark
/// @param editmode The editing mode of the hex editor (RAM/PPU/OAM/ROM)
int toggleBookmark(HWND hwnd, uint32 address, int editmode)
{
	int val = findBookmark(address, editmode);
	
	// If there's no bookmark at the given address add one.
	if (val == -1)
	{
		return addBookmark(hwnd, address, editmode);
	}
	else // else remove the bookmark
	{
		removeBookmark(val);
		return 0;
	}
}
*/
/// Updates the bookmark menu in the hex window
/// @param menu Handle of the bookmark menu
void updateBookmarkMenus(HMENU menu)
{
	// Remove all bookmark menus
	for (int i = 0; i<nextBookmark + 1; i++)
		RemoveMenu(menu, ID_FIRST_BOOKMARK + i, MF_BYCOMMAND);
	RemoveMenu(menu, ID_BOOKMARKLIST_SEP, MF_BYCOMMAND);

	if (nextBookmark != 0)
	{
		// Add the menus again
		AppendMenu(menu, MF_SEPARATOR, ID_BOOKMARKLIST_SEP, NULL);
		for (int i = 0;i<nextBookmark;i++)
		{
			// Get the text of the menu
			char buffer[0x100];
			sprintf(buffer, i < 10 ? "&%d. %s:$%04X - %s" : "%d. $%04X - %s", i, EditString[hexBookmarks[i].editmode], hexBookmarks[i].address, hexBookmarks[i].description);

			AppendMenu(menu, MF_STRING, ID_FIRST_BOOKMARK + i, buffer);
		}

		for (int i = 0; i < 10; ++i)
		{
			if (hexBookmarkShortcut[i] != -1)
			{
				char buffer[0x100];
				GetMenuString(menu, ID_FIRST_BOOKMARK + hexBookmarkShortcut[i], buffer, 50, MF_BYCOMMAND);
				sprintf(&buffer[strlen(buffer)], "\tCtrl+%d\0", (i + 1) % 10);
				ModifyMenu(menu, ID_FIRST_BOOKMARK + hexBookmarkShortcut[i], MF_BYCOMMAND, ID_FIRST_BOOKMARK + hexBookmarkShortcut[i], buffer);
			}
		}

	}
}

/// Returns the address to scroll to if a given bookmark was activated
/// @param bookmark Index of the bookmark
/// @return The address to scroll to or -1 if the bookmark index is invalid.
int handleBookmarkMenu(int bookmark)
{
	if (bookmark < nextBookmark)
	{
		return hexBookmarks[bookmark].address;
	}
	
	return -1;
}

/// Removes all bookmarks
/// @param menu Handle of the bookmark menu
void removeAllBookmarks(HMENU menu)
{
	for (int i = 0;i<nextBookmark;i++)
	{
		RemoveMenu(menu, ID_FIRST_BOOKMARK + i, MF_BYCOMMAND);
	}
	RemoveMenu(menu, ID_BOOKMARKLIST_SEP, MF_BYCOMMAND);
	
	nextBookmark = 0;
	numHexBookmarkShortcut = 0;
	for (int i = 0; i < 10; ++i)
		hexBookmarkShortcut[i] = -1;
}