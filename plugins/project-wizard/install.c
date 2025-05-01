/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    install.c
    Copyright (C) 2004 Sebastien Granjoux

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

/*
 * Handle installation of all files
 *
 *---------------------------------------------------------------------------*/

#include <config.h>

#include "install.h"

#include "plugin.h"
#include "file.h"
#include "parser.h"
#include "action.h"

#include <libanjuta/interfaces/ianjuta-file-loader.h>
#include <libanjuta/anjuta-autogen.h>
#include <libanjuta/anjuta-launcher.h>
#include <libanjuta/anjuta-debug.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>

/*---------------------------------------------------------------------------*/

#define FILE_BUFFER_SIZE	4096

#define AUTOGEN_START_MACRO_LEN	7
#define AUTOGEN_MARKER_1	"autogen5"
#define AUTOGEN_MARKER_2	"template"

/*---------------------------------------------------------------------------*/

struct _NPWInstall
{
	AnjutaAutogen* gen;
	NPWFileListParser* file_parser;
	GList* file_list;
	GList* current_file;
	NPWActionListParser* action_parser;
	GList* action_list;
	GList* action;
	AnjutaLauncher* launcher;
	NPWPlugin* plugin;
	const gchar* project_file;
	gboolean success;
};

/*---------------------------------------------------------------------------*/

static void on_install_end_install_file (AnjutaAutogen* gen, gpointer data);
static void on_run_terminated (AnjutaLauncher* launcher, gint pid, gint status, gulong time, NPWInstall* this);
static gboolean npw_install_install_file (NPWInstall* this);
static gboolean npw_run_action (NPWInstall* this);
static gboolean npw_open_action (NPWInstall* this);

/* Helper functions
 *---------------------------------------------------------------------------*/

static gboolean
npw_is_autogen_template_file (FILE* tpl)
{
	gint len;
	const gchar* marker[] = {"", AUTOGEN_MARKER_1, AUTOGEN_MARKER_2, NULL};
	const gchar** key;
	const gchar* ptr;
	gint c;


	for (key = marker; *key != NULL; ++key)
	{
		/* Skip first whitespace */
		do
		{
			c = fgetc (tpl);
			if (c == EOF) return FALSE;
		}
		while (isspace (c));

		/* Test start marker, then autogen5, then template */
		ptr = *key;
		len = *ptr == '\0' ? AUTOGEN_START_MACRO_LEN : strlen (ptr);
		do
		{
			if (len == 0) return FALSE;
			--len;
			if ((*ptr != '\0') && (tolower (c) != *ptr++)) return FALSE;
			c = fgetc (tpl);
			/* FIXME: Test this EOF test */
			if (c == EOF) return FALSE;
		}
		while (!isspace (c));
		if ((**key != '\0') && (len != 0)) return FALSE;
	}

	return TRUE;
}

static gboolean
npw_is_autogen_template (const gchar* filename)
{
	FILE* tpl;
	gboolean autogen;

	tpl = fopen (filename, "rt");
	if (tpl == NULL) return FALSE;

	autogen = npw_is_autogen_template_file (tpl);
	fclose (tpl);

	return autogen;
}

static gboolean
npw_copy_file (const gchar* destination, const gchar* source)
{
	gchar* buffer;
	FILE* src;
	FILE* dst;
	guint len;
	gboolean ok;

	buffer = g_new (gchar, FILE_BUFFER_SIZE);

	/* Copy file */
	src = fopen (source, "rb");
	if (src == NULL)
	{
		return FALSE;
	}

	dst = fopen (destination, "wb");
	if (dst == NULL)
	{
		return FALSE;
	}

	ok = TRUE;
	for (;!feof (src);)
	{
		len = fread (buffer, 1, FILE_BUFFER_SIZE, src);
		if ((len != FILE_BUFFER_SIZE) && !feof (src))
		{
			ok = FALSE;
			break;
		}

		if (len != fwrite (buffer, 1, len, dst))
		{
			ok = FALSE;
			break;
		}
	}

	fclose (dst);
	fclose (src);

	g_free (buffer);

	return ok;
}

