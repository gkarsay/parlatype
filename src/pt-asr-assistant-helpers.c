/* Copyright (C) Gabor Karsay 2019 <gabor.karsay@gmx.at>
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


#include "config.h"
#include "pt-asr-assistant-helpers.h"


void
search_result_free (SearchResult *r)
{
	g_ptr_array_free (r->lms, TRUE);
	g_ptr_array_free (r->dicts, TRUE);
	g_ptr_array_free (r->hmms, TRUE);

	g_slice_free (SearchResult, r);
}

static void
actually_search (GFile         *folder,
                 GCancellable  *cancel,
                 GPtrArray     *lms,
                 GPtrArray     *dicts,
                 GPtrArray     *hmms,
                 GError       **forward_error)
{
	GError          *error = NULL;
	GFileEnumerator *files;
	gint             hmm_hint = 0;

	files = g_file_enumerate_children (folder,
                                           G_FILE_ATTRIBUTE_STANDARD_NAME","
                                           G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                           cancel,
                                           &error);

	if (error) {
		g_propagate_error (forward_error, error);
		return;
	}

	while (TRUE) {
		if (g_cancellable_set_error_if_cancelled (cancel, &error))
			break;

		GFileInfo *info;
		GFile     *file;
		if (!g_file_enumerator_iterate (files, &info, &file, cancel, &error))
			break;
		if (!info)
			break;
		const char *name = g_file_info_get_name (info);
		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
			GFile *subdir;
			subdir = g_file_get_child (folder, name);
			actually_search (subdir, cancel, lms, dicts, hmms, &error);
			g_object_unref (subdir);
			if (error)
				break;
		}

		if (g_str_has_suffix (name, ".lm.bin"))
			g_ptr_array_add (lms, g_object_ref (file));

		if (g_str_has_suffix (name, ".dict") || g_str_has_suffix (name, ".dic"))
			g_ptr_array_add (dicts, g_object_ref (file));

		if (g_strcmp0 (name, "feat.params") == 0
		    || g_strcmp0 (name, "feature_transform") == 0
		    || g_strcmp0 (name, "mdef") == 0
		    || g_strcmp0 (name, "means") == 0
		    || g_strcmp0 (name, "mixture_weights") == 0
		    || g_strcmp0 (name, "noisedict") == 0
		    || g_strcmp0 (name, "sendump") == 0
		    || g_strcmp0 (name, "transition_matrices") == 0
		    || g_strcmp0 (name, "variances") == 0) {
			hmm_hint++;
		}
	}

	if (hmm_hint > 4) {
		g_ptr_array_add (hmms, g_object_ref (folder));
	}

	g_object_unref (files);

	if (error)
		g_propagate_error (forward_error, error);
}

static void
recursive_search (GTask        *task,
                  gpointer      soure_object,
                  gpointer      task_data,
                  GCancellable *cancel)
{
	GError       *error = NULL;
	GFile        *folder;
	SearchResult *r;

	folder = task_data;
	r = g_slice_new (SearchResult);
	r->lms   = g_ptr_array_new_with_free_func (g_object_unref);
	r->dicts = g_ptr_array_new_with_free_func (g_object_unref);
	r->hmms  = g_ptr_array_new_with_free_func (g_object_unref);

	actually_search (folder, cancel, r->lms, r->dicts, r->hmms, &error);

	if (error) {
		g_task_return_error (task, error);
		search_result_free (r);
	} else {
		g_task_return_pointer (task, r, (GDestroyNotify) search_result_free);
	}
}


gpointer
_pt_asr_assistant_recursive_search_finish (PtAsrAssistant  *self,
                                           GAsyncResult    *result,
                                           GError         **error)
{
	g_return_val_if_fail (G_IS_TASK (result), NULL);
	g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

static gboolean
failed_timeout (GCancellable *cancel)
{
	g_cancellable_cancel (cancel);
	return G_SOURCE_REMOVE;
}


void
_pt_asr_assistant_recursive_search_async (PtAsrAssistant      *self,
                                          GFile               *folder,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
	GTask        *task;
	GCancellable *cancel;

	cancel = g_cancellable_new ();
	task = g_task_new (self, cancel, callback, user_data);
	g_task_set_return_on_cancel (task, TRUE);
	g_task_set_task_data (task, folder, g_object_unref);
	g_task_run_in_thread (task, recursive_search);
	g_object_unref (task);

	/* Add a timeout of 0.5 seconds to avoid infinite running */
	g_timeout_add (500, (GSourceFunc) failed_timeout, cancel);
}
