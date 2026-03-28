#include "config.h"

#include "dummy-greeter.h"

#include "parlatype.h"

#include <gst/app/gstappsink.h>

struct _DummyGreeter
{
  GObject parent_instance;

  PtPlayer   *player;
  PtTextView *text_view;
  GstElement *appsink;

  char *dummy_file;
  uint  source_handler;
  int   counter;
};

enum
{
  PROP_DUMMY_FILE = 1,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];
static void        pt_asr_iface_init (PtAsrIfaceInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (DummyGreeter, dummy_greeter, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (PT_TYPE_ASR_IFACE, pt_asr_iface_init))

static gboolean
insert_text_cb (DummyGreeter *self)
{
  pt_text_view_insert_asr_final (self->text_view, " *");
  self->source_handler = 0;
  return G_SOURCE_REMOVE;
}

static GstFlowReturn
new_sample_cb (GstAppSink *sink,
               gpointer    user_data)
{
  DummyGreeter *self = DUMMY_GREETER (user_data);

  /*GMainContext *context = g_main_context_get_thread_default ();
  while (self->source_handler != 0)
    {
      g_main_context_iteration (context, FALSE);
    }*/

  GstSample *sample = gst_app_sink_pull_sample (sink);
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  GstMapInfo map;

  if (!gst_buffer_map (buffer, &map, GST_MAP_READ))
    {
      gst_sample_unref (sample);
      return GST_FLOW_ERROR;
    };
  /* One sample is 2 bytes (16 bit signed integer), see caps in
   * setup_pipeline(). The buffer contains several samples, the number
   * of elements/samples is map.size / sample size. */
  // g_array_append_vals (priv->hires, map.data, map.size / 2);
  gst_buffer_unmap (buffer, &map);
  gst_sample_unref (sample);

  if (self->counter > 100)
    {
      self->source_handler = g_idle_add (G_SOURCE_FUNC (insert_text_cb), self);
      self->counter = 0;
    }

  self->counter++;
  return GST_FLOW_OK;
}

static void
dummy_greeter_load (PtAsrIface *pt_asr_iface,
                    PtPlayer   *player,
                    PtTextView *text_view)
{
  g_return_if_fail (PT_IS_ASR_IFACE (pt_asr_iface));
  g_return_if_fail (PT_IS_PLAYER (player));
  g_return_if_fail (PT_IS_TEXT_VIEW (text_view));

  DummyGreeter *self = DUMMY_GREETER (pt_asr_iface);
  self->player = player;
  self->text_view = text_view;
  self->appsink = pt_player_get_appsink (self->player);

  g_assert (self->appsink != NULL);

  g_object_set (self->appsink, "emit-signals", TRUE, "sync", FALSE, NULL);
  g_signal_connect (self->appsink, "new-sample", G_CALLBACK (new_sample_cb), self);

  pt_text_view_insert_asr_final (self->text_view, "Greetings from Dummy Greeter!");
}

static void
dummy_greeter_unload (PtAsrIface *self)
{
}

static void
pt_asr_iface_init (PtAsrIfaceInterface *iface)
{
  iface->load = dummy_greeter_load;
  iface->unload = dummy_greeter_unload;
}

static void
dummy_greeter_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  DummyGreeter *self = DUMMY_GREETER (object);

  switch (property_id)
    {
    case PROP_DUMMY_FILE:
      g_clear_pointer (&self->dummy_file, g_free);
      self->dummy_file = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
dummy_greeter_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  DummyGreeter *self = DUMMY_GREETER (object);

  switch (property_id)
    {
    case PROP_DUMMY_FILE:
      g_value_set_string (value, self->dummy_file);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
dummy_greeter_init (DummyGreeter *self)
{
}

static void
dummy_greeter_class_init (DummyGreeterClass *klass)
{
  G_OBJECT_CLASS (klass)->set_property = dummy_greeter_set_property;
  G_OBJECT_CLASS (klass)->get_property = dummy_greeter_get_property;

  obj_properties[PROP_DUMMY_FILE] =
      g_param_spec_string (
          "config-file", NULL, NULL,
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}
