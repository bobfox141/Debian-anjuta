/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4; coding: utf-8 -*- */
/* amp-node.c
 *
 * Copyright (C) 2009  Sébastien Granjoux
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "amp-node.h"

#include "am-properties.h"

#include "amp-root.h"
#include "amp-module.h"
#include "amp-package.h"
#include "amp-group.h"
#include "amp-target.h"
#include "amp-object.h"
#include "amp-source.h"

#include <libanjuta/anjuta-debug.h>
#include <libanjuta/interfaces/ianjuta-project.h>

#include <glib/gi18n.h>

#include <memory.h>
#include <string.h>
#include <ctype.h>


/* Helper functions
 *---------------------------------------------------------------------------*/

void
amp_set_error (GError **error, gint code, const gchar *message)
{
        if (error != NULL) {
                if (*error != NULL) {
                        gchar *tmp;

                        /* error already created, just change the code
                         * and prepend the string */
                        (*error)->code = code;
                        tmp = (*error)->message;
                        (*error)->message = g_strconcat (message, "\n\n", tmp, NULL);
                        g_free (tmp);

                } else {
                        *error = g_error_new_literal (IANJUTA_PROJECT_ERROR,
                                                      code,
                                                      message);
                }
        }
}


/* Public functions
 *---------------------------------------------------------------------------*/

AnjutaProjectNode *
amp_node_new_valid(AnjutaProjectNode *parent, AnjutaProjectNodeType type, GFile *file, const gchar *name, GError **error)
{
	AnjutaProjectNode *node = NULL;
	AnjutaProjectNode *group = NULL;
	GFile *new_file = NULL;

	switch (type & ANJUTA_PROJECT_TYPE_MASK) {
		case ANJUTA_PROJECT_GROUP:
			if ((file == NULL) && (name != NULL))
			{
				if (g_path_is_absolute (name))
				{
					new_file = g_file_new_for_path (name);
				}
				else
				{
					new_file = g_file_get_child (anjuta_project_node_get_file (parent), name);
				}
				file = new_file;
			}
			/* We can create group named . to create a root node in an empty project */
			if (!g_file_equal (anjuta_project_node_get_file (parent), file))
			{
				node = ANJUTA_PROJECT_NODE (amp_group_node_new_valid (file, name, FALSE, error));
				if (node != NULL) node->type = type;
			}
			else
			{
				node = parent;
			}
			break;
		case ANJUTA_PROJECT_TARGET:
			node = ANJUTA_PROJECT_NODE (amp_target_node_new_valid (name, type, NULL, 0, parent, error));
			break;
		case ANJUTA_PROJECT_OBJECT:
			node = ANJUTA_PROJECT_NODE (amp_object_node_new_valid (file, type, error));
			break;
		case ANJUTA_PROJECT_SOURCE:
			/* Look for parent */
			group = anjuta_project_node_parent_type (parent, ANJUTA_PROJECT_GROUP);

			if ((file == NULL) && (name != NULL))
			{
				/* Find file from name */
				if (anjuta_project_node_get_node_type (group) == ANJUTA_PROJECT_GROUP)
				{
					if (g_path_is_absolute (name))
					{
						new_file = g_file_new_for_path (name);
					}
					else
					{
						new_file = g_file_get_child (anjuta_project_node_get_file (group), name);
					}
				}
				else
				{
					new_file = g_file_new_for_commandline_arg (name);
				}
				file = new_file;
			}

			/* Check that source file is inside the project if it is not belonging to a package */
			if ((anjuta_project_node_get_node_type (group) == ANJUTA_PROJECT_GROUP) && (anjuta_project_node_get_node_type (parent) != ANJUTA_PROJECT_MODULE))
			{
				AnjutaProjectNode *root;
				gchar *relative;

				root = anjuta_project_node_root (group);
				relative = g_file_get_relative_path (anjuta_project_node_get_file (root), file);
				g_free (relative);
				if (relative == NULL)
				{
					/* Source outside the project directory, so copy the file */
					GFile *dest;
					gchar *basename;

					basename = g_file_get_basename (file);
					dest = g_file_get_child (anjuta_project_node_get_file (group), basename);
					g_free (basename);

					g_file_copy_async (file, dest, G_FILE_COPY_BACKUP, G_PRIORITY_DEFAULT, NULL, NULL, NULL, NULL, NULL);
					if (new_file != NULL) g_object_unref (new_file);
					new_file = dest;
					file = dest;
				}
			}

			node = ANJUTA_PROJECT_NODE (amp_source_node_new_valid (file, type, error));
			break;
		case ANJUTA_PROJECT_MODULE:
			node = ANJUTA_PROJECT_NODE (amp_module_node_new_valid (name, error));
			if (node != NULL) node->type = type;
			break;
		case ANJUTA_PROJECT_PACKAGE:
			node = ANJUTA_PROJECT_NODE (amp_package_node_new_valid (name, error));
			if (node != NULL) node->type = type;
			break;
		default:
			g_assert_not_reached ();
			break;
	}
	if (new_file != NULL) g_object_unref (new_file);

	return node;
}

