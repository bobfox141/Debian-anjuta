/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * symbol-db-model.c
 * Copyright (C) Naba Kumar 2010 <naba@gnome.org>
 * 
 * anjuta is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * anjuta is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgda/libgda.h>
#include "symbol-db-marshal.h"
#include "symbol-db-model.h"

#define SYMBOL_DB_MODEL_STAMP 5364558

/* Constants */

#define SYMBOL_DB_MODEL_PAGE_SIZE 50
#define SYMBOL_DB_MODEL_ENSURE_CHILDREN_BATCH_SIZE 10

typedef struct _SymbolDBModelPage SymbolDBModelPage;
struct _SymbolDBModelPage
{
	gint begin_offset, end_offset;
	SymbolDBModelPage *prev;
	SymbolDBModelPage *next;
};

typedef struct _SymbolDBModelNode SymbolDBModelNode;
struct _SymbolDBModelNode {

	gint n_columns;
	
	/* Column values of the node. This is an array of GValues of length
	 * n_column. and holds the values in order of columns given at
	 * object initialized.
	 */
	GValue *values;

	/* List of currently active (cached) pages */
	SymbolDBModelPage *pages;
	
	/* Data structure */
	gint level;
	SymbolDBModelNode *parent;
	gint offset;

	/* Children states */
	gint children_ref_count;
	gboolean has_child_ensured;
	gboolean has_child;
	gboolean children_ensured;
	guint n_children;
	SymbolDBModelNode **children;
};

 struct _SymbolDBModelPriv {
	/* Keeps track of model freeze count. When the model is frozen, it
	 * avoid retreiving data from backend. It does not freeze the frontend
	 * view at all and instead use empty data for the duration of freeze.
	 */
	gint freeze_count;
	
	gint n_columns;      /* Number of columns in the model */
	GType *column_types; /* Type of each column in the model */
	gint *query_columns; /* Corresponding GdaDataModel column */
	
	SymbolDBModelNode *root;
};

enum {
	SIGNAL_GET_HAS_CHILD,
	SIGNAL_GET_N_CHILDREN,
	SIGNAL_GET_CHILDREN,
	LAST_SIGNAL
};

static guint symbol_db_model_signals[LAST_SIGNAL] = { 0 };

/* Declarations */

static void sdb_model_node_free (SymbolDBModelNode *node, gboolean force);

static void sdb_model_tree_model_init (GtkTreeModelIface *iface);

static gboolean sdb_model_get_query_value_at (SymbolDBModel *model,
                                              GdaDataModel *data_model,
                                              gint position, gint column,
                                              GValue *value);

static gboolean sdb_model_get_query_value (SymbolDBModel *model,
                                           GdaDataModel *data_model,
                                           GdaDataModelIter *iter,
                                           gint column,
                                           GValue *value);

static gboolean sdb_model_get_has_child (SymbolDBModel *model,
                                         SymbolDBModelNode *node);

static gint sdb_model_get_n_children (SymbolDBModel *model,
                                      gint tree_level,
                                      GValue column_values[]);

static GdaDataModel* sdb_model_get_children (SymbolDBModel *model,
                                             gint tree_level,
                                             GValue column_values[],
                                             gint offset, gint limit);

static void sdb_model_ensure_node_children (SymbolDBModel *model,
                                            SymbolDBModelNode *parent,
                                            gboolean emit_has_child,
                                            gboolean fake_child);

/* Class definition */
G_DEFINE_TYPE_WITH_CODE (SymbolDBModel, sdb_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                sdb_model_tree_model_init))
/* Node */

/**
 * sdb_model_node_get_child:
 * @node: The node whose child has to be fetched.
 * @child_offset: Offset of the child of this node.
 *
 * Fetches the content of the @child_offset child of the @node. The return
 * value can be NULL if the child hasn't been yet cached from backend. Only
 * when the child node is in cache does this function return a child. If you
 * you want to fetch the child from backend, call sdb_model_page_fault().
 * 
 * Returns: The child node, or NULL if the child hasn't yet been cached.
 */
static GNUC_INLINE SymbolDBModelNode*
sdb_model_node_get_child (SymbolDBModelNode *node, gint child_offset)
{
	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (child_offset >= 0 && child_offset < node->n_children, NULL);
	if(node->children)
		return node->children[child_offset];
	return NULL;
}

/**
 * sdb_model_node_set_child:
 * @node: The node whose child has to be set.
 * @child_offset: Offset of the child to set.
 * @val: Child node to set.
 * 
 * Sets the child of @node at @child_offset to @val.
 */
static void
sdb_model_node_set_child (SymbolDBModelNode *node, gint child_offset,
                          SymbolDBModelNode *val)
{
	g_return_if_fail (node != NULL);
	g_return_if_fail (node->children_ensured == TRUE);
	g_return_if_fail (child_offset >= 0 && child_offset < node->n_children);

	/* If children nodes array hasn't been allocated, now is the time */
	if (!node->children)
		node->children = g_new0 (SymbolDBModelNode*, node->n_children);
	if (val)
	{
		g_warn_if_fail (node->children[child_offset] == NULL);
	}
	node->children[child_offset] = val;
}

/**
 * sdb_model_node_cleanse:
 * @node: The node to cleanse
 * @force: If forcefuly cleansed disregarding references to children
 * 
 * It destroys all children of the node and resets the node to not
 * children-ensured state. Any cache for children nodes is also destroyed.
 * The node will be in children unensured state, which means the status
 * of it's children would be unknown. Cleansing only happens if there are
 * no referenced children nodes, unless it is forced with @force = TRUE.
 * 
 * Returns: TRUE if successfully cleansed, otherwise FALSE.
 */
