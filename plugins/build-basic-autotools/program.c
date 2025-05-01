/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    program.c
    Copyright (C) 2008 Sébastien Granjoux

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
 * This objects is used to keep all data useful to run a program:
 *	- working directory
 *  - program name
 *  - program arguments
 *  - program environment variables
 *  - a callback when program end
 *
 * It is just a container, so it is not able to run the program by itself, but
 * all data are available.
 * It includes some functions to modify the program data, take care of
 * memory allocation.
 *---------------------------------------------------------------------------*/


#include "program.h"

#include <libanjuta/anjuta-utils.h>

#include <glib/gi18n.h>

#include <string.h>
#include <ctype.h>
#include <stdarg.h>


/* Helper functions
 *---------------------------------------------------------------------------*/

static gchar **
build_strv_insert_before (gchar ***pstrv, gint pos)
{
	gsize count;
	gchar **strv = *pstrv;

	if (strv == NULL)
	{
		/* First argument, allocate memory */
		strv = g_new0 (gchar *, 2);
		pos = 0;
		count = 1;
	}
	else
	{
		gchar **new_strv;

		/* Increase array */
		count = g_strv_length (strv);

		new_strv = g_new (gchar *, count + 2);
		if ((pos >= 0) && (pos < count))
			memcpy (&new_strv[pos + 1], &strv[pos], sizeof (gchar *) * (count - pos));
		else
			pos = count;
		if (pos > 0) memcpy(new_strv, strv, (sizeof (gchar *)) * pos);
		g_free (strv);
		strv = new_strv;
		count++;
	}

	strv[count] = NULL;
	*pstrv = strv;

	return &strv[pos];
}

static gboolean
build_strv_remove (gchar **strv, gint pos)
{
	gsize count = g_strv_length (strv);

	g_return_val_if_fail (pos >= 0, FALSE);

	if (pos >= count)
	{
		/* Argument does not exist */

		return FALSE;
	}
	else
	{
		g_free (strv[pos]);
		memcpy (&strv[pos], &strv[pos + 1], sizeof(gchar *) * (count - pos));

		return TRUE;
	}
}

/* Private functions
 *---------------------------------------------------------------------------*/

static gint
build_program_find_env (BuildProgram *prog, const gchar *name)
{
	if (prog->envp != NULL)
	{
		gint i;
		gchar **envp = prog->envp;
		gsize len = strlen (name);

		/* Look for an already existing variable */
		for (i = 0; envp[i] != NULL; i++)
		{
			if ((strncmp (envp[i], name, len) == 0) && (envp[i][len] == '='))
			{
				return i;
			}
		}
	}

	return -1;
}

/* Public functions
 *---------------------------------------------------------------------------*/

gboolean
build_program_set_command (BuildProgram *prog, const gchar *command)
{
	gboolean ok;
	gchar **arg;

	g_return_val_if_fail (prog != NULL, FALSE);

	if (prog->argv) g_strfreev (prog->argv);

	/* Store args and environment variables as string array */
	ok = g_shell_parse_argv (command, NULL, &prog->argv, NULL) ? TRUE : FALSE;
	for (arg = prog->argv; *arg != NULL; arg++)
	{
		gchar *new_arg;

		new_arg = anjuta_util_shell_expand (*arg);
		g_free (*arg);
		*arg = new_arg;
	}

	return ok;
}

const gchar *
build_program_get_basename (BuildProgram *prog)
{
	const gchar *base;

	if ((prog->argv == NULL) || (prog->argv[0] == NULL)) return NULL;

	base = strrchr (prog->argv[0], G_DIR_SEPARATOR);

	return base == NULL ? prog->argv[0] : base;
}


void
build_program_set_working_directory (BuildProgram *prog, GFile *directory)
{
	g_free (prog->work_dir);

	prog->work_dir = g_file_get_path (directory);
}


gboolean
build_program_insert_arg (BuildProgram *prog, gint pos, const gchar *arg)
{
	gchar **parg;

	parg = build_strv_insert_before (&prog->argv, pos);
	*parg = anjuta_util_shell_expand (arg);

	return TRUE;
}

