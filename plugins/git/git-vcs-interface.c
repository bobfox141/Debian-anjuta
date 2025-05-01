/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) James Liggett 2008 <jrliggett@cox.net>
 * 
 * anjuta is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * anjuta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with anjuta.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include "git-vcs-interface.h"

void
git_ivcs_iface_init (IAnjutaVcsIface *iface)
{
	iface->add = git_ivcs_add;
	iface->checkout = git_ivcs_checkout;
	iface->diff = git_ivcs_diff;
	iface->query_status = git_ivcs_query_status;
	iface->remove = git_ivcs_remove;
}

void
git_ivcs_add (IAnjutaVcs *obj, GList *files, AnjutaAsyncNotify *notify,
			  GError **err)
{
	gchar *project_root_directory;
	GList *path_list;
	GitAddCommand *add_command;
	
	project_root_directory = ANJUTA_PLUGIN_GIT (obj)->project_root_directory;
	
	if (project_root_directory)
	{
		path_list = anjuta_util_convert_gfile_list_to_relative_path_list (files,
		                                                                  project_root_directory);
		add_command = git_add_command_new_list (project_root_directory,
		                                        path_list, FALSE);
		
		anjuta_util_glist_strings_free (path_list);
		
		g_signal_connect (G_OBJECT (add_command), "command-finished", 
		                  G_CALLBACK (g_object_unref), 
		                  NULL);
		
		if (notify)
		{
			g_signal_connect_swapped (G_OBJECT (add_command), "command-finished",
			                          G_CALLBACK (anjuta_async_notify_notify_finished),
			                          notify);
		}
		
		anjuta_command_start (ANJUTA_COMMAND (add_command));
	}
	
}

void
git_ivcs_checkout (IAnjutaVcs *obj, 
				   const gchar *repository_location, GFile *dest,
				   GCancellable *cancel,
				   AnjutaAsyncNotify *notify, GError **err)
{
	GFile *parent;
	gchar *path, *dir_name;
	GitCloneCommand *clone_command;
	Git *plugin;

	parent = g_file_get_parent (dest);
	path = g_file_get_path (parent);
	dir_name = g_file_get_basename (dest);
	
	clone_command = git_clone_command_new (path, repository_location, dir_name);
	plugin = ANJUTA_PLUGIN_GIT (obj);

	g_object_unref (parent);
	g_free (path);
	g_free (dir_name);

	git_pane_create_message_view (plugin);

	g_signal_connect (G_OBJECT (clone_command), "data-arrived",
	                  G_CALLBACK (git_pane_on_command_info_arrived),
	                  plugin);

	g_signal_connect (G_OBJECT (clone_command), "command-finished",
	                  G_CALLBACK (g_object_unref),
	                  NULL);

	if (cancel)
	{
		g_signal_connect_swapped (G_OBJECT (cancel), "cancelled",
		                          G_CALLBACK (anjuta_command_cancel),
		                          clone_command);
	}

	if (notify)
	{
		g_signal_connect_swapped (G_OBJECT (clone_command), 
		                          "command-finished",
		                          G_CALLBACK (anjuta_async_notify_notify_finished),
		                          notify);
	}

	anjuta_command_start (ANJUTA_COMMAND (clone_command));
}

static void
on_diff_command_data_arrived (AnjutaCommand *command, 
							  IAnjutaVcsDiffCallback callback)
{
	GQueue *output;
	gchar *line;
	
	output = git_raw_output_command_get_output (GIT_RAW_OUTPUT_COMMAND (command));
	
	while (g_queue_peek_head (output))
	{
		line = g_queue_pop_head (output);
		callback (g_object_get_data (G_OBJECT (command), "file"), line,
				  g_object_get_data (G_OBJECT (command), "user-data"));
		g_free (line);
	}
}

void 
git_ivcs_diff (IAnjutaVcs *obj, GFile* file,  
			   IAnjutaVcsDiffCallback callback, gpointer user_data,  
			   GCancellable* cancel, AnjutaAsyncNotify *notify,
			   GError **err)
{
	gchar *project_root_directory;
	GitDiffCommand *diff_command;
	
	project_root_directory = ANJUTA_PLUGIN_GIT (obj)->project_root_directory;
	
	if (project_root_directory)
	{
		diff_command = git_diff_command_new (project_root_directory);
		
		g_object_set_data_full (G_OBJECT (diff_command), "file", 
		                        g_object_ref (file),
		                        (GDestroyNotify) g_object_unref);
		g_object_set_data (G_OBJECT (diff_command), "user-data", user_data);
		
		g_signal_connect (G_OBJECT (diff_command), "command-finished", 
		                  G_CALLBACK (g_object_unref),
		                  NULL);
		
		g_signal_connect (G_OBJECT (diff_command), "data-arrived",
		                  G_CALLBACK (on_diff_command_data_arrived),
		                  callback);
		
/* FIXME: Reenable when canceling is implemented. */
#if 0
		if (cancel)
		{
			g_signal_connect_swapped (G_OBJECT (cancel), "cancelled",
			                          G_CALLBACK (anjuta_command_cancel),
			                          diff_command);
		}
#endif
		
		if (notify)
		{
			g_signal_connect_swapped (G_OBJECT (diff_command), "command-finished",
			                          G_CALLBACK (anjuta_async_notify_notify_finished),
			                          notify);
		}
		
		anjuta_command_start (ANJUTA_COMMAND (diff_command));
	}
}

