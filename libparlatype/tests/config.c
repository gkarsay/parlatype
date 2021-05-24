/* Copyright (C) Gabor Karsay 2021 <gabor.karsay@gmx.at>
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
#include <gtk/gtk.h>
#include <pt-config.h>
#include <pt-config.c>	/* for static methods */



/* Tests -------------------------------------------------------------------- */

static void
construct (void)
{
	PtConfig *config;
	GFile    *testfile;
	GFile    *file;
	gchar    *testpath;
	gchar    *name;
	gboolean  is_valid;
	gboolean  is_installed;

	testpath = g_test_build_filename (G_TEST_BUILT, "config-test.asr", NULL);
	testfile = g_file_new_for_path (testpath);
	config = pt_config_new (testfile);
	g_assert_true (PT_IS_CONFIG (config));

	g_object_get (config,
		      "file", &file,
		      "name", &name,
		      "is-valid", &is_valid,
		      "is-installed", &is_installed,
		      NULL);

	g_assert_true (testfile == file);
	g_assert_true (is_valid);
	g_assert_false (is_installed);
	g_assert_cmpstr (name, ==, "Test Model");

	g_object_unref (config);
	g_object_unref (file);
	g_free (name);
	g_object_unref (testfile);
	g_free (testpath);
}

static void
static_methods (void)
{
	PtConfig *config;
	GFile    *testfile;
	gchar    *testpath;

	testpath = g_test_build_filename (G_TEST_BUILT, "config-test.asr", NULL);
	testfile = g_file_new_for_path (testpath);
	config = pt_config_new (testfile);

	GValue value;

	/* pt_config_get_value() works only on a formally valid config.
	 * Don't test for illegal values. */

	value = pt_config_get_value (config, "Parameters", "String", G_TYPE_STRING);
	g_assert_true (G_VALUE_HOLDS (&value, G_TYPE_STRING));
	g_assert_cmpstr (g_value_get_string (&value), ==, "Some String");
	g_value_unset (&value);

	value = pt_config_get_value (config, "Parameters", "Number", G_TYPE_INT);
	g_assert_true (G_VALUE_HOLDS (&value, G_TYPE_INT));
	g_assert_cmpint (g_value_get_int (&value), ==, 42);
	g_value_unset (&value);

	value = pt_config_get_value (config, "Parameters", "Boolean", G_TYPE_BOOLEAN);
	g_assert_true (G_VALUE_HOLDS (&value, G_TYPE_BOOLEAN));
	g_assert_true (g_value_get_boolean (&value));
	g_value_unset (&value);

	value = pt_config_get_value (config, "Files", "File", G_TYPE_NONE);
	g_assert_true (G_VALUE_HOLDS (&value, G_TYPE_STRING));
	g_assert_cmpstr (g_value_get_string (&value), ==, "/home/me/model/subdir/test.dict");
	g_value_unset (&value);

	value = pt_config_get_value (config, "Files", "Folder", G_TYPE_NONE);
	g_assert_true (G_VALUE_HOLDS (&value, G_TYPE_STRING));
	g_assert_cmpstr (g_value_get_string (&value), ==, "/home/me/model/subdir/subdir");
	g_value_unset (&value);

	g_object_unref (config);
	g_object_unref (testfile);
	g_free (testpath);
}