static gboolean
sdb_model_node_cleanse (SymbolDBModelNode *node, gboolean force)
{
	SymbolDBModelPage *page, *next;
	gint i;
	
	g_return_val_if_fail (node != NULL, FALSE);

	/* Can not cleanse a node if there are refed children */
	if (!force)
	{
		g_return_val_if_fail (node->children_ref_count == 0, FALSE);
	}
	
	if (node->children)
	{
		/* There should be no children with any ref. Children with ref count 0
		 * are floating children and can be destroyed.
		 */
		for (i = 0; i < node->n_children; i++)
		{
			SymbolDBModelNode *child = sdb_model_node_get_child (node, i);
			if (child)
			{
				if (!force)
				{
					/* Assert on nodes with ref count > 0 */
					g_warn_if_fail (child->children_ref_count == 0);
				}
				sdb_model_node_free (child, force);
				sdb_model_node_set_child (node, i, NULL);
			}
		}
	}
	
	/* Reset cached pages */
	page = node->pages;
	while (page)
	{
		next = page->next;
		g_slice_free (SymbolDBModelPage, page);
		page = next;
	}
	node->pages = NULL;
	node->children_ensured = FALSE;
	node->n_children = 0;

	/* Destroy arrays */
	g_free (node->children);
	node->children = NULL;

	return TRUE;
}

/**
 * sdb_model_node_free:
 * @node: The node to free.
 * @force: Force the free despite any referenced children
 *
 * Frees the node if there is no referenced children, unless @force is TRUE
 * in which case it is freed anyways. All children recursively are also
 * destroyed.
 */
static void
sdb_model_node_free (SymbolDBModelNode *node, gboolean force)
{
	if (!sdb_model_node_cleanse (node, force))
	    return;
	
	g_slice_free1 (sizeof(GValue) * node->n_columns, node->values);
	g_slice_free (SymbolDBModelNode, node);
}

/**
 * sdb_model_node_remove_page:
 * @node: The node with the page
 * @page: The page to remove
 *
 * Removes the cache @page from the @node. The associated nodes are all
 * destroyed and set to NULL. They could be re-fetched later if needed.
 */
static void
sdb_model_node_remove_page (SymbolDBModelNode *node,
                            SymbolDBModelPage *page)
{
	if (page->prev)
		page->prev->next = page->next;
	else
		node->pages = page->next;
		
	if (page->next)
		page->next->prev = page->prev;

	/* FIXME: Destroy the page */
}

/**
 * sdb_model_node_insert_page:
 * @node: The node for which the page is inserted
 * @page: The page being inserted
 * @after: The page after which @page is inserted
 * 
 * Inserts the @page after @after page. The page should have been already
 * fetched and their nodes (children of @node) should have been already 
 * created.
 */
static void
sdb_model_node_insert_page (SymbolDBModelNode *node,
                            SymbolDBModelPage *page,
                            SymbolDBModelPage *after)
{
	
	/* Insert the new page after "after" page */
	if (after)
	{
		page->next = after->next;
		after->next = page;
	}
	else /* Insert at head */
	{
		page->next = node->pages;
		node->pages = page;
	}
}

/**
 * symbold_db_model_node_find_child_page:
 * @node: The node
 * @child_offset: Offset of the child node.
 * @prev_page: A pointer to page to return previous cache page found
 *
 * Find the cache page associated with child node of @node at @child_offset.
 * If the page is found, it returns the page, otherwise NULL is returned. Also,
 * if the page is found, prev_page pointer is set to the previous page to
 * the one found (NULL if it's the first page in the list).
 *
 * Returns: The page associated with the child node, or NULL if not found.
 */
static SymbolDBModelPage*
sdb_model_node_find_child_page (SymbolDBModelNode *node,
                                gint child_offset,
                                SymbolDBModelPage **prev_page)
{
	SymbolDBModelPage *page;
	
	page = node->pages;
	
	*prev_page = NULL;
	
	/* Find the page which holds result for given child_offset */
	while (page)
	{
		if (child_offset >= page->begin_offset &&
		    child_offset < page->end_offset)
		{
			/* child_offset has associated page */
			return page;
		}
		if (child_offset < page->begin_offset)
		{
			/* Insert point is here */
			break;
		}
		*prev_page = page;
		page = page->next;
	}
	return NULL;
}

/**
 * sdb_model_node_ref_child:
 * @node: The node whose child is being referenced.
 * 
 * References a child of @node and references all its parents recursively.
 * A referenced node essentially means someone is holding a reference to it
 * outside the model and we are supposed track its position. Currently, we
 * don't track reference of nodes themselves but instead maitain their ref
 * counts in parent @node. Ref counting is currently unused, so there is no
 * practical thing happening using it at the moment.
 */
static void
sdb_model_node_ref_child (SymbolDBModelNode *node)
{
	g_return_if_fail (node != NULL);

	node->children_ref_count++;

	if (node->parent)
	{
		/* Increate associated ref count on parent */
		sdb_model_node_ref_child (node->parent);
	}
}

/**
 * sdb_model_node_unref_child:
 * @node: The node whose child is being unrefed
 * @child_offset: Offset of the child being unrefed
 * 
 * Unrefs the child at @child_offset in @node. Also, unrefs its parents
 * recursively. currently ref/unref is not used, see
 * sdb_model_node_ref_child() for more details.
 */
