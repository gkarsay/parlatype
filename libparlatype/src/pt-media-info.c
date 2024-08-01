/* Copyright 2024 Gabor Karsay <gabor.karsay@gmx.at>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION: pt-media-info
 * @short_description: Provides metadata for the current track
 * @include: parlatype/parlatype.h
 *
 * A read-only object that is owned by #PtPlayer and provides metadata
 * for the currently playing track, like title or artist.
 *
 * Call pt_player_get_media_info() to obtain the object and query
 * the current track’s metadata.
 *
 * Whenever metadata changes, a signal #PtMediaInfo::media-info-changed is
 * emitted.
 */

#include "config.h"

#include "pt-media-info.h"

#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/tag/tag.h>

struct _PtMediaInfo
{
  GObject parent;

  GstTagList *tags;
  GstCaps    *caps;

  gchar *album;
  GStrv  artist;
  gchar *title;
};

G_DEFINE_TYPE (PtMediaInfo, pt_media_info, G_TYPE_OBJECT)

/**
 * pt_media_info_get_album:
 * @self: a #PtMediaInfo
 *
 * Gets the album’s name.
 *
 * Return value: (transfer none) (nullable): the album’s name or NULL.
 *
 * Since: 4.3
 */
gchar *
pt_media_info_get_album (PtMediaInfo *self)
{
  g_return_val_if_fail (PT_IS_MEDIA_INFO (self), NULL);

  return self->album;
}

/**
 * pt_media_info_get_artist:
 * @self: a #PtMediaInfo
 *
 * Gets the track’s artists.
 *
 * Return value: (transfer none) (nullable): the tracks’s artists as a NULL-terminated array or NULL.
 *
 * Since: 4.3
 */
GStrv
pt_media_info_get_artist (PtMediaInfo *self)
{
  g_return_val_if_fail (PT_IS_MEDIA_INFO (self), NULL);

  return self->artist;
}

/**
 * pt_media_info_get_title:
 * @self: a #PtMediaInfo
 *
 * Gets the track’s title.
 *
 * Return value: (transfer none) (nullable): the track’s title or NULL.
 *
 * Since: 4.3
 */
gchar *
pt_media_info_get_title (PtMediaInfo *self)
{
  g_return_val_if_fail (PT_IS_MEDIA_INFO (self), NULL);

  return self->title;
}

static GStrv
build_array (const GstTagList *list,
             const gchar      *tag)
{
  GStrv         result;
  GStrvBuilder *builder;
  guint         size;
  gchar        *string;

  builder = g_strv_builder_new ();
  size = gst_tag_list_get_tag_size (list, tag);
  for (guint i = 0; i < size; i++)
    {
      gst_tag_list_get_string_index (list, tag, i, &string);
      g_strv_builder_add (builder, string);
      g_free (string);
    }
  result = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);
  return result;
}

static void
parse_tags (const GstTagList *list,
            const gchar      *tag,
            gpointer          user_data)
{
  PtMediaInfo *self = PT_MEDIA_INFO (user_data);

  if (g_str_equal (tag, GST_TAG_ALBUM))
    gst_tag_list_get_string (list, tag, &self->album);
  else if (g_str_equal (tag, GST_TAG_ARTIST))
    self->artist = build_array (list, tag);
  else if (g_str_equal (tag, GST_TAG_TITLE))
    gst_tag_list_get_string (list, tag, &self->title);
}

static void
build_tag_string (const GstTagList *list,
                  const gchar      *tag,
                  gpointer          user_data)
{
  GString *string = (GString *) user_data;
  GValue   val = G_VALUE_INIT;
  gchar   *valstr;

  gst_tag_list_copy_value (&val, list, tag);

  if (G_VALUE_HOLDS_STRING (&val))
    valstr = g_value_dup_string (&val);
  else if (g_str_equal (tag, "image"))
    valstr = g_strdup ("<image data skipped>");
  else
    valstr = gst_value_serialize (&val);

  g_string_append_printf (string, "%s: %s\n", tag, valstr);

  g_free (valstr);
  g_value_unset (&val);
}

/**
 * pt_media_info_dump_tags:
 * @self: a #PtMediaInfo
 *
 * Returns a string with all available GstTags in the current track,
 * even those without a getter method in PtMediaInfo. This is meant
 * for debug purposes.

 * Return value: (transfer full): multiline string with all tags or NULL.
 * After use free with g_free().
 *
 * Since: 4.3
 */
