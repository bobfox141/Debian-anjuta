/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * projectparser.c
 * Copyright (C) Sébastien Granjoux 2009 <seb.sfo@free.fr>
 *
 * main.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * main.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "am-project.h"
#include "libanjuta/anjuta-debug.h"
#include "libanjuta/anjuta-project.h"
#include "libanjuta/interfaces/ianjuta-project.h"

#include <gio/gio.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

static gchar* output_file = NULL;
static FILE* output_stream = NULL;
gboolean no_id = FALSE;

static GOptionEntry entries[] =
{
  { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output_file, "Output file (default stdout)", "output_file" },
  { "no-id", 0, 0, G_OPTION_ARG_NONE, &no_id, "Do not display node ID", "" },
  { NULL }
};

#define INDENT 4

static void
open_output (void)
{
	output_stream = output_file == NULL ? stdout : fopen (output_file, "wt");
}

static void
close_output (void)
{
	if (output_stream != NULL) fclose (output_stream);
	output_stream = NULL;
}

static void
print (const gchar *message, ...)
{
	va_list args;

	if (output_stream == NULL) open_output();

	va_start (args, message);
	vfprintf (output_stream, message, args);
	va_end (args);
	fputc('\n', output_stream);
}

static void
print_error (const gchar *message, ...)
{
	va_list args;

	va_start (args, message);
	if ((output_stream == NULL) || (output_stream == stdout))
	{
		vfprintf (stderr, message, args);
		fputc('\n', stderr);
	}
	else
	{
		vfprintf (output_stream, message, args);
		fputc('\n', output_stream);
	}
	va_end (args);
}

static void list_children (IAnjutaProject *project, AnjutaProjectNode *root, AnjutaProjectNode *parent, gint indent, const gchar *path);

static void
list_property (IAnjutaProject *project, AnjutaProjectNode *parent, gint indent)
{
	GList *item;
	GString *value;

	value = g_string_new (NULL);

	for (item = anjuta_project_node_get_properties (parent); item != NULL; item = g_list_next (item))
	{
		AnjutaProjectProperty *prop;
		AnjutaProjectPropertyInfo *info;
		GList *list;
		gchar *name;

		prop = (AnjutaProjectProperty *)item->data;
		info = prop->info;

		/* Default property */
		if (info->default_value == prop) continue;

		switch (info->type)
		{
		case ANJUTA_PROJECT_PROPERTY_STRING:
		case ANJUTA_PROJECT_PROPERTY_LIST:
				g_string_assign (value, prop->value == NULL ? "" : prop->value);
				break;
		case ANJUTA_PROJECT_PROPERTY_BOOLEAN:
				g_string_assign (value, (prop->value != NULL) && (*prop->value == '1') ? "true" : "false");
				break;
		case ANJUTA_PROJECT_PROPERTY_MAP:
				g_string_assign (value, "");
				for (list = anjuta_project_node_get_properties (parent); list != NULL; list = g_list_next (list))
				{
					AnjutaProjectProperty *list_prop = (AnjutaProjectProperty *)list->data;

					if (list_prop->info == info)
					{
						if ((value->len == 0) && (list_prop != prop))
						{
							/* This list has already been displayed */
							break;
						}

						if (value->len != 0) g_string_append_c (value, ' ');
						g_string_append_printf (value, "%s = %s", list_prop->name == NULL ? "?" : list_prop->name, list_prop->value == NULL ? "" : list_prop->value);
					}
				}
				if (value->len == 0) continue;
				break;
		}

		name = g_strdup (info->name);
		if (*(name + strlen (name) - 1) == ':')
		{
			*(name + strlen (name) - 1) = '\0';
		}
		print ("%*sPROPERTY (%s): %s", indent * INDENT, "", name, value->str);
		g_free (name);
	}

	g_string_free (value, TRUE);
}

