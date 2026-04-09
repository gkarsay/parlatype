#include "pt-window-addin.h"
#include "ptp-test-buttons.h"

#include <libpeas.h>
#include <parlatype.h>

void
ptp_test_buttons_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              PT_TYPE_WINDOW_ADDIN,
                                              PTP_TYPE_TEST_BUTTONS);
}