/* Installer object
 *---------------------------------------------------------------------------*/

NPWInstall* npw_install_new (NPWPlugin* plugin)
{
	NPWInstall* this;

	/* Skip if already created */
	if (plugin->install != NULL) return plugin->install;

	this = g_new0(NPWInstall, 1);
	this->gen = anjuta_autogen_new ();
	this->plugin = plugin;
	this->success = TRUE;
	npw_plugin_create_view (plugin);

	plugin->install = this;

	return this;
}

void npw_install_free (NPWInstall* this)
{
	if (this->file_parser != NULL)
	{
		npw_file_list_parser_free (this->file_parser);
	}
	if (this->file_list != NULL)
	{
		g_list_foreach (this->file_list, (GFunc)npw_file_free, NULL);
		g_list_free (this->file_list);
	}
	if (this->action_parser != NULL)
	{
		npw_action_list_parser_free (this->action_parser);
	}
	if (this->action_list != NULL)
	{
		g_list_foreach (this->action_list, (GFunc)npw_action_free, NULL);
		g_list_free (this->action_list);
	}
	if (this->launcher != NULL)
	{
		g_signal_handlers_disconnect_by_func (G_OBJECT (this->launcher), G_CALLBACK (on_run_terminated), this);
		g_object_unref (this->launcher);
	}
	g_object_unref (this->gen);
	this->plugin->install = NULL;
	g_free (this);
}

gboolean
npw_install_set_property (NPWInstall* this, GHashTable* values)
{
	anjuta_autogen_write_definition_file (this->gen, values, NULL);

	return TRUE;
}

gboolean
npw_install_set_wizard_file (NPWInstall* this, const gchar* filename)
{
	if (this->file_list != NULL)
	{
		g_list_foreach (this->file_list, (GFunc)npw_file_free, NULL);
		g_list_free (this->file_list);
		this->file_list = NULL;
	}
	if (this->file_parser != NULL)
	{
		npw_file_list_parser_free (this->file_parser);
	}
	this->file_parser = npw_file_list_parser_new (filename);

	anjuta_autogen_set_input_file (this->gen, filename, "[+","+]");

	return TRUE;
}

gboolean
npw_install_set_library_path (NPWInstall* this, const gchar *directory)
{
	anjuta_autogen_set_library_path (this->gen, directory);

	return TRUE;
}

static void
on_install_read_action_list (const gchar* output, gpointer data)
{
	NPWInstall* this = (NPWInstall*)data;

	npw_action_list_parser_parse (this->action_parser, output, strlen (output), NULL);
}

static void
on_install_end_action (gpointer data)
{
	NPWInstall* this = (NPWInstall*)data;

	for (;;)
	{
		NPWAction *action;

		if (this->action == NULL)
		{
			if (this->success)
			{
				/* Run action on success only */
				this->action = g_list_first (this->action_list);
			}
		}
		else
		{
			this->action = g_list_next (this->action);
		}
		if (this->action == NULL)
		{
			DEBUG_PRINT ("Project wizard done");
			/* The wizard could have been deactivated when loading the new
			 * project. Hence, the following check.
			 */
			if (anjuta_plugin_is_active (ANJUTA_PLUGIN (this->plugin)))
				anjuta_plugin_deactivate (ANJUTA_PLUGIN (this->plugin));
			npw_install_free (this);
			return;
		}
		action = (NPWAction *)this->action->data;

		switch (npw_action_get_type (action))
		{
		case NPW_RUN_ACTION:
			npw_run_action (this);
			return;
		case NPW_OPEN_ACTION:
			npw_open_action (this);
			break;
		default:
			break;
		}
	}
}

static void
on_install_read_all_action_list (AnjutaAutogen* gen, gpointer data)
{
	NPWInstall* this = (NPWInstall*)data;

	this->action_list = npw_action_list_parser_end_parse (this->action_parser, NULL);

	on_install_end_install_file (NULL, this);
}

