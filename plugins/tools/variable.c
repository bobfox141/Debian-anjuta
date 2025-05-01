/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    variable.c
    Copyright (C) 2005 Sebastien Granjoux

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
 * Keeps variables used by the tool plugin. This includes values defined
 * by Anjuta in several flavour (by example file uri, path or just name) and
 * other values ask at run time with a dialog.
 * Each variable includes a short help message.
 * 
 *---------------------------------------------------------------------------*/

#include <config.h>

#include "variable.h"

#include <libanjuta/anjuta-utils.h>
#include <libanjuta/interfaces/ianjuta-document-manager.h>
#include <libanjuta/interfaces/ianjuta-file-manager.h>
#include <libanjuta/interfaces/ianjuta-project-manager.h>
#include <libanjuta/interfaces/ianjuta-editor.h>
#include <libanjuta/interfaces/ianjuta-editor-selection.h>
#include <libanjuta/interfaces/ianjuta-file.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <string.h>

/* List of variables understood by the tools editor and their values.
 *---------------------------------------------------------------------------*/

/* enum and following variable_list table must be in the same order */

enum {
	ATP_PROJECT_ROOT_URI = 0,
	ATP_PROJECT_ROOT_DIRECTORY,
	ATP_FILE_MANAGER_CURRENT_GFILE,
	ATP_FILE_MANAGER_CURRENT_URI,
	ATP_FILE_MANAGER_CURRENT_DIRECTORY,
	ATP_FILE_MANAGER_CURRENT_FULL_FILENAME,
	ATP_FILE_MANAGER_CURRENT_FULL_FILENAME_WITHOUT_EXT,
	ATP_FILE_MANAGER_CURRENT_FILENAME,
	ATP_FILE_MANAGER_CURRENT_FILENAME_WITHOUT_EXT,
	ATP_FILE_MANAGER_CURRENT_EXTENSION,
	ATP_PROJECT_MANAGER_CURRENT_URI,
	ATP_PROJECT_MANAGER_CURRENT_DIRECTORY,
	ATP_PROJECT_MANAGER_CURRENT_FULL_FILENAME,
	ATP_PROJECT_MANAGER_CURRENT_FULL_FILENAME_WITHOUT_EXT,
	ATP_PROJECT_MANAGER_CURRENT_FILENAME,
	ATP_PROJECT_MANAGER_CURRENT_FILENAME_WITHOUT_EXT,
	ATP_PROJECT_MANAGER_CURRENT_EXTENSION,
	ATP_EDITOR_CURRENT_FILENAME,
	ATP_EDITOR_CURRENT_FILENAME_WITHOUT_EXT,
	ATP_EDITOR_CURRENT_DIRECTORY,
	ATP_EDITOR_CURRENT_SELECTION,
	ATP_EDITOR_CURRENT_WORD,
	ATP_EDITOR_CURRENT_LINE,
	ATP_ASK_USER_STRING,
	ATP_VARIABLE_COUNT
		
};

