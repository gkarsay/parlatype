/* Copyright 2025 Gabor Karsay <gabor.karsay@gmx.at>
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

/**
 * PtSpeechRecognizer:
 *
 * Interface for plugins which recognize speech.
 */

#include "config.h"

#include "pt-speech-recognizer.h"

G_DEFINE_INTERFACE (PtSpeechRecognizer, pt_speech_recognizer, G_TYPE_OBJECT)

static void
pt_speech_recognizer_default_init (PtSpeechRecognizerInterface *iface)
{
  /**
   * PtSpeechRecognizer:busy
   *
   * Whether the recognizer is currently working.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("busy",
                                                             "Busy",
                                                             "Busy",
                                                             FALSE,
                                                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/**
 * pt_speech_recognizer_load:
 * @self: an #PtSpeechRecognizer
 *
 * Plugins should undo starting here.
 */
void
pt_speech_recognizer_load (PtSpeechRecognizer *self,
                           PtPlayer           *player,
                           PtTextView         *text_view,
                           GtkWidget          *asr_controls)
{
  PtSpeechRecognizerInterface *iface;

  g_return_if_fail (PT_IS_SPEECH_RECOGNIZER (self));

  iface = PT_SPEECH_RECOGNIZER_GET_IFACE (self);
  g_return_if_fail (iface->load != NULL);

  iface->load (self, player, text_view, asr_controls);
}
/**
 * pt_speech_recognizer_start:
 * @self: a #PtSpeechRecognizer
 *
 * ASR plugins should prepare for speech recognition here
 */
void
pt_speech_recognizer_start (PtSpeechRecognizer *self)
{
  PtSpeechRecognizerInterface *iface;

  g_return_if_fail (PT_IS_SPEECH_RECOGNIZER (self));

  iface = PT_SPEECH_RECOGNIZER_GET_IFACE (self);
  g_return_if_fail (iface->start != NULL);

  iface->start (self);
}

/**
 * pt_speech_recognizer_stop:
 * @self: an #PtSpeechRecognizer
 *
 * Plugins should undo starting here.
 */
void
pt_speech_recognizer_stop (PtSpeechRecognizer *self)
{
  PtSpeechRecognizerInterface *iface;

  g_return_if_fail (PT_IS_SPEECH_RECOGNIZER (self));

  iface = PT_SPEECH_RECOGNIZER_GET_IFACE (self);
  g_return_if_fail (iface->stop != NULL);

  iface->stop (self);
}

/**
 * pt_speech_recognizer_unload:
 * @self: an #PtSpeechRecognizer
 *
 * Plugins should undo starting here.
 */
void
pt_speech_recognizer_unload (PtSpeechRecognizer *self)
{
  PtSpeechRecognizerInterface *iface;

  g_return_if_fail (PT_IS_SPEECH_RECOGNIZER (self));

  iface = PT_SPEECH_RECOGNIZER_GET_IFACE (self);
  g_return_if_fail (iface->unload != NULL);

  iface->unload (self);
}