static void
on_install_read_file_list (const gchar* output, gpointer data)
{
	NPWInstall* this = (NPWInstall*)data;

	npw_file_list_parser_parse (this->file_parser, output, strlen (output), NULL);
}

static void
on_install_read_all_file_list (AnjutaAutogen* gen, gpointer data)
{
	NPWInstall* this = (NPWInstall*)data;

	this->file_list = npw_file_list_parser_end_parse (this->file_parser, NULL);

	this->current_file = NULL;
	this->project_file = NULL;

	if (this->action_list != NULL)
	{
		g_list_foreach (this->action_list, (GFunc)npw_action_free, NULL);
		g_list_free (this->action_list);
		this->action_list = NULL;
	}
	if (this->action_parser != NULL)
	{
		npw_action_list_parser_free (this->action_parser);
	}
	this->action_parser = npw_action_list_parser_new ();
	anjuta_autogen_set_output_callback (this->gen, on_install_read_action_list, this, NULL);
	anjuta_autogen_execute (this->gen, on_install_read_all_action_list, this, NULL);
}

static void
on_install_end_install_file (AnjutaAutogen* gen, gpointer data)
{
	NPWInstall* this = (NPWInstall*)data;

	/* Warning gen could be invalid */

	for (;;)
	{
		NPWFile *file;

		if (this->current_file == NULL)
		{
			this->current_file = g_list_first (this->file_list);
		}
		else
		{
			NPWFile *file = (NPWFile *)this->current_file->data;

			if (npw_file_get_execute (file))
			{
				gint previous;
				/* Make this file executable */
				previous = umask (0666);
				chmod (npw_file_get_destination (file), 0777 & ~previous);
				umask (previous);
			}
			if (npw_file_get_project (file))
			{
				/* Check if project is NULL */
				this->project_file = npw_file_get_destination (file);
			}
			this->current_file = g_list_next (this->current_file);
		}
		if (this->current_file == NULL)
		{
			/* IAnjutaFileLoader* loader; */
			/* All files have been installed */
			if (this->success)
			{
				npw_plugin_print_view (this->plugin,
					IANJUTA_MESSAGE_VIEW_TYPE_INFO,
					 _("New project has been created successfully."),
					 "");
			}
			else
			{
				npw_plugin_print_view (this->plugin,
					IANJUTA_MESSAGE_VIEW_TYPE_ERROR,
					 _("New project creation has failed."),
					 "");
			}
			on_install_end_action (this);

			return;
		}
		file = (NPWFile *)this->current_file->data;
		switch (npw_file_get_type (file))
		{
		case NPW_FILE:
			npw_install_install_file (this);
			return;
		default:
			g_warning("Unknown file type %d\n", npw_file_get_type (file));
			break;
		}
	}
}

gboolean
npw_install_launch (NPWInstall* this)
{
	anjuta_autogen_set_output_callback (this->gen, on_install_read_file_list, this, NULL);
	anjuta_autogen_execute (this->gen, on_install_read_all_file_list, this, NULL);

	return TRUE;
}

