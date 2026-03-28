#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define PTP_TYPE_POCKETSPHINX (ptp_pocketsphinx_get_type())

G_DECLARE_FINAL_TYPE (PtpPocketsphinx, ptp_pocketsphinx, PTP, POCKETSPHINX, GObject)

/*
 * Boxing of decoder.
 */
#define PS_DECODER_TYPE (ps_decoder_get_type ())
GType ps_decoder_get_type (void);

G_END_DECLS