static void
sdb_model_node_unref_child (SymbolDBModelNode *node, gint child_offset)
{
	g_return_if_fail (node != NULL);
	g_return_if_fail (node->children_ref_count > 0);
	
	node->children_ref_count--;

	/* If ref count reaches 0, cleanse this node */
	if (node->children_ref_count <= 0)
	{
		sdb_model_node_cleanse (node, FALSE);
	}

	if (node->parent)
	{
		/* Reduce ref count on parent as well */
		sdb_model_node_unref_child (node->parent, node->offset);
	}
}

/**
 * sdb_model_node_new:
 * @model: The model for which the node is being created
 * @parent: The parent node
 * @child_offset: Offset of this node as child of @parent
 * @data_model: The data model from which content of the node is fetched
 * @data_iter: Iter for @data_model where the content of this node is found
 *
 * This creates a new node for @model as a child of @parent at @child_offset
 * and initilizes the columns content from @data_model at @data_iter.
 * 
 * Returns: The newly created node.
 */
static SymbolDBModelNode *
sdb_model_node_new (SymbolDBModel *model, SymbolDBModelNode *parent,
                    gint child_offset, GdaDataModel *data_model,
                    GdaDataModelIter *data_iter)
{
	gint i;
	SymbolDBModelPriv *priv = model->priv;
	SymbolDBModelNode* node = g_slice_new0 (SymbolDBModelNode);
	node->n_columns = priv->n_columns;
	node->values = g_slice_alloc0 (sizeof (GValue) * priv->n_columns);
	for (i = 0; i < priv->n_columns; i++)
	{
		g_value_init (&(node->values[i]), priv->column_types[i]);
		sdb_model_get_query_value (model,
		                                 data_model,
		                                 data_iter,
		                                 i, &(node->values[i]));
	}
	node->offset = child_offset;
	node->parent = parent;
	node->level = parent->level + 1;
	return node;
}

/* SymbolDBModel implementation */

/**
 * sdb_model_iter_is_valid:
 * @model: The tree model
 * @iter: An iter for the model
 *
 * Checks if the iterator is valid for the model.
 *
 * Returns: TRUE if valid, FALSE if not
 */
