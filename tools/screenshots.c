/* Copyright (C) Gabor Karsay 2022 <gabor.karsay@gmx.at>
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
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <locale.h> /* setlocale */
#include <pt-app.h>
#include <pt-asr-dialog.h>
#include <pt-preferences.h>
#include <pt-window-private.h>
#include <pt-window.h>

static GMainLoop *loop;
static GMainLoop *other_loop;
static char *arg_output_dir = NULL;
static char *output_dir = NULL;
static char *mp3_example_uri;

static const GOptionEntry test_args[] = {
  { "output", 'o',
    G_OPTION_FLAG_NONE,
    G_OPTION_ARG_FILENAME, &arg_output_dir,
    "Directory to save image files to", "DIR" },
  { NULL }
};

static gboolean
parse_command_line (int *argc, char ***argv)
{
  GError *error = NULL;
  GOptionContext *context;
  GFile *file;

  context = g_option_context_new ("- take Parlatype screenshots");
  g_option_context_add_main_entries (context, test_args, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, argc, argv, &error))
    {
      g_printerr ("option parsing failed: %s\n", error->message);
      g_option_context_free (context);
      g_error_free (error);
      return FALSE;
    }

  g_option_context_free (context);

  if (arg_output_dir)
    {
      file = g_file_new_for_commandline_arg (arg_output_dir);
      g_free (arg_output_dir);

      if (!g_file_make_directory_with_parents (file, NULL, &error))
        {
          if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
            {
              g_printerr ("creating directory failed: %s\n", error->message);
              g_error_free (error);
              g_object_unref (file);
              return FALSE;
            }
          g_error_free (error);
        }

      output_dir = g_file_get_path (file);
      g_object_unref (file);
    }
  else
    {
      g_printerr ("output directory is mandatory\n");
      return FALSE;
    }

  return TRUE;
}

#ifdef HAVE_ASR
static void
copy_asr_configs (void)
{
  gchar *basename;
  gchar *sys_folder_path;
  gchar *dest_folder_path;
  gchar *dest_path;
  GFile *source;
  GFile *dest;
  GFile *sys_folder;
  GFileEnumerator *files;
  GError *error = NULL;

  sys_folder_path = g_build_path (G_DIR_SEPARATOR_S, PT_SOURCE_DIR, "data", "asr", NULL);
  sys_folder = g_file_new_for_path (ASR_DIR);
  dest_folder_path = g_build_path (G_DIR_SEPARATOR_S,
                                   g_get_user_config_dir (),
                                   PACKAGE_NAME, NULL);

  files = g_file_enumerate_children (sys_folder,
                                     G_FILE_ATTRIBUTE_STANDARD_NAME,
                                     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                     NULL, /* cancellable */
                                     &error);

  while (!error)
    {
      GFileInfo *info;
      if (!g_file_enumerator_iterate (files,
                                      &info,   /* no free necessary */
                                      &source, /* no free necessary */
                                      NULL,    /* cancellable       */
                                      &error))
        /* this is an error */
        break;

      if (!info)
        /* this is the end of enumeration */
        break;

      basename = g_file_get_basename (source);
      if (g_str_has_suffix (basename, ".asr"))
        {
          dest_path = g_build_path (G_DIR_SEPARATOR_S,
                                    dest_folder_path,
                                    basename, NULL);
          dest = g_file_new_for_path (dest_path);
          g_file_copy (source,
                       dest,
                       G_FILE_COPY_TARGET_DEFAULT_PERMS,
                       NULL, /* cancellable            */
                       NULL, /* progress callback      */
                       NULL, /* progress callback data */
                       &error);
          g_free (dest_path);
          g_object_unref (dest);

          if (error && g_error_matches (error, G_IO_ERROR,
                                        G_IO_ERROR_EXISTS))
            {
              g_clear_error (&error);
            }
        }
      g_free (basename);
    }

  g_clear_object (&files);
  g_object_unref (sys_folder);
  g_free (dest_folder_path);
  g_free (sys_folder_path);
}
#endif

