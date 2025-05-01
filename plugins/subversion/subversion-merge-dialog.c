/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) James Liggett 2007 <jrliggett@cox.net>
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

#include "subversion-merge-dialog.h"

#define BROWSE_BUTTON_MERGE_DIALOG "browse_button_merge_dialog"

static void
on_merge_command_finished (AnjutaCommand *command, guint return_code,
						   Subversion *plugin)
{
	AnjutaStatus *status;
	
	status = anjuta_shell_get_status (ANJUTA_PLUGIN (plugin)->shell,
									  NULL);
	
	anjuta_status (status, _("Subversion: Merge complete."), 5);
	
	report_errors (command, return_code);
	
	svn_merge_command_destroy (SVN_MERGE_COMMAND (command));
}

static void
on_subversion_merge_response (GtkDialog *dialog, gint response,
							  SubversionData *data)
{
	GtkWidget *merge_first_path_entry;
	GtkWidget *merge_second_path_entry;
	GtkWidget *merge_working_copy_path_entry;
	GtkWidget *merge_start_revision_radio;
	GtkWidget *merge_start_revision_entry;
	GtkWidget *merge_end_revision_radio;
	GtkWidget *merge_end_revision_entry;
	GtkWidget *merge_no_recursive_check;
	GtkWidget *merge_ignore_ancestry_check;
	GtkWidget *merge_force_check;
	GtkWidget *merge_dry_run_check;
	const gchar *first_path;
	const gchar *second_path;
	const gchar *working_copy_path;
	const gchar *start_revision_text;
	glong start_revision;
	const gchar *end_revision_text;
	glong end_revision;
	SvnMergeCommand *merge_command;
	
	if (response == GTK_RESPONSE_OK)
	{
		merge_first_path_entry = GTK_WIDGET (gtk_builder_get_object (data->bxml, 
													   "merge_first_path_entry"));
		merge_second_path_entry = GTK_WIDGET (gtk_builder_get_object (data->bxml,
														"merge_second_path_entry"));
		merge_working_copy_path_entry = GTK_WIDGET (gtk_builder_get_object (data->bxml,
															  "merge_working_copy_path_entry"));
		merge_start_revision_radio = GTK_WIDGET (gtk_builder_get_object (data->bxml,
														   "merge_start_revision_radio"));
		merge_start_revision_entry = GTK_WIDGET (gtk_builder_get_object (data->bxml,
														   "merge_start_revision_entry"));
		merge_end_revision_radio = GTK_WIDGET (gtk_builder_get_object (data->bxml,
														 "merge_end_revision_radio"));
		merge_end_revision_entry = GTK_WIDGET (gtk_builder_get_object (data->bxml,
														 "merge_end_revision_entry"));
		merge_no_recursive_check = GTK_WIDGET (gtk_builder_get_object (data->bxml,
														 "merge_no_recursive_check"));
		merge_ignore_ancestry_check = GTK_WIDGET (gtk_builder_get_object (data->bxml,
															"merge_ignore_ancestry_check"));
		merge_force_check = GTK_WIDGET (gtk_builder_get_object (data->bxml, 
												  "merge_force_check"));
		merge_dry_run_check = GTK_WIDGET (gtk_builder_get_object (data->bxml,
													"merge_dry_run_check"));
		
		if (!check_input (GTK_WIDGET (dialog), merge_first_path_entry,
						  _("Please enter the first path.")))
		{
			return;
		}
		
		if (!check_input (GTK_WIDGET (dialog), merge_second_path_entry,
						  _("Please enter the second path.")))
		{
			return;
		}
		
		if (!check_input (GTK_WIDGET (dialog), merge_working_copy_path_entry,
						  _("Please enter a working copy path.")))
		{
			return;
		}
		
		first_path = gtk_entry_get_text (GTK_ENTRY (merge_first_path_entry));
		second_path = gtk_entry_get_text (GTK_ENTRY (merge_second_path_entry));
		working_copy_path = gtk_entry_get_text (GTK_ENTRY (merge_working_copy_path_entry));
		
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (merge_start_revision_radio)))
		{
			start_revision_text = gtk_entry_get_text (GTK_ENTRY (merge_start_revision_entry));
			
			if (!check_input (GTK_WIDGET (dialog), merge_start_revision_entry,
						  	  _("Please enter the start revision.")))
			{
				return;
			}
			
			start_revision = atol (start_revision_text);
		}
		else
			start_revision = SVN_MERGE_REVISION_HEAD;
			
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (merge_end_revision_radio)))
		{
			end_revision_text = gtk_entry_get_text (GTK_ENTRY (merge_end_revision_entry));
			
			if (!check_input (GTK_WIDGET (dialog), merge_end_revision_entry,
						  	  _("Please enter the end revision.")))
			{
				return;
			}
			
			end_revision = atol (end_revision_text);
		}
		else
			end_revision = SVN_MERGE_REVISION_HEAD;
			
		create_message_view (data->plugin);
			
		merge_command = svn_merge_command_new ((gchar *) first_path,
											   (gchar *) second_path,
											   start_revision,
											   end_revision,
											   (gchar *) working_copy_path,
											   !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (merge_no_recursive_check)),
											   gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (merge_ignore_ancestry_check)),
											   gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (merge_force_check)),
											   gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (merge_dry_run_check)));
		
		g_signal_connect (G_OBJECT (merge_command), "command-finished",
						  G_CALLBACK (on_merge_command_finished),
						  data->plugin);
			
		g_signal_connect (G_OBJECT (merge_command), "data-arrived",
						  G_CALLBACK (on_command_info_arrived),
						  data->plugin);
			
		anjuta_command_start (ANJUTA_COMMAND (merge_command));							  
	}
	
	subversion_data_free (data);
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_merge_first_path_browse_button_clicked (GtkButton *button, 
										   SubversionData *data)
{
	GtkWidget *subversion_merge;
	GtkWidget *merge_first_path_entry;
	GtkWidget *file_chooser_dialog;
	gchar *selected_path;
	
	subversion_merge = GTK_WIDGET (gtk_builder_get_object (data->bxml, "subversion_merge"));
	merge_first_path_entry = GTK_WIDGET (gtk_builder_get_object (data->bxml, 
												   "merge_first_path_entry"));
	file_chooser_dialog = gtk_file_chooser_dialog_new ("Select file or folder",
													   GTK_WINDOW (subversion_merge),
													   GTK_FILE_CHOOSER_ACTION_OPEN,
													   GTK_STOCK_CANCEL,
													   GTK_RESPONSE_CANCEL,
													   GTK_STOCK_OPEN,
													   GTK_RESPONSE_ACCEPT,
													   NULL);
	
	if (gtk_dialog_run (GTK_DIALOG (file_chooser_dialog)) == GTK_RESPONSE_ACCEPT)
	{
		selected_path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_chooser_dialog));
		gtk_entry_set_text (GTK_ENTRY (merge_first_path_entry), selected_path);
		g_free (selected_path);
	}
	
	gtk_widget_destroy (GTK_WIDGET (file_chooser_dialog));	
}