static void
on_status_command_data_arrived (AnjutaCommand *command, 
								IAnjutaVcsStatusCallback callback)
{
	GQueue *status_queue;
	GitStatus *status;
	const gchar *working_directory;
	gchar *path;
	gchar *full_path;
	GFile *file;
	
	status_queue = git_status_command_get_status_queue (GIT_STATUS_COMMAND (command));
	
	while (g_queue_peek_head (status_queue))
	{
		status = g_queue_pop_head (status_queue);
		working_directory = g_object_get_data (G_OBJECT (command), 
		                                       "working-directory");
		path = git_status_get_path (status);
		full_path = g_strconcat (working_directory, G_DIR_SEPARATOR_S, path, 
		                         NULL);
		file = g_file_new_for_path (full_path);

		DEBUG_PRINT ("Working directory: %s\n", working_directory);
		DEBUG_PRINT ("File %s Status %i\n", full_path, git_status_get_vcs_status (status));

		if (file)
		{
			callback (file, 
			          git_status_get_vcs_status (status),
			          g_object_get_data (G_OBJECT (command), "user-data"));

			g_object_unref (file);
		}

		g_free (path);
		g_free (full_path);
		
		g_object_unref (status);
	}
}

void 
git_ivcs_query_status (IAnjutaVcs *obj, GFile *file, 
					   IAnjutaVcsStatusCallback callback,
					   gpointer user_data, GCancellable *cancel,
					   AnjutaAsyncNotify *notify, GError **err)
{
	gchar *path;
	GitStatusCommand *status_command;

	path = g_file_get_path (file);
	status_command = git_status_command_new (path, ~0);

	g_free (path);

	g_object_set_data (G_OBJECT (status_command), "user-data", user_data);
	g_object_set_data (G_OBJECT (status_command), "working-directory",
	                   ANJUTA_PLUGIN_GIT (obj)->project_root_directory);

	g_signal_connect (G_OBJECT (status_command), "data-arrived",
	                  G_CALLBACK (on_status_command_data_arrived),
	                  callback);

	g_signal_connect (G_OBJECT (status_command), "command-finished",
	                  G_CALLBACK (g_object_unref),
	                  NULL);

#if 0
	if (cancel)
	{
		g_signal_connect_swapped (G_OBJECT (cancel), "cancelled",
		                          G_CALLBACK (anjuta_command_cancel),
		                          status_command);
	}
#endif

	if (notify)
	{
		g_signal_connect_swapped (G_OBJECT (status_command), "command-finished",
		                          G_CALLBACK (anjuta_async_notify_notify_finished),
		                          notify);
	}

	anjuta_command_queue_push (ANJUTA_PLUGIN_GIT (obj)->command_queue,
	                           ANJUTA_COMMAND (status_command));
}

void 
git_ivcs_remove (IAnjutaVcs *obj, GList *files, 
				 AnjutaAsyncNotify *notify, GError **err)
{
	gchar *project_root_directory;
	GList *path_list;
	GitRemoveCommand *remove_command;
	
	project_root_directory = ANJUTA_PLUGIN_GIT (obj)->project_root_directory;
	
	if (project_root_directory)
	{
		path_list = anjuta_util_convert_gfile_list_to_relative_path_list (files,
		                                                                  project_root_directory);
		remove_command = git_remove_command_new_list (project_root_directory,
		                                              path_list, FALSE);
		
		anjuta_util_glist_strings_free (path_list);
		
		g_signal_connect (G_OBJECT (remove_command), "command-finished", 
		                  G_CALLBACK (g_object_unref), 
		                  NULL);
		
		if (notify)
		{
			g_signal_connect_swapped (G_OBJECT (remove_command), "command-finished",
			                          G_CALLBACK (anjuta_async_notify_notify_finished),
			                          notify);
		}
		
		anjuta_command_start (ANJUTA_COMMAND (remove_command));
	}
}
