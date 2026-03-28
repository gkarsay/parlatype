#include "dummy-greeter.h"

#include <libpeas.h>
#include <parlatype.h>

void
pt_dummy_greeter_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              PT_TYPE_ASR_IFACE,
                                              DUMMY_TYPE_GREETER);
}
