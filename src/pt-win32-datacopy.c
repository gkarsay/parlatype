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
#include "pt-win32-helpers.h"
#include "pt-win32-datacopy.h"

struct _PtWin32DatacopyPrivate
{
	HWND hwnd;	/* Handle of hidden window */
};



G_DEFINE_TYPE_WITH_PRIVATE (PtWin32Datacopy, pt_win32_datacopy, PT_CONTROLLER_TYPE)



static void
pt_win32_datacopy_copydata_cb (HWND              window,
                               WPARAM            sender,
                               LPARAM            data,
                               PtWin32Datacopy *self)
{
	PtWindow  *ptwin  = pt_controller_get_window (PT_CONTROLLER (self));
	PtPlayer  *player = pt_controller_get_player (PT_CONTROLLER (self));
	gchar     *timestamp;
	gchar     *senddata;
	gunichar2 *senddata_utf16;
	gdouble    value;

	COPYDATASTRUCT *copydata = (COPYDATASTRUCT*) data;
	COPYDATASTRUCT  response;

	switch (copydata->dwData) {
		case PLAY_PAUSE:
			pt_player_play_pause (player);
			break;
		case GOTO_TIMESTAMP:
			timestamp = (gchar*) copydata->lpData;
			pt_player_goto_timestamp (player, timestamp);
			break;
		case JUMP_BACK:
			pt_player_jump_back (player);
			break;
		case JUMP_FORWARD:
			pt_player_jump_forward (player);
			break;
		case DECREASE_SPEED:
			g_object_get (player, "speed", &value, NULL);
			pt_player_set_speed (player, value - 0.1);
			break;
		case INCREASE_SPEED:
			g_object_get (player, "speed", &value, NULL);
			pt_player_set_speed (player, value + 0.1);
			break;
		case OPEN_FILE:
			pt_window_open_file (ptwin, (gchar*) copydata->lpData);
			/* fall through */
		case PRESENT_WINDOW:
			/* Unfortunately this only "activates" the window but
			 * doesn't bring it to the front. */
			gtk_window_present (GTK_WINDOW (ptwin));
			break;
		case GET_TIMESTAMP:
			senddata = pt_player_get_timestamp (player);
			response.dwData = RESPONSE_TIMESTAMP;
			if (!senddata)
				response.cbData = 0;
			else
				response.cbData = sizeof (TCHAR) * (strlen (senddata) + 1);
			response.lpData = senddata;
			SendMessage ((HWND) sender, WM_COPYDATA,
			             (WPARAM) (HWND) window,
			             (LPARAM) (LPVOID) &response);
			g_free (senddata);
			break;
		case GET_URI:
			senddata = pt_player_get_uri (player);
			response.dwData = RESPONSE_URI;
			if (!senddata) {
				senddata_utf16 = NULL;
				response.cbData = 0;
			} else {
				senddata_utf16 = g_utf8_to_utf16 (senddata, -1, NULL, NULL, NULL);
				response.cbData = sizeof (WCHAR) * (g_utf8_strlen (senddata, -1) + 1);
			}
			response.lpData = senddata_utf16;
			SendMessage ((HWND) sender, WM_COPYDATA,
			             (WPARAM) (HWND) window,
			             (LPARAM) (LPVOID) &response);
			g_free (senddata);
			g_free (senddata_utf16);
			break;
		default:
			break;
	}
}

static LRESULT CALLBACK
message_handler (HWND hwnd,
                 UINT Message,
                 WPARAM wParam,
                 LPARAM lParam)
{
	if (Message != WM_COPYDATA)
		return DefWindowProc (hwnd, Message, wParam, lParam);

	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                 "MESSAGE", "message handler: WM_COPYDATA");
	PtWin32Datacopy *self = (PtWin32Datacopy*)
	                  (uintptr_t) GetWindowLongPtr (hwnd, GWLP_USERDATA);
	pt_win32_datacopy_copydata_cb (hwnd, wParam, lParam, self);

	return TRUE;
}

void
pt_win32_datacopy_start (PtWin32Datacopy *self)
{
	WNDCLASS  wc;
	gchar    *err;

	memset (&wc, 0, sizeof (wc));
	wc.lpfnWndProc	 = message_handler;
	wc.hInstance	 = GetModuleHandle (NULL);
	wc.lpszClassName = PT_HIDDEN_WINDOW_CLASS;

	if (!RegisterClass (&wc)) {
		err = pt_win32_get_last_error_msg ();
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			          "MESSAGE", "RegisterClass error: %s", err);
		g_free (err);
		return;
	}

	self->priv->hwnd = CreateWindow (PT_HIDDEN_WINDOW_CLASS,
	                                 PT_HIDDEN_WINDOW_TITLE,
	                                 0,			/* window style */
	                                 0, 0,			/* x and y coordinates */
	                                 0, 0,			/* with and height */
	                                 NULL,			/* parent window */
	                                 NULL,			/* menu */
	                                 wc.hInstance,
	                                 NULL);

	if (!self->priv->hwnd) {
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
		                  "MESSAGE", "Can't create hidden window");
		UnregisterClass (PT_HIDDEN_WINDOW_CLASS, NULL);
		return;
	}

	SetWindowLongPtr (self->priv->hwnd, GWLP_USERDATA, (uintptr_t) self);
	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "Created hidden window");
}

static void
pt_win32_datacopy_init (PtWin32Datacopy *self)
{
	self->priv = pt_win32_datacopy_get_instance_private (self);
}

static void
pt_win32_datacopy_dispose (GObject *object)
{
	PtWin32Datacopy *self = PT_WIN32_DATACOPY (object);

	if (self->priv->hwnd)
		DestroyWindow (self->priv->hwnd);

	G_OBJECT_CLASS (pt_win32_datacopy_parent_class)->dispose (object);
}

static void
pt_win32_datacopy_class_init (PtWin32DatacopyClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = pt_win32_datacopy_dispose;
}

PtWin32Datacopy *
pt_win32_datacopy_new (PtWindow *win)
{
	return g_object_new (PT_WIN32_DATACOPY_TYPE, "win", win, NULL);
}