static const struct
{
	char *name;
	char *help;
	ATPFlags flag;
} variable_list[] = {
 {IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI, N_("Project root URI"), ATP_DEFAULT_VARIABLE},
 {"project_root_directory", N_("Project root path"), ATP_DIRECTORY_VARIABLE },
 {IANJUTA_FILE_MANAGER_SELECTED_FILE, "File Manager file", ATP_NO_VARIABLE},
 {"file_manager_current_uri", N_("Selected URI in the file manager plugin"), ATP_DEFAULT_VARIABLE },
 {"file_manager_current_directory", N_("Selected directory in the file manager plugin"), ATP_DIRECTORY_VARIABLE },
 {"file_manager_current_full_filename", N_("Selected full file name in the file manager plugin"), ATP_FILE_VARIABLE },
 {"file_manager_current_full_filename_without_ext", N_("Selected full file name without extension in the file manager plugin"), ATP_FILE_VARIABLE },
 {"file_manager_current_filename", N_("Selected file name in the file manager plugin"), ATP_FILE_VARIABLE },
 {"file_manager_current_filename_without_ext", N_("Selected file name without extension in the file manager plugin"), ATP_FILE_VARIABLE },
 {"file_manager_current_extension", N_("Selected file's extension in the file manager plugin"), ATP_DEFAULT_VARIABLE },
 {"project_manager_current_uri", N_("Selected URI in the project manager plugin"), ATP_DEFAULT_VARIABLE },
 {"project_manager_current_directory", N_("Selected directory in the project manager plugin"), ATP_DIRECTORY_VARIABLE },
 {"project_manager_current_full_filename", N_("Selected full file name in the project manager plugin"), ATP_FILE_VARIABLE },
 {"project_manager_current_full_filename_without_ext", N_("Selected full file name without extension in the project manager plugin"), ATP_FILE_VARIABLE },
 {"project_manager_current_filename", N_("Selected file name in the project manager plugin"), ATP_FILE_VARIABLE },
 {"project_manager_current_filename_without_ext", N_("Selected file name without extension in the project manager plugin"), ATP_FILE_VARIABLE },
 {"project_manager_current_extension", N_("Selected file extension in the project manager plugin"), ATP_DEFAULT_VARIABLE },
 {"editor_current_filename", N_("Currently edited file name"), ATP_FILE_VARIABLE },
 {"editor_current_filename_without_ext", N_("Currently edited file name without extension"), ATP_FILE_VARIABLE },
 {"editor_current_directory", N_("Currently edited file directory"), ATP_DIRECTORY_VARIABLE },
 {"editor_current_selection", N_("Currently selected text in editor"), ATP_FILE_VARIABLE },
 {"editor_current_word", N_("Current word in editor"), ATP_FILE_VARIABLE },
 {"editor_current_line", N_("Current line in editor"), ATP_FILE_VARIABLE },
 {"ask_user_string", N_("Ask the user to get additional parameters"), ATP_INTERACTIVE_VARIABLE }
};

/* Helper functions
 *---------------------------------------------------------------------------*/

/* Get path from uri */
static gchar*
get_path_from_uri (char* uri)
{
	gchar* val;
	GFile *file;

	if (uri == NULL) return NULL;

	file = g_file_new_for_uri (uri);
	g_free (uri);
	val = g_file_get_path (file);
	g_object_unref (file);

	return val;
}

/* Remove file name from a path */
static gchar*
remove_filename (char* path)
{
	gchar *val;

	if (path == NULL) return NULL;
	
	val = g_path_get_dirname (path);
	g_free (path);

	return val;
}

/* Remove directory from a path */
static gchar*
remove_directory (char* path)
{
	gchar *val;

	if (path == NULL) return NULL;
	
	val = g_path_get_basename (path);
	g_free (path);

	return val;
}


/* Remove directory and base filename from a path */
static gchar*
remove_all_but_extension (char* path)
{
	gchar* ext;
	gchar* dir;

	if (path == NULL) return NULL;

	dir = strrchr (path, G_DIR_SEPARATOR);
	ext = strrchr (path, '.');
	if ((ext != NULL) && ((dir == NULL) || (dir < ext)))
	{
		strcpy(path, ext + 1);
	}
	else
	{
		*path = '\0';
	}

	return path;
}

/* Remove extension from a filename */
static gchar*
remove_extension (char* path)
{
	gchar* ext;
	gchar* dir;

	if (path == NULL) return NULL;

	dir = strrchr (path, G_DIR_SEPARATOR);
	ext = strrchr (path, '.');
	if ((ext != NULL) && ((dir == NULL) || (dir < ext)))
	{
		*ext = '\0';
	}

	return path;
}

/* Get variable index from name
 *---------------------------------------------------------------------------*/

