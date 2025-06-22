/* Copyright 2022 Gabor Karsay <gabor.karsay@gmx.at>
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

#include <adwaita.h>
#include <editor-theme-selector-private.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <locale.h> /* setlocale */
#include <pt-app.h>
#include <pt-asr-dialog.h>
#include <pt-preferences.h>
#include <pt-window-private.h>
#include <pt-window.h>

static GMainLoop *loop;
static GMainLoop *other_loop;
static char      *arg_output_dir = NULL;
static char      *output_dir = NULL;
static gboolean   save_node = FALSE;
static char      *mp3_example_uri;

static const GOptionEntry test_args[] = {
  { "output", 'o',
    G_OPTION_FLAG_NONE,
    G_OPTION_ARG_FILENAME, &arg_output_dir,
    "Directory to save image files to", "DIR" },
  { "save-node", 's',
    G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, &save_node,
    "Save render node", NULL },
  { NULL }
};

static gboolean
parse_command_line (int *argc, char ***argv)
{
  GError         *error = NULL;
  GOptionContext *context;
  GFile          *file;

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
  gchar           *basename;
  gchar           *sys_folder_path;
  gchar           *dest_folder_path;
  gchar           *dest_path;
  GFile           *source;
  GFile           *dest;
  GFile           *sys_folder;
  GFileEnumerator *files;
  GError          *error = NULL;

  sys_folder_path = g_build_path (G_DIR_SEPARATOR_S, PT_SOURCE_DIR, "data", "asr", NULL);
  sys_folder = g_file_new_for_path (sys_folder_path);
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
quit_loop (gpointer loop)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static void
save_paintable (GdkPaintable *paintable,
                gpointer      user_data)
{
  char           *filename = user_data;
  char           *full_filename;
  GdkTexture     *texture = NULL;
  GtkSnapshot    *snapshot;
  GskRenderNode  *node;
  GskRenderer    *renderer;
  GtkWidget      *widget, *parent;
  GtkNative      *native;
  gint            x, y, width, height;
  graphene_rect_t bounds;

  GskRenderNode *original_node;
  g_signal_handlers_disconnect_by_func (paintable, save_paintable, user_data);

  widget = gtk_widget_paintable_get_widget (GTK_WIDGET_PAINTABLE (paintable));
  parent = GTK_WIDGET (gtk_widget_get_root (widget));
  native = gtk_widget_get_native (widget);

  snapshot = gtk_snapshot_new ();

  if (GTK_IS_POPOVER (widget))
    {
      /* Snapshot parent first */
      g_assert (gtk_widget_compute_bounds (parent, parent, &bounds));

      GdkSurface   *surface;
      double        transform_x, transform_y;
      GdkPaintable *paintable_parent = gtk_widget_paintable_new (parent);
      GdkSurface   *surface_from, *surface_to;
      GtkNative    *native_parent;
      double        xp, yp;

      surface = gtk_native_get_surface (GTK_NATIVE (parent));
      gtk_native_get_surface_transform (GTK_NATIVE (parent),
                                        &transform_x, &transform_y);
      x = floor (transform_x);
      y = floor (transform_y);
      width = gdk_surface_get_width (surface);
      height = gdk_surface_get_height (surface);

      gdk_paintable_snapshot (paintable_parent, snapshot, bounds.size.width, bounds.size.height);

      /* Get border, looks clumsy, but I don't know better */
      GskRenderNode *parent_node = gtk_snapshot_free_to_node (snapshot);
      original_node = g_steal_pointer (&parent_node);
      parent_node = gsk_render_node_ref (gsk_clip_node_get_child (original_node));
      gsk_render_node_unref (original_node);
      snapshot = gtk_snapshot_new ();
      gtk_snapshot_append_node (snapshot, parent_node);

      /* Get coordinates of popover, take offset into account */
      native_parent = gtk_widget_get_native (parent);
      surface_from = gtk_native_get_surface (native);
      surface_to = gtk_native_get_surface (native_parent);
      gdk_surface_translate_coordinates (surface_from, surface_to, &xp, &yp);
      xp -= transform_x;
      yp -= transform_y;

      /* Change offset of snapshot to the place where the popover should be */
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (xp, yp));
    }

  if (GTK_IS_NATIVE (widget))
    {
      g_assert (gtk_widget_compute_bounds (widget, widget, &bounds));

      GdkSurface *surface;
      double      transform_x, transform_y;

      surface = gtk_native_get_surface (GTK_NATIVE (widget));
      gtk_native_get_surface_transform (GTK_NATIVE (widget),
                                        &transform_x, &transform_y);
      x = floor (transform_x);
      y = floor (transform_y);
      width = gdk_surface_get_width (surface);
      height = gdk_surface_get_height (surface);
    }
  else
    {
      g_assert (gtk_widget_compute_bounds (widget, parent, &bounds));

      x = gtk_widget_get_margin_start (widget);
      y = gtk_widget_get_margin_top (widget);
      width = bounds.size.width + x + gtk_widget_get_margin_end (widget);
      height = bounds.size.height + y + gtk_widget_get_margin_bottom (widget);
      g_timeout_add (300, quit_loop, other_loop);
      g_main_loop_run (other_loop);
    }

  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
  gdk_paintable_snapshot (paintable, snapshot, bounds.size.width, bounds.size.height);

  /* Get node, save it */
  node = gtk_snapshot_free_to_node (snapshot);
  if (save_node)
    {
      GError *error = NULL;
      char   *dataname = g_strdup (filename);
      char   *ext = g_strrstr (dataname, ".png");
      g_strlcpy (ext, ".dat", sizeof (ext));
      char *node_full_filename = g_build_filename (output_dir, dataname, NULL);
      if (!gsk_render_node_write_to_file (node, node_full_filename, &error))
        {
          g_printerr ("Error writing node to %s: %s\n", node_full_filename, error->message);
          g_clear_error (&error);
        }
      g_free (node_full_filename);
      g_free (dataname);
    }

  /* Shadow is a GskClipNode, we need its child */
  if (gsk_render_node_get_node_type (node) == GSK_CLIP_NODE)
    {
      original_node = g_steal_pointer (&node);
      node = gsk_render_node_ref (gsk_clip_node_get_child (original_node));
      gsk_render_node_unref (original_node);
    }

  /* Render and save screenshot */
  renderer = gtk_native_get_renderer (native);

  if (GTK_IS_POPOVER (widget))
    texture = gsk_renderer_render_texture (renderer, node, NULL);
  else
    texture = gsk_renderer_render_texture (renderer, node, &GRAPHENE_RECT_INIT (0, 0, width, height));

  full_filename = g_build_filename (output_dir, filename, NULL);
  g_print ("save screenshot: %s\n", full_filename);
  gdk_texture_save_to_png (texture, full_filename);

  g_free (full_filename);
  g_object_unref (texture);
  gsk_render_node_unref (node);

  g_idle_add (quit_loop, loop);
}