static gboolean
quit_when_idle (gpointer loop)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static void
save_paintable (GdkPaintable *paintable,
                gpointer user_data)
{
  char *filename = user_data;
  char *full_filename;
  GdkTexture *texture = NULL;
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  GskRenderer *renderer;
  GtkWidget *widget;
  GtkNative *native;
  gint width, height;
  gboolean popover;

  GskShadow backdrop_shadow = { .color = { 0, 0, 0, .alpha = 0.30 }, .dx = 0, .dy = 1, .radius = 1 };
  GskRenderNode *original_node;
  g_signal_handlers_disconnect_by_func (paintable, save_paintable, user_data);

  widget = gtk_widget_paintable_get_widget (GTK_WIDGET_PAINTABLE (paintable));
  native = gtk_widget_get_native (widget);
  popover = GTK_IS_POPOVER (widget);
  snapshot = gtk_snapshot_new ();

  if (popover)
    {
      /* Snapshot parent window */
      GtkWidget *parent;
      GdkPaintable *paintable_parent;
      GtkNative *native_parent;
      gint parent_width, parent_height;
      GdkSurface *surface_from, *surface_to;
      double xp, yp, xp_offset, yp_offset;

      /* wait for parent window */
      parent = GTK_WIDGET (gtk_widget_get_root (widget));
      paintable_parent = gtk_widget_paintable_new (parent);
      g_signal_connect_swapped (paintable_parent, "invalidate-contents",
                                G_CALLBACK (g_main_loop_quit), other_loop);
      g_main_loop_run (other_loop);

      parent_width = gdk_paintable_get_intrinsic_width (paintable_parent);
      parent_height = gdk_paintable_get_intrinsic_height (paintable_parent);
      gdk_paintable_snapshot (paintable_parent,
                              snapshot,
                              parent_width,
                              parent_height);

      gtk_snapshot_push_shadow (snapshot, &backdrop_shadow, 1);
      gtk_snapshot_pop (snapshot);

      /* Get border, looks clumsy, but I don't know better */
      GskRenderNode *parent_node = gtk_snapshot_free_to_node (snapshot);
      original_node = g_steal_pointer (&parent_node);
      parent_node = gsk_render_node_ref (gsk_clip_node_get_child (original_node));
      gsk_render_node_unref (original_node);
      snapshot = gtk_snapshot_new ();
      gtk_snapshot_append_node (snapshot, parent_node);

      /* There is some kind of offset between surface and widget coordinates */
      native_parent = gtk_widget_get_native (parent);
      gtk_native_get_surface_transform (native_parent, &xp_offset, &yp_offset);

      /* Get coordinates of popover, take offset into account */
      surface_from = gtk_native_get_surface (native);
      surface_to = gtk_native_get_surface (native_parent);
      gdk_surface_translate_coordinates (surface_from, surface_to, &xp, &yp);
      xp -= xp_offset;
      yp -= yp_offset;

      /* Change offset of snapshot to the place where the popover should be */
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (xp, yp));

      /* now wait for popover */
      g_signal_connect_swapped (paintable, "invalidate-contents",
                                G_CALLBACK (g_main_loop_quit), other_loop);
      g_main_loop_run (other_loop);
    }

  /* snapshot widget (might be a popover) */
  width = gdk_paintable_get_intrinsic_width (paintable);
  height = gdk_paintable_get_intrinsic_height (paintable);
  gdk_paintable_snapshot (paintable,
                          snapshot,
                          width,
                          height);

  /* Add backdrop shadow */
  gtk_snapshot_push_shadow (snapshot, &backdrop_shadow, 1);
  gtk_snapshot_pop (snapshot);

  /* Get node, shadow is a GskClipNode */
  node = gtk_snapshot_free_to_node (snapshot);
  if (gsk_render_node_get_node_type (node) == GSK_CLIP_NODE)
    {
      original_node = g_steal_pointer (&node);
      node = gsk_render_node_ref (gsk_clip_node_get_child (original_node));
      gsk_render_node_unref (original_node);
    }

  /* Render and save screenshot */
  renderer = gtk_native_get_renderer (native);
  texture = gsk_renderer_render_texture (renderer, node, NULL);

  full_filename = g_build_filename (output_dir, filename, NULL);
  g_print ("save screenshot: %s\n", full_filename);
  gdk_texture_save_to_png (texture, full_filename);

  g_free (full_filename);
  g_object_unref (texture);
  gsk_render_node_unref (node);

  g_idle_add (quit_when_idle, loop);
}