static void
on_merge_second_path_browse_button_clicked (GtkButton *button, 
										    SubversionData *data)
{
	GtkWidget *subversion_merge;
	GtkWidget *merge_second_path_entry;
	GtkWidget *file_chooser_dialog;
	gchar *selected_path;
	
	subversion_merge = GTK_WIDGET (gtk_builder_get_object (data->bxml, "subversion_merge"));
	merge_second_path_entry = GTK_WIDGET (gtk_builder_get_object (data->bxml, 
												    "merge_second_path_entry"));
	file_chooser_dialog = gtk_file_chooser_dialog_new ("Select file or folder",
													   GTK_WINDOW (subversion_merge),
													   GTK_FILE_CHOOSER_ACTION_OPEN,
													   GTK_STOCK_CANCEL,
													   GTK_RESPONSE_CANCEL,
													   GTK_STOCK_OPEN,
													   GTK_RESPONSE_ACCEPT,
													   NULL);
	
	if (gtk_dialog_run (GTK_DIALOG (file_chooser_dialog)) == GTK_RESPONSE_ACCEPT)
	{
		selected_path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_chooser_dialog));
		gtk_entry_set_text (GTK_ENTRY (merge_second_path_entry), selected_path);
		g_free (selected_path);
	}
	
	gtk_widget_destroy (GTK_WIDGET (file_chooser_dialog));	
}

static void
on_merge_use_first_path_check_toggled (GtkToggleButton *toggle_button, 
								 	   SubversionData *data)
{
	GtkWidget *merge_first_path_entry;
	GtkWidget *merge_second_path_entry;
	gboolean active;
	const gchar *first_path;
	
	merge_second_path_entry = GTK_WIDGET (gtk_builder_get_object (data->bxml,
													"merge_second_path_entry"));
	
	active = gtk_toggle_button_get_active (toggle_button);
	
	if (active)
	{
		merge_first_path_entry = GTK_WIDGET (gtk_builder_get_object (data->bxml,
													   "merge_first_path_entry"));
		first_path = gtk_entry_get_text (GTK_ENTRY (merge_first_path_entry));
		
		gtk_entry_set_text (GTK_ENTRY (merge_second_path_entry), first_path);
	}
	
	gtk_widget_set_sensitive (merge_second_path_entry, !active);
	
}

