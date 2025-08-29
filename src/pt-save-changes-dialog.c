/* Copyright 2021 Christian Hergert <chergert@redhat.com>
 * Copyright 2025 Gabor Karsay <gabor.karsay@gmx.at>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Taken from gnome-text-editor/src/editor-save-changes-dialog.c */

#include "config.h"

#include "pt-save-changes-dialog.h"

#include "pt-utils.h"
#include "pt-window.h"

#include <adwaita.h>
#include <glib/gi18n.h>

typedef struct
{
  PtTextView     *text_view;
  GFile          *file;
  AdwAlertDialog *dialog;
  GtkWindow      *parent;
} SaveRequest;

static void
save_request_clear (gpointer data)
{
  SaveRequest *sr = data;

  g_clear_object (&sr->text_view);
  g_clear_object (&sr->dialog);
}

static void pt_save_changes_dialog_save (AdwAlertDialog *dialog, SaveRequest *sr);

static void
pt_save_changes_dialog_save_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  PtTextView *text_view = PT_TEXT_VIEW (object);
  GError     *error = NULL;
  AdwDialog  *dialog = ADW_DIALOG (user_data);
  GTask      *task = g_object_get_data (G_OBJECT (dialog), "TASK");

  if (!pt_text_view_save_finish (text_view, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
file_chooser_cb (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  SaveRequest *sr = user_data;

  sr->file = gtk_file_dialog_save_finish (GTK_FILE_DIALOG (source), result, NULL);

  if (sr->file == NULL)
    {
      GTask *task = g_object_get_data (G_OBJECT (sr->dialog), "TASK");
      g_task_return_boolean (task, FALSE);
      return;
    }

  pt_save_changes_dialog_save (sr->dialog, sr);
}

static void
pt_save_changes_dialog_save (AdwAlertDialog *dialog,
                             SaveRequest    *sr)
{
  gtk_widget_set_sensitive (GTK_WIDGET (dialog), FALSE);

  if (sr->file == NULL)
    {
      GtkFileDialog *file_chooser = gtk_file_dialog_new ();
      gtk_file_dialog_set_modal (file_chooser, TRUE);

      gtk_file_dialog_set_title (file_chooser, _ ("Save as"));
      GListModel *filters = pt_file_filters_new (PT_FILTER_TEXT, PT_FILTER_OPEN_DOCUMENT, PT_FILTER_ALL, 0);

      gtk_file_dialog_set_filters (file_chooser, filters);

      const char *home_path = g_get_home_dir ();
      GFile      *initial_folder = g_file_new_for_path (home_path);
      gtk_file_dialog_set_initial_folder (file_chooser, initial_folder);

      gtk_file_dialog_save (file_chooser, sr->parent,
                            NULL,
                            file_chooser_cb,
                            sr);

      g_object_unref (initial_folder);
      g_object_unref (file_chooser);
      return;
    }

  pt_text_view_save_async (sr->text_view,
                           sr->file,
                           NULL,
                           pt_save_changes_dialog_save_cb,
                           dialog);
}

static void
pt_save_changes_dialog_response (AdwAlertDialog *dialog,
                                 const char     *response,
                                 SaveRequest    *sr)
{
  GTask *task;

  if (!g_strcmp0 (response, "discard"))
    {
      task = g_object_get_data (G_OBJECT (dialog), "TASK");
      g_task_return_boolean (task, TRUE);
    }
  else if (!g_strcmp0 (response, "save"))
    {
      pt_save_changes_dialog_save (dialog, sr);
    }
  else
    {
      task = g_object_get_data (G_OBJECT (dialog), "TASK");
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CANCELLED,
                               "The user cancelled the request");
    }
}

static AdwDialog *
pt_save_changes_dialog_new (GtkWindow  *parent,
                            PtTextView *text_view)
{
  AdwDialog *dialog;

  g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);

  dialog = adw_alert_dialog_new (_ ("Save Changes?"),
                                 _ ("Open documents contain unsaved changes. Changes which are not saved will be permanently lost."));
  gtk_widget_add_css_class (GTK_WIDGET (dialog), "save-changes");

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "cancel", _ ("_Cancel"),
                                  "discard", _ ("_Discard"),
                                  "save", _ ("_Save"),
                                  NULL);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
                                            "discard", ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
                                            "save", ADW_RESPONSE_SUGGESTED);

  adw_alert_dialog_set_prefer_wide_layout (ADW_ALERT_DIALOG (dialog), TRUE);

  SaveRequest *sr = g_new (SaveRequest, 1);
  sr->text_view = g_object_ref (text_view);
  sr->dialog = g_object_ref (ADW_ALERT_DIALOG (dialog));
  sr->parent = parent;
  sr->file = (void *) pt_text_view_get_text_file (text_view);

  g_signal_connect_data (dialog,
                         "response",
                         G_CALLBACK (pt_save_changes_dialog_response),
                         sr,
                         (GClosureNotify) save_request_clear,
                         0);

  return dialog;
}

void
pt_save_changes_dialog_run_async (GtkWindow          *parent,
                                  PtTextView         *text_view,
                                  GCancellable       *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer            user_data)
{
  GTask     *task;
  AdwDialog *dialog;

  g_return_if_fail (GTK_IS_WINDOW (parent));
  g_return_if_fail (PT_IS_TEXT_VIEW (text_view));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  dialog = pt_save_changes_dialog_new (parent, text_view);
  task = g_task_new (parent, cancellable, callback, user_data);
  g_task_set_source_tag (task, pt_save_changes_dialog_run_async);

  g_object_set_data_full (G_OBJECT (dialog),
                          "TASK",
                          g_steal_pointer (&task),
                          g_object_unref);
  adw_dialog_present (dialog, GTK_WIDGET (parent));
}

gboolean
pt_save_changes_dialog_run_finish (GAsyncResult *result,
                                   GError      **error)
{
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
