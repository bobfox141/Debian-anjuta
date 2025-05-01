/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    cvs-execute.c
    Copyright (C) 2004 Johannes Schmid

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "cvs-execute.h"
#include "plugin.h"

#include <libanjuta/interfaces/ianjuta-message-manager.h>
#include <libanjuta/interfaces/ianjuta-document-manager.h>
#include <libanjuta/interfaces/ianjuta-editor.h>
#include <libanjuta/anjuta-launcher.h>
#include <libanjuta/anjuta-debug.h>

#include <unistd.h>

#define CVS_ICON ""
#define CVS_INFO_REGEXP "(cvs update:.|cvs server:.)"
#define CVS_ERR_REGEXP "^C ."


static GtkWidget* status_text;

static void
on_mesg_view_destroy(CVSPlugin* plugin, gpointer destroyed_view)
{
	plugin->mesg_view = NULL;
}

static void
on_cvs_mesg_format (IAnjutaMessageView *view, const gchar *line,
					  AnjutaPlugin *plugin)
{
	IAnjutaMessageViewType type;
	GRegex *info, *err;
	GError *error = NULL;
	
	g_return_if_fail (line != NULL);

	/* Compile the regexps for message types. */
	info = g_regex_new (CVS_INFO_REGEXP, 0, 0, &error);
	if (error != NULL)
	{
		g_error_free (error);
		return;
	}
	err = g_regex_new (CVS_ERR_REGEXP, 0, 0, &error);
	if (error != NULL)
	{
		g_error_free (error);
		return;
	}		
	
	/* Match against type regexps to find the message type. */
	if (g_regex_match (info, line, 0, NULL))
		type = IANJUTA_MESSAGE_VIEW_TYPE_INFO;
	else if (g_regex_match (info, line, 0, NULL))
		type = IANJUTA_MESSAGE_VIEW_TYPE_ERROR;
	else 
		type = IANJUTA_MESSAGE_VIEW_TYPE_NORMAL;

	ianjuta_message_view_append (view, type, line, "", NULL);
	
	g_regex_unref (info);
	g_regex_unref (err);
}

static void
on_cvs_mesg_parse (IAnjutaMessageView *view, const gchar *line,
					 AnjutaPlugin *plugin)
{
	/* FIXME: Parse the line and determine if there is filename to goto.
	   If there is, extract filename then open it.

	*/

#if 0
	gchar *filename;
	gint lineno;
	
	if ((filename = parse_filename (line)))
	{
		GFile *file;
		IAnjutaFileLoader *loader;
		
		/* Go to file and line number */
		loader = anjuta_shell_get_interface (plugin->shell, IAnjutaFileLoader,
											 NULL);
		
		/* FIXME: Determine full file path */
		file = g_file_new_for_path (filename);
		ianjuta_file_loader_load (loader, uri, FALSE, NULL);
		g_object_unref (file);
		g_free (filename);
	}
#endif
}

static void
on_cvs_terminated (AnjutaLauncher *launcher, gint child_pid, gint status,
				   gulong time_taken, CVSPlugin *plugin)
{
	g_return_if_fail (plugin != NULL);
	/* DEBUG_PRINT ("%s", "Shuting down cvs message view"); */
	
	if (status != 0)
	{
		ianjuta_message_view_append (plugin->mesg_view,
									 IANJUTA_MESSAGE_VIEW_TYPE_INFO,
			_("CVS command failed. See above for details"), "", NULL);
	}
	else
	{
		gchar *mesg;
		mesg = g_strdup_printf (ngettext("CVS command successful! Time taken: %ld second",
										 "CVS command successful! Time taken: %ld seconds",
										 time_taken), time_taken);
		ianjuta_message_view_append (plugin->mesg_view,
									 IANJUTA_MESSAGE_VIEW_TYPE_INFO,
									 mesg, "", NULL);
		g_free (mesg);
	}
	plugin->executing_command = FALSE;
}

static void
on_cvs_message (AnjutaLauncher *launcher,
					   AnjutaLauncherOutputType output_type,
					   const gchar * mesg, gpointer user_data)
{
	CVSPlugin* plugin = ANJUTA_PLUGIN_CVS (user_data);
	if (plugin->mesg_view)
		ianjuta_message_view_buffer_append (plugin->mesg_view, mesg, NULL);
}

static void 
on_cvs_status(AnjutaLauncher *launcher,
					   AnjutaLauncherOutputType output_type,
					   const gchar * mesg, gpointer user_data)
{
	CVSPlugin* plugin = ANJUTA_PLUGIN_CVS (user_data);
	switch(output_type)
	{
		case ANJUTA_LAUNCHER_OUTPUT_STDERR:
			if (plugin->mesg_view)
				ianjuta_message_view_buffer_append (plugin->mesg_view,
													mesg, NULL);
			break;
		default:
		{
			GtkTextBuffer* textbuf;
			GtkTextIter end;
			
			if (status_text)
			{
				textbuf =
					gtk_text_view_get_buffer(GTK_TEXT_VIEW(status_text));
				gtk_text_buffer_get_end_iter(textbuf, &end);
				
				gtk_text_buffer_insert(textbuf, &end, mesg, -1);
			}
		}
	}	
}

