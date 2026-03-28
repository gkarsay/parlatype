#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define DUMMY_TYPE_GREETER (dummy_greeter_get_type())

G_DECLARE_FINAL_TYPE (DummyGreeter, dummy_greeter, DUMMY, GREETER, GObject)

G_END_DECLS
