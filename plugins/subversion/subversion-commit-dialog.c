/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) James Liggett 2007 <jrliggett@cox.net>
 *
 * Portions based on the original Subversion plugin 
 * Copyright (C) Johannes Schmid 2005 
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

#include "subversion-commit-dialog.h"

static void
on_commit_command_finished (AnjutaCommand *command, guint return_code,
							Subversion *plugin)
{
	AnjutaStatus *status;
	
	status = anjuta_shell_get_status (ANJUTA_PLUGIN (plugin)->shell,
									  NULL);
	
	anjuta_status (status, _("Subversion: Commit complete."), 5);
	
	report_errors (command, return_code);
	
	svn_commit_command_destroy (SVN_COMMIT_COMMAND (command));
}

static GList*
subversion_commit_prepend_log (GList *logs, gchar *data)
{
	logs = g_list_prepend(logs, data);	
		
	if (g_list_length(logs) > MAX_LOG_NUM)
		logs = g_list_remove(logs, g_list_last(logs)->data);
			
	return logs; 
}

static void
on_subversion_commit_response(GtkDialog* dialog, gint response, 
							  SubversionData* data)
{	
	switch (response)
	{
		case GTK_RESPONSE_OK:
		{
			gchar* log;
			GtkWidget* logtext;
			GtkWidget* norecurse;
			GtkWidget* commit_prev_msg_enable;
			GtkWidget* commit_prev_msg_combo;
			GtkWidget *commit_status_view;
			GList *selected_paths;
			SvnCommitCommand *commit_command;
			guint pulse_timer_id;
			gboolean msg_enable_selected;
			
			logtext = GTK_WIDGET (gtk_builder_get_object (data->bxml, "subversion_log_view"));
			commit_prev_msg_enable = GTK_WIDGET (gtk_builder_get_object (data->bxml, "commit_prev_msg_enable"));
			
			log = get_log_from_textview(logtext);
			msg_enable_selected = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(commit_prev_msg_enable));
			
			if (!g_utf8_strlen(log, -1) && (msg_enable_selected == FALSE))
			{
				gint result;
				GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW(dialog), 
														GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO,
														GTK_BUTTONS_YES_NO, 
														_("Are you sure that you want to pass an empty log message?"));
				result = gtk_dialog_run(GTK_DIALOG(dlg));
				gtk_widget_destroy(dlg);
				if (result == GTK_RESPONSE_NO)
					break;
			}
			
			commit_prev_msg_combo = GTK_WIDGET (gtk_builder_get_object (data->bxml, "commit_prev_msg_combo"));
													   
			norecurse = GTK_WIDGET (gtk_builder_get_object (data->bxml, "subversion_commit_norecurse"));
			
			commit_status_view = GTK_WIDGET (gtk_builder_get_object (data->bxml, 
													   "commit_status_view"));
			
			selected_paths = anjuta_vcs_status_tree_view_get_selected (ANJUTA_VCS_STATUS_TREE_VIEW (commit_status_view));

			
			if (msg_enable_selected == TRUE)
			{
				commit_command = svn_commit_command_new (selected_paths, 
													 gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT (commit_prev_msg_combo)),
													 !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(norecurse)));
			}
			else
			{
				commit_command = svn_commit_command_new (selected_paths, 
													 (gchar *) log,
													 !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(norecurse)));
			}
			
			svn_command_free_path_list (selected_paths);
			
			create_message_view (data->plugin);
		
			pulse_timer_id = status_bar_progress_pulse (data->plugin,
														_("Subversion: " 
														  "Committing changes "
														  "to the "
														  "repository…"));
			
			g_signal_connect (G_OBJECT (commit_command), "command-finished",
							  G_CALLBACK (stop_status_bar_progress_pulse),
							  GUINT_TO_POINTER (pulse_timer_id));
			
			g_signal_connect (G_OBJECT (commit_command), "command-finished",
							  G_CALLBACK (on_commit_command_finished),
							  data->plugin);

			g_signal_connect (G_OBJECT (commit_command), "command-finished",
							  G_CALLBACK (subversion_plugin_status_changed_emit),
							  data->plugin);
			
			g_signal_connect (G_OBJECT (commit_command), "data-arrived",
							  G_CALLBACK (on_command_info_arrived),
							  data->plugin);
			
			anjuta_command_start (ANJUTA_COMMAND (commit_command));
			
			if (g_utf8_strlen(log, -1) && msg_enable_selected == FALSE)
				data->plugin->svn_commit_logs = subversion_commit_prepend_log(data->plugin->svn_commit_logs, log);
				
			subversion_data_free(data);
			gtk_widget_destroy (GTK_WIDGET(dialog));
			break;
		}
		default:
		{
			subversion_data_free(data);
			gtk_widget_destroy(GTK_WIDGET(dialog));
			break;
		}
	}
}

static void
select_all_files (AnjutaCommand *command, guint return_code, 
				  AnjutaVcsStatusTreeView *status_view)
{
	anjuta_vcs_status_tree_view_select_all (status_view);
}

static void 
on_prev_message_enable_clicked (GtkToggleButton *button, gpointer data)
{
	if (gtk_toggle_button_get_active(button) == FALSE)
	{
		gtk_widget_set_sensitive(GTK_WIDGET(data), TRUE);
	}
	else
	{
		gtk_widget_set_sensitive(GTK_WIDGET(data), FALSE);
	}
}

