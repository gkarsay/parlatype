/* Copyright 2026 Gabor Karsay <gabor.karsay@gmx.at>
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

#define G_LOG_DOMAIN "ptp-test-buttons"

#include "config.h"

#include "ptp-test-buttons.h"

#include "pt-window-addin.h"
#include "pt-window.h"

struct _PtpTestButtons
{
  GObject parent_instance;

  PtWindow  *window;
  GtkWidget *doc_controls;
  GtkWidget *load_button;
  GtkWidget *save_button;
  GFile     *test_file;
};

static void pt_window_addin_iface_init (PtWindowAddinInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (PtpTestButtons, ptp_test_buttons, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (PT_TYPE_WINDOW_ADDIN, pt_window_addin_iface_init))

static void
ptp_test_buttons_unload (PtWindowAddin *addin)
{
  PtpTestButtons *self = PTP_TEST_BUTTONS (addin);

  gtk_box_remove (GTK_BOX (self->doc_controls), self->load_button);
  gtk_box_remove (GTK_BOX (self->doc_controls), self->save_button);
  g_object_unref (self->test_file);
}

static void
load_button_clicked_cb (GtkButton *button,
                        gpointer   user_data)
{
  PtpTestButtons *self = PTP_TEST_BUTTONS (user_data);

  pt_window_load_document (self->window, self->test_file);
}

static void
save_button_clicked_cb (GtkButton *button,
                        gpointer   user_data)
{
  PtpTestButtons *self = PTP_TEST_BUTTONS (user_data);

  pt_window_save (self->window, self->test_file);
}

static void
ptp_test_buttons_load (PtWindowAddin *addin,
                       PtWindow      *window)
{
  PtpTestButtons *self = PTP_TEST_BUTTONS (addin);
  self->window = window;
  self->doc_controls = pt_window_get_doc_controls (self->window);

  GSettings *settings = g_settings_new (APP_ID ".test-buttons");
  char      *test_path = g_settings_get_string (settings, "test-file");
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "test-file: %s", test_path);
  self->test_file = g_file_new_for_path (test_path);
  g_object_unref (settings);
  g_free (test_path);

  self->load_button = gtk_button_new_with_label ("Load testfile");
  self->save_button = gtk_button_new_with_label ("Save testfile");

  gtk_box_append (GTK_BOX (self->doc_controls), self->load_button);
  gtk_box_append (GTK_BOX (self->doc_controls), self->save_button);

  g_signal_connect (self->load_button, "clicked", G_CALLBACK (load_button_clicked_cb), self);
  g_signal_connect (self->save_button, "clicked", G_CALLBACK (save_button_clicked_cb), self);
}

static void
pt_window_addin_iface_init (PtWindowAddinInterface *iface)
{
  iface->load = ptp_test_buttons_load;
  iface->unload = ptp_test_buttons_unload;
}

static void
ptp_test_buttons_init (PtpTestButtons *self)
{
}

static void
ptp_test_buttons_class_init (PtpTestButtonsClass *klass)
{
}