static void
list_source (IAnjutaProject *project, AnjutaProjectNode *root, AnjutaProjectNode *source, gint indent, const gchar *path)
{
	GFile *file;
	gchar *rel_path;

	if (source == NULL) return;

	file = anjuta_project_node_get_file (source);
	rel_path = g_file_get_relative_path (anjuta_project_node_get_file (root), file);
	print ("%*sSOURCE (%s): %s", indent * INDENT, "", no_id ? "" : path, rel_path);

	list_property (project, source, indent + 1);

	g_free (rel_path);
}

static void
list_target (IAnjutaProject *project, AnjutaProjectNode *root, AnjutaProjectNode *target, gint indent, const gchar *path)
{
	print ("%*sTARGET (%s): %s", indent * INDENT, "", no_id ? "" : path, anjuta_project_node_get_name (target));

	list_property (project, target, indent + 1);

	list_children (project, root, target, indent, path);
}

static void
list_group (IAnjutaProject *project, AnjutaProjectNode *root, AnjutaProjectNode *group, gint indent, const gchar *path)
{
	AnjutaProjectNode *parent;
	gchar *rel_path;
	GFile *file;

	parent = anjuta_project_node_parent (group);
	file = anjuta_project_node_get_file (parent);
	rel_path = g_file_get_relative_path (file, anjuta_project_node_get_file (group));
	print ("%*sGROUP (%s): %s", indent * INDENT, "", no_id ? "" : path, rel_path);
	g_free (rel_path);

	list_property (project, group, indent + 1);

	list_children (project, root, group, indent, path);
}

static void
list_package (IAnjutaProject *project, AnjutaProjectNode *root, AnjutaProjectNode *package, gint indent, const gchar *path)
{
	print ("%*sPACKAGE (%s): %s", indent * INDENT, "", no_id ? "" : path, anjuta_project_node_get_name (package));
	list_property (project, package, indent + 1);
}

static void
list_module (IAnjutaProject *project, AnjutaProjectNode *root, AnjutaProjectNode *module, gint indent, const gchar *path)
{
	print ("%*sMODULE (%s): %s", indent * INDENT, "", no_id ? "" : path, anjuta_project_node_get_name (module));

	list_property (project, module, indent + 1);

	list_children (project, root, module, indent, path);
}

static void
list_children (IAnjutaProject *project, AnjutaProjectNode *root, AnjutaProjectNode *parent, gint indent, const gchar *path)
{
	AnjutaProjectNode *node;
	guint count;

	indent++;

	count = 0;
	for (node = anjuta_project_node_first_child (parent); node != NULL; node = anjuta_project_node_next_sibling (node))
	{
		if (anjuta_project_node_get_state (node) & ANJUTA_PROJECT_REMOVED) continue;
		if (anjuta_project_node_get_node_type (node) == ANJUTA_PROJECT_MODULE)
		{
			gchar *child_path = g_strdup_printf ("%s%s%d", path != NULL ? path : "", path != NULL ? ":" : "", count);
			list_module (project, root, node, indent, child_path);
			g_free (child_path);
		}
		count++;
	}

	count = 0;
	for (node = anjuta_project_node_first_child (parent); node != NULL; node = anjuta_project_node_next_sibling (node))
	{
		if (anjuta_project_node_get_state (node) & ANJUTA_PROJECT_REMOVED) continue;
		if (anjuta_project_node_get_node_type (node) == ANJUTA_PROJECT_PACKAGE)
		{
			gchar *child_path = g_strdup_printf ("%s%s%d", path != NULL ? path : "", path != NULL ? ":" : "", count);
			list_package (project, root, node, indent, child_path);
			g_free (child_path);
		}
		count++;
	}

	count = 0;
	for (node = anjuta_project_node_first_child (parent); node != NULL; node = anjuta_project_node_next_sibling (node))
	{
		if (anjuta_project_node_get_state (node) & ANJUTA_PROJECT_REMOVED) continue;
		if (anjuta_project_node_get_node_type (node) == ANJUTA_PROJECT_GROUP)
		{
			gchar *child_path = g_strdup_printf ("%s%s%d", path != NULL ? path : "", path != NULL ? ":" : "", count);
			list_group (project, root, node, indent, child_path);
			g_free (child_path);
		}
		count++;
	}

	count = 0;
	for (node = anjuta_project_node_first_child (parent); node != NULL; node = anjuta_project_node_next_sibling (node))
	{
		if (anjuta_project_node_get_state (node) & ANJUTA_PROJECT_REMOVED) continue;
		if (anjuta_project_node_get_node_type (node) == ANJUTA_PROJECT_TARGET)
		{
			gchar *child_path = g_strdup_printf ("%s%s%d", path != NULL ? path : "", path != NULL ? ":" : "", count);
			list_target (project, root, node, indent, child_path);
			g_free (child_path);
		}
		count++;
	}

	count = 0;
	for (node = anjuta_project_node_first_child (parent); node != NULL; node = anjuta_project_node_next_sibling (node))
	{
		if (anjuta_project_node_get_state (node) & ANJUTA_PROJECT_REMOVED) continue;
		if (anjuta_project_node_get_node_type (node) == ANJUTA_PROJECT_SOURCE)
		{
			gchar *child_path = g_strdup_printf ("%s%s%d", path != NULL ? path : "", path != NULL ? ":" : "", count);
			list_source (project, root, node, indent, child_path);
			g_free (child_path);
		}
		count++;
	}
}