static guint
atp_variable_get_id (const ATPVariable* this, const gchar* name)
{
	guint i;

	for (i = 0; i < ATP_VARIABLE_COUNT; ++i)
	{
		if (strcmp (variable_list[i].name, name) == 0) break;
	}

	return i;
}

static guint
atp_variable_get_id_from_name_part (const ATPVariable* this, const gchar* name, gsize length)
{
	guint i;

	for (i = 0; i < ATP_VARIABLE_COUNT; ++i)
	{
		if ((strncmp (variable_list[i].name, name, length) == 0)
			      	&& (variable_list[i].name[length] == '\0')) break;
	}

	return i;
}

/* Get Anjuta variables
 *---------------------------------------------------------------------------*/

static gpointer
atp_variable_get_anjuta_variable (const ATPVariable *this, guint id)
{
	GValue value = {0,};
	GError* err = NULL;

	anjuta_shell_get_value (this->shell, variable_list[id].name, &value, &err);
	if (err != NULL)
	{
		/* Value probably does not exist */
		g_error_free (err);

		return NULL;
	}
	else
	{
		gpointer ret;
		
		if (G_VALUE_HOLDS (&value, G_TYPE_STRING))
		{
			ret = g_value_dup_string (&value);
		}
		else if (G_VALUE_HOLDS (&value, G_TYPE_FILE))
		{
			ret = g_value_dup_object  (&value);
		}
		else
		{
			ret = NULL;
		}
		g_value_unset (&value);

		return ret;
	}
}

static gchar*
atp_variable_get_project_manager_variable (const ATPVariable *this, guint id)
{
	IAnjutaProjectManager *prjman;
	gchar* val = NULL;
	GFile *file;
	GError* err = NULL;

	prjman = anjuta_shell_get_interface (this->shell, IAnjutaProjectManager, NULL);

	if (prjman == NULL) return NULL;

	switch (id)
	{
	case ATP_PROJECT_MANAGER_CURRENT_URI:
		file = ianjuta_project_manager_get_selected (prjman, &err);
		if (file != NULL)
		{
			val = g_file_get_uri (file);
			g_object_unref (file);
		}
		break;
	default:
		g_return_val_if_reached (NULL);
	}	

	if (err != NULL)
	{
		g_error_free (err);

		return NULL;
	}
	else
	{
		return val;
	}
}

static IAnjutaEditor*
get_current_editor(IAnjutaDocumentManager* docman)
{
	if (docman == NULL)
		return NULL;
	IAnjutaDocument* document = ianjuta_document_manager_get_current_document(docman, NULL);
	if (IANJUTA_IS_EDITOR(document))
		return IANJUTA_EDITOR(document);
	else
		return NULL;
}

static gchar*
atp_variable_get_editor_variable (const ATPVariable *this, guint id)
{
	IAnjutaDocumentManager *docman;
	IAnjutaEditor *ed;
	gchar* val;
	gchar* path;
	GFile* file;
	GError* err = NULL;

	docman = anjuta_shell_get_interface (this->shell, IAnjutaDocumentManager, NULL);
	ed = get_current_editor(docman);

	if (ed == NULL) return NULL;
	switch (id)
	{
	case ATP_EDITOR_CURRENT_SELECTION:
		val = ianjuta_editor_selection_get (IANJUTA_EDITOR_SELECTION (ed),
											&err);
		break;
	case ATP_EDITOR_CURRENT_WORD:
		val = ianjuta_editor_get_current_word (ed, &err);
		break;
	case ATP_EDITOR_CURRENT_LINE:
		val = g_strdup_printf ("%d", ianjuta_editor_get_lineno (ed, &err));
		break;
	case ATP_EDITOR_CURRENT_FILENAME:
		val = g_strdup (ianjuta_document_get_filename (IANJUTA_DOCUMENT(ed), &err));
		break;
	case ATP_EDITOR_CURRENT_DIRECTORY:
		file = ianjuta_file_get_file (IANJUTA_FILE (ed), &err);
		path = g_file_get_path (file);
		val = remove_filename(path);
		g_object_unref (file);
		break;
	default:
		g_return_val_if_reached (NULL);
	}	

	if (err != NULL)
	{
		g_error_free (err);

		return NULL;
	}
	else
	{
		return val;
	}
}