static void
public_methods (void)
{
	PtConfig *config;
	GFile    *testfile;
	gchar    *testpath;

	testpath = g_test_build_filename (G_TEST_BUILT, "config-test.asr", NULL);
	testfile = g_file_new_for_path (testpath);
	config = pt_config_new (testfile);

	gchar *name, *plugin, *base, *lang_code, *lang_name, *other;
	gboolean installed, success;

	name = pt_config_get_name (config);
	g_assert_cmpstr (name, ==, "Test Model");

	/* Change name */
	success = pt_config_set_name (config, "New Name");
	g_assert_true (success);
	name = pt_config_get_name (config);
	g_assert_cmpstr (name, ==, "New Name");

	/* Reset name again for future tests */
	success = pt_config_set_name (config, "Test Model");
	g_assert_true (success);
	name = pt_config_get_name (config);
	g_assert_cmpstr (name, ==, "Test Model");

	plugin = pt_config_get_plugin (config);
	g_assert_cmpstr (plugin, ==, "parlasphinx");

	base = pt_config_get_base_folder (config);
	g_assert_cmpstr (base, ==, "/home/me/model");

	/* Change base folder */
	success = pt_config_set_base_folder (config, "/home/me/somewhere_else");
	g_assert_true (success);
	base = pt_config_get_base_folder (config);
	g_assert_cmpstr (base, ==, "/home/me/somewhere_else");

	/* Reset base folder again for future tests */
	success = pt_config_set_base_folder (config, "/home/me/model");
	g_assert_true (success);
	base = pt_config_get_base_folder (config);
	g_assert_cmpstr (base, ==, "/home/me/model");

	lang_code = pt_config_get_lang_code (config);
	g_assert_cmpstr (lang_code, ==, "de");

	lang_name = pt_config_get_lang_name (config);
	g_assert_cmpstr (lang_name, ==, "German");

	installed = pt_config_is_installed (config);
	g_assert_false (installed);

	other = pt_config_get_other (config, "does-not-exist");
	g_assert_null (other);

	other = pt_config_get_other (config, "Language");
	g_assert_cmpstr (other, ==, "de");
	g_free (other);

	g_object_unref (config);
	g_object_unref (testfile);
	g_free (testpath);
}

static int
check_folder_for_configs (GFile *folder)
{
	GFileEnumerator *files;
	GFile    *file;
	PtConfig *config;
	GError   *error = NULL;
	int       num_tested = 0;

	files = g_file_enumerate_children (folder,
                                           G_FILE_ATTRIBUTE_STANDARD_NAME,
                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                           NULL,	/* cancellable */
                                           &error);
	g_assert_no_error (error);

	while (TRUE) {
		GFileInfo *info;
		g_file_enumerator_iterate (files, &info, &file, NULL, &error);
		g_assert_no_error (error);

		if (!info)
			break;

		const char *name = g_file_info_get_name (info);
		if (g_str_has_suffix (name, ".asr")) {
			num_tested++;
			config = pt_config_new (file);
			g_print ("Testing %s ", name);
			if (g_str_has_prefix (name, "invalid"))
				g_assert_false (pt_config_is_valid (config));
			else
				g_assert_true (pt_config_is_valid (config));
			g_print ("OK\n");
			g_object_unref (config);
		}
	}

	g_object_unref (files);
	return num_tested;
}

static void
crafted_valid (void)
{
	gchar *path;
	GFile *folder;
	int    num_tested;

	path = g_test_build_filename (G_TEST_BUILT, G_DIR_SEPARATOR_S, NULL);
	folder = g_file_new_for_path (path);
	num_tested = check_folder_for_configs (folder);
	g_assert_cmpint (num_tested, ==, 9);

	g_object_unref (folder);
	g_free (path);
}

static void
dist_valid (void)
{
	gchar *path;
	GFile *folder;
	int    num_tested;

	path = g_test_build_filename (G_TEST_DIST, "..", "..", "data", "asr", NULL);
	folder = g_file_new_for_path (path);
	num_tested = check_folder_for_configs (folder);
	g_assert_cmpint (num_tested, ==, 5);

	g_object_unref (folder);
	g_free (path);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/config/construct", construct);
	g_test_add_func ("/config/static_methods", static_methods);
	g_test_add_func ("/config/public_methods", public_methods);
	g_test_add_func ("/config/crafted_valid", crafted_valid);
	g_test_add_func ("/config/dist_valid", dist_valid);
	gtk_init (NULL, NULL);

	return g_test_run ();
}