gboolean
build_program_replace_arg (BuildProgram *prog, gint pos, const gchar *arg)
{
	if (pos >= g_strv_length (prog->argv))
	{
		return build_program_insert_arg (prog, pos, arg);
	}
	else
	{
		g_free (prog->argv[pos]);
		prog->argv[pos] = anjuta_util_shell_expand (arg);
	}

	return TRUE;
}

gboolean
build_program_remove_arg (BuildProgram *prog, gint pos)
{
	return build_strv_remove (prog->argv, pos);
}


gboolean
build_program_add_env_list (BuildProgram *prog, GList *vars)
{
	GList *item;
	gboolean ok = TRUE;

	for (item = vars; item != NULL; item = g_list_next (item))
	{
		const gchar *value;
		gchar *equal;
		gchar *name;

		name = g_strdup ((gchar *)item->data);
		equal = strchr (name, '=');
		if (equal != NULL)
		{
			*equal = '\0';
			value = equal + 1;
		}
		else
		{
			value = NULL;
		}

		ok = ok && build_program_add_env (prog, name, value);
		g_free (name);
	}

	return ok;
}


gboolean
build_program_add_env (BuildProgram *prog, const gchar *name, const gchar *value)
{
	gint found = build_program_find_env (prog, name);
	gchar *name_and_value = g_strconcat (name, "=", value, NULL);

	if (found == -1)
	{
		/* Append variable */
		*build_strv_insert_before (&prog->envp, -1) = name_and_value;
	}
	else
	{
		g_free (prog->envp[found]);
		prog->envp[found] = name_and_value;
	}
	return TRUE;
}

gboolean
build_program_remove_env (BuildProgram *prog, const gchar *name)
{
	gint found = build_program_find_env (prog, name);
	if (found == -1)
	{
		/* Variable not found */
		return FALSE;
	}
	else
	{
		return build_strv_remove (prog->envp, found);
	}
}

gboolean
build_program_override (BuildProgram *prog, IAnjutaEnvironment *env)
{
	gboolean ok;
	GError* error = NULL;

	if (env == NULL) return TRUE;

	ok = ianjuta_environment_override (env, &prog->work_dir, &prog->argv, &prog->envp, &error);
	if (!ok && error)
	{
		GtkWidget* dialog;
		g_message ("ENV error!");
		dialog = gtk_message_dialog_new (NULL,
		                                 0,
		                                 GTK_MESSAGE_ERROR,
		                                 GTK_BUTTONS_OK,
		                                 _("Error while setting up build environment:\n %s"),
		                                 error->message);
		g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_widget_show (dialog);
		g_error_free (error);
	}
	return ok;
}

void
build_program_set_callback (BuildProgram *prog, IAnjutaBuilderCallback callback, gpointer user_data)
{
	prog->callback = callback;
	prog->user_data = user_data;
}

void build_program_callback (BuildProgram *prog, GObject *sender, IAnjutaBuilderHandle handle, GError *err)
{
	if (prog->callback != NULL)
	{
		prog->callback (sender, handle, err, prog->user_data);
		prog->callback = NULL;
	}
}


/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

BuildProgram*
build_program_new (void)
{
	BuildProgram *prog ;

	prog = g_new0 (BuildProgram, 1);

	return prog;
}

BuildProgram*
build_program_new_with_command (GFile *directory, const gchar *command,...)
{
	BuildProgram *prog;
	gchar *full_command;
	va_list args;

	prog = build_program_new ();
	if (prog == NULL) return NULL;

	build_program_set_working_directory (prog, directory);

	/* Avoid terminal control sequence */
	build_program_add_env (prog, "TERM", "dumb");

	va_start (args, command);
	full_command = g_strdup_vprintf (command, args);
	va_end (args);
	build_program_set_command (prog, full_command);
	g_free (full_command);

	return prog;
}

void
build_program_free (BuildProgram *prog)
{
	if (prog->callback != NULL)
	{
		GError *err;

		/* Emit command-finished signal abort */
		err = g_error_new_literal (ianjuta_builder_error_quark (),
								   IANJUTA_BUILDER_ABORTED,
								   _("Command aborted"));
		prog->callback (NULL, NULL, err, prog->user_data);
		g_error_free (err);
	}
	g_free (prog->work_dir);
	if (prog->argv) g_strfreev (prog->argv);
	if (prog->envp) g_strfreev (prog->envp);
	g_free (prog);
}