/* Access functions
 *---------------------------------------------------------------------------*/

guint
atp_variable_get_count (const ATPVariable* this)
{
	return ATP_VARIABLE_COUNT;
}

const gchar*
atp_variable_get_name (const ATPVariable* this, guint id)
{
	return id >= ATP_VARIABLE_COUNT ? NULL : variable_list[id].name;
}

const gchar*
atp_variable_get_help (const ATPVariable* this, guint id)
{
	return id >= ATP_VARIABLE_COUNT ? NULL : _(variable_list[id].help);
}

ATPFlags
atp_variable_get_flag (const ATPVariable* this, guint id)
{
	return id >= ATP_VARIABLE_COUNT ? ATP_NO_VARIABLE : variable_list[id].flag;
}

gchar*
atp_variable_get_value (const ATPVariable* this, const gchar* name)
{
	guint id;

	id = atp_variable_get_id (this, name);

	return atp_variable_get_value_from_id (this, id);
}

gchar*
atp_variable_get_value_from_name_part (const ATPVariable* this, const gchar* name, gsize length)
{
	guint id;

	id = atp_variable_get_id_from_name_part (this, name, length);

	return atp_variable_get_value_from_id (this, id);
}

/* Return NULL only if the variable doesn't exist
 * If not NULL, return value must be freed when not used */
