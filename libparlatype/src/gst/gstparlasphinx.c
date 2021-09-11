/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2014 Alpha Cephei Inc.
 * Copyright (c) 2007 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced
 * Research Projects Agency and the National Science Foundation of the
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 * Author: David Huggins-Daines <dhuggins@cs.cmu.edu>
 * Slightly modified by Gabor Karsay <gabor.karsay@gmx.at> to make it work
 * with Parlatype.
 * TODO test and propose patch to upstream
 */

/**
 * element-pocketsphix
 *
 * The element runs the speech recognition on incoming audio buffers and
 * generates an element messages named <classname>&quot;pocketsphinx&quot;</classname>
 * for each hypothesis and one for the final result. The message's structure
 * contains these fields:
 *
 * <itemizedlist>
 * <listitem>
 *   <para>
 *   #GstClockTime
 *   <classname>&quot;timestamp&quot;</classname>:
 *   the timestamp of the buffer that triggered the message.
 *   </para>
 * </listitem>
 * <listitem>
 *   <para>
 *   #gboolean
 *   <classname>&quot;final&quot;</classname>:
 *   %FALSE for intermediate messages.
 *   </para>
 * </listitem>
 * <listitem>
 *   <para>
 *   #gin32
 *   <classname>&quot;confidence&quot;</classname>:
 *   posterior probability (confidence) of the result in log domain
 *   </para>
 * </listitem>
 * <listitem>
 *   <para>
 *   #gchar
 *   <classname>&quot;hypothesis&quot;</classname>:
 *   the recognized text
 *   </para>
 * </listitem>
 * </itemizedlist>
 *
 * <refsect2>
 * <title>Example pipeline</title>
 * |[
 * gst-launch-1.0 -m autoaudiosrc ! audioconvert ! audioresample ! pocketsphinx ! fakesink
 * ]|
 * </refsect2>
 */

#include <config.h>

#include <string.h>
#include <gst/gst.h>

#include <sphinxbase/err.h>
#include <sphinxbase/strfuncs.h>

#include "gstparlasphinx.h"

GST_DEBUG_CATEGORY_STATIC(parlasphinx_debug);
#define GST_CAT_DEFAULT parlasphinx_debug


static void
gst_parlasphinx_set_property(GObject * object, guint prop_id,
                          const GValue * value, GParamSpec * pspec);

static void
gst_parlasphinx_get_property(GObject * object, guint prop_id,
                              GValue * value, GParamSpec * pspec);

static GstStateChangeReturn
gst_parlasphinx_change_state(GstElement *element, GstStateChange transition);

static GstFlowReturn
gst_parlasphinx_chain(GstPad * pad, GstObject *parent, GstBuffer * buffer);

static gboolean
gst_parlasphinx_event(GstPad *pad, GstObject *parent, GstEvent *event);

static void
gst_parlasphinx_finalize_utt(GstParlaSphinx *ps);

static void
gst_parlasphinx_finalize(GObject * gobject);

enum
{
    PROP_0,
    PROP_HMM_DIR,
    PROP_LM_FILE,
    PROP_LMCTL_FILE,
    PROP_DICT_FILE,
    PROP_MLLR_FILE,
    PROP_FSG_FILE,
    PROP_ALLPHONE_FILE,
    PROP_KWS_FILE,
    PROP_JSGF_FILE,
    PROP_FWDFLAT,
    PROP_BESTPATH,
    PROP_MAXHMMPF,
    PROP_MAXWPF,
    PROP_BEAM,
    PROP_WBEAM,
    PROP_PBEAM,
    PROP_DSRATIO,

    PROP_LATDIR,
    PROP_LM_NAME,
    PROP_DECODER
};

/*
 * Static data.
 */

/* Default command line. (will go away soon and be constructed using properties) */
static char *default_argv[] = {
    "gst-parlasphinx",
};
static const int default_argc = sizeof(default_argv)/sizeof(default_argv[0]);

static GstStaticPadTemplate sink_factory =
    GST_STATIC_PAD_TEMPLATE("sink",
                            GST_PAD_SINK,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS("audio/x-raw, "
                                            "format = (string) { S16LE }, "
                                            "channels = (int) 1, "
                                            "rate = (int) 16000")
        );