static void
waveviewer_load_progress (GtkWidget *waveviewer,
                          double fraction,
                          gpointer user_data)
{
  if (fraction == 1)
    {
      g_signal_handlers_disconnect_by_func (waveviewer, waveviewer_load_progress, user_data);
      g_idle_add (quit_when_idle, loop);
    }
}

static void
take_screenshots (GtkApplication *app,
                  gpointer user_data)
{
  GList *windows;
  PtWindow *win;
  PtPlayer *player;
  GtkWidget *waveviewer;
  PtPreferencesDialog *dlg;
  GdkPaintable *paintable;

  /* Initialize static variables */
  loop = g_main_loop_new (NULL, FALSE);
  other_loop = g_main_loop_new (NULL, FALSE);

  gchar *filename = g_build_filename (PT_SOURCE_DIR, "tools", "resources", "I have a dream.mp3", NULL);
  GFile *file = g_file_new_for_path (filename);
  mp3_example_uri = g_file_get_uri (file);

  g_object_unref (file);
  g_free (filename);

  windows = gtk_application_get_windows (app);
  win = PT_WINDOW (windows->data);
  waveviewer = PT_WINDOW (win)->waveviewer;
  player = PT_WINDOW (win)->player;

  /* Main window ------------------------------------------------------------ */

  gtk_window_set_default_size (GTK_WINDOW (win), 600, 350);

  g_signal_connect (waveviewer, "load-progress",
                    G_CALLBACK (waveviewer_load_progress), NULL);
  pt_window_open_file (win, mp3_example_uri);

  /* Wait until waveform is fully loaded */
  g_main_loop_run (loop);

  /* Select position, wait for cursor, will be on the left */
  pt_player_jump_to_position (player, 177400);

  gint64 pos = 0;
  while (pos != 177400)
    {
      g_main_context_iteration (NULL, FALSE);
      g_object_get (waveviewer, "playback-cursor", &pos, NULL);
    }

  /* Select position, cursor is somewhere in the middle */
  pt_player_jump_to_position (player, 179650);

  paintable = gtk_widget_paintable_new (GTK_WIDGET (win));
  g_signal_connect (paintable, "invalidate-contents",
                    G_CALLBACK (save_paintable), "parlatype-main-window.png");
  g_main_loop_run (loop);
  g_object_unref (paintable);

  /* Preferences Dialog, first page ----------------------------------------- */

  dlg = pt_preferences_dialog_new (GTK_WINDOW (win));
  paintable = gtk_widget_paintable_new (GTK_WIDGET (dlg));
  g_signal_connect (paintable, "invalidate-contents",
                    G_CALLBACK (save_paintable), "parlatype-prefs-waveform.png");
  gtk_window_present (GTK_WINDOW (dlg));
  g_main_loop_run (loop);

  /* Preferences Dialog, second page ---------------------------------------- */

  g_signal_connect (paintable, "invalidate-contents",
                    G_CALLBACK (save_paintable), "parlatype-prefs-controls.png");
  adw_preferences_window_set_visible_page_name (ADW_PREFERENCES_WINDOW (dlg),
                                                "controls_page");
  g_main_loop_run (loop);

  /* Preferences Dialog, third page ----------------------------------------- */

  g_signal_connect (paintable, "invalidate-contents",
                    G_CALLBACK (save_paintable), "parlatype-prefs-timestamps.png");
  adw_preferences_window_set_visible_page_name (ADW_PREFERENCES_WINDOW (dlg),
                                                "timestamps_page");
  g_main_loop_run (loop);

#ifdef HAVE_ASR
  /* Preferences Dialog, fourth page without models ------------------------- */

  g_signal_connect (paintable, "invalidate-contents",
                    G_CALLBACK (save_paintable), "asr-setup-initial.png");
  adw_preferences_window_set_visible_page_name (ADW_PREFERENCES_WINDOW (dlg),
                                                "asr_page");
  g_main_loop_run (loop);

  gtk_window_destroy (GTK_WINDOW (dlg));
  g_object_unref (paintable);

  /* Preferences Dialog, fourth page with models ---------------------------- */

  copy_asr_configs ();

  dlg = pt_preferences_dialog_new (GTK_WINDOW (win));
  paintable = gtk_widget_paintable_new (GTK_WIDGET (dlg));
  g_signal_connect (paintable, "invalidate-contents",
                    G_CALLBACK (save_paintable), "asr-setup-downloadable.png");
  adw_preferences_window_set_visible_page_name (ADW_PREFERENCES_WINDOW (dlg),
                                                "asr_page");
  gtk_window_present (GTK_WINDOW (dlg));
  g_main_loop_run (loop);

  gtk_window_destroy (GTK_WINDOW (dlg));
  g_object_unref (paintable);

  /* ASR Model dialog ------------------------------------------------------- */

  /* get config */
  gchar *config_name = g_build_filename (g_get_user_config_dir (), PACKAGE_NAME, "de.sphinx.bartsch.voxforge.asr", NULL);
  GFile *config_file = g_file_new_for_path (config_name);
  PtConfig *config = pt_config_new (config_file);

  /* show dialog with config */
  PtAsrDialog *asr_dlg;
  asr_dlg = pt_asr_dialog_new (GTK_WINDOW (win));
  gtk_window_present (GTK_WINDOW (asr_dlg));
  paintable = gtk_widget_paintable_new (GTK_WIDGET (asr_dlg));
  g_signal_connect (paintable, "invalidate-contents",
                    G_CALLBACK (save_paintable), "asr-setup-details.png");
  pt_asr_dialog_set_config (asr_dlg, config);
  g_main_loop_run (loop);
  gtk_window_destroy (GTK_WINDOW (asr_dlg));

  /* Main window with primary menu ------------------------------------------ */

  /* fake install config and set as active */
  gchar *model_path = g_build_path (G_DIR_SEPARATOR_S, PT_BUILD_DIR, "tools", "parlatype", NULL);
  pt_config_set_base_folder (config, model_path);
  g_settings_set_string (win->editor, "asr-config", config_name);
  g_object_unref (config);
  g_object_unref (config_file);
  g_free (config_name);
  g_free (model_path);

  GtkWidget *menu_button = PT_WINDOW (win)->primary_menu_button;
  GtkPopover *popover = gtk_menu_button_get_popover (GTK_MENU_BUTTON (menu_button));
  gtk_popover_set_autohide (popover, false); /* essential */

  paintable = gtk_widget_paintable_new (GTK_WIDGET (popover));
  g_signal_connect (paintable, "invalidate-contents",
                    G_CALLBACK (save_paintable), "asr-switch-to-asr.png");
  gtk_menu_button_popup (GTK_MENU_BUTTON (menu_button));
  g_main_loop_run (loop);
#endif

  g_object_unref (paintable);
  g_free (mp3_example_uri);
  g_main_loop_unref (loop);
  g_main_loop_unref (other_loop);

  g_application_quit (G_APPLICATION (app));
}

int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE_NAME, LOCALEDIR);
  bind_textdomain_codeset (PACKAGE_NAME, "UTF-8");
  textdomain (PACKAGE_NAME);

  if (!parse_command_line (&argc, &argv))
    return 1;

  PtApp *app;
  int app_status;

  app = pt_app_new ();
  g_signal_connect_after (app, "activate", G_CALLBACK (take_screenshots), NULL);
  app_status = g_application_run (G_APPLICATION (app), 0, NULL);

  g_object_unref (app);

  return app_status;
}