static void
list_root (IAnjutaProject *project, AnjutaProjectNode *root)
{
	print ("ROOT (%s): %s", no_id ? "" : "0", anjuta_project_node_get_name (root));
	list_property (project, root, 1);
	list_children (project, root, root, 0, "0");
}

static AnjutaProjectNode *
get_node (IAnjutaProject *project, AnjutaProjectNode *root, const char *path)
{
	AnjutaProjectNode *node = root;

	if (path != NULL)
	{
		gchar *end;
		guint root = g_ascii_strtoull (path, &end, 10);

		/* All path start with 0 which is the id of the root node */
		if ((end == path) || (root != 0))
		{
			/* error */
			return NULL;
		}
		if (*end != '\0') end++;
		path = end;

		for (; *path != '\0';)
		{
			guint child = g_ascii_strtoull (path, &end, 10);

			/* Check if the number is valid.
			 * Use an invalid number to get the root node
			 */
			if (path == end) break;

			if (end == path)
			{
				/* error */
				return NULL;
			}

			node = anjuta_project_node_nth_child (node, child);
			if (node == NULL)
			{
				/* no node */
				return NULL;
			}

			if (*end == '\0') break;
			path = end + 1;
		}
	}

	return node;
}

static GFile *
get_file (AnjutaProjectNode *target, const char *id)
{
	AnjutaProjectNode *group = (AnjutaProjectNode *)anjuta_project_node_parent (target);

	return g_file_resolve_relative_path (anjuta_project_node_get_file (group), id);
}

static gint
compare_name (const gchar *id, const gchar *name)
{
		const gchar *ptr;
		gboolean next = FALSE;
		gint miss = 0;

		for (ptr = name; *ptr != '\0'; ptr++)
		{
				if (!next && (*id != '\0') && (g_ascii_toupper (*ptr) == g_ascii_toupper (*id)))
				{
					id++;
				}
				else
				{
					miss++;
					next = !g_ascii_isspace (*ptr);
				}
		}

		return (*id == '\0') ? miss : -1;
}

static AnjutaProjectPropertyInfo *
get_project_property (IAnjutaProject *project, AnjutaProjectNode *parent, const gchar *id)
{
	GList *item;
	AnjutaProjectPropertyInfo *info = NULL;
	gint best = G_MAXINT;

	for (item = anjuta_project_node_get_properties_info (parent); item != NULL; item = g_list_next (item))
	{
		gint miss;

		/* Find property based on their id */
		if (strcmp (id, ((AnjutaProjectPropertyInfo *)item->data)->id) == 0)
		{
			info = ((AnjutaProjectPropertyInfo *)item->data);
			break;
		}

		/* Else use the best name */
		miss = compare_name (id, ((AnjutaProjectPropertyInfo *)item->data)->name);
		if ((miss >= 0) && (miss < best))
		{
			best = miss;
			info =  ((AnjutaProjectPropertyInfo *)item->data);
		}
	}

	return info;
}

