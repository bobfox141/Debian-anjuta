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

#include "subversion-update-dialog.h"

#define BROWSE_BUTTON_UPDATE_DIALOG "browse_button_update_dialog"

static void
on_update_command_finished (AnjutaCommand *command, guint return_code,
							Subversion *plugin)
{
	AnjutaStatus *status;
	
	status = anjuta_shell_get_status (ANJUTA_PLUGIN (plugin)->shell,
									  NULL);
	
	anjuta_status (status, _("Subversion: Update complete."), 5);
	
	report_errors (command, return_code);
	
	svn_update_command_destroy (SVN_UPDATE_COMMAND (command));	
}

static void
on_subversion_update_response(GtkDialog* dialog, gint response, SubversionData* data)
{	
	switch (response)
	{
		case GTK_RESPONSE_OK:
		{
			const gchar* revision;
		
			GtkWidget* norecurse;
			GtkWidget* revisionentry;
			GtkWidget* fileentry = GTK_WIDGET (gtk_builder_get_object (data->bxml, "subversion_update_filename"));
			const gchar* filename = g_strdup(gtk_entry_get_text(GTK_ENTRY(fileentry)));
			SvnUpdateCommand *update_command;
			
			norecurse = GTK_WIDGET (gtk_builder_get_object (data->bxml, "subversion_update_norecurse"));
			revisionentry = GTK_WIDGET (gtk_builder_get_object (data->bxml, "subversion_revision"));
			revision = gtk_entry_get_text(GTK_ENTRY(revisionentry));
			
			if (!check_input (GTK_WIDGET (dialog), 
							  fileentry, _("Please enter a path.")))
			{
				break;
			}
			
			update_command = svn_update_command_new ((gchar *) filename, 
													 (gchar *) revision, 
													 !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(norecurse)));
			create_message_view (data->plugin);
			
			g_signal_connect (G_OBJECT (update_command), "command-finished",
							  G_CALLBACK (on_update_command_finished),
							  data->plugin);

			g_signal_connect (G_OBJECT (update_command), "command-finished",
							  G_CALLBACK (subversion_plugin_status_changed_emit),
							  data->plugin);
			
			g_signal_connect (G_OBJECT (update_command), "data-arrived",
							  G_CALLBACK (on_command_info_arrived),
							  data->plugin);
			
			anjuta_command_start (ANJUTA_COMMAND (update_command));
			
			subversion_data_free(data);
			gtk_widget_destroy(GTK_WIDGET(dialog));
			break;
		}
		default:
		{
			gtk_widget_destroy(GTK_WIDGET(dialog));
			subversion_data_free(data);
			break;
		}
	}
}

static void
subversion_update_dialog (GtkAction* action, Subversion* plugin, gchar *filename)
{
	GtkBuilder* bxml = gtk_builder_new ();
	GtkWidget* dialog; 
	GtkWidget* fileentry;
	GtkWidget* project;
	GtkWidget* button;
	SubversionData* data;
	GError* error = NULL;

	if (!gtk_builder_add_from_file (bxml, GLADE_FILE, &error))
	{
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}

	dialog = GTK_WIDGET (gtk_builder_get_object (bxml, "subversion_update"));
	fileentry = GTK_WIDGET (gtk_builder_get_object (bxml, "subversion_update_filename"));
	if (filename)
		gtk_entry_set_text(GTK_ENTRY(fileentry), filename);
	
	project = GTK_WIDGET (gtk_builder_get_object (bxml, "subversion_project"));
	g_object_set_data (G_OBJECT (project), "fileentry", fileentry);
	g_signal_connect(G_OBJECT(project), "toggled", 
		G_CALLBACK(on_whole_project_toggled), plugin);
	init_whole_project(plugin, project, !filename);

	button = GTK_WIDGET (gtk_builder_get_object (bxml, BROWSE_BUTTON_UPDATE_DIALOG));
	g_signal_connect(G_OBJECT(button), "clicked", 
		G_CALLBACK(on_subversion_browse_button_clicked), fileentry);

	data = subversion_data_new(plugin, bxml);
	g_signal_connect(G_OBJECT(dialog), "response", 
		G_CALLBACK(on_subversion_update_response), data);
	
	gtk_widget_show(dialog);	
}

void 
on_menu_subversion_update (GtkAction* action, Subversion* plugin)
{
	subversion_update_dialog(action, plugin, NULL);
}

void 
on_fm_subversion_update (GtkAction* action, Subversion* plugin)
{
	subversion_update_dialog(action, plugin, plugin->fm_current_filename);
}