gchar*
atp_variable_get_value_from_id (const ATPVariable* this, guint id)
{
	gchar *val = NULL;
	GFile *file;

	switch (id)
	{
	case ATP_PROJECT_ROOT_URI:
		val = atp_variable_get_anjuta_variable (this, id);
		break;
	case ATP_PROJECT_ROOT_DIRECTORY:
		val = atp_variable_get_anjuta_variable (this, ATP_PROJECT_ROOT_URI);
		val = get_path_from_uri (val);
		break;
	case ATP_FILE_MANAGER_CURRENT_URI:
		file = atp_variable_get_anjuta_variable (this, 	ATP_FILE_MANAGER_CURRENT_GFILE);
		if (file != NULL)
		{
			val = g_file_get_uri (file);
			g_object_unref (file);
		}
		break;
	case ATP_FILE_MANAGER_CURRENT_DIRECTORY:
		file = atp_variable_get_anjuta_variable (this, 	ATP_FILE_MANAGER_CURRENT_GFILE);
		if (file != NULL)
		{
			val = g_file_get_path (file);			
			g_object_unref (file);
			val = remove_filename (val);
		}
		break;
	case ATP_FILE_MANAGER_CURRENT_FULL_FILENAME:
		file = atp_variable_get_anjuta_variable (this, 	ATP_FILE_MANAGER_CURRENT_GFILE);
		if (file != NULL)
		{
			val = g_file_get_path (file);			
			g_object_unref (file);
		}
		break;
	case ATP_FILE_MANAGER_CURRENT_FULL_FILENAME_WITHOUT_EXT:
		file = atp_variable_get_anjuta_variable (this, 	ATP_FILE_MANAGER_CURRENT_GFILE);
		if (file != NULL)
		{
			val = g_file_get_path (file);			
			g_object_unref (file);
			val = remove_extension (val);
		}
		break;
	case ATP_FILE_MANAGER_CURRENT_FILENAME:
		file = atp_variable_get_anjuta_variable (this, 	ATP_FILE_MANAGER_CURRENT_GFILE);
		if (file != NULL)
		{
			val = g_file_get_path (file);			
			g_object_unref (file);
			val = remove_directory (val);
		}
		break;
	case ATP_FILE_MANAGER_CURRENT_FILENAME_WITHOUT_EXT:
		file = atp_variable_get_anjuta_variable (this, 	ATP_FILE_MANAGER_CURRENT_GFILE);
		if (file != NULL)
		{
			val = g_file_get_path (file);			
			g_object_unref (file);
			val = remove_directory (val);
			val = remove_extension (val);
		}
		break;	
	case ATP_FILE_MANAGER_CURRENT_EXTENSION:
		file = atp_variable_get_anjuta_variable (this, 	ATP_FILE_MANAGER_CURRENT_GFILE);
		if (file != NULL)
		{
			val = g_file_get_path (file);			
			g_object_unref (file);
			val = remove_all_but_extension (val);
		}
		break;
	case ATP_PROJECT_MANAGER_CURRENT_URI:
		val = atp_variable_get_project_manager_variable (this, id);
		break;
	case ATP_PROJECT_MANAGER_CURRENT_DIRECTORY:
		val = atp_variable_get_project_manager_variable (this, ATP_PROJECT_MANAGER_CURRENT_URI);
		val = get_path_from_uri (val);
		val = remove_filename (val);
		break;
	case ATP_PROJECT_MANAGER_CURRENT_FULL_FILENAME:
		val = atp_variable_get_project_manager_variable (this, ATP_PROJECT_MANAGER_CURRENT_URI);
		val = get_path_from_uri (val);
		break;
	case ATP_PROJECT_MANAGER_CURRENT_FULL_FILENAME_WITHOUT_EXT:
		val = atp_variable_get_project_manager_variable (this, ATP_PROJECT_MANAGER_CURRENT_URI);
		val = get_path_from_uri (val);
		val = remove_extension (val);
		break;
	case ATP_PROJECT_MANAGER_CURRENT_FILENAME:
		val = atp_variable_get_anjuta_variable (this, ATP_PROJECT_MANAGER_CURRENT_URI);
		val = get_path_from_uri (val);
		val = remove_directory (val);
		break;
	case ATP_PROJECT_MANAGER_CURRENT_FILENAME_WITHOUT_EXT:
		val = atp_variable_get_anjuta_variable (this, ATP_PROJECT_MANAGER_CURRENT_URI);
		val = get_path_from_uri (val);
		val = remove_directory (val);
		val = remove_extension (val);
		break;	
	case ATP_PROJECT_MANAGER_CURRENT_EXTENSION:
		val = atp_variable_get_anjuta_variable (this, ATP_PROJECT_MANAGER_CURRENT_URI);
		val = get_path_from_uri (val);
		val = remove_all_but_extension (val);
		break;
	case ATP_EDITOR_CURRENT_FILENAME_WITHOUT_EXT:
		val = atp_variable_get_editor_variable (this, ATP_EDITOR_CURRENT_FILENAME);
		val = remove_extension (val);
		break;
	case ATP_EDITOR_CURRENT_FILENAME:
	case ATP_EDITOR_CURRENT_DIRECTORY:
	case ATP_EDITOR_CURRENT_SELECTION:
	case ATP_EDITOR_CURRENT_WORD:
	case ATP_EDITOR_CURRENT_LINE:
		val = atp_variable_get_editor_variable (this, id);
		break;
	case ATP_ASK_USER_STRING:
		val = NULL;
		anjuta_util_dialog_input (GTK_WINDOW (this->shell),
								  _("Command line parameters"), NULL, &val);
		break;
	default:
		/* Variable does not exist */
		return NULL;
	}

	/* Variable exist */
	return val == NULL ? g_new0 (gchar, 1) : val;
}

/* Creation and Destruction
 *---------------------------------------------------------------------------*/

ATPVariable*
atp_variable_construct (ATPVariable* this, AnjutaShell* shell)
{
	this->shell = shell;

	return this;
}

void
atp_variable_destroy (ATPVariable* this)
{
}