static gboolean
sdb_model_iter_is_valid (GtkTreeModel *model, GtkTreeIter *iter)
{
	SymbolDBModelNode *parent_node;
	gint offset;

	g_return_val_if_fail (SYMBOL_DB_IS_MODEL (model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (iter->stamp == SYMBOL_DB_MODEL_STAMP, FALSE);

	parent_node = (SymbolDBModelNode*) iter->user_data;
	offset = GPOINTER_TO_INT (iter->user_data2);
	
	g_return_val_if_fail (parent_node != NULL, FALSE);
	g_return_val_if_fail (offset >= 0 && offset < parent_node->n_children,
	                      FALSE);
	return TRUE;
}

/**
 * sdb_model_page_fault:
 * @parent_node: The node which needs children data fetched
 * @child_offset: Offset of the child where page fault occured
 *
 * Page fault should happen on a child which is not yet available in cache
 * and needs to be fetched from backend database. Fetch happens in a page
 * size of SYMBOL_DB_MODEL_PAGE_SIZE chunks before and after the requested
 * child node. Also, the page will adjust the boundry to any preceeding or
 * or following pages so that they don't overlap.
 *
 * Returns: The newly fetched page
 */
static SymbolDBModelPage*
sdb_model_page_fault (SymbolDBModel *model,
                      SymbolDBModelNode *parent_node,
                      gint child_offset)
{
	SymbolDBModelPriv *priv;
	SymbolDBModelPage *page, *prev_page, *page_found;
	gint i;
	GdaDataModelIter *data_iter;
	GdaDataModel *data_model = NULL;

	/* Insert after prev_page */
	page_found = sdb_model_node_find_child_page (parent_node,
	                                             child_offset,
	                                             &prev_page);

	if (page_found)
		return page_found;

	/* If model is frozen, can't fetch data from backend */
	priv = model->priv;
	if (priv->freeze_count > 0)
		return NULL;
	
	/* New page to cover current child_offset */
	page = g_slice_new0 (SymbolDBModelPage);

	/* Define page range */
	page->begin_offset = child_offset - SYMBOL_DB_MODEL_PAGE_SIZE;
	page->end_offset = child_offset + SYMBOL_DB_MODEL_PAGE_SIZE;

	sdb_model_node_insert_page (parent_node, page, prev_page);
	
	/* Adjust boundries not to overlap with preceeding or following page */
	if (prev_page && prev_page->end_offset > page->begin_offset)
		page->begin_offset = prev_page->end_offset;

	if (page->next && page->end_offset >= page->next->begin_offset)
		page->end_offset = page->next->begin_offset;

	/* Adjust boundries not to preceed 0 index */
	if (page->begin_offset < 0)
		page->begin_offset = 0;
	
	/* Load a page from database */
	data_model = sdb_model_get_children (model, parent_node->level,
	                                     parent_node->values,
	                                     page->begin_offset,
	                                     page->end_offset - page->begin_offset);

	/* Fill up the page */
	data_iter = gda_data_model_create_iter (data_model);
	if (gda_data_model_iter_move_to_row (data_iter, 0))
	{
		for (i = page->begin_offset; i < page->end_offset; i++)
		{
			if (i >= parent_node->n_children)
			{
				/* FIXME: There are more rows in DB. Extend node */
				break;
			}
			SymbolDBModelNode *node =
				sdb_model_node_new (model, parent_node, i,
				                    data_model, data_iter);
			g_assert (sdb_model_node_get_child (parent_node, i) == NULL);
			sdb_model_node_set_child (parent_node, i, node);
			if (!gda_data_model_iter_move_next (data_iter))
			{
				if (i < (page->end_offset - 1))
				{
					/* FIXME: There are fewer rows in DB. Shrink node */
				}
				break;
			}
		}
	}

	if (data_iter)
		g_object_unref (data_iter);
	if (data_model)
		g_object_unref (data_model);
	return page;
}

/* GtkTreeModel implementation */

static GtkTreeModelFlags
sdb_model_get_flags (GtkTreeModel *tree_model)
{
	return 0;
}

static GType
sdb_model_get_column_type (GtkTreeModel *tree_model,
                           gint index)
{
	SymbolDBModelPriv *priv = SYMBOL_DB_MODEL (tree_model)->priv;
	g_return_val_if_fail (index < priv->n_columns, G_TYPE_INVALID);
	return priv->column_types [index];
}

static gint
sdb_model_get_n_columns (GtkTreeModel *tree_model)
{
	SymbolDBModelPriv *priv;
	priv = SYMBOL_DB_MODEL (tree_model)->priv;
	return priv->n_columns;
}

static gboolean
sdb_model_get_iter (GtkTreeModel *tree_model,
					GtkTreeIter *iter,
                    GtkTreePath *path)
{
	gint i;
	SymbolDBModelNode *node, *parent_node;
	gint depth, *indx;
	SymbolDBModelPriv *priv;
	
	g_return_val_if_fail (SYMBOL_DB_IS_MODEL(tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	gchar *path_str = gtk_tree_path_to_string (path);
	g_free (path_str);
	
	depth = gtk_tree_path_get_depth (path);
	g_return_val_if_fail (depth > 0, FALSE);

	priv = SYMBOL_DB_MODEL (tree_model)->priv;
	indx = gtk_tree_path_get_indices (path);

	parent_node = NULL;
	node = priv->root;
	for (i = 0; i < depth; i++)
	{
		parent_node = node;
		if (!node->children_ensured)
			sdb_model_ensure_node_children (SYMBOL_DB_MODEL (tree_model),
				                            node, FALSE, FALSE);
		if (node->n_children <= 0)
		{
			/* No child available. View thinks there is child.
			 * It's an inconsistent state. Do something fancy to fix it.
			 */
			symbol_db_model_update (SYMBOL_DB_MODEL (tree_model)); /* Untested path */
			break;
		}
		if (indx[i] >= node->n_children)
		{
			g_warning ("Invalid path to iter conversion; no children list found at depth %d", i);
			break;
		}
		node = sdb_model_node_get_child (node, indx[i]);
	}
	if (i != depth)
	{
		return FALSE;
	}
	iter->stamp = SYMBOL_DB_MODEL_STAMP;
	iter->user_data = parent_node;
	iter->user_data2 = GINT_TO_POINTER (indx[i-1]);

	g_assert (sdb_model_iter_is_valid (tree_model, iter));

	return TRUE;
}

static GtkTreePath*
sdb_model_get_path (GtkTreeModel *tree_model,
                          GtkTreeIter *iter)
{
	SymbolDBModelNode *node;
	GtkTreePath* path;
	gint offset;
	
	g_return_val_if_fail (sdb_model_iter_is_valid (tree_model, iter),
	                      NULL);

	path = gtk_tree_path_new ();

	node = (SymbolDBModelNode*)iter->user_data;
	offset = GPOINTER_TO_INT (iter->user_data2);
	do {
		gtk_tree_path_prepend_index (path, offset);
		if (node)
			offset = node->offset;
		node = node->parent;
	} while (node);
	return path;
}

static void
sdb_model_get_value (GtkTreeModel *tree_model,
                     GtkTreeIter *iter, gint column,
                     GValue *value)
{
	SymbolDBModelPriv *priv;
	SymbolDBModelNode *parent_node, *node;
	gint offset;
	
	g_return_if_fail (sdb_model_iter_is_valid (tree_model, iter));

	priv = SYMBOL_DB_MODEL (tree_model)->priv;
	
	g_return_if_fail (column >= 0);
	g_return_if_fail (column < priv->n_columns);
	
	parent_node = (SymbolDBModelNode*) iter->user_data;
	offset = GPOINTER_TO_INT (iter->user_data2);

	if (sdb_model_node_get_child (parent_node, offset) == NULL)
		sdb_model_page_fault (SYMBOL_DB_MODEL (tree_model),
		                      parent_node, offset);
	node = sdb_model_node_get_child (parent_node, offset);
	g_value_init (value, priv->column_types[column]);

	if (node == NULL)
		return;
	
	/* View accessed the node, so update any pending has-child status */
	if (!node->has_child_ensured)
	{
		sdb_model_get_has_child (SYMBOL_DB_MODEL (tree_model), node);
	}
	g_value_copy (&(node->values[column]), value);
}

static gboolean
sdb_model_iter_next (GtkTreeModel *tree_model,
                     GtkTreeIter *iter)
{
	SymbolDBModelNode *node;
	gint offset;
	
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (iter->stamp == SYMBOL_DB_MODEL_STAMP, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);
	

	node = (SymbolDBModelNode*)(iter->user_data);
	offset = GPOINTER_TO_INT (iter->user_data2);
	offset++;
	
	if (offset >= node->n_children)
		return FALSE;
	iter->user_data2 = GINT_TO_POINTER (offset);

	g_assert (sdb_model_iter_is_valid (tree_model, iter));

	return TRUE;
}

static gboolean
sdb_model_iter_children (GtkTreeModel *tree_model,
                         GtkTreeIter *iter,
                         GtkTreeIter *parent)
{
	SymbolDBModelNode *node, *parent_node;
	SymbolDBModelPriv *priv;

	if (parent)
	{
		g_return_val_if_fail (sdb_model_iter_is_valid (tree_model, parent),
		                      FALSE);
	}
	
	g_return_val_if_fail (SYMBOL_DB_IS_MODEL(tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	priv = SYMBOL_DB_MODEL (tree_model)->priv;
	if (parent == NULL)
	{
		node = priv->root;
	}
	else
	{
		gint offset;
		parent_node = (SymbolDBModelNode*) parent->user_data;
		offset = GPOINTER_TO_INT (parent->user_data2);
		node = sdb_model_node_get_child (parent_node, offset);
		if (!node)
		{
			sdb_model_page_fault (SYMBOL_DB_MODEL (tree_model),
			                      parent_node, offset);
			node = sdb_model_node_get_child (parent_node, offset);
		}
		g_return_val_if_fail (node != NULL, FALSE);
	}

	/* Apparently view can call this funtion without testing has_child first */
	if (!sdb_model_get_has_child (SYMBOL_DB_MODEL(tree_model), node))
		return FALSE;

	if (!node->children_ensured)
		sdb_model_ensure_node_children (SYMBOL_DB_MODEL (tree_model),
		                                node, FALSE, TRUE);

	iter->user_data = node;
	iter->user_data2 = GINT_TO_POINTER (0);
	iter->stamp = SYMBOL_DB_MODEL_STAMP;

	/* View trying to access children of childless node seems typical */
	if (node->n_children <= 0)
		return FALSE;
	
	g_assert (sdb_model_iter_is_valid (tree_model, iter));

	return TRUE;
}

static gboolean
sdb_model_iter_has_child (GtkTreeModel *tree_model,
                                GtkTreeIter *iter)
{
	gint offset;
	SymbolDBModelNode *node, *parent_node;

	g_return_val_if_fail (sdb_model_iter_is_valid (tree_model, iter),
	                      FALSE);
	
	parent_node = (SymbolDBModelNode*) iter->user_data;
	offset = GPOINTER_TO_INT (iter->user_data2);

	node = sdb_model_node_get_child (parent_node, offset);

	/* If node is not loaded, has-child is defered. See get_value() */
	if (node == NULL)
		return FALSE;
	return sdb_model_get_has_child (SYMBOL_DB_MODEL (tree_model), node);
}

static gint
sdb_model_iter_n_children (GtkTreeModel *tree_model,
                           GtkTreeIter *iter)
{
	gint offset;
	SymbolDBModelPriv *priv;
	SymbolDBModelNode *node, *parent_node;
	
	g_return_val_if_fail (SYMBOL_DB_IS_MODEL (tree_model), 0);
	priv = SYMBOL_DB_MODEL (tree_model)->priv;

	if (iter)
	{
		g_return_val_if_fail (sdb_model_iter_is_valid (tree_model, iter), 0);
	}

	if (iter == NULL)
		node = priv->root;
	else
	{
		parent_node = (SymbolDBModelNode*) iter->user_data;
		offset = GPOINTER_TO_INT (iter->user_data2);
		node = sdb_model_node_get_child (parent_node, offset);
	}

	if (node == NULL)
		return 0;
	if (!node->children_ensured)
		sdb_model_ensure_node_children (SYMBOL_DB_MODEL (tree_model),
		                                node, FALSE, FALSE);
	return node->n_children;
}

static gboolean
sdb_model_iter_nth_child (GtkTreeModel *tree_model,
                          GtkTreeIter *iter,
                          GtkTreeIter *parent,
                          gint n)
{
	SymbolDBModelNode *node;

	g_return_val_if_fail (SYMBOL_DB_IS_MODEL(tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (n >= 0, FALSE);

	if (!sdb_model_iter_children (tree_model, iter, parent))
		return FALSE;
	
	node = (SymbolDBModelNode*) (iter->user_data);

	g_return_val_if_fail (n < node->n_children, FALSE);
	
	iter->user_data2 = GINT_TO_POINTER (n);

	g_assert (sdb_model_iter_is_valid (tree_model, iter));

	return TRUE;
}

static gboolean
sdb_model_iter_parent (GtkTreeModel *tree_model,
                       GtkTreeIter *iter,
                       GtkTreeIter *child)
{
	SymbolDBModelNode *parent_node;
	
	g_return_val_if_fail (sdb_model_iter_is_valid (tree_model, child),
	                      FALSE);
	
	g_return_val_if_fail (iter != NULL, FALSE);
	
	parent_node = (SymbolDBModelNode*) child->user_data;
	g_return_val_if_fail (parent_node->parent != NULL, FALSE);
	
	iter->user_data = parent_node->parent;
	iter->user_data2 = GINT_TO_POINTER (parent_node->offset);
	iter->stamp = SYMBOL_DB_MODEL_STAMP;

	g_assert (sdb_model_iter_is_valid (tree_model, iter));

	return TRUE;
}

static void
sdb_model_iter_ref (GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
	SymbolDBModelNode *parent_node;

	g_return_if_fail (sdb_model_iter_is_valid (tree_model, iter));

	parent_node = (SymbolDBModelNode*) iter->user_data;

	sdb_model_node_ref_child (parent_node);
}

static void
sdb_model_iter_unref (GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
	SymbolDBModelNode *parent_node;
	gint child_offset;
	
	g_return_if_fail (sdb_model_iter_is_valid (tree_model, iter));

	parent_node = (SymbolDBModelNode*) iter->user_data;
	child_offset = GPOINTER_TO_INT (iter->user_data2);

	sdb_model_node_unref_child (parent_node, child_offset);
}

static void
sdb_model_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags = sdb_model_get_flags;
	iface->get_n_columns = sdb_model_get_n_columns;
	iface->get_column_type = sdb_model_get_column_type;
	iface->get_iter = sdb_model_get_iter;
	iface->get_path = sdb_model_get_path;
	iface->get_value = sdb_model_get_value;
	iface->iter_next = sdb_model_iter_next;
	iface->iter_children = sdb_model_iter_children;
	iface->iter_has_child = sdb_model_iter_has_child;
	iface->iter_n_children = sdb_model_iter_n_children;
	iface->iter_nth_child = sdb_model_iter_nth_child;
	iface->iter_parent = sdb_model_iter_parent;
	iface->ref_node = sdb_model_iter_ref;
	iface->unref_node = sdb_model_iter_unref;
}

/* SymbolDBModel implementation */

static void
sdb_model_emit_has_child (SymbolDBModel *model, SymbolDBModelNode *node)
{
		GtkTreePath *path;
		GtkTreeIter iter = {0};

		iter.stamp = SYMBOL_DB_MODEL_STAMP;
		iter.user_data = node->parent;
		iter.user_data2 = GINT_TO_POINTER (node->offset);
		
		path = sdb_model_get_path (GTK_TREE_MODEL (model), &iter);
		gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model),
		                                      path, &iter);
		gtk_tree_path_free (path);
}

/**
 * sdb_model_ensure_node_children:
 * @model: The tree model
 * @node: The node for which the children are being ensured
 * @emit_has_child: Should it emit children status change signal to model
 * 
 * When a node is initially created, there is no status of its children. This
 * function determines the number of children of the node and initializes
 * children array. They children node themselves are not initialized yet.
 */
static void
sdb_model_ensure_node_children (SymbolDBModel *model,
                                SymbolDBModelNode *node,
                                gboolean emit_has_child,
                                gboolean fake_child)
{
	SymbolDBModelPriv *priv;
	gboolean old_has_child;
	
	g_return_if_fail (node->n_children == 0);
	g_return_if_fail (node->children == NULL);
	g_return_if_fail (node->children_ensured == FALSE);

	priv = model->priv;

	/* Can not ensure if model is frozen */
	if (priv->freeze_count > 0)
		return;
	
	/* Initialize children array and count */
	old_has_child = node->has_child;
	node->n_children = 
		sdb_model_get_n_children (model, node->level,
		                          node->values);
	node->has_child = (node->n_children > 0);
	node->children_ensured = TRUE;
	node->has_child_ensured = TRUE;

	if (fake_child && old_has_child && !node->has_child)
	{
		node->n_children = 1;
		node->has_child = TRUE;
		return;
	}
	
	if ((old_has_child != node->has_child) && node->parent)
	{
		sdb_model_emit_has_child (model, node);
	}
}

/**
 * sdb_model_update_node_children:
 * @model: The model being updated
 * @node: The node being updated
 * @emit_has_child: Whether to emit has-child-changed signal
 * 
 * Updates the children of @node. All existing children are destroyed and
 * new ones fetched. Update signals are also emited for the views to know
 * about updates.
 */
static void
sdb_model_update_node_children (SymbolDBModel *model,
                                SymbolDBModelNode *node,
                                gboolean emit_has_child)
{
	/* Delete all nodes */
	if (node->n_children > 0)
	{
		GtkTreePath *path;
		GtkTreeIter iter = {0};
		gint i;

		/* Set the iter to first child */
		iter.stamp = SYMBOL_DB_MODEL_STAMP;
		iter.user_data = node;
		iter.user_data2 = GINT_TO_POINTER (0);

		/* Get path to it */
		path = sdb_model_get_path (GTK_TREE_MODEL (model), &iter);

		/* Delete all children */
		for (i = 0; i < node->n_children; i++)
			gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
		gtk_tree_path_free (path);
	}

	sdb_model_node_cleanse (node, TRUE);
	sdb_model_ensure_node_children (model, node, emit_has_child, FALSE);
	
	/* Add all new nodes */
	if (node->n_children > 0)
	{
		GtkTreePath *path;
		GtkTreeIter iter = {0};
		gint i;
		
		iter.stamp = SYMBOL_DB_MODEL_STAMP;
		iter.user_data = node;
		iter.user_data2 = 0;
		path = sdb_model_get_path (GTK_TREE_MODEL (model), &iter);
		if (path == NULL)
			path = gtk_tree_path_new_first ();
		for (i = 0; i < node->n_children; i++)
		{
			iter.user_data2 = GINT_TO_POINTER (i);
			gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
			gtk_tree_path_next (path);
		}
		gtk_tree_path_free (path);
	}
}

/**
 * sdb_model_get_query_value_real:
 * @model: The model
 * @data_model: The backend data model
 * @iter: The tree model iterator
 * @column: The model column
 * @value: Pointer to retun value
 *
 * Retrieves model data at row @iter and column @column from backend data
 * model @data_model. This function retrieves the column data from its map
 * given at model initialization. It can be overriden in derived classes to
 * retrive custom column values (based on given data model at the given iter).
 *
 * Returns: TRUE if @value set successfully, else FALSE.
 */
static gboolean
sdb_model_get_query_value_real (SymbolDBModel *model,
                                GdaDataModel *data_model,
                                GdaDataModelIter *iter, gint column,
                                GValue *value)
{
	const GValue *ret;
	SymbolDBModelPriv *priv = model->priv;
	gint query_column = priv->query_columns[column];

	if (query_column < 0)
		return FALSE;

	ret = gda_data_model_iter_get_value_at (iter, query_column);
	if (!ret || !G_IS_VALUE (ret))
		return FALSE;

	g_value_copy (ret, value);
	return TRUE;
}

static gboolean
sdb_model_get_query_value (SymbolDBModel *model,
                           GdaDataModel *data_model,
                           GdaDataModelIter *iter, gint column,
                           GValue *value)
{
	return SYMBOL_DB_MODEL_GET_CLASS(model)->get_query_value(model, data_model,
	                                                         iter, column,
	                                                         value);
}

/**
 * sdb_model_get_query_value_at_real:
 * @model: The model
 * @data_model: The backend data model where value is derived.
 * @position: Position of the row.
 * @column: The column being retrieved.
 * @value: Return value
 *
 * Same as sdb_model_get_query_value_real() except it uses integer index
 * for row positioning instead of an iter. It will be used when some backend
 * data model does not support iter.
 *
 * Returns: TRUE if @value set successfully, else FALSE.
 */
static gboolean
sdb_model_get_query_value_at_real (SymbolDBModel *model,
                                         GdaDataModel *data_model,
                                         gint position, gint column,
                                         GValue *value)
{
	const GValue *ret;
	SymbolDBModelPriv *priv = model->priv;
	gint query_column = priv->query_columns[column];
	g_value_init (value, priv->column_types[column]);

	if (query_column < 0)
		return FALSE;

	ret = gda_data_model_get_value_at (data_model, query_column, position,
	                                   NULL);
	if (!ret || !G_IS_VALUE (ret))
		return FALSE;

	g_value_copy (ret, value);
	return TRUE;
}

static gboolean
sdb_model_get_query_value_at (SymbolDBModel *model,
                              GdaDataModel *data_model,
                              gint position, gint column, GValue *value)
{
	return SYMBOL_DB_MODEL_GET_CLASS(model)->get_query_value_at (model,
	                                                             data_model,
	                                                             position,
	                                                             column,
	                                                             value);
}

/**
 * sdb_model_get_has_child_real:
 * @model: The model
 * @tree_level: The tree level where the node is.
 * @column_values: The node column values.
 *
 * Gets if the node has 1 or more children with given column values.
 *
 * Returns: TRUE if node has children, otherwise FALSE
 */
static gboolean
sdb_model_get_has_child_real (SymbolDBModel *model, gint tree_level,
                              GValue column_values[])
{
	gboolean has_child;
	g_signal_emit_by_name (model, "get-has_child", tree_level, column_values,
	                       &has_child);
	return has_child;
}

static gboolean
sdb_model_get_has_child (SymbolDBModel *model, SymbolDBModelNode *node)
{
	if (node->has_child_ensured)
		return node->has_child;
	
	node->has_child_ensured = TRUE;
	node->has_child =
		SYMBOL_DB_MODEL_GET_CLASS(model)->get_has_child (model,
		                                                 node->level,
		                                                 node->values);
	if (node->has_child)
	{
		sdb_model_emit_has_child (model, node);
	}
	return node->has_child;
}

/**
 * sdb_model_get_n_children_real:
 * @model: The model
 * @tree_level: The tree level where the node is.
 * @column_values: The node column values.
 *
 * Gets the number of children of the node with given column values.
 *
 * Returns: Number of children
 */
static gint
sdb_model_get_n_children_real (SymbolDBModel *model, gint tree_level,
                               GValue column_values[])
{
	gint n_children;
	g_signal_emit_by_name (model, "get-n-children", tree_level, column_values,
	                       &n_children);
	return n_children;
}

static gint
sdb_model_get_n_children (SymbolDBModel *model, gint tree_level,
                          GValue column_values[])
{
	return SYMBOL_DB_MODEL_GET_CLASS(model)->get_n_children (model, tree_level,
	                                                         column_values);
}

/**
 * sdb_model_get_children_real:
 * @model: The model
 * @tree_level: The tree level where the node is.
 * @column_values: The node column values.
 * @offset: Offset of the start row
 * @limit: Number of rows to fetch
 *
 * Fetches the children data from backend database. The results are returned
 * as GdaDataModel. The children to fetch starts form @offset and retrieves
 * @limit amount.
 *
 * Returns: Data model holding the rows data, or NULL if there is no data.
 */
static GdaDataModel*
sdb_model_get_children_real (SymbolDBModel *model, gint tree_level,
                             GValue column_values[], gint offset,
                             gint limit)
{
	GdaDataModel *data_model;
	g_signal_emit_by_name (model, "get-children", tree_level,
	                       column_values, offset, limit, &data_model);
	return data_model;
}

static GdaDataModel*
sdb_model_get_children (SymbolDBModel *model, gint tree_level,
                        GValue column_values[], gint offset,
                        gint limit)
{
	return SYMBOL_DB_MODEL_GET_CLASS(model)->
		get_children (model, tree_level, column_values, offset, limit);
}

/* Object implementation */

static void
sdb_model_finalize (GObject *object)
{
	SymbolDBModelPriv *priv;

	priv = SYMBOL_DB_MODEL (object)->priv;;
	g_free (priv->column_types);
	g_free (priv->query_columns);
	sdb_model_node_cleanse (priv->root, TRUE);
	g_slice_free (SymbolDBModelNode, priv->root);

	g_free (priv);
	
	G_OBJECT_CLASS (sdb_model_parent_class)->finalize (object);
}

static void
sdb_model_set_property (GObject *object, guint prop_id,
                        const GValue *value, GParamSpec *pspec)
{
	g_return_if_fail (SYMBOL_DB_IS_MODEL (object));
	/* SymbolDBModel* model = SYMBOL_DB_MODEL(object);
	SymbolDBModelPriv* priv = GET_PRIV (model); */
	
	switch (prop_id)
	{
	}
}

static void
sdb_model_get_property (GObject *object, guint prop_id, GValue *value,
                        GParamSpec *pspec)
{
	g_return_if_fail (SYMBOL_DB_IS_MODEL (object));
	/* SymbolDBModel* model = SYMBOL_DB_MODEL(object);
	SymbolDBModelPriv* priv = GET_PRIV (model); */
	
	switch (prop_id)
	{
	}
}

static void
sdb_model_init (SymbolDBModel *object)
{
	SymbolDBModelPriv *priv;

	priv = g_new0 (SymbolDBModelPriv, 1);
	object->priv = priv;
	
	priv->root = g_slice_new0 (SymbolDBModelNode);
	priv->freeze_count = 0;
	priv->n_columns = 0;
	priv->column_types = NULL;
	priv->query_columns = NULL;
}

static void
sdb_model_class_init (SymbolDBModelClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	klass->get_query_value = sdb_model_get_query_value_real;
	klass->get_query_value_at = sdb_model_get_query_value_at_real;
	klass->get_has_child = sdb_model_get_has_child_real;
	klass->get_n_children = sdb_model_get_n_children_real;
	klass->get_children = sdb_model_get_children_real;
	
	object_class->finalize = sdb_model_finalize;
	object_class->set_property = sdb_model_set_property;
	object_class->get_property = sdb_model_get_property;
	
	/* Signals */
	symbol_db_model_signals[SIGNAL_GET_HAS_CHILD] =
		g_signal_new ("get-has-child",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0, NULL, NULL,
					  symbol_db_cclosure_marshal_BOOLEAN__INT_POINTER,
		              G_TYPE_BOOLEAN,
		              2,
		              G_TYPE_INT,
		              G_TYPE_POINTER);
	symbol_db_model_signals[SIGNAL_GET_N_CHILDREN] =
		g_signal_new ("get-n-children",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0, NULL, NULL,
					  symbol_db_cclosure_marshal_INT__INT_POINTER,
		              G_TYPE_INT,
		              2,
		              G_TYPE_INT,
		              G_TYPE_POINTER);
	symbol_db_model_signals[SIGNAL_GET_CHILDREN] =
		g_signal_new ("get-children",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0, NULL, NULL,
					  symbol_db_cclosure_marshal_OBJECT__INT_POINTER_INT_INT,
		              GDA_TYPE_DATA_MODEL,
		              4,
		              G_TYPE_INT,
		              G_TYPE_POINTER,
		              G_TYPE_INT,
		              G_TYPE_INT);
}

void
symbol_db_model_set_columns (SymbolDBModel *model, gint n_columns,
                             GType *types, gint *query_columns)
{
	SymbolDBModelPriv *priv;

	g_return_if_fail (n_columns > 0);
	g_return_if_fail (SYMBOL_DB_IS_MODEL (model));

	priv = model->priv;
	
	g_return_if_fail (priv->n_columns <= 0);
	g_return_if_fail (priv->column_types == NULL);
	g_return_if_fail (priv->query_columns == NULL);
	
	priv->n_columns = n_columns;
	priv->column_types = g_new0(GType, n_columns);
	priv->query_columns = g_new0(gint, n_columns);
	memcpy (priv->column_types, types, n_columns * sizeof (GType));
	memcpy (priv->query_columns, query_columns, n_columns * sizeof (gint));
}

GtkTreeModel *
symbol_db_model_new (gint n_columns, ...)
{
	GtkTreeModel *model;
	SymbolDBModelPriv *priv;
	va_list args;
	gint i;

	g_return_val_if_fail (n_columns > 0, NULL);

	model = g_object_new (SYMBOL_DB_TYPE_MODEL, NULL);
	priv = SYMBOL_DB_MODEL (model)->priv;
	
	priv->n_columns = n_columns;
	priv->column_types = g_new0(GType, n_columns);
	priv->query_columns = g_new0(gint, n_columns);
	
	va_start (args, n_columns);

	for (i = 0; i < n_columns; i++)
	{
		priv->column_types[i] = va_arg (args, GType);
		priv->query_columns[i] = va_arg (args, gint);
	}
	va_end (args);

	return model;
}

GtkTreeModel*
symbol_db_model_newv (gint n_columns, GType *types, gint *query_columns)
{
	GtkTreeModel *model;
	g_return_val_if_fail (n_columns > 0, NULL);
	model = g_object_new (SYMBOL_DB_TYPE_MODEL, NULL);
	symbol_db_model_set_columns (SYMBOL_DB_MODEL (model), n_columns,
	                             types, query_columns);
	return model;
}

void
symbol_db_model_update (SymbolDBModel *model)
{
	SymbolDBModelPriv *priv;

	g_return_if_fail (SYMBOL_DB_IS_MODEL (model));

	priv = model->priv;

	sdb_model_update_node_children (model, priv->root, FALSE);
}

void
symbol_db_model_freeze (SymbolDBModel *model)
{
	SymbolDBModelPriv *priv;

	g_return_if_fail (SYMBOL_DB_IS_MODEL (model));
	
	priv = model->priv;
	priv->freeze_count++;
}

void
symbol_db_model_thaw (SymbolDBModel *model)
{
	SymbolDBModelPriv *priv;

	g_return_if_fail (SYMBOL_DB_IS_MODEL (model));

	priv = model->priv;

	if (priv->freeze_count > 0)
		priv->freeze_count--;
	
	if (priv->freeze_count <= 0)
		symbol_db_model_update (model);
}
