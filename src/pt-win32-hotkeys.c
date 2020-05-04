/* Copyright (C) Gabor Karsay 2020 <gabor.karsay@gmx.at>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "config.h"
#include <windows.h>
#include <gtk/gtk.h>
#include <pt-player.h>
#include "pt-window.h"
#include "pt-win32-hotkeys.h"

struct _PtWin32HotkeysPrivate
{
	HWND hwnd;
};

enum
{
	PT_HOTKEY_PLAY_PAUSE,
	PT_HOTKEY_STOP,
	PT_HOTKEY_PREV_TRACK,
	PT_HOTKEY_NEXT_TRACK,
	PT_NUM_HOTKEYS
};

static UINT hotkeycode[PT_NUM_HOTKEYS] = {
	VK_MEDIA_PLAY_PAUSE,
	VK_MEDIA_STOP,
	VK_MEDIA_PREV_TRACK,
	VK_MEDIA_NEXT_TRACK
};

#define PT_HOTKEY_WINDOW_CLASS "ParlatypeHotkeyWindowClass"
#define PT_HOTKEY_WINDOW_TITLE "ParlatypeHotkeyWindowTitle"

G_DEFINE_TYPE_WITH_PRIVATE (PtWin32Hotkeys, pt_win32_hotkeys, PT_CONTROLLER_TYPE)


static LRESULT CALLBACK
message_handler (HWND   hwnd,
                 UINT   uMsg,
                 WPARAM wParam,
                 LPARAM lParam)
{
	if (uMsg != WM_HOTKEY)
		return DefWindowProc (hwnd, uMsg, wParam, lParam);

	PtWin32Hotkeys *self = (PtWin32Hotkeys*) (uintptr_t)
	                    GetWindowLongPtr (hwnd, GWLP_USERDATA);
	PtPlayer *player = pt_controller_get_player (PT_CONTROLLER (self));

	switch (wParam) {
		case PT_HOTKEY_PLAY_PAUSE:
			pt_player_play_pause (player);
			break;
		case PT_HOTKEY_STOP:
			pt_player_pause (player);
			break;
		case PT_HOTKEY_PREV_TRACK:
			pt_player_jump_back (player);
			break;
		case PT_HOTKEY_NEXT_TRACK:
			pt_player_jump_forward (player);
			break;
		default:
			return DefWindowProc (hwnd, uMsg, wParam, lParam);
	}

	return 0;
}

void
pt_win32_hotkeys_start (PtWin32Hotkeys *self)
{
	WNDCLASS wc;

	memset (&wc, 0, sizeof (wc));
	wc.lpfnWndProc	 = message_handler;
	wc.hInstance	 = GetModuleHandle (NULL);
	wc.lpszClassName = PT_HOTKEY_WINDOW_CLASS;

	if (!RegisterClass (&wc)) {
		//LogLastWinError ();
		return;
	}

	self->priv->hwnd = CreateWindow (PT_HOTKEY_WINDOW_CLASS,
	                                 PT_HOTKEY_WINDOW_TITLE,
	                                 0,			/* window style */
	                                 0, 0,			/* x and y coordinates */
	                                 0, 0,			/* with and height */
	                                 NULL,			/* parent window */
	                                 NULL,			/* menu */
	                                 wc.hInstance,
	                                 NULL);

	if (!self->priv->hwnd) {
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
		                  "MESSAGE", "Can't create hotkey window");
		UnregisterClass (PT_HOTKEY_WINDOW_CLASS, NULL);
		return;
	}

	SetWindowLongPtr (self->priv->hwnd, GWLP_USERDATA, (uintptr_t) self);
	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "Created hotkey window");

	for (int i = 0; i < PT_NUM_HOTKEYS; i++) {
		RegisterHotKey (self->priv->hwnd,	/* our window */
		                i,			/* hotkey ID */
		                0,			/* no modifier */
		                hotkeycode[i]);	/* key code */
	}
}

static void
pt_win32_hotkeys_init (PtWin32Hotkeys *self)
{
	self->priv = pt_win32_hotkeys_get_instance_private (self);

	self->priv->hwnd = 0;
}

static void
pt_win32_hotkeys_dispose (GObject *object)
{
	PtWin32Hotkeys *self = PT_WIN32_HOTKEYS (object);

	for (int i = 0; i < PT_NUM_HOTKEYS; i++) {
		UnregisterHotKey (self->priv->hwnd, i);
	}

	if (self->priv->hwnd)
		DestroyWindow (self->priv->hwnd);

	G_OBJECT_CLASS (pt_win32_hotkeys_parent_class)->dispose (object);
}

static void
pt_win32_hotkeys_class_init (PtWin32HotkeysClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = pt_win32_hotkeys_dispose;
}

PtWin32Hotkeys *
pt_win32_hotkeys_new (PtWindow *win)
{
	return g_object_new (PT_WIN32_HOTKEYS_TYPE, "win", win, NULL);
}