static void
on_merge_start_revision_radio_toggled (GtkToggleButton *toggle_button,
									   SubversionData *data)
{
	GtkWidget *merge_start_revision_entry;
	GtkWidget *subversion_merge;
	gboolean active;
	
	merge_start_revision_entry = GTK_WIDGET (gtk_builder_get_object (data->bxml, 
													   "merge_start_revision_entry"));
	subversion_merge = GTK_WIDGET (gtk_builder_get_object (data->bxml, "subversion_merge"));
	
	active = gtk_toggle_button_get_active (toggle_button);
	
	gtk_widget_set_sensitive (merge_start_revision_entry, active);
	
	if (active)
	{
		gtk_window_set_focus (GTK_WINDOW (subversion_merge), 
							  merge_start_revision_entry);
	}
}

static void
on_merge_end_revision_radio_toggled (GtkToggleButton *toggle_button,
									 SubversionData *data)
{
	GtkWidget *merge_end_revision_entry;
	GtkWidget *subversion_merge;
	gboolean active;
	
	merge_end_revision_entry = GTK_WIDGET (gtk_builder_get_object (data->bxml, 
													   "merge_end_revision_entry"));
	subversion_merge = GTK_WIDGET (gtk_builder_get_object (data->bxml, "subversion_merge"));
	
	active = gtk_toggle_button_get_active (toggle_button);
	
	gtk_widget_set_sensitive (merge_end_revision_entry, active);
	
	if (active)
	{
		gtk_window_set_focus (GTK_WINDOW (subversion_merge), 
							  merge_end_revision_entry);
	}
}

static void 
subversion_merge_dialog (GtkAction *action, Subversion *plugin)
{
	GtkBuilder *bxml = gtk_builder_new ();
	GtkWidget *subversion_merge;
	GtkWidget *merge_first_path_browse_button;
	GtkWidget *merge_second_path_browse_button;
	GtkWidget *merge_use_first_path_check;
	GtkWidget *merge_working_copy_path_entry;
	GtkWidget *merge_start_revision_radio;
	GtkWidget *merge_end_revision_radio;
	GtkWidget *button;
	SubversionData *data;
	GError* error = NULL;

	if (!gtk_builder_add_from_file (bxml, GLADE_FILE, &error))
	{
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}
	subversion_merge = GTK_WIDGET (gtk_builder_get_object (bxml,
											  "subversion_merge"));
	merge_first_path_browse_button = GTK_WIDGET (gtk_builder_get_object (bxml,
														   "merge_first_path_browse_button"));
	merge_second_path_browse_button = GTK_WIDGET (gtk_builder_get_object (bxml, 
															"merge_second_path_browse_button"));
	merge_use_first_path_check = GTK_WIDGET (gtk_builder_get_object (bxml,
													   "merge_use_first_path_check"));
	merge_working_copy_path_entry = GTK_WIDGET (gtk_builder_get_object (bxml,
														  "merge_working_copy_path_entry"));
	merge_start_revision_radio = GTK_WIDGET (gtk_builder_get_object (bxml,
													   "merge_start_revision_radio"));
	merge_end_revision_radio = GTK_WIDGET (gtk_builder_get_object (bxml,
													 "merge_end_revision_radio"));
	
	data = subversion_data_new (plugin, bxml);
	
	gtk_entry_set_text (GTK_ENTRY (merge_working_copy_path_entry),
						plugin->project_root_dir);
	
	g_signal_connect (G_OBJECT (subversion_merge), "response",
					  G_CALLBACK (on_subversion_merge_response),
					  data);

	button = GTK_WIDGET (gtk_builder_get_object (bxml, BROWSE_BUTTON_MERGE_DIALOG));
	g_signal_connect(G_OBJECT(button), "clicked", 
		G_CALLBACK(on_subversion_browse_button_clicked), merge_working_copy_path_entry);

	g_signal_connect (G_OBJECT (merge_first_path_browse_button), "clicked",
					  G_CALLBACK (on_merge_first_path_browse_button_clicked),
					  data);
	
	g_signal_connect (G_OBJECT (merge_second_path_browse_button), "clicked",
					  G_CALLBACK (on_merge_second_path_browse_button_clicked),
					  data);
	
	g_signal_connect (G_OBJECT (merge_use_first_path_check), "toggled",
					  G_CALLBACK (on_merge_use_first_path_check_toggled),
					  data);
	
	g_signal_connect (G_OBJECT (merge_start_revision_radio), "toggled",
					  G_CALLBACK (on_merge_start_revision_radio_toggled),
					  data);
	
	g_signal_connect (G_OBJECT (merge_end_revision_radio), "toggled",
					  G_CALLBACK (on_merge_end_revision_radio_toggled),
					  data);
	
	gtk_dialog_run (GTK_DIALOG (subversion_merge));
}

void 
on_menu_subversion_merge (GtkAction *action, Subversion *plugin)
{
	subversion_merge_dialog (action, plugin);
}
