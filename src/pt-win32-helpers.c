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
#include "pt-win32-datacopy.h"
#include "pt-window.h"



gchar *
pt_win32_get_help_uri (void)
{
	gchar      *uri;
	gchar      *exe_path;
	gchar      *help_path;
	GRegex     *r_long;
	GRegex     *r_short;
	GMatchInfo *match;
	gchar      *l_raw;
	gchar      *l_long;
	gchar      *l_short;
	gchar      *p_long;
	gchar      *p_short;
	gchar      *path;
	GFile      *file;

	exe_path = g_win32_get_package_installation_directory_of_module (NULL);
	help_path = g_build_filename(exe_path, "share", "help", NULL);
	l_raw = g_win32_getlocale ();
	r_long = g_regex_new ("^[A-Z]*_[A-Z]*", G_REGEX_CASELESS, 0, NULL);
	r_short = g_regex_new ("^[A-Z]*", G_REGEX_CASELESS, 0, NULL);

	g_regex_match (r_long, l_raw, 0, &match);
	if (g_match_info_matches (match)) {
		l_long = g_match_info_fetch (match, 0);
		p_long = g_build_filename (help_path, l_long, APP_ID, "index.html", NULL);
	}
	g_match_info_free (match);

	g_regex_match (r_short, l_raw, 0, &match);
	if (g_match_info_matches (match)) {
		l_short = g_match_info_fetch (match, 0);
		p_short = g_build_filename (help_path, l_short, APP_ID, "index.html", NULL);
	}
	g_match_info_free (match);

	if (g_file_test (p_long, G_FILE_TEST_EXISTS))
		path = g_strdup (p_long);
	else if (g_file_test (p_short, G_FILE_TEST_EXISTS))
		path = g_strdup (p_short);
	else path = g_build_filename (help_path, "C", APP_ID, "index.html", NULL);

	file = g_file_new_for_path (path);
	uri = g_file_get_uri (file);

	g_object_unref (file);
	g_free (path);
	g_free (p_long);
	g_free (p_short);
	g_free (l_long);
	g_free (l_short);
	g_free (l_raw);
	g_regex_unref (r_long);
	g_regex_unref (r_short);
	g_free (help_path);
	g_free (exe_path);

	return uri;
}

void
pt_win32_add_audio_patterns (GtkFileFilter *filter)
{
	gtk_file_filter_add_pattern (filter, "*.aac");
	gtk_file_filter_add_pattern (filter, "*.aif");
	gtk_file_filter_add_pattern (filter, "*.aiff");
	gtk_file_filter_add_pattern (filter, "*.flac");
	gtk_file_filter_add_pattern (filter, "*.iff");
	gtk_file_filter_add_pattern (filter, "*.mpa");
	gtk_file_filter_add_pattern (filter, "*.mp3");
	gtk_file_filter_add_pattern (filter, "*.m4a");
	gtk_file_filter_add_pattern (filter, "*.oga");
	gtk_file_filter_add_pattern (filter, "*.ogg");
	gtk_file_filter_add_pattern (filter, "*.wav");
	gtk_file_filter_add_pattern (filter, "*.wma");
}

gboolean
pt_win32_present_other_instance (GApplication *app)
{
	/* In an MSYS2 environment we have D-Bus and everything works like on
	 * Linux. The installer doesn't have D-Bus though and we have to
	 * ensure uniqueness in a different way. */

	GDBusConnection *dbus;
	dbus = g_application_get_dbus_connection (app);
	if (dbus)
		return FALSE;

	HWND other_instance = FindWindow (PT_HIDDEN_WINDOW_CLASS,
	                                  PT_HIDDEN_WINDOW_TITLE);
	if (other_instance) {
		COPYDATASTRUCT data;
		data.dwData = PRESENT_WINDOW;
		data.cbData = 0;
		data.lpData = NULL;
		SendMessage ((HWND) other_instance, WM_COPYDATA,
		             (WPARAM) 0,
		             (LPARAM) (LPVOID) &data);
		return TRUE;
	}

	return FALSE;
}

gboolean
pt_win32_open_in_other_instance (GApplication *app,
				 gchar        *uri)
{
	GDBusConnection *dbus;
	dbus = g_application_get_dbus_connection (app);
	if (dbus)
		return FALSE;

	HWND other_instance = FindWindow (PT_HIDDEN_WINDOW_CLASS,
	                                  PT_HIDDEN_WINDOW_TITLE);
	if (other_instance) {
		COPYDATASTRUCT data;
		data.dwData = OPEN_FILE;
		data.cbData = sizeof (char) * strlen (uri);
		data.lpData = uri;
		SendMessage ((HWND) other_instance, WM_COPYDATA,
		             (WPARAM) 0,
		             (LPARAM) (LPVOID) &data);
		g_free (uri);
		return TRUE;
	}

	return FALSE;
}


gchar*
pt_win32_get_last_error_msg (void)
{
	gchar *msg;
	LPVOID lpMsgBuf;

	FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER
	                 | FORMAT_MESSAGE_FROM_SYSTEM
	                 | FORMAT_MESSAGE_IGNORE_INSERTS,
	               NULL,
	               GetLastError (),
	               MAKELANGID (LANG_NEUTRAL, SUBLANG_NEUTRAL),
	               (LPTSTR) &lpMsgBuf,
	               0,
	               NULL);

	msg = g_strdup ((gchar*) lpMsgBuf);
	LocalFree (lpMsgBuf);
	return msg;
}
