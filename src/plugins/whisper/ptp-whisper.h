#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define PTP_TYPE_WHISPER (ptp_whisper_get_type())

G_DECLARE_FINAL_TYPE (PtpWhisper, ptp_whisper, PTP, WHISPER, GObject)


G_END_DECLS
