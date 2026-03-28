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

#include <glib.h>
#include <gst/gst.h>
#include <pt-media-info-private.h>
#include <pt-media-info.h>

/* Tests -------------------------------------------------------------------- */

static void
media_info_new (void)
{
  PtMediaInfo *info = _pt_media_info_new ();

  g_assert_true (PT_IS_MEDIA_INFO (info));
  g_assert_null (pt_media_info_get_album (info));
  g_assert_null (pt_media_info_get_artist (info));
  g_assert_null (pt_media_info_get_title (info));
  g_assert_null (pt_media_info_dump_caps (info));
  g_assert_null (pt_media_info_dump_tags (info));

  g_object_unref (info);
}

static void
caps (void)
{
  PtMediaInfo *info = _pt_media_info_new ();
  GstCaps     *caps1, *caps2;
  gchar       *caps_dump;

  caps1 = gst_caps_from_string (
      "audio/mpeg, mpegversion=(int)1, mpegaudioversion=(int)1, layer=(int)3, rate=(int)48000,"
      " channels=(int)2, parsed=(boolean)true");

  _pt_media_info_update_caps (info, caps1);
  caps_dump = pt_media_info_dump_caps (info);
  g_assert_cmpstr (caps_dump, ==,
                   "audio/mpeg\n  mpegversion: 1\n  mpegaudioversion: 1\n  layer: 3\n  rate: 48000\n"
                   "  channels: 2\n  parsed: true\n");
  g_free (caps_dump);

  caps2 = gst_caps_from_string ("audio/mpeg, channels=(int)42");
  _pt_media_info_update_caps (info, caps2);
  caps_dump = pt_media_info_dump_caps (info);
  g_assert_cmpstr (caps_dump, ==, "audio/mpeg\n  channels: 42\n");
  g_free (caps_dump);

  _pt_media_info_update_caps (info, NULL);
  g_assert_null (pt_media_info_dump_caps (info));

  g_object_unref (info);
}

static void
tags (void)
{
  PtMediaInfo *info = _pt_media_info_new ();
  GstTagList  *tags1, *tags2;
  gchar       *tags_dump;

  tags1 = gst_tag_list_new (GST_TAG_TRACK_NUMBER, 1,
                            GST_TAG_ARTIST, "Some Artist",
                            GST_TAG_ARTIST, "Some Other Artist",
                            GST_TAG_ALBUM, "Some Album",
                            GST_TAG_TITLE, "Track 1",
                            NULL);

  _pt_media_info_update_tags (info, tags1);

  g_assert_cmpstr (pt_media_info_get_album (info), ==, "Some Album");
  g_assert_cmpstr (pt_media_info_get_artist (info)[0], ==, "Some Artist");
  g_assert_cmpstr (pt_media_info_get_artist (info)[1], ==, "Some Other Artist");
  g_assert_null (pt_media_info_get_artist (info)[2]);
  g_assert_cmpstr (pt_media_info_get_title (info), ==, "Track 1");

  tags_dump = pt_media_info_dump_tags (info);
  g_assert_cmpstr (tags_dump, ==,
                   "track-number: 1\nartist: Some Artist, Some Other Artist\nalbum: Some Album\ntitle: Track 1\n");
  g_free (tags_dump);

  tags2 = gst_tag_list_new (GST_TAG_TRACK_NUMBER, 1,
                            GST_TAG_ARTIST, "Some Artist",
                            GST_TAG_ARTIST, "Some Other Artist",
                            GST_TAG_ALBUM, "Some Album",
                            GST_TAG_TITLE, "Some Title",
                            NULL);
  _pt_media_info_update_tags (info, tags2);
  g_assert_cmpstr (pt_media_info_get_album (info), ==, "Some Album");
  g_assert_cmpstr (pt_media_info_get_artist (info)[0], ==, "Some Artist");
  g_assert_cmpstr (pt_media_info_get_artist (info)[1], ==, "Some Other Artist");
  g_assert_null (pt_media_info_get_artist (info)[2]);
  g_assert_cmpstr (pt_media_info_get_title (info), ==, "Some Title");

  tags_dump = pt_media_info_dump_tags (info);
  g_assert_cmpstr (tags_dump, ==,
                   "track-number: 1\nartist: Some Artist, Some Other Artist\nalbum: Some Album\ntitle: Some Title\n");
  g_free (tags_dump);

  _pt_media_info_update_tags (info, NULL);
  g_assert_null (pt_media_info_get_album (info));
  g_assert_null (pt_media_info_get_artist (info));
  g_assert_null (pt_media_info_get_title (info));
  g_assert_null (pt_media_info_dump_tags (info));

  g_object_unref (info);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  gst_init (NULL, NULL);

  g_test_add_func ("/mediainfo/new", media_info_new);
  g_test_add_func ("/mediainfo/caps", caps);
  g_test_add_func ("/mediainfo/tags", tags);

  return g_test_run ();
}
