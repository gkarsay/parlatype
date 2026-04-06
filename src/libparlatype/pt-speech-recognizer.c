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
   * PtSpeechRecognizer:player
   *
   * Player to get audio from.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("player",
                                                            "Player",
                                                            "Player",
                                                            PT_TYPE_PLAYER,
                                                            G_PARAM_READWRITE |
                                                                G_PARAM_CONSTRUCT_ONLY |
                                                                G_PARAM_STATIC_STRINGS));

  /**
   * PtSpeechRecognizer:textview
   *
   * Text view to write results to.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("textview",
                                                            "Textview",
                                                            "Textview",
                                                            PT_TYPE_TEXT_VIEW,
                                                            G_PARAM_READWRITE |
                                                                G_PARAM_CONSTRUCT_ONLY |
                                                                G_PARAM_STATIC_STRINGS));
}

/**
 * pt_speech_recognizer_load:
 * @self: a #PtSpeechRecognizer
 *
 * ASR plugins should prepare for speech recognition here
 */
void
pt_speech_recognizer_load (PtSpeechRecognizer *self)
{
  PtSpeechRecognizerInterface *iface;

  g_return_if_fail (PT_IS_SPEECH_RECOGNIZER (self));

  iface = PT_SPEECH_RECOGNIZER_GET_IFACE (self);
  g_return_if_fail (iface->load != NULL);

  iface->load (self);
}

/**
 * pt_speech_recognizer_unload:
 * @self: an #PtSpeechRecognizer
 *
 * Plugins should undo loading here.
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