static void
subversion_commit_dialog_populate_logs (gpointer msg, gpointer user_data)
{
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(user_data), (gchar*) msg);
}

static void
subversion_commit_dialog (GtkAction* action, Subversion* plugin, 
						  gchar *filename)
{
	GtkBuilder* bxml = gtk_builder_new ();
	GtkWidget* dialog; 
	GtkWidget *logtext; 
	GtkWidget *commit_select_all_button;
	GtkWidget *commit_clear_button;
	GtkWidget *commit_status_view;
	GtkWidget *commit_status_progress_bar;
	GtkWidget *commit_prev_msg_enable;
	GtkWidget *commit_prev_msg_combo;
	GtkCellRenderer *cell;
  	GtkListStore *store;
	SvnStatusCommand *status_command;
	SubversionData* data;
	GError* error = NULL;

	if (!gtk_builder_add_from_file (bxml, GLADE_FILE, &error))
	{
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}
	
	dialog = GTK_WIDGET (gtk_builder_get_object (bxml, "subversion_commit"));
	commit_select_all_button = GTK_WIDGET (gtk_builder_get_object (bxml, 
													 "commit_select_all_button"));
	commit_clear_button = GTK_WIDGET (gtk_builder_get_object (bxml,
												"commit_clear_button"));
	commit_status_view = GTK_WIDGET (gtk_builder_get_object (bxml, "commit_status_view"));
	commit_status_progress_bar = GTK_WIDGET (gtk_builder_get_object (bxml,
													   "commit_status_progress_bar"));
	logtext = GTK_WIDGET (gtk_builder_get_object (bxml, "subversion_log_view"));
	status_command = svn_status_command_new (plugin->project_root_dir, 
											 TRUE, TRUE);
	commit_prev_msg_enable = GTK_WIDGET (gtk_builder_get_object (bxml,
													   "commit_prev_msg_enable"));
	commit_prev_msg_combo = GTK_WIDGET (gtk_builder_get_object (bxml,
													   "commit_prev_msg_combo"));
													   
	g_signal_connect (G_OBJECT (commit_select_all_button), "clicked",
					  G_CALLBACK (select_all_status_items),
					  commit_status_view);
	
	g_signal_connect (G_OBJECT (commit_clear_button), "clicked",
					  G_CALLBACK (clear_all_status_selections),
					  commit_status_view);
	
	g_signal_connect (G_OBJECT (status_command), "command-finished",
					  G_CALLBACK (select_all_files),
					  commit_status_view);
	
	g_signal_connect(G_OBJECT (commit_prev_msg_enable), "toggled",
	                 G_CALLBACK(on_prev_message_enable_clicked),
	                 logtext);

	pulse_progress_bar (GTK_PROGRESS_BAR (commit_status_progress_bar));
	
	g_signal_connect (G_OBJECT (status_command), "command-finished",
					  G_CALLBACK (cancel_data_arrived_signal_disconnect),
					  commit_status_view);
	
	g_signal_connect (G_OBJECT (status_command), "command-finished",
					  G_CALLBACK (hide_pulse_progress_bar),
					  commit_status_progress_bar);
	
	g_signal_connect (G_OBJECT (status_command), "command-finished",
					  G_CALLBACK (on_status_command_finished),
					  NULL);
	
	g_signal_connect (G_OBJECT (status_command), "data-arrived",
					  G_CALLBACK (on_status_command_data_arrived),
					  commit_status_view);
	
	g_object_weak_ref (G_OBJECT (commit_status_view),
					   (GWeakNotify) disconnect_data_arrived_signals,
					   status_command);
	
	anjuta_command_start (ANJUTA_COMMAND (status_command));
	
	data = subversion_data_new(plugin, bxml);
	g_signal_connect(G_OBJECT(dialog), "response", 
		G_CALLBACK(on_subversion_commit_response), data);
	
	store = gtk_list_store_new (1, G_TYPE_STRING);
	cell = gtk_cell_renderer_text_new ();
  	gtk_cell_layout_clear(GTK_CELL_LAYOUT(commit_prev_msg_combo));
    gtk_combo_box_set_model(GTK_COMBO_BOX(commit_prev_msg_combo), NULL);                              
	gtk_combo_box_set_model(GTK_COMBO_BOX(commit_prev_msg_combo), GTK_TREE_MODEL(store));
	
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (commit_prev_msg_combo), cell, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (commit_prev_msg_combo), cell,
                                  "text", 0,
                                  NULL);
	g_object_unref (store);
	
	gtk_widget_show_all (dialog);
	
	
	
	g_list_foreach(plugin->svn_commit_logs, subversion_commit_dialog_populate_logs, commit_prev_msg_combo);
	gtk_combo_box_set_active(GTK_COMBO_BOX(commit_prev_msg_combo), 0);
}

void 
on_menu_subversion_commit (GtkAction* action, Subversion* plugin)
{
	subversion_commit_dialog(action, plugin, plugin->current_editor_filename);
}

void 
on_fm_subversion_commit (GtkAction* action, Subversion* plugin)
{
	subversion_commit_dialog(action, plugin, plugin->fm_current_filename);
}