static GstStaticPadTemplate src_factory =
    GST_STATIC_PAD_TEMPLATE("src",
                            GST_PAD_SRC,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS("audio/x-raw, "
                                            "format = (string) { S16LE }, "
                                            "channels = (int) 1, "
                                            "rate = (int) 16000")
        );

/*
 * Boxing of ps_decoder_t.
 */
GType
ps_decoder_get_type(void)
{
    static GType ps_decoder_type = 0;

    if (G_UNLIKELY(ps_decoder_type == 0)) {
        ps_decoder_type = g_boxed_type_register_static
            ("PSDecoder",
             /* Conveniently, these should just work. */
             (GBoxedCopyFunc) ps_retain,
             (GBoxedFreeFunc) ps_free);
    }

    return ps_decoder_type;
}


G_DEFINE_TYPE(GstParlaSphinx, gst_parlasphinx, GST_TYPE_ELEMENT);

static void
gst_parlasphinx_class_init(GstParlaSphinxClass * klass)
{
    GObjectClass *gobject_class;
    GstElementClass *element_class;;

    gobject_class =(GObjectClass *) klass;
    element_class = (GstElementClass *)klass;

    gobject_class->set_property = gst_parlasphinx_set_property;
    gobject_class->get_property = gst_parlasphinx_get_property;
    gobject_class->finalize = gst_parlasphinx_finalize;

    /* TODO: We will bridge cmd_ln.h properties to GObject
     * properties here somehow eventually. */
    g_object_class_install_property
        (gobject_class, PROP_HMM_DIR,
         g_param_spec_string("hmm", "HMM Directory",
                             "Directory containing acoustic model parameters",
                             NULL,
                             G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_LM_FILE,
         g_param_spec_string("lm", "LM File",
                             "Language model file",
                             NULL,
                             G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_LMCTL_FILE,
         g_param_spec_string("lmctl", "LM Control File",
                             "Language model control file (for class LMs)",
                             NULL,
                             G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_FSG_FILE,
         g_param_spec_string("fsg", "FSG File",
                             "Finite state grammar file",
                             NULL,
                             G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_ALLPHONE_FILE,
         g_param_spec_string("allphone", "Allphone File",
                             "Phonetic language model file",
                             NULL,
                             G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_KWS_FILE,
         g_param_spec_string("kws", "Keyphrases File",
                             "List of keyphrases for spotting",
                             NULL,
                             G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_JSGF_FILE,
         g_param_spec_string("jsgf", "Grammar file",
                             "File with grammar in Java Speech Grammar Format (JSGF)",
                             NULL,
                             G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_DICT_FILE,
         g_param_spec_string("dict", "Dictionary File",
                             "Dictionary File",
                             NULL,
                             G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_FWDFLAT,
         g_param_spec_boolean("fwdflat", "Flat Lexicon Search",
                              "Enable Flat Lexicon Search",
                              FALSE,
                              G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_BESTPATH,
         g_param_spec_boolean("bestpath", "Graph Search",
                              "Enable Graph Search",
                              FALSE,
                              G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_MAXHMMPF,
         g_param_spec_int("maxhmmpf", "Maximum HMMs per frame",
                          "Maximum number of HMMs searched per frame",
                          1, 100000, 1000,
                          G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_MAXWPF,
         g_param_spec_int("maxwpf", "Maximum words per frame",
                          "Maximum number of words searched per frame",
                          1, 100000, 10,
                          G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_BEAM,
         g_param_spec_double("beam", "Beam width applied to every frame in Viterbi search",
                          "Beam width applied to every frame in Viterbi search",
                          -1, 1, 1e-48,
                          G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_PBEAM,
         g_param_spec_double("pbeam", "Beam width applied to phone transitions",
                          "Beam width applied to phone transitions",
                          -1, 1, 1e-48,
                          G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_WBEAM,
         g_param_spec_double("wbeam", "Beam width applied to word exits",
                          "Beam width applied to phone transitions",
                          -1, 1, 7e-29,
                          G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_DSRATIO,
         g_param_spec_int("dsratio", "Frame downsampling ratio",
                          "Evaluate acoustic model every N frames",
                          1, 10, 1,
                          G_PARAM_READWRITE));

    /* Could be changed on runtime when ps is already initialized */
    g_object_class_install_property
        (gobject_class, PROP_LM_NAME,
         g_param_spec_string("lmname", "LM Name",
                             "Language model name (to select LMs from lmctl)",
                             NULL,
                             G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_LATDIR,
         g_param_spec_string("latdir", "Lattice Directory",
                             "Output Directory for Lattices",
                             NULL,
                             G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class, PROP_DECODER,
         g_param_spec_boxed("decoder", "Decoder object",
                            "The underlying decoder",
                            PS_DECODER_TYPE,
                            G_PARAM_READABLE));


    GST_DEBUG_CATEGORY_INIT(parlasphinx_debug, "pocketsphinx", 0,
                            "Automatic Speech Recognition");


    element_class->change_state = gst_parlasphinx_change_state;

    gst_element_class_add_pad_template(element_class,
                                       gst_static_pad_template_get(&sink_factory));
    gst_element_class_add_pad_template(element_class,
                                       gst_static_pad_template_get(&src_factory));

    gst_element_class_set_static_metadata(element_class, "PocketSphinx", "Filter/Audio", "Convert speech to text", "CMUSphinx-devel <cmusphinx-devel@lists.sourceforge.net>");

}

static void
gst_parlasphinx_set_string(GstParlaSphinx *ps,
                            const gchar *key, const GValue *value)
{
    if (value != NULL) {
        cmd_ln_set_str_r(ps->config, key, g_value_get_string(value));
    } else {
        cmd_ln_set_str_r(ps->config, key, NULL);
    }
}

static void
gst_parlasphinx_set_int(GstParlaSphinx *ps,
                         const gchar *key, const GValue *value)
{
    cmd_ln_set_int32_r(ps->config, key, g_value_get_int(value));
}

static void
gst_parlasphinx_set_boolean(GstParlaSphinx *ps,
                             const gchar *key, const GValue *value)
{
    cmd_ln_set_boolean_r(ps->config, key, g_value_get_boolean(value));
}

static void
gst_parlasphinx_set_double(GstParlaSphinx *ps,
                         const gchar *key, const GValue *value)
{
    cmd_ln_set_float_r(ps->config, key, g_value_get_double(value));
}

static void
gst_parlasphinx_set_property(GObject * object, guint prop_id,
                              const GValue * value, GParamSpec * pspec)
{
    GstParlaSphinx *ps = GST_PARLASPHINX(object);

    switch (prop_id) {

    case PROP_HMM_DIR:
        gst_parlasphinx_set_string(ps, "-hmm", value);
        break;
    case PROP_LM_FILE:
        /* FSG and LM are mutually exclusive. */
        gst_parlasphinx_set_string(ps, "-lm", value);
        gst_parlasphinx_set_string(ps, "-lmctl", NULL);
        gst_parlasphinx_set_string(ps, "-fsg", NULL);
        gst_parlasphinx_set_string(ps, "-allphone", NULL);
        gst_parlasphinx_set_string(ps, "-kws", NULL);
        gst_parlasphinx_set_string(ps, "-jsgf", NULL);
        break;
    case PROP_LMCTL_FILE:
        /* FSG and LM are mutually exclusive. */
        gst_parlasphinx_set_string(ps, "-lm", NULL);
        gst_parlasphinx_set_string(ps, "-lmctl", value);
        gst_parlasphinx_set_string(ps, "-fsg", NULL);
        gst_parlasphinx_set_string(ps, "-allphone", NULL);
        gst_parlasphinx_set_string(ps, "-kws", NULL);
        gst_parlasphinx_set_string(ps, "-jsgf", NULL);
        break;
    case PROP_DICT_FILE:
        gst_parlasphinx_set_string(ps, "-dict", value);
        break;
    case PROP_MLLR_FILE:
        gst_parlasphinx_set_string(ps, "-mllr", value);
        break;
    case PROP_FSG_FILE:
        /* FSG and LM are mutually exclusive */
        gst_parlasphinx_set_string(ps, "-lm", NULL);
        gst_parlasphinx_set_string(ps, "-lmctl", NULL);
        gst_parlasphinx_set_string(ps, "-fsg", value);
        gst_parlasphinx_set_string(ps, "-allphone", NULL);
        gst_parlasphinx_set_string(ps, "-kws", NULL);
        gst_parlasphinx_set_string(ps, "-jsgf", NULL);
        break;
    case PROP_ALLPHONE_FILE:
        /* FSG and LM are mutually exclusive. */
        gst_parlasphinx_set_string(ps, "-lm", NULL);
        gst_parlasphinx_set_string(ps, "-lmctl", NULL);
        gst_parlasphinx_set_string(ps, "-fsg", NULL);
        gst_parlasphinx_set_string(ps, "-allphone", value);
        gst_parlasphinx_set_string(ps, "-kws", NULL);
        gst_parlasphinx_set_string(ps, "-jsgf", NULL);
        break;
    case PROP_KWS_FILE:
        /* FSG and LM are mutually exclusive. */
        gst_parlasphinx_set_string(ps, "-lm", NULL);
        gst_parlasphinx_set_string(ps, "-lmctl", NULL);
        gst_parlasphinx_set_string(ps, "-fsg", NULL);
        gst_parlasphinx_set_string(ps, "-allphone", NULL);
        gst_parlasphinx_set_string(ps, "-jsgf", NULL);
        gst_parlasphinx_set_string(ps, "-kws", value);
        break;
    case PROP_JSGF_FILE:
        /* FSG and LM are mutually exclusive. */
        gst_parlasphinx_set_string(ps, "-lm", NULL);
        gst_parlasphinx_set_string(ps, "-lmctl", NULL);
        gst_parlasphinx_set_string(ps, "-fsg", NULL);
        gst_parlasphinx_set_string(ps, "-allphone", NULL);
        gst_parlasphinx_set_string(ps, "-kws", NULL);
        gst_parlasphinx_set_string(ps, "-jsgf", value);
        break;
    case PROP_FWDFLAT:
        gst_parlasphinx_set_boolean(ps, "-fwdflat", value);
        break;
    case PROP_BESTPATH:
        gst_parlasphinx_set_boolean(ps, "-bestpath", value);
        break;
    case PROP_MAXHMMPF:
        gst_parlasphinx_set_int(ps, "-maxhmmpf", value);
        break;
    case PROP_MAXWPF:
        gst_parlasphinx_set_int(ps, "-maxwpf", value);
        break;
    case PROP_BEAM:
        gst_parlasphinx_set_double(ps, "-beam", value);
        break;
    case PROP_PBEAM:
        gst_parlasphinx_set_double(ps, "-pbeam", value);
        break;
    case PROP_WBEAM:
        gst_parlasphinx_set_double(ps, "-wbeam", value);
        break;
    case PROP_DSRATIO:
        gst_parlasphinx_set_int(ps, "-ds", value);
        break;


    case PROP_LATDIR:
        if (ps->latdir)
            g_free(ps->latdir);
        ps->latdir = g_strdup(g_value_get_string(value));
        break;
    case PROP_LM_NAME:
        gst_parlasphinx_set_string(ps, "-fsg", NULL);
        gst_parlasphinx_set_string(ps, "-lm", NULL);
        gst_parlasphinx_set_string(ps, "-allphone", NULL);
        gst_parlasphinx_set_string(ps, "-kws", NULL);
        gst_parlasphinx_set_string(ps, "-jsgf", NULL);
        gst_parlasphinx_set_string(ps, "-lmname", value);

        /*
         * Chances are that lmctl is already loaded and all
         * corresponding searches are configured, so we simply
         * try to set the search
         */

        if (value != NULL && ps->ps) {
            ps_set_search(ps->ps, g_value_get_string(value));
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        return;
    }

    /* If decoder was already initialized, reinit */
    if (ps->ps && prop_id != PROP_LATDIR && prop_id != PROP_LM_NAME)
	ps_reinit(ps->ps, ps->config);
}

static void
gst_parlasphinx_get_property(GObject * object, guint prop_id,
                              GValue * value, GParamSpec * pspec)
{
    GstParlaSphinx *ps = GST_PARLASPHINX(object);

    switch (prop_id) {
    case PROP_DECODER:
        g_value_set_boxed(value, ps->ps);
        break;
    case PROP_HMM_DIR:
        g_value_set_string(value, cmd_ln_str_r(ps->config, "-hmm"));
        break;
    case PROP_LM_FILE:
        g_value_set_string(value, cmd_ln_str_r(ps->config, "-lm"));
        break;
    case PROP_LMCTL_FILE:
        g_value_set_string(value, cmd_ln_str_r(ps->config, "-lmctl"));
        break;
    case PROP_LM_NAME:
        g_value_set_string(value, cmd_ln_str_r(ps->config, "-lmname"));
        break;
    case PROP_DICT_FILE:
        g_value_set_string(value, cmd_ln_str_r(ps->config, "-dict"));
        break;
    case PROP_MLLR_FILE:
        g_value_set_string(value, cmd_ln_str_r(ps->config, "-mllr"));
        break;
    case PROP_FSG_FILE:
        g_value_set_string(value, cmd_ln_str_r(ps->config, "-fsg"));
        break;
    case PROP_ALLPHONE_FILE:
        g_value_set_string(value, cmd_ln_str_r(ps->config, "-allphone"));
        break;
    case PROP_KWS_FILE:
        g_value_set_string(value, cmd_ln_str_r(ps->config, "-kws"));
        break;
    case PROP_JSGF_FILE:
        g_value_set_string(value, cmd_ln_str_r(ps->config, "-jsgf"));
        break;
    case PROP_FWDFLAT:
        g_value_set_boolean(value, cmd_ln_boolean_r(ps->config, "-fwdflat"));
        break;
    case PROP_BESTPATH:
        g_value_set_boolean(value, cmd_ln_boolean_r(ps->config, "-bestpath"));
        break;
    case PROP_LATDIR:
        g_value_set_string(value, ps->latdir);
        break;
    case PROP_MAXHMMPF:
        g_value_set_int(value, cmd_ln_int32_r(ps->config, "-maxhmmpf"));
        break;
    case PROP_MAXWPF:
        g_value_set_int(value, cmd_ln_int32_r(ps->config, "-maxwpf"));
        break;
    case PROP_BEAM:
        g_value_set_double(value, cmd_ln_float_r(ps->config, "-beam"));
        break;
    case PROP_PBEAM:
        g_value_set_double(value, cmd_ln_float_r(ps->config, "-pbeam"));
        break;
    case PROP_WBEAM:
        g_value_set_double(value, cmd_ln_float_r(ps->config, "-wbeam"));
        break;
    case PROP_DSRATIO:
        g_value_set_int(value, cmd_ln_int32_r(ps->config, "-ds"));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gst_parlasphinx_finalize(GObject * gobject)
{
    GstParlaSphinx *ps = GST_PARLASPHINX(gobject);

    ps_free(ps->ps);
    cmd_ln_free_r(ps->config);
    g_free(ps->last_result);
    g_free(ps->latdir);

    G_OBJECT_CLASS(gst_parlasphinx_parent_class)->finalize(gobject);
}

static void
gst_parlasphinx_init(GstParlaSphinx * ps)
{
    ps->sinkpad =
        gst_pad_new_from_static_template(&sink_factory, "sink");
    ps->srcpad =
        gst_pad_new_from_static_template(&src_factory, "src");

    /* Parse default command-line options. */
    ps->config = cmd_ln_parse_r(NULL, ps_args(), default_argc, default_argv, FALSE);
    ps_default_search_args(ps->config);

    /* Set up pads. */
    gst_element_add_pad(GST_ELEMENT(ps), ps->sinkpad);
    gst_pad_set_chain_function(ps->sinkpad, gst_parlasphinx_chain);
    gst_pad_set_event_function(ps->sinkpad, gst_parlasphinx_event);
    gst_pad_use_fixed_caps(ps->sinkpad);

    gst_element_add_pad(GST_ELEMENT(ps), ps->srcpad);
    gst_pad_use_fixed_caps(ps->srcpad);

    /* Initialize time. */
    ps->last_result_time = 0;
    ps->last_result = NULL;
}

static GstStateChangeReturn
gst_parlasphinx_change_state(GstElement *element, GstStateChange transition)
{
    GstParlaSphinx *ps = GST_PARLASPHINX(element);
    GstStateChangeReturn ret;
    GstEvent   *seek = NULL;
    GstSegment *seg = NULL;
    gboolean success;

    /* handle upward state changes */
    switch (transition) {
         case GST_STATE_CHANGE_NULL_TO_READY:
            ps->ps = ps_init(ps->config);
            if (ps->ps == NULL) {
                GST_ELEMENT_ERROR(GST_ELEMENT(ps), LIBRARY, INIT,
                          ("Failed to initialize PocketSphinx"),
                          ("Failed to initialize PocketSphinx"));
                return GST_STATE_CHANGE_FAILURE;
            }
            break;
        default:
            break;
    }

    ret = GST_ELEMENT_CLASS(gst_parlasphinx_parent_class)->change_state(element, transition);

    /* handle downward state changes */
    switch (transition) {
        case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
            if (ps->eos)
                break;
            //gst_parlasphinx_finalize_utt(ps); // crash in pocketsphinx
            /* Work around a problem with unknown cause:
             * When changing to paused position queries return the running
             * time instead of stream time */
            seg = &ps->segment;
            if (seg->position > seg->stop)
                seg->position = seg->stop;
            seek = gst_event_new_seek (1.0, GST_FORMAT_TIME,
                                       GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
                                       GST_SEEK_TYPE_SET, seg->position,
                                       GST_SEEK_TYPE_NONE, -1);
            success = gst_pad_push_event (ps->sinkpad, seek);
            GST_DEBUG_OBJECT (ps, "pushed segment event: %s", success ? "yes" : "no");
            break;
        case GST_STATE_CHANGE_READY_TO_NULL:
            ps_free(ps->ps);
            ps->ps = NULL;
        default:
            break;
    }

    return ret;
}

static void
gst_parlasphinx_post_message(GstParlaSphinx *ps, gboolean final,
    GstClockTime timestamp, gint32 prob, const gchar *hyp)
{
    GstStructure *s = gst_structure_new ("pocketsphinx",
        "timestamp", G_TYPE_UINT64, timestamp,
        "final", G_TYPE_BOOLEAN, final,
        "confidence", G_TYPE_LONG, prob,
        "hypothesis", G_TYPE_STRING, hyp, NULL);

    gst_element_post_message (GST_ELEMENT (ps), gst_message_new_element (GST_OBJECT (ps), s));
}

static GstFlowReturn
gst_parlasphinx_chain(GstPad * pad, GstObject *parent, GstBuffer * buffer)
{
    GstParlaSphinx *ps;
    GstMapInfo info;
    gboolean in_speech;
    GstClockTime position, duration;

    ps = GST_PARLASPHINX(parent);

    if (GST_STATE (GST_ELEMENT (ps)) != GST_STATE_PLAYING)
        return gst_pad_push (ps->srcpad, buffer);

    /* Track current position */
    position = GST_BUFFER_TIMESTAMP (buffer);
    if (GST_CLOCK_TIME_IS_VALID (position)) {
        duration = GST_BUFFER_DURATION (buffer);
        if (GST_CLOCK_TIME_IS_VALID (duration)) {
            position += duration;
        }
    ps->segment.position = position;
    }

    /* Start an utterance for the first buffer we get */
    if (!ps->listening_started) {
        ps->listening_started = TRUE;
        ps->speech_started = FALSE;
        ps_start_utt(ps->ps);
    }

    gst_buffer_map (buffer, &info, GST_MAP_READ);
    ps_process_raw(ps->ps,
                   (short*) info.data,
                   info.size / sizeof(short),
                   FALSE, FALSE);
    gst_buffer_unmap (buffer, &info);

    in_speech = ps_get_in_speech(ps->ps);
    if (in_speech && !ps->speech_started) {
        ps->speech_started = TRUE;
    }
    if (!in_speech && ps->speech_started) {
        gst_parlasphinx_finalize_utt(ps);
    } else if (ps->last_result_time == 0
        /* Get a partial result every now and then, see if it is different. */
        /* Check every 100 milliseconds. */
        || (GST_BUFFER_TIMESTAMP(buffer) - ps->last_result_time) > GST_MSECOND * 500) {
        int32 score;
        char const *hyp;

        hyp = ps_get_hyp(ps->ps, &score);
        ps->last_result_time = GST_BUFFER_TIMESTAMP(buffer);
        if (hyp && strlen(hyp) > 0) {
            if (ps->last_result == NULL || 0 != strcmp(ps->last_result, hyp)) {
                g_free(ps->last_result);
                ps->last_result = g_strdup(hyp);
                gst_parlasphinx_post_message(ps, FALSE, ps->last_result_time,
                    ps_get_prob(ps->ps), hyp);
            }
        }
    }

    return gst_pad_push (ps->srcpad, buffer);
}


static void
gst_parlasphinx_finalize_utt(GstParlaSphinx *ps)
{
    char const *hyp;
    int32 score;

    hyp = NULL;
    if (!ps->listening_started)
        return;

    ps_end_utt(ps->ps);
    ps->listening_started = FALSE;
    hyp = ps_get_hyp(ps->ps, &score);

    if (hyp)
        gst_parlasphinx_post_message(ps, TRUE, GST_CLOCK_TIME_NONE,
                                     ps_get_prob(ps->ps), hyp);

    if (ps->latdir) {
        char *latfile;
        char uttid[16];

        sprintf(uttid, "%09u", ps->uttno);
        ps->uttno++;
        latfile = string_join(ps->latdir, "/", uttid, ".lat", NULL);
        ps_lattice_t *dag;
        if ((dag = ps_get_lattice(ps->ps)))
            ps_lattice_write(dag, latfile);
        ckd_free(latfile);
    }
}

static gboolean
gst_parlasphinx_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    GstParlaSphinx *ps;

    ps = GST_PARLASPHINX(parent);
    ps->eos = FALSE;

    switch (event->type) {
    case GST_EVENT_EOS:
    {
        gst_parlasphinx_finalize_utt(ps);
        ps->eos = TRUE;
        return gst_pad_event_default(pad, parent, event);
    }
    case GST_EVENT_SEGMENT:
        /* keep current segment, used for tracking current position */
        gst_event_copy_segment (event, &ps->segment);
        /* fall through */
    default:
        return gst_pad_event_default(pad, parent, event);
    }
}

static void
gst_parlasphinx_log(void *user_data, err_lvl_t lvl, const char *fmt, ...)
{
    static const int gst_level[ERR_MAX] = {GST_LEVEL_DEBUG, GST_LEVEL_INFO,
             GST_LEVEL_INFO, GST_LEVEL_WARNING, GST_LEVEL_ERROR, GST_LEVEL_ERROR};

     va_list ap;
     va_start(ap, fmt);
     gst_debug_log_valist(parlasphinx_debug, gst_level[lvl], "", "", 0, NULL, fmt, ap);
     va_end(ap);
}


static gboolean
plugin_init(GstPlugin * plugin)
{

    err_set_callback(gst_parlasphinx_log, NULL);

    if (!gst_element_register(plugin, "parlasphinx",
                              GST_RANK_NONE, GST_TYPE_PARLASPHINX))
        return FALSE;
    return TRUE;
}

/**
 * gst_parlasphinx_register:
 *
 * Registers a plugin holding our single element to use privately in this
 * library.
 *
 * Return value: TRUE if successful, otherwise FALSE
 */
gboolean
gst_parlasphinx_register (void)
{
	return gst_plugin_register_static (
			GST_VERSION_MAJOR,
			GST_VERSION_MINOR,
			"parlasphinx",
			"ParlaSphinx plugin",
			plugin_init,
			PACKAGE_VERSION,
			"BSD",
			"libparlatype",
			"Parlatype",
			"https://www.parlatype.org/");
}
