/*  patch-plugin.c (C) 2002 Johannes Schmid
 *  					     2005 Massimo Cora'  [ported to the new plugin architetture]
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
 
#include <libanjuta/anjuta-launcher.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/interfaces/ianjuta-project-manager.h>

#include "plugin.h"
#include "patch-plugin.h"

#define BUILDER_FILE PACKAGE_DATA_DIR"/glade/patch-plugin.ui"

static void patch_level_changed (GtkAdjustment *adj);

static void on_ok_clicked (GtkButton *button, PatchPlugin* p_plugin);
static void on_cancel_clicked (GtkButton *button, PatchPlugin* plugin);

static void on_msg_arrived (AnjutaLauncher *launcher,
							AnjutaLauncherOutputType output_type,
							const gchar* line, gpointer data);
static void on_msg_buffer (IAnjutaMessageView *view, const gchar *line,
					  PatchPlugin *plugin);

static void on_patch_terminated (AnjutaLauncher *launcher,
								 gint child_pid, gint status, gulong time_taken,
								 gpointer data);


static gint patch_level = 0;


static void
patch_level_changed (GtkAdjustment *adj)
{
	patch_level = (gint) gtk_adjustment_get_value (adj);
}

static gchar* get_project_uri(PatchPlugin *plugin)
{
	GValue value = {0, };
	gchar* uri;
	GError* err = NULL;
	anjuta_shell_get_value (ANJUTA_PLUGIN (plugin)->shell,
													IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI,
													&value,
													&err);
	if (err)
	{
		g_error_free(err);
		return NULL;
	}
	uri = g_value_dup_string (&value);
	return uri;
}
  
void
patch_show_gui (PatchPlugin *plugin)
{
	GtkWidget* table;
	GtkWidget* scale;
	GtkAdjustment* adj;
	GError* error = NULL;
	gchar* uri = get_project_uri(plugin);
	GtkFileFilter* filter;
	
	GtkBuilder* bxml = gtk_builder_new ();

	if (!gtk_builder_add_from_file (bxml, BUILDER_FILE, &error))
	{
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}

	plugin->dialog = GTK_WIDGET (gtk_builder_get_object (bxml, "patch_dialog"));
	plugin->output_label = GTK_WIDGET (gtk_builder_get_object (bxml, "output"));
	
	table = GTK_WIDGET (gtk_builder_get_object (bxml, "patch_table"));

	plugin->file_chooser = gtk_file_chooser_button_new(_("File/Directory to patch"),
	                                                   GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	gtk_widget_set_hexpand (GTK_WIDGET (plugin->file_chooser), TRUE);
	
	plugin->patch_chooser = gtk_file_chooser_button_new(_("Patch file"),
	                                                    GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_widget_set_hexpand (GTK_WIDGET (plugin->patch_chooser), TRUE);
	
	if (uri)
	{
		gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER(plugin->file_chooser),
																						 uri);
		gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER(plugin->patch_chooser),
																						 uri);
	}
	g_free(uri);
	
	gtk_file_chooser_button_set_width_chars(GTK_FILE_CHOOSER_BUTTON(plugin->file_chooser),
																					30);
	gtk_file_chooser_button_set_width_chars(GTK_FILE_CHOOSER_BUTTON(plugin->patch_chooser),
																					30);
	
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*.diff");
	gtk_file_filter_add_pattern (filter, "*.patch");
	gtk_file_filter_set_name (filter, _("Patches"));  
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (plugin->patch_chooser),
															 filter);
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_filter_set_name (filter, _("All files"));  
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (plugin->patch_chooser),
															 filter);

	gtk_grid_attach (GTK_GRID (table), plugin->file_chooser, 1, 0, 1, 1);
	gtk_grid_attach (GTK_GRID (table), plugin->patch_chooser, 1, 1, 1, 1);
	
	scale = GTK_WIDGET (gtk_builder_get_object (bxml, "patch_level_scale"));
	adj = gtk_range_get_adjustment(GTK_RANGE(scale));
	g_signal_connect (G_OBJECT(adj), "value_changed",
			    G_CALLBACK (patch_level_changed), NULL);
		
	plugin->patch_button = GTK_WIDGET (gtk_builder_get_object (bxml, "patch_button"));
	plugin->cancel_button = GTK_WIDGET (gtk_builder_get_object (bxml, "cancel_button"));
	plugin->dry_run_check = GTK_WIDGET (gtk_builder_get_object (bxml, "dryrun"));
	
	g_signal_connect (G_OBJECT (plugin->patch_button), "clicked", 
			G_CALLBACK(on_ok_clicked), plugin);
	g_signal_connect (G_OBJECT (plugin->cancel_button), "clicked", 
			G_CALLBACK(on_cancel_clicked), plugin);
	
	plugin->executing = FALSE;
	gtk_widget_show_all (plugin->dialog);
}


static void 
on_ok_clicked (GtkButton *button, PatchPlugin* p_plugin)
{
	gchar* tmp;
	gchar* directory;
	gchar* patch_file;
	GString* command;
	gchar* message;
	IAnjutaMessageManager *mesg_manager;
	
	g_return_if_fail (p_plugin != NULL);

	mesg_manager = anjuta_shell_get_interface 
		(ANJUTA_PLUGIN (p_plugin)->shell,	IAnjutaMessageManager, NULL);
	
	g_return_if_fail (mesg_manager != NULL);

	tmp = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(p_plugin->file_chooser));
	if (!g_file_test (tmp, G_FILE_TEST_IS_DIR))
	{
		g_free (tmp);
		anjuta_util_dialog_error(GTK_WINDOW(p_plugin->dialog), 
			_("Please select the directory where the patch should be applied"));
		return;
	}
	directory = g_shell_quote (tmp);
	g_free (tmp);
	
	tmp = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(p_plugin->patch_chooser));
	patch_file = g_shell_quote (tmp);
	g_free (tmp);

	p_plugin->mesg_view =
		ianjuta_message_manager_add_view (mesg_manager, _("Patch"), 
		ICON_FILE, NULL);
	
	ianjuta_message_manager_set_current_view (mesg_manager, p_plugin->mesg_view, NULL);	

	g_signal_connect (G_OBJECT (p_plugin->mesg_view), "buffer-flushed",
						  G_CALLBACK (on_msg_buffer), p_plugin);

	command = g_string_new (NULL);
	
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p_plugin->dry_run_check)))
		g_string_printf (command, "patch --dry-run -d %s -p %d -f -i %s",
				  directory, patch_level, patch_file);
	else
		g_string_printf (command, "patch -d %s -p %d -f -i %s",
				  directory, patch_level, patch_file);
	
	message = g_strdup_printf (_("Patching %s using %s\n"), 
			  directory, patch_file);

	g_free (patch_file);
	g_free (directory);
	
	ianjuta_message_view_append (p_plugin->mesg_view,
								 IANJUTA_MESSAGE_VIEW_TYPE_NORMAL,
								 message, "", NULL);
	
	ianjuta_message_view_append (p_plugin->mesg_view,
								 IANJUTA_MESSAGE_VIEW_TYPE_NORMAL,
								 _("Patching…\n"), "", NULL);

	g_signal_connect (p_plugin->launcher, "child-exited",
					  G_CALLBACK (on_patch_terminated), p_plugin);
	
	if (!anjuta_launcher_is_busy (p_plugin->launcher))
	{
		anjuta_launcher_execute (p_plugin->launcher, command->str,
								 (AnjutaLauncherOutputCallback)on_msg_arrived, p_plugin);
		p_plugin->executing = TRUE;
		gtk_label_set_text(GTK_LABEL(p_plugin->output_label), _("Patching…"));
		gtk_widget_set_sensitive(p_plugin->patch_button, FALSE);
	}
	else
		anjuta_util_dialog_error(GTK_WINDOW(p_plugin->dialog),
			_("There are unfinished jobs: please wait until they are finished."));
	g_string_free(command, TRUE);
}

static void 
on_cancel_clicked (GtkButton *button, PatchPlugin* plugin)
{
	if (plugin->executing == TRUE)
	{
		anjuta_launcher_reset(plugin->launcher);
	}
	gtk_widget_hide (GTK_WIDGET(plugin->dialog));
	gtk_widget_destroy (GTK_WIDGET(plugin->dialog));
}

static void 
on_msg_arrived (AnjutaLauncher *launcher,
							AnjutaLauncherOutputType output_type,
							const gchar* line, gpointer data)
{	
	g_return_if_fail (line != NULL);	
	
	PatchPlugin* p_plugin = ANJUTA_PLUGIN_PATCH (data);
	ianjuta_message_view_buffer_append (p_plugin->mesg_view, line, NULL);
}

static void 
on_patch_terminated (AnjutaLauncher *launcher,
								 gint child_pid, gint status, gulong time_taken,
								 gpointer data)
{
	g_return_if_fail (launcher != NULL);
	
	PatchPlugin* plugin = ANJUTA_PLUGIN_PATCH (data);
	
	g_signal_handlers_disconnect_by_func (G_OBJECT (launcher),
										  G_CALLBACK (on_patch_terminated),
										  plugin);

	if (status != 0)
	{
		gtk_label_set_text(GTK_LABEL(plugin->output_label), 
			_("Patch failed.\nPlease review the failure messages.\n"
			"Examine and remove any rejected files.\n"));
		gtk_widget_set_sensitive(plugin->patch_button, TRUE);	
	}
	else
	{
		gtk_label_set_text(GTK_LABEL(plugin->output_label), _("Patching complete"));
		if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(plugin->dry_run_check)))
		{
			gtk_widget_hide (plugin->dialog);
			gtk_widget_destroy (plugin->dialog);
		}
		else
			gtk_widget_set_sensitive(plugin->patch_button, TRUE);
	}

	plugin->executing = FALSE;
	plugin->mesg_view = NULL;
}

static void
on_msg_buffer (IAnjutaMessageView *view, const gchar *line,
					  PatchPlugin *plugin)
{
	ianjuta_message_view_append (view, IANJUTA_MESSAGE_VIEW_TYPE_NORMAL, line, "", NULL);
}