gchar *
pt_media_info_dump_tags (PtMediaInfo *self)
{
  g_return_val_if_fail (PT_IS_MEDIA_INFO (self), NULL);

  if (self->tags == NULL)
    return NULL;

  GString *string = g_string_new (NULL);
  gst_tag_list_foreach (self->tags, build_tag_string, string);

  return g_string_free_and_steal (string);
}

static gboolean
build_structure_string (GQuark        field,
                        const GValue *value,
                        gpointer      user_data)
{
  GString *string = (GString *) user_data;
  gchar   *str = gst_value_serialize (value);

  g_string_append_printf (string, "  %s: %s\n", g_quark_to_string (field), str);
  g_free (str);
  return TRUE;
}

static gboolean
build_caps_string (GstCapsFeatures *features,
                   GstStructure    *structure,
                   gpointer         user_data)
{
  GString *string = (GString *) user_data;

  g_string_append_printf (string, "%s\n", gst_structure_get_name (structure));
  gst_structure_foreach (structure, build_structure_string, string);

  return TRUE;
}

/**
 * pt_media_info_dump_caps:
 * @self: a #PtMediaInfo
 *
 * Returns a string with all available GstCaps in the current track,
 * even those without a getter method in PtMediaInfo. This is meant
 * for debug purposes.

 * Return value: (transfer full): multiline string with all caps or NULL.
 * After use free with g_free().
 *
 * Since: 4.3
 */
gchar *
pt_media_info_dump_caps (PtMediaInfo *self)
{
  g_return_val_if_fail (PT_IS_MEDIA_INFO (self), NULL);

  if (self->caps == NULL)
    return NULL;

  GString *string = g_string_new (NULL);
  gst_caps_foreach (self->caps, build_caps_string, string);

  return g_string_free_and_steal (string);
}

void
_pt_media_info_update_tags (PtMediaInfo *self,
                            GstTagList  *tags)
{
  g_return_if_fail (PT_IS_MEDIA_INFO (self));

  if (tags != NULL && gst_tag_list_is_empty (tags))
    g_clear_pointer (&tags, gst_tag_list_unref);

  if (self->tags == NULL && tags == NULL)
    return;

  if (self->tags != NULL && tags != NULL &&
      gst_tag_list_is_equal (self->tags, tags))
    return;

  g_clear_pointer (&self->tags, gst_tag_list_unref);
  g_clear_pointer (&self->album, g_free);
  g_clear_pointer (&self->artist, g_strfreev);
  g_clear_pointer (&self->title, g_free);

  if (tags != NULL)
    {
      self->tags = tags;
      gst_tag_list_foreach (self->tags, parse_tags, self);
    }

  g_signal_emit_by_name (self, "media-info-changed");
}

void
_pt_media_info_update_caps (PtMediaInfo *self,
                            GstCaps     *caps)
{
  g_return_if_fail (PT_IS_MEDIA_INFO (self));

  if (self->caps == NULL && caps == NULL)
    return;

  if (self->caps != NULL && caps != NULL &&
      gst_caps_is_equal (self->caps, caps))
    return;

  g_clear_pointer (&self->caps, gst_caps_unref);

  if (caps != NULL)
    {
      self->caps = caps;
    }
}

/* --------------------- Init and GObject management ------------------------ */

static void
pt_media_info_dispose (GObject *object)
{
  PtMediaInfo *self = PT_MEDIA_INFO (object);

  g_clear_pointer (&self->tags, gst_tag_list_unref);
  g_clear_pointer (&self->caps, gst_caps_unref);
  g_clear_pointer (&self->album, g_free);
  g_clear_pointer (&self->artist, g_strfreev);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (pt_media_info_parent_class)->dispose (object);
}

static void
pt_media_info_init (PtMediaInfo *self)
{
}

static void
pt_media_info_class_init (PtMediaInfoClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = pt_media_info_dispose;

  /**
   * PtMediaInfo::media-info-changed:
   * @self: the player emitting the signal
   *
   * Emitted when the player has updated the metadata of the current stream. This
   * will typically happen just after opening a stream.
   *
   * Call pt_media_info_get_<artist|title|etc.>() to query the updated metadata.
   */
  g_signal_new ("media-info-changed",
                PT_TYPE_MEDIA_INFO,
                G_SIGNAL_RUN_LAST,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0);
}

PtMediaInfo *
_pt_media_info_new (void)
{
  return g_object_new (PT_TYPE_MEDIA_INFO, NULL);
}