gboolean
amp_node_load (AmpNode *node,
               AmpNode *parent,
               AmpProject *project,
               GError **error)
{
  g_return_val_if_fail (AMP_IS_NODE (node), FALSE);

  return AMP_NODE_GET_CLASS (node)->load (node, parent, project, error);
}

gboolean
amp_node_save (AmpNode *node,
               AmpNode *parent,
               AmpProject *project,
               GError **error)
{
  g_return_val_if_fail (AMP_IS_NODE (node), FALSE);

  return AMP_NODE_GET_CLASS (node)->save (node, parent, project, error);
}

gboolean
amp_node_update (AmpNode *node,
                 AmpNode *new_node)
{
  g_return_val_if_fail (AMP_IS_NODE (node), FALSE);

  return AMP_NODE_GET_CLASS (node)->update (node, new_node);
}

AmpNode*
amp_node_copy (AmpNode *node)
{
  g_return_val_if_fail (AMP_IS_NODE (node), FALSE);

  return AMP_NODE_GET_CLASS (node)->copy (node);
}

gboolean
amp_node_write (AmpNode *node,
                AmpNode *parent,
                AmpProject *project,
                GError **error)
{
  g_return_val_if_fail (AMP_IS_NODE (node), FALSE);

  return AMP_NODE_GET_CLASS (node)->write (node, parent, project, error);
}

gboolean
amp_node_erase (AmpNode *node,
                AmpNode *parent,
                AmpProject *project,
                GError **error)
{
  g_return_val_if_fail (AMP_IS_NODE (node), FALSE);

  return AMP_NODE_GET_CLASS (node)->erase (node, parent, project, error);
}

/* AmpNode implementation
 *---------------------------------------------------------------------------*/

static gboolean
amp_node_real_load (AmpNode *node,
                    AmpNode *parent,
                    AmpProject *project,
                    GError **error)
{
	return FALSE;
}

static gboolean
amp_node_real_save (AmpNode *node,
                    AmpNode *parent,
                    AmpProject *project,
                    GError **error)
{
	/* Save nothing by default */
	return TRUE;
}

static gboolean
amp_node_real_update (AmpNode *node,
                      AmpNode *new_node)
{
	return FALSE;
}

static AmpNode*
amp_node_real_copy (AmpNode *old_node)
{
	AnjutaProjectNode *new_node;

	/* Create new node */
	new_node = g_object_new (G_TYPE_FROM_INSTANCE (old_node), NULL);
	if (old_node->parent.file != NULL) new_node->file = g_file_dup (old_node->parent.file);
	if (old_node->parent.name != NULL) new_node->name = g_strdup (old_node->parent.name);
	/* Keep old parent, Needed for source node to find project root node */
	new_node->parent = old_node->parent.parent;

	return AMP_NODE (new_node);	
}

static gboolean
amp_node_real_write (AmpNode *node,
                     AmpNode *parent,
                     AmpProject *project,
                     GError **error)
{
		return FALSE;
}

static gboolean
amp_node_real_erase (AmpNode *node,
                     AmpNode *parent,
                     AmpProject *project,
                     GError **error)
{
		return FALSE;
}


/* GObjet implementation
 *---------------------------------------------------------------------------*/

G_DEFINE_DYNAMIC_TYPE (AmpNode, amp_node, ANJUTA_TYPE_PROJECT_NODE);

static void
amp_node_init (AmpNode *node)
{
}

static void
amp_node_finalize (GObject *object)
{
	AmpNode *node = AMP_NODE (object);

	/* Subclasses set this directly and it should not be freed. */
	node->parent.properties_info = NULL;

	g_list_foreach (node->parent.properties, (GFunc)amp_property_free, NULL);
	g_list_free (node->parent.properties);
	node->parent.properties = NULL;

	G_OBJECT_CLASS (amp_node_parent_class)->finalize (object);
}

static void
amp_node_class_init (AmpNodeClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = amp_node_finalize;

	klass->load = amp_node_real_load;
	klass->save = amp_node_real_save;
	klass->update = amp_node_real_update;
	klass->copy = amp_node_real_copy;
	klass->write = amp_node_real_write;
	klass->erase = amp_node_real_erase;
}

static void
amp_node_class_finalize (AmpNodeClass *klass)
{
}



/* Register function
 *---------------------------------------------------------------------------*/

void
amp_node_register (GTypeModule *module)
{
	amp_node_register_type (module);
	amp_group_node_register (module);
	amp_root_node_register (module);
	amp_module_node_register (module);
	amp_package_node_register (module);
	amp_target_node_register (module);
	amp_object_node_register (module);
	amp_source_node_register (module);
}