static void
on_cvs_diff(AnjutaLauncher *launcher,
			AnjutaLauncherOutputType output_type,
			const gchar * mesg, gpointer user_data)
{
	g_return_if_fail (user_data != NULL);
	CVSPlugin* plugin = ANJUTA_PLUGIN_CVS (user_data);
	
	switch(output_type)
	{
		case ANJUTA_LAUNCHER_OUTPUT_STDERR:
			if (plugin->mesg_view)
				ianjuta_message_view_buffer_append (plugin->mesg_view,
													mesg, NULL);
			break;
		default:
			ianjuta_editor_append (plugin->diff_editor, mesg, -1, NULL);
	}
}

static gboolean
on_cvs_status_destroy(GtkWidget* window, GdkEvent* event, gpointer data)
{
	status_text = NULL;
	
	return FALSE; /* Do not block the event */
}

static void 
cvs_execute_common (CVSPlugin* plugin, const gchar* command, const gchar* dir,
					AnjutaLauncherOutputCallback output)
{
	IAnjutaMessageManager *mesg_manager;
	
	g_return_if_fail (command != NULL);
	g_return_if_fail (dir != NULL);	
	
	if (plugin->executing_command)
	{
		anjuta_util_dialog_error
			(NULL,_("CVS command is running — please wait until it finishes!"),
			 NULL);
		return;
	}
		
	mesg_manager = anjuta_shell_get_interface 
		(ANJUTA_PLUGIN (plugin)->shell,	IAnjutaMessageManager, NULL);
	plugin->mesg_view = 
	    ianjuta_message_manager_get_view_by_name(mesg_manager, _("CVS"), NULL);
	if (!plugin->mesg_view)
	{
		plugin->mesg_view =
		     ianjuta_message_manager_add_view (mesg_manager, _("CVS"), 
											   CVS_ICON, NULL);
		g_object_weak_ref (G_OBJECT (plugin->mesg_view), 
						  (GWeakNotify)on_mesg_view_destroy, plugin);
		g_signal_connect (G_OBJECT (plugin->mesg_view), "buffer-flushed",
						  G_CALLBACK (on_cvs_mesg_format), plugin);
		g_signal_connect (G_OBJECT (plugin->mesg_view), "message-clicked",
						  G_CALLBACK (on_cvs_mesg_parse), plugin);
	}
	ianjuta_message_view_clear(plugin->mesg_view, NULL);

	if (plugin->launcher == NULL)
	{
		plugin->launcher = anjuta_launcher_new ();
		
		g_signal_connect (G_OBJECT (plugin->launcher), "child-exited",
						  G_CALLBACK (on_cvs_terminated), plugin);
	}
	chdir (dir);
	plugin->executing_command = TRUE;

	/* DEBUG_PRINT ("CVS Executing: %s", command); */
	ianjuta_message_view_append (plugin->mesg_view,
								 IANJUTA_MESSAGE_VIEW_TYPE_NORMAL,
								 command, "", NULL);
	anjuta_launcher_execute (plugin->launcher, command, output, plugin);
}

void 
cvs_execute(CVSPlugin* plugin, const gchar* command, const gchar* dir)
{
	cvs_execute_common(plugin, command, dir, on_cvs_message);	
}

void
cvs_execute_status(CVSPlugin* plugin, const gchar* command, const gchar* dir)
{
	GtkBuilder* bxml;
	GtkWidget* window;
	GError* error = NULL;
	bxml = gtk_builder_new();
	if (!gtk_builder_add_from_file(bxml, GLADE_FILE, &error))
	{
		g_warning("Couldn't load builder file: %s", error->message);
		g_error_free(error);
	}

	window = GTK_WIDGET(gtk_builder_get_object(bxml, "cvs_status_output"));
	status_text = GTK_WIDGET(gtk_builder_get_object(bxml, "cvs_status_text"));
	
	g_signal_connect(G_OBJECT(window), "delete-event", 
		G_CALLBACK(on_cvs_status_destroy), status_text);
	
	gtk_widget_show(window);
	cvs_execute_common(plugin, command, dir, on_cvs_status);	
}

void
cvs_execute_diff(CVSPlugin* plugin, const gchar* command, const gchar* dir)
{
	IAnjutaDocumentManager *docman;
	
	docman = anjuta_shell_get_interface (ANJUTA_PLUGIN (plugin)->shell,
	                                     IAnjutaDocumentManager, NULL);
	plugin->diff_editor =
		ianjuta_document_manager_add_buffer(docman, "cvs.diff", "", NULL);
	cvs_execute_common(plugin, command, dir, on_cvs_diff);
}

void
cvs_execute_log(CVSPlugin* plugin, const gchar* command, const gchar* dir)
{
	IAnjutaDocumentManager *docman;
	
	docman = anjuta_shell_get_interface (ANJUTA_PLUGIN (plugin)->shell,
	                                     IAnjutaDocumentManager, NULL);
	plugin->diff_editor =
		ianjuta_document_manager_add_buffer(docman, "cvs.log", "", NULL);
	cvs_execute_common(plugin, command, dir, on_cvs_diff);
}