static gboolean
npw_install_install_file (NPWInstall* this)
{
	gchar* buffer;
	gchar* sep;
	guint len;
	const gchar* destination;
	const gchar* source;
	gchar* msg;
	gboolean use_autogen;
	gboolean ok = TRUE;
	NPWFile *file = (NPWFile *)this->current_file->data;


	destination = npw_file_get_destination (file);
	source = npw_file_get_source (file);

	/* Check if file already exist */
	if (g_file_test (destination, G_FILE_TEST_EXISTS))
	{
		msg = g_strdup_printf (_("Skipping %s: file already exists"), destination);
		npw_plugin_print_view (this->plugin, IANJUTA_MESSAGE_VIEW_TYPE_WARNING, msg, "");
		g_free (msg);
		on_install_end_install_file (this->gen, this);

		return FALSE;
	}

	/* Check if autogen is needed */
	switch (npw_file_get_autogen (file))
	{
	case NPW_TRUE:
		use_autogen = TRUE;
		break;
	case NPW_FALSE:
		use_autogen = FALSE;
		break;
	case NPW_DEFAULT:
		use_autogen = npw_is_autogen_template (source);
		break;
	default:
		use_autogen = FALSE;
	}

	len = strlen (destination) + 1;
	buffer = g_new (gchar, MAX (FILE_BUFFER_SIZE, len));
	strcpy (buffer, destination);
	sep = buffer;
	for (;;)
	{
		/* Get directory one by one */
		sep = strstr (sep,G_DIR_SEPARATOR_S);
		if (sep == NULL) break;
		/* Create directory if necessary */
		*sep = '\0';
		if ((*buffer != '~') && (*buffer != '\0'))
		{
			if (!g_file_test (buffer, G_FILE_TEST_EXISTS))
			{
				if (mkdir (buffer, 0755) == -1)
				{
					msg = g_strdup_printf (_("Creating %s … Failed to create directory"), destination);
					ok = FALSE;
					break;
				}
			}
		}
		*sep++ = G_DIR_SEPARATOR_S[0];
	}

	if (ok)
	{
		if (use_autogen)
		{
			anjuta_autogen_set_input_file (this->gen, source, NULL, NULL);
			anjuta_autogen_set_output_file (this->gen, destination);
			ok = anjuta_autogen_execute (this->gen, on_install_end_install_file, this, NULL);
			msg = g_strdup_printf (_("Creating %s (using AutoGen)… %s"), destination, ok ? "Ok" : "Fail to Execute");
		}
		else
		{
			ok = npw_copy_file (destination, source);
			msg = g_strdup_printf (_("Creating %s … %s"), destination, ok ? "Ok" : "Fail to copy file");
		}
	}

	/* Record failure and display error message */
	if (!ok)
	{
		this->success = FALSE;
	}
	npw_plugin_print_view (this->plugin, ok ? IANJUTA_MESSAGE_VIEW_TYPE_INFO : IANJUTA_MESSAGE_VIEW_TYPE_ERROR, msg, "");
	g_free (msg);

	/* Next file is called automatically if autogen succeed */
	if (!ok || !use_autogen)
		on_install_end_install_file (this->gen, this);

	return ok;
}

static void
on_run_terminated (AnjutaLauncher* launcher, gint pid, gint status, gulong time, NPWInstall* this)
{
	on_install_end_action (this);
}

static void
on_run_output (AnjutaLauncher* launcher, AnjutaLauncherOutputType type, const gchar* output, gpointer data)
{
	NPWInstall* this = (NPWInstall*)data;

	npw_plugin_append_view (this->plugin, output);
}

static gboolean
npw_run_action (NPWInstall* this)
{
	gchar *msg;
	NPWAction *action = (NPWAction *)this->action->data;

	if (this->launcher == NULL)
	{
		this->launcher = anjuta_launcher_new ();
	}
	g_signal_connect (G_OBJECT (this->launcher), "child-exited", G_CALLBACK (on_run_terminated), this);
	/* The %s is a name of a unix command line, by example
	 * cp foobar.c project */
	msg = g_strdup_printf (_("Executing: %s"), npw_action_get_command (action));
	npw_plugin_print_view (this->plugin, IANJUTA_MESSAGE_VIEW_TYPE_INFO, msg, "");
	g_free (msg);

	return anjuta_launcher_execute (this->launcher, npw_action_get_command (action), on_run_output, this);
}

static gboolean
npw_open_action (NPWInstall* this)
{
	IAnjutaFileLoader* loader;
	NPWAction *action = (NPWAction *)this->action->data;

	loader = anjuta_shell_get_interface (ANJUTA_PLUGIN (this->plugin)->shell, IAnjutaFileLoader, NULL);
	if (loader)
	{
		GFile* file = g_file_new_for_path (npw_action_get_file (action));
		ianjuta_file_loader_load (loader, file, FALSE, NULL);
		g_object_unref (file);

		return TRUE;
	}

	return FALSE;
}