static AnjutaProjectNodeType
get_target_type (IAnjutaProject *project, const char *id)
{
	AnjutaProjectNodeType type;
	const GList *list;
	const GList *item;
	gint best = G_MAXINT;

	list = ianjuta_project_get_node_info (project, NULL);
	type = 0;
	for (item = list; item != NULL; item = g_list_next (item))
	{
		AnjutaProjectNodeInfo *info = (AnjutaProjectNodeInfo *)item->data;

		if ((info->type & ANJUTA_PROJECT_TYPE_MASK) == ANJUTA_PROJECT_TARGET)
		{
			gint miss;

			miss = compare_name (id, info->name);

			if ((miss >= 0) && (miss < best))
			{
				best = miss;
				type = info->type;
			}
		}
	}

	return type;
}

static void
amp_project_wait_ready (IAnjutaProject *project)
{
	while (amp_project_is_busy (AMP_PROJECT (project)))
	{
		g_main_context_iteration (NULL, TRUE);
	}
}

/* Dummy type module
 *---------------------------------------------------------------------------*/

typedef struct
{
	GTypeModuleClass parent;
} DummyTypeModuleClass;

typedef struct
{
	GTypeModule parent;
} DummyTypeModule;

static GType dummy_type_module_get_type (void);

G_DEFINE_TYPE (DummyTypeModule, dummy_type_module, G_TYPE_TYPE_MODULE)

static gboolean
dummy_type_module_load (GTypeModule *gmodule)
{
	return TRUE;
}

static void
dummy_type_module_unload (GTypeModule *gmodule)
{
}

/* GObject functions
 *---------------------------------------------------------------------------*/

static void
dummy_type_module_class_init (DummyTypeModuleClass *klass)
{
	GTypeModuleClass *gmodule_class = (GTypeModuleClass *)klass;

	gmodule_class->load = dummy_type_module_load;
	gmodule_class->unload = dummy_type_module_unload;
}

static void
dummy_type_module_init (DummyTypeModule *module)
{
}


/* Automake parsing function
 *---------------------------------------------------------------------------*/

