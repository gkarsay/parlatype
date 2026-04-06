#include "ptp-pocketsphinx.h"

#include <libpeas.h>
#include <parlatype.h>

void
ptp_pocketsphinx_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              PT_TYPE_SPEECH_RECOGNIZER,
                                              PTP_TYPE_POCKETSPHINX);
}
