/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    debug_tree.h
    Copyright (C) 2006  Sébastien Granjoux

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

#ifndef _DEBUG_TREE_H_
#define _DEBUG_TREE_H_

#include "queue.h"
#include "plugin.h"

#include <gtk/gtk.h>
#include <libanjuta/interfaces/ianjuta-debugger.h>
#include <libanjuta/interfaces/ianjuta-debugger-variable.h>
#include <libanjuta/anjuta-plugin.h>

G_BEGIN_DECLS

typedef struct _DebugTree DebugTree;

DebugTree* debug_tree_new (AnjutaPlugin* plugin);
DebugTree* debug_tree_new_with_view (AnjutaPlugin *plugin, GtkTreeView *view);
void debug_tree_free (DebugTree *tree);

void debug_tree_dump (void);

void debug_tree_connect (DebugTree *tree, DmaDebuggerQueue *debugger);
void debug_tree_disconnect (DebugTree *tree);

void debug_tree_remove_all (DebugTree *tree);
void debug_tree_replace_list (DebugTree *tree, const GList *expressions);
void debug_tree_add_watch (DebugTree *tree, const IAnjutaDebuggerVariableObject* var, gboolean auto_update);
void debug_tree_add_dummy (DebugTree *tree, GtkTreeIter *parent);
void debug_tree_add_full_watch_list (DebugTree *tree, GList *expressions);
void debug_tree_add_watch_list (DebugTree *tree, GList *expressions, gboolean auto_update);
void debug_tree_update_tree (DebugTree *tree);
void debug_tree_update_all (DmaDebuggerQueue *debugger);

GList* debug_tree_get_full_watch_list (DebugTree *tree);


GtkWidget *debug_tree_get_tree_widget (DebugTree *tree);
gboolean debug_tree_get_current (DebugTree *tree, GtkTreeIter* iter);
gboolean debug_tree_remove (DebugTree *tree, GtkTreeIter* iter);
gboolean debug_tree_update (DebugTree *tree, GtkTreeIter* iter, gboolean force);
void debug_tree_set_auto_update (DebugTree* tree, GtkTreeIter* iter, gboolean state);
gboolean debug_tree_get_auto_update (DebugTree* tree, GtkTreeIter* iter);

GtkTreeModel *debug_tree_get_model (DebugTree *tree);
void debug_tree_set_model (DebugTree *tree, GtkTreeModel *model);
void debug_tree_new_model (DebugTree *tree);
void debug_tree_remove_model (DebugTree *tree, GtkTreeModel *model);

gchar *debug_tree_get_selected (DebugTree *tree);
gchar *debug_tree_get_first (DebugTree *tree);

gchar* debug_tree_find_variable_value (DebugTree *tree, const gchar *name);

G_END_DECLS

#endif
