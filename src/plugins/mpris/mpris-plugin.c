#include "pt-window-addin.h"
#include "ptp-mpris.h"

#include <libpeas.h>
#include <parlatype.h>

void
ptp_mpris_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              PT_TYPE_WINDOW_ADDIN,
                                              PTP_TYPE_MPRIS);
}