static void
waveviewer_load_progress (GtkWidget *waveviewer,
                          double     fraction,
                          gpointer   user_data)
{
  if (fraction == 1)
    {
      g_signal_handlers_disconnect_by_func (waveviewer, waveviewer_load_progress, user_data);
      g_idle_add (quit_loop, loop);
    }
}

#ifdef HAVE_ASR
static void
save_paintable_after_config_update (PtPreferencesDialog *dlg,
                                    gpointer             user_data)
{
  GdkPaintable *paintable = GDK_PAINTABLE (user_data);

  g_signal_handlers_disconnect_by_func (dlg, save_paintable_after_config_update, user_data);
  g_signal_connect (paintable, "invalidate-contents",
                    G_CALLBACK (save_paintable), "asr-setup-downloadable.png");
}
#endif

static void
take_screenshots (GtkApplication *app,
                  gpointer        user_data)
{
  GList        *windows;
  PtWindow     *win;
  PtPlayer     *player;
  GtkWidget    *waveviewer;
  GdkPaintable *paintable;

  /* Initialize static variables */
  loop = g_main_loop_new (NULL, FALSE);
  other_loop = g_main_loop_new (NULL, FALSE);

  gchar *filename = g_build_filename (PT_SOURCE_DIR, "tools", "resources", "I have a dream.mp3", NULL);
  GFile *file = g_file_new_for_path (filename);
  mp3_example_uri = g_file_get_uri (file);

  g_object_unref (file);
  g_free (filename);

  g_object_set (gtk_settings_get_default (),
                "gtk-enable-animations", FALSE,
                "gtk-font-name", "Cantarell 11",
                "gtk-icon-theme-name", "Adwaita",
                "gtk-decoration-layout", ":close",
                "gtk-hint-font-metrics", TRUE,
                NULL);

  windows = gtk_application_get_windows (app);
  win = PT_WINDOW (windows->data);
  waveviewer = _pt_window_get_waveviewer (win);
  player = _pt_window_get_player (win);

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

#ifdef HAVE_ASR
  /* Preferences Dialog, asr page without models ---------------------------- */

  PtPreferencesDialog *dlg;

  gtk_window_set_default_size (GTK_WINDOW (win), 1000, 2000);
  dlg = pt_preferences_dialog_new ();
  paintable = gtk_widget_paintable_new (GTK_WIDGET (dlg));
  adw_dialog_present (ADW_DIALOG (dlg), GTK_WIDGET (win));
  g_signal_connect (paintable, "invalidate-contents",
                    G_CALLBACK (save_paintable), "asr-setup-initial.png");
  adw_preferences_dialog_set_visible_page_name (ADW_PREFERENCES_DIALOG (dlg),
                                                "asr_page");
  g_main_loop_run (loop);

  adw_dialog_close (ADW_DIALOG (dlg));
  g_object_unref (paintable);

  /* Preferences Dialog, asr page with models ------------------------------- */

  copy_asr_configs ();

  /* get config */
  gchar    *config_name = g_build_filename (g_get_user_config_dir (), PACKAGE_NAME, "de.sphinx.bartsch.voxforge.asr", NULL);
  GFile    *config_file = g_file_new_for_path (config_name);
  PtConfig *config = pt_config_new (config_file);
  g_assert (pt_config_is_valid (config));

  /* fake install config and set as active */
  gchar *model_path = g_build_path (G_DIR_SEPARATOR_S, PT_BUILD_DIR, "tools", "parlatype", NULL);
  pt_config_set_base_folder (config, model_path);
  g_assert (pt_config_is_installed (config));
  GSettings *editor = _pt_window_get_settings (win);
  g_settings_set_string (editor, "asr-config", config_name);

  dlg = pt_preferences_dialog_new ();
  paintable = gtk_widget_paintable_new (GTK_WIDGET (dlg));
  g_signal_connect (dlg, "configs-updated",
                    G_CALLBACK (save_paintable_after_config_update), paintable);
  adw_preferences_dialog_set_visible_page_name (ADW_PREFERENCES_DIALOG (dlg),
                                                "asr_page");
  adw_dialog_present (ADW_DIALOG (dlg), GTK_WIDGET (win));
  g_main_loop_run (loop);

  adw_dialog_close (ADW_DIALOG (dlg));
  g_object_unref (paintable);

  /* ASR Model dialog ------------------------------------------------------- */

  /* show dialog with config */
  PtAsrDialog *asr_dlg;
  asr_dlg = pt_asr_dialog_new ();
  paintable = gtk_widget_paintable_new (GTK_WIDGET (asr_dlg));
  g_signal_connect (paintable, "invalidate-contents",
                    G_CALLBACK (save_paintable), "asr-setup-details.png");
  pt_asr_dialog_set_config (asr_dlg, config);
  adw_dialog_present (ADW_DIALOG (asr_dlg), GTK_WIDGET (win));
  g_main_loop_run (loop);
  adw_dialog_close (ADW_DIALOG (asr_dlg));

  /* Main window with primary menu ------------------------------------------ */

  gtk_window_set_default_size (GTK_WINDOW (win), 600, 350);
  g_timeout_add (300, quit_loop, loop);
  g_main_loop_run (loop);

  GtkWidget  *menu_button = _pt_window_get_primary_menu_button (win);
  GtkPopover *popover = gtk_menu_button_get_popover (GTK_MENU_BUTTON (menu_button));
  gtk_popover_set_autohide (popover, false); /* essential */

  paintable = gtk_widget_paintable_new (GTK_WIDGET (popover));
  g_signal_connect (paintable, "invalidate-contents",
                    G_CALLBACK (save_paintable), "asr-switch-to-asr.png");
  gtk_menu_button_popup (GTK_MENU_BUTTON (menu_button));
  g_main_loop_run (loop);
  g_object_unref (paintable);

  /* reset config */
  g_settings_set_string (editor, "asr-config", "");

  g_object_unref (config);
  g_object_unref (config_file);
  g_free (config_name);
  g_free (model_path);
#endif

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

  g_setenv ("ADW_DEBUG_COLOR_SCHEME", "default", TRUE);
  g_setenv ("ADW_DEBUG_HIGH_CONTRAST", "0", TRUE);
  g_setenv ("ADW_DEBUG_ACCENT_COLOR", "blue", TRUE);

  PtApp *app;
  int    app_status;

  app = pt_app_new ();
  g_signal_connect_after (app, "activate", G_CALLBACK (take_screenshots), NULL);
  app_status = g_application_run (G_APPLICATION (app), 0, NULL);

  g_object_unref (app);

  return app_status;
}
