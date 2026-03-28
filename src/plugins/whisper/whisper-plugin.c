#include "ptp-whisper.h"

#include <libpeas.h>
#include <parlatype.h>

void
ptp_whisper_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              PT_TYPE_ASR_IFACE,
                                              PTP_TYPE_WHISPER);
}