int
main(int argc, char *argv[])
{
	IAnjutaProject *project = NULL;
	AnjutaProjectNode *node;
	AnjutaProjectNode *child;
	AnjutaProjectNode *sibling;
	AnjutaProjectNode *root = NULL;
	char **command;
	GOptionContext *context;
	GError *error = NULL;

	/* Initialize program */
	g_type_init ();


	/* Parse options */
 	context = g_option_context_new ("list [args]");
  	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_set_summary (context, "test new autotools project manger");
	if (!g_option_context_parse (context, &argc, &argv, &error))
    {
		exit (1);
    }
	if (argc < 2)
	{
		printf ("PROJECT: %s", g_option_context_get_help (context, TRUE, NULL));
		exit (1);
	}

	open_output ();

	/* Execute commands */
	for (command = &argv[1]; *command != NULL; command++)
	{
		if (g_ascii_strcasecmp (*command, "load") == 0)
		{
			GFile *file = g_file_new_for_commandline_arg (*(++command));

			if (project == NULL)
			{
				gint best = 0;
				gint probe;
				GType type;
				GTypeModule *module;

				/* Register project types */
				module = g_object_new (dummy_type_module_get_type (), NULL);
				amp_project_register (module);

				/* Check for project type */
				probe = amp_project_probe (file, NULL);
				if (probe > best)
				{
					best = probe;
					type = AMP_TYPE_PROJECT;
				}

				if (best == 0)
				{
					print_error ("Error: No backend for loading project in %s", *command);
					break;
				}
				else
				{
					project = IANJUTA_PROJECT (amp_project_new (file, NULL, NULL));
				}
			}

			root = ianjuta_project_get_root (project, &error);
			ianjuta_project_load_node (project, root, &error);
			g_object_unref (file);
		}
		else if (g_ascii_strcasecmp (*command, "list") == 0)
		{
			list_root (project, root);
		}
		else if (g_ascii_strcasecmp (*command, "move") == 0)
		{
			if (AMP_IS_PROJECT (project))
			{
				amp_project_move (AMP_PROJECT (project), *(++command));
			}
		}
		else if (g_ascii_strcasecmp (*command, "save") == 0)
		{
			ianjuta_project_save_node (project, root, NULL);
		}
		else if (g_ascii_strcasecmp (*command, "remove") == 0)
		{
			node = get_node (project, root, *(++command));
			ianjuta_project_remove_node (project, node, NULL);
		}
		else if (g_ascii_strcasecmp (*command, "dump") == 0)
		{
			if (g_ascii_strcasecmp (command[1], "makefile") == 0)
			{
				node = get_node (project, root, command[2]);
				amp_project_dump (AMP_PROJECT (project), node, DUMP_MAKEFILE);
				command +=2;
			}
			else if (g_ascii_strcasecmp (command[1], "configure") == 0)
			{
				amp_project_dump (AMP_PROJECT (project), root, DUMP_CONFIGURE);
				command +=1;
			}
			else
			{
				print_error ("Error: unknown command %s %s", *command, command[1]);
				break;
			}
		}
		else if (g_ascii_strcasecmp (command[0], "add") == 0)
		{
			node = get_node (project, root, command[2]);
			if (g_ascii_strcasecmp (command[1], "group") == 0)
			{
				if ((command[4] != NULL) && (g_ascii_strcasecmp (command[4], "before") == 0))
				{
					sibling = get_node (project, root, command[5]);
					child = ianjuta_project_add_node_before (project, node, sibling, ANJUTA_PROJECT_GROUP, NULL, command[3], &error);
					command += 2;
				}
				else if ((command[4] != NULL) && (g_ascii_strcasecmp (command[4], "after") == 0))
				{
					sibling = get_node (project, root, command[5]);
					child = ianjuta_project_add_node_after (project, node, sibling, ANJUTA_PROJECT_GROUP, NULL, command[3], &error);
					command += 2;
				}
				else
				{
					child = ianjuta_project_add_node_before (project, node, NULL, ANJUTA_PROJECT_GROUP, NULL, command[3], &error);
				}
			}
			else if (g_ascii_strcasecmp (command[1], "target") == 0)
			{
				if ((command[5] != NULL) && (g_ascii_strcasecmp (command[5], "before") == 0))
				{
					sibling = get_node (project, root, command[6]);
					child = ianjuta_project_add_node_before (project, node, sibling, ANJUTA_PROJECT_TARGET | get_target_type (project, command[4]), NULL, command[3], &error);
					command += 2;
				}
				else if ((command[5] != NULL) && (g_ascii_strcasecmp (command[5], "after") == 0))
				{
					sibling = get_node (project, root, command[6]);
					child = ianjuta_project_add_node_after (project, node, sibling, ANJUTA_PROJECT_TARGET | get_target_type (project, command[4]), NULL, command[3], &error);
					command += 2;
				}
				else
				{
					child = ianjuta_project_add_node_before (project, node, NULL, ANJUTA_PROJECT_TARGET | get_target_type (project, command[4]), NULL, command[3], &error);
				}
				command++;
			}
			else if (g_ascii_strcasecmp (command[1], "source") == 0)
			{
				GFile *file = get_file (node, command[3]);

				if ((command[4] != NULL) && (g_ascii_strcasecmp (command[4], "before") == 0))
				{
					sibling = get_node (project, root, command[5]);
					child = ianjuta_project_add_node_before (project, node, sibling, ANJUTA_PROJECT_SOURCE, file, NULL, &error);
					command += 2;
				}
				else if ((command[4] != NULL) && (g_ascii_strcasecmp (command[4], "after") == 0))
				{
					sibling = get_node (project, root, command[5]);
					child = ianjuta_project_add_node_after (project, node, sibling, ANJUTA_PROJECT_SOURCE, file, NULL, &error);
					command += 2;
				}
				else
				{
					child = ianjuta_project_add_node_before (project, node, NULL, ANJUTA_PROJECT_SOURCE, file, NULL, &error);
				}
				g_object_unref (file);
			}
			else if (g_ascii_strcasecmp (command[1], "module") == 0)
			{
				if ((command[4] != NULL) && (g_ascii_strcasecmp (command[4], "before") == 0))
				{
					sibling = get_node (project, root, command[5]);
					child = ianjuta_project_add_node_before (project, node, sibling, ANJUTA_PROJECT_MODULE, NULL, command[3], &error);
					command += 2;
				}
				else if ((command[4] != NULL) && (g_ascii_strcasecmp (command[4], "after") == 0))
				{
					sibling = get_node (project, root, command[5]);
					child = ianjuta_project_add_node_after (project, node, sibling, ANJUTA_PROJECT_MODULE, NULL, command[3], &error);
					command += 2;
				}
				else
				{
					child = ianjuta_project_add_node_before (project, node, NULL, ANJUTA_PROJECT_MODULE, NULL, command[3], &error);
				}
			}
			else if (g_ascii_strcasecmp (command[1], "package") == 0)
			{
				if ((command[4] != NULL) && (g_ascii_strcasecmp (command[4], "before") == 0))
				{
					sibling = get_node (project, root, command[5]);
					child = ianjuta_project_add_node_before (project, node, sibling, ANJUTA_PROJECT_PACKAGE, NULL, command[3], &error);
					command += 2;
				}
				else if ((command[4] != NULL) && (g_ascii_strcasecmp (command[4], "after") == 0))
				{
					sibling = get_node (project, root, command[5]);
					child = ianjuta_project_add_node_after (project, node, sibling, ANJUTA_PROJECT_PACKAGE, NULL, command[3], &error);
					command += 2;
				}
				else
				{
					child = ianjuta_project_add_node_before (project, node, NULL, ANJUTA_PROJECT_PACKAGE, NULL, command[3], &error);
				}
			}
			else
			{
				print_error ("Error: unknown command %s", *command);

				break;
			}
			command += 3;
		}
		else if (g_ascii_strcasecmp (command[0], "set") == 0)
		{
			if (AMP_IS_PROJECT (project))
			{
				AnjutaProjectPropertyInfo *info;

				node = get_node (project, root, command[1]);
				info = get_project_property (project, node, command[2]);
				if (info != NULL)
				{
					gchar *value = g_shell_unquote (command[3], NULL);
					ianjuta_project_set_property (project, node, info->id, NULL, value, NULL);
					g_free (value);
				}
			}
			command += 3;
		}
		else if (g_ascii_strcasecmp (command[0], "clear") == 0)
		{
			if (AMP_IS_PROJECT (project))
			{
				AnjutaProjectPropertyInfo *info;

				node = get_node (project, root, command[1]);
				info = get_project_property (project, node, command[2]);
				if (info != NULL)
				{
					ianjuta_project_remove_property (project, node, info->id, NULL, NULL);
				}
			}
			command += 2;
		}
		else
		{
			print_error ("Error: unknown command %s", *command);

			break;
		}
		amp_project_wait_ready (project);
		if (error != NULL)
		{
			print_error ("Error: %s", error->message == NULL ? "unknown error" : error->message);

			g_error_free (error);
			error = NULL;
		}
	}

	/* Free objects */
	if (project) g_object_unref (project);
	close_output ();

	return (0);
}
