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
#include "mock-plugin.h"


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
main_methods (void)
{
	PtConfig *config;
	GFile    *testfile, *nullfile, *cmpfile;
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
	g_assert_cmpstr (plugin, ==, "unknown-plugin");

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

	other = pt_config_get_key (config, "does-not-exist");
	g_assert_null (other);

	other = pt_config_get_key (config, "Language");
	g_assert_cmpstr (other, ==, "de");
	g_free (other);

	/* Setting a NULL file and previous file again */
	nullfile = g_file_new_for_path ("does-not-exist");
	pt_config_set_file (config, nullfile);
	g_assert_false (pt_config_is_valid (config));

	pt_config_set_file (config,testfile);
	g_assert_true (pt_config_is_valid (config));

	cmpfile = pt_config_get_file (config);
	g_assert_true (cmpfile == testfile);

	g_object_unref (config);
	g_object_unref (testfile);
	g_object_unref (nullfile);
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
	g_assert_cmpint (num_tested, >=, 13);

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
	g_assert_cmpint (num_tested, >=, 5);

	g_object_unref (folder);
	g_free (path);
}

static void
apply (void)
{
	PtConfig   *config;
	GFile      *testfile;
	gchar      *testpath;
	MockPlugin *plugin;
	gchar      *prop_file, *prop_string;
	gint        prop_int;
	gfloat      prop_float;
	gdouble     prop_double;
	gboolean    prop_bool;
	GError     *error = NULL;
	gboolean    success;

	testpath = g_test_build_filename (G_TEST_DIST, "data", "config-apply.asr", NULL);
	testfile = g_file_new_for_path (testpath);
	config = pt_config_new (testfile);
	g_object_unref (testfile);
	g_free (testpath);

	plugin = mock_plugin_new ();
	success = pt_config_apply (config, G_OBJECT (plugin), &error);
	g_assert_true (success);
	g_assert_no_error (error);

	g_object_get (plugin, "file",   &prop_file,
	                      "string", &prop_string,
	                      "int",    &prop_int,
	                      "float",  &prop_float,
	                      "double", &prop_double,
	                      "bool",   &prop_bool,
	                      NULL);

	g_assert_cmpstr (prop_file, ==, "/home/me/model/subdir/file.dat");
	g_assert_cmpstr (prop_string, ==, "Example");
	g_assert_cmpint (prop_int, ==, 42);
	g_assert_cmpfloat_with_epsilon (prop_float, 0.42, 0.00001);
	g_assert_cmpfloat_with_epsilon (prop_double, 0.42, 0.00001);
	g_assert_true (prop_bool);

	g_free (prop_file);
	g_free (prop_string);
	g_object_unref (config);

	/* Test for error 1 */
	testpath = g_test_build_filename (G_TEST_DIST, "data", "config-apply-error1.asr", NULL);
	testfile = g_file_new_for_path (testpath);
	config = pt_config_new (testfile);
	g_object_unref (testfile);
	g_free (testpath);

	success = pt_config_apply (config, G_OBJECT (plugin), &error);
	g_assert_false (success);
	g_assert_error (error, PT_ERROR, PT_ERROR_PLUGIN_MISSING_PROPERTY);
	g_clear_error (&error);
	g_object_unref (config);

	/* Test for error 2 */
	testpath = g_test_build_filename (G_TEST_DIST, "data", "config-apply-error2.asr", NULL);
	testfile = g_file_new_for_path (testpath);
	config = pt_config_new (testfile);
	g_object_unref (testfile);
	g_free (testpath);

	success = pt_config_apply (config, G_OBJECT (plugin), &error);
	g_assert_false (success);
	g_assert_error (error, PT_ERROR, PT_ERROR_PLUGIN_NOT_WRITABLE);
	g_clear_error (&error);
	g_object_unref (config);

	/* Test for error 3 */
	testpath = g_test_build_filename (G_TEST_DIST, "data", "config-apply-error3.asr", NULL);
	testfile = g_file_new_for_path (testpath);
	config = pt_config_new (testfile);
	g_object_unref (testfile);
	g_free (testpath);

	success = pt_config_apply (config, G_OBJECT (plugin), &error);
	g_assert_false (success);
	g_assert_error (error, PT_ERROR, PT_ERROR_PLUGIN_WRONG_VALUE);
	g_clear_error (&error);
	g_object_unref (config);

	g_object_unref (plugin);
}


int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/config/construct", construct);
	g_test_add_func ("/config/main_methods", main_methods);
	g_test_add_func ("/config/crafted_valid", crafted_valid);
	g_test_add_func ("/config/dist_valid", dist_valid);
	g_test_add_func ("/config/apply", apply);
	gtk_init (NULL, NULL);

	return g_test_run ();
}
