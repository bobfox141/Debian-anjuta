/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*  combo-flags.c
 *  Copyright (C) 2006 Armin Burgmeier
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "combo-flags.h"

#include <gtk/gtk.h>

#include <gdk/gdkkeysyms.h>

#include <libanjuta/anjuta-marshal.h>

typedef struct _CgComboFlagsCellInfo CgComboFlagsCellInfo;
struct _CgComboFlagsCellInfo
{
  GtkCellRenderer *cell;
  GSList *attributes;

  GtkCellLayoutDataFunc func;
  gpointer func_data;
  GDestroyNotify destroy;

  guint expand : 1;
  guint pack : 1;
};

typedef struct _CgComboFlagsPrivate CgComboFlagsPrivate;
struct _CgComboFlagsPrivate
{
	GtkTreeModel* model;

	GtkWidget* window;
	GtkWidget* treeview;
	GtkTreeViewColumn* column;

	GdkDevice* pointer_device;
	GdkDevice* keyboard_device;

	GSList* cells;

	gboolean editing_started;
	gboolean editing_canceled;
};

#define CG_COMBO_FLAGS_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE( \
		(o), \
		CG_TYPE_COMBO_FLAGS, \
		CgComboFlagsPrivate \
	))

enum {
	PROP_0,
	PROP_MODEL
};

enum {
	SELECTED,
	LAST_SIGNAL
};
static guint combo_flags_signals[LAST_SIGNAL];

static void cg_combo_flags_cell_layout_init (GtkCellLayoutIface *iface);
static void cg_combo_flags_cell_editable_init (GtkCellEditableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CgComboFlags, cg_combo_flags, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
                                                cg_combo_flags_cell_layout_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE,
                                                cg_combo_flags_cell_editable_init));

static CgComboFlagsCellInfo *
cg_combo_flags_get_cell_info (CgComboFlags *combo,
                              GtkCellRenderer *cell)
{
	CgComboFlagsPrivate *priv;
	GSList *i;

	priv = CG_COMBO_FLAGS_PRIVATE (combo);
	for (i = priv->cells; i != NULL; i = i->next)
	{
	    CgComboFlagsCellInfo *info = (CgComboFlagsCellInfo *) i->data;

	    if (info != NULL && info->cell == cell)
			return info;
	}

	return NULL;
}

static void
cg_combo_flags_cell_layout_pack_start (GtkCellLayout *layout,
                                       GtkCellRenderer *cell,
                                       gboolean expand)
{
	CgComboFlags *combo;
	CgComboFlagsPrivate *priv;
	CgComboFlagsCellInfo *info;

	combo = CG_COMBO_FLAGS (layout);
	priv = CG_COMBO_FLAGS_PRIVATE (combo);

	g_object_ref (cell);
	g_object_ref_sink (cell);

	info = g_new0 (CgComboFlagsCellInfo, 1);
	info->cell = cell;
	info->expand = expand;
	info->pack = GTK_PACK_START;

	priv->cells = g_slist_append (priv->cells, info);

	if (priv->column != NULL)
		gtk_tree_view_column_pack_start (priv->column, cell, expand);
}

static void
cg_combo_flags_cell_layout_pack_end (GtkCellLayout *layout,
                                     GtkCellRenderer *cell,
                                     gboolean expand)
{
	CgComboFlags *combo;
	CgComboFlagsPrivate *priv;
	CgComboFlagsCellInfo *info;

	combo = CG_COMBO_FLAGS (layout);
	priv = CG_COMBO_FLAGS_PRIVATE (combo);

	g_object_ref (cell);
	g_object_ref_sink (cell);

	info = g_new0 (CgComboFlagsCellInfo, 1);
	info->cell = cell;
	info->expand = expand;
	info->pack = GTK_PACK_END;

	priv->cells = g_slist_append (priv->cells, info);

	if (priv->column != NULL)
		gtk_tree_view_column_pack_end (priv->column, cell, expand);
}

static void
cg_combo_flags_cell_layout_clear_attributes (GtkCellLayout *layout,
                                             GtkCellRenderer *cell)
{
	CgComboFlags *combo;
	CgComboFlagsPrivate *priv;
	CgComboFlagsCellInfo *info;
	GSList *list;

	combo = CG_COMBO_FLAGS (layout);
	priv = CG_COMBO_FLAGS_PRIVATE (combo);

	info = cg_combo_flags_get_cell_info (combo, cell);
	g_return_if_fail (info != NULL);

	list = info->attributes;
	while (list && list->next)
	{
		g_free (list->data);
		list = list->next->next;
	}

	g_slist_free (info->attributes);
	info->attributes = NULL;

	if (priv->column != NULL)
	{
	    gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (priv->column), 
	                                      cell);
	}

	gtk_widget_queue_resize (GTK_WIDGET (combo));
}

static void
cg_combo_flags_cell_layout_clear (GtkCellLayout *layout)
{
	CgComboFlags *combo;
	CgComboFlagsPrivate *priv;
	GSList *i;

	combo = CG_COMBO_FLAGS (layout);
	priv = CG_COMBO_FLAGS_PRIVATE (combo);
 
	if (priv->column != NULL)
		gtk_tree_view_column_clear (priv->column);

	for (i = priv->cells; i; i = i->next)
	{
		CgComboFlagsCellInfo* info = (CgComboFlagsCellInfo*)i->data;

		cg_combo_flags_cell_layout_clear_attributes (layout, info->cell);
		g_object_unref (info->cell);
		g_free (info);
		i->data = NULL;
	}

	g_slist_free (priv->cells);
	priv->cells = NULL;
}

static void
cg_combo_flags_cell_layout_add_attribute (GtkCellLayout *layout,
                                          GtkCellRenderer *cell,
                                          const gchar *attribute,
                                          gint column)
{
	CgComboFlags *combo;
	CgComboFlagsPrivate *priv;
	CgComboFlagsCellInfo *info;

	combo = CG_COMBO_FLAGS (layout);
	priv = CG_COMBO_FLAGS_PRIVATE(combo);
	info = cg_combo_flags_get_cell_info (combo, cell);

	info->attributes = g_slist_prepend (info->attributes,
	                                    GINT_TO_POINTER (column));
	info->attributes = g_slist_prepend (info->attributes,
	                                    g_strdup (attribute));

	if (priv->column != NULL)
	{
		gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->column),
                                       cell, attribute, column);
	}

	gtk_widget_queue_resize (GTK_WIDGET (combo));
}

static void
cg_combo_flags_cell_data_func (GtkCellLayout *cell_layout,
                               GtkCellRenderer *cell,
                               GtkTreeModel *tree_model,
                               GtkTreeIter *iter,
                               gpointer data)
{
  CgComboFlagsCellInfo *info;
  info = (CgComboFlagsCellInfo *) data;

  if (info->func == NULL)
    return;

  (*info->func) (cell_layout, cell, tree_model, iter, info->func_data);
}

static void
cg_combo_flags_cell_layout_set_cell_data_func (GtkCellLayout *layout,
                                               GtkCellRenderer *cell,
                                               GtkCellLayoutDataFunc func,
                                               gpointer func_data,
                                               GDestroyNotify destroy)
{
	CgComboFlags *combo;
	CgComboFlagsPrivate *priv;
	CgComboFlagsCellInfo *info;
	GDestroyNotify old_destroy;

	combo = CG_COMBO_FLAGS (layout);
	priv = CG_COMBO_FLAGS_PRIVATE (combo);
	info = cg_combo_flags_get_cell_info (combo, cell);
	g_return_if_fail (info != NULL);

	if (info->destroy)
	{
		old_destroy = info->destroy;

		info->destroy = NULL;
		old_destroy (info->func_data);
	}

	info->func = func;
	info->func_data = func_data;
	info->destroy = destroy;

	if (priv->column != NULL)
	{
    	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (priv->column),
    	                                    cell, func, func_data, NULL);
	}

	gtk_widget_queue_resize (GTK_WIDGET (combo));
}

static void
cg_combo_flags_cell_layout_reorder (GtkCellLayout *layout,
                                    GtkCellRenderer *cell,
                                    gint position)
{
	CgComboFlags *combo;
	CgComboFlagsPrivate *priv;
	CgComboFlagsCellInfo *info;
	GSList *link;

	combo = CG_COMBO_FLAGS (layout);
	priv = CG_COMBO_FLAGS_PRIVATE (combo);

	info = cg_combo_flags_get_cell_info (combo, cell);
	g_return_if_fail (info != NULL);

	link = g_slist_find (priv->cells, info);
	g_return_if_fail (link != NULL);

	priv->cells = g_slist_remove_link (priv->cells, link);
	priv->cells = g_slist_insert (priv->cells, info, position);

	if (priv->column != NULL)
		gtk_cell_layout_reorder (GTK_CELL_LAYOUT (priv->column), cell, position);

	gtk_widget_queue_draw (GTK_WIDGET (combo));
}

static void
cg_combo_flags_cell_editable_start_editing (GtkCellEditable *cell_editable,
                                            G_GNUC_UNUSED GdkEvent *event)
{
	CgComboFlags *combo;
	CgComboFlagsPrivate *priv;

	combo = CG_COMBO_FLAGS (cell_editable);
	priv = CG_COMBO_FLAGS_PRIVATE (combo);
	
	priv->editing_started = TRUE;
	gtk_widget_grab_focus (GTK_WIDGET (combo));
	cg_combo_flags_popup (combo);
}

static void
cg_combo_flags_sync_cells (CgComboFlags *combo,
                           GtkCellLayout *cell_layout)
{
	CgComboFlagsPrivate *priv;
	CgComboFlagsCellInfo *info;
	GSList *j;
	GSList *k;

	priv = CG_COMBO_FLAGS_PRIVATE (combo);
	for (k = priv->cells; k != NULL; k = k->next)
	{
		info = (CgComboFlagsCellInfo *) k->data;

		if (info->pack == GTK_PACK_START)
			gtk_cell_layout_pack_start (cell_layout, info->cell, info->expand);
		else if (info->pack == GTK_PACK_END)
			gtk_cell_layout_pack_end (cell_layout, info->cell, info->expand);

		gtk_cell_layout_set_cell_data_func (cell_layout, info->cell,
		                                    cg_combo_flags_cell_data_func,
		                                    info, NULL);

		for (j = info->attributes; j != NULL; j = j->next->next)
		{
			gtk_cell_layout_add_attribute (cell_layout, info->cell, j->data,
			                               GPOINTER_TO_INT (j->next->data));
		}
	}
}

static void
cg_combo_flags_get_position (CgComboFlags *combo,
                             gint *x, 
                             gint *y, 
                             gint *width,
                             gint *height)
{
	CgComboFlagsPrivate *priv;
	GtkAllocation allocation;
	GtkRequisition popup_req;
	GdkWindow *window;
	GdkScreen *screen;
	GdkRectangle monitor;
	gint monitor_num;
  
	priv = CG_COMBO_FLAGS_PRIVATE (combo);
	
	g_assert (priv->window != NULL);

	window = gtk_widget_get_window (GTK_WIDGET (combo));
	gdk_window_get_origin (window, x, y);

	gtk_widget_get_allocation (GTK_WIDGET (combo), &allocation);

	if (!gtk_widget_get_has_window (GTK_WIDGET (combo)))
	{
		*x += allocation.x;
		*y += allocation.y;
	}

	gtk_widget_get_preferred_size (priv->window, &popup_req, NULL);

	*width = allocation.width;
	if (popup_req.width > *width) *width = popup_req.width;
	*height = popup_req.height;

	screen = gtk_widget_get_screen (GTK_WIDGET(combo));
	monitor_num = gdk_screen_get_monitor_at_window (screen, window);

	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	if (*x < monitor.x)
	{
		*x = monitor.x;
	}
	else if (*x + *width > monitor.x + monitor.width)
	{
		*x = monitor.x + monitor.width - *width;
	}
  
	if (*y + allocation.height + *height <=
	    monitor.y + monitor.height)
	{
		*y += allocation.height;
	}
	else if (*y - *height >= monitor.y)
	{
		*y -= *height;
	}
	else if (monitor.y + monitor.height -
	         (*y + allocation.height) > *y - monitor.y)
	{
		*y += allocation.height;
		*height = monitor.y + monitor.height - *y;
	}
	else
	{
		*height = *y - monitor.y;
		*y = monitor.y;
	}
}

static gboolean
cg_combo_flags_treeview_button_press_cb (G_GNUC_UNUSED GtkWidget *widget,
                                         GdkEventButton *event,
                                         gpointer data)
{
	CgComboFlags *combo;
	CgComboFlagsPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	
	combo = CG_COMBO_FLAGS(data);
	priv = CG_COMBO_FLAGS_PRIVATE(combo);

	switch (event->button)
	{
	case 1:
		selection =
			gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));

		if(gtk_tree_selection_get_selected (selection, NULL, &iter) == TRUE)
		{
			g_signal_emit (G_OBJECT (combo), combo_flags_signals[SELECTED],
			               0, &iter, CG_COMBO_FLAGS_SELECTION_TOGGLE);

			return TRUE;
		}

		return FALSE;
		break;
	case 3:
		priv->editing_canceled = FALSE;
		cg_combo_flags_popdown (combo);
		return TRUE;
		break;
	default:
		return FALSE;
		break;
	}
}

static gboolean
cg_combo_flags_treeview_key_press_cb (G_GNUC_UNUSED GtkWidget *widget,
                                      GdkEventKey *event,
                                      gpointer data)
{
	CgComboFlags *combo;
	CgComboFlagsPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	
	combo = CG_COMBO_FLAGS (data);
	priv = CG_COMBO_FLAGS_PRIVATE (combo);
	
	switch (event->keyval)
	{
	case GDK_KEY_space:
	case GDK_KEY_KP_Space:
		selection =
			gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));

		if(gtk_tree_selection_get_selected (selection, NULL, &iter) == TRUE)
		{
			g_signal_emit (G_OBJECT (combo), combo_flags_signals[SELECTED],
			               0, &iter, CG_COMBO_FLAGS_SELECTION_TOGGLE);

			return TRUE;
		}

		return FALSE;
		break;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		selection =
			gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));

		if(gtk_tree_selection_get_selected (selection, NULL, &iter) == TRUE)
		{
			g_signal_emit (G_OBJECT (combo), combo_flags_signals[SELECTED],
			               0, &iter, CG_COMBO_FLAGS_SELECTION_SELECT);
		}

		priv->editing_canceled = FALSE;
		cg_combo_flags_popdown (combo);
		return TRUE;
		break;
	default:
		return FALSE;
		break;
	}
}

static gboolean
cg_combo_flags_window_button_press_cb (G_GNUC_UNUSED GtkWidget *widget,
                                       G_GNUC_UNUSED GdkEventButton *event,
                                       gpointer data)
{
	/* Probably the mouse clicked somewhere else but not into the window,
	 * otherwise the treeview would have received the event. */
	CgComboFlags *combo;
	CgComboFlagsPrivate *priv;
	
	combo = CG_COMBO_FLAGS (data);
	priv = CG_COMBO_FLAGS_PRIVATE (combo);
	
	priv->editing_canceled = FALSE;
	cg_combo_flags_popdown (combo);

	return TRUE;
}

static gboolean
cg_combo_flags_window_key_press_cb (G_GNUC_UNUSED GtkWidget *widget,
                                    GdkEventKey *event,
                                    gpointer data)
{
	CgComboFlags *combo;
	CgComboFlagsPrivate *priv;
	
	combo = CG_COMBO_FLAGS (data);
	priv = CG_COMBO_FLAGS_PRIVATE (combo);
	
	switch (event->keyval)
	{
	case GDK_KEY_Escape:
		priv->editing_canceled = TRUE;
		cg_combo_flags_popdown (combo);
		return TRUE;
		break;
	default:
		return FALSE;
		break;
	}
}

static void
cg_combo_flags_init (CgComboFlags *combo_flags)
{
	CgComboFlagsPrivate *priv;
	priv = CG_COMBO_FLAGS_PRIVATE (combo_flags);

	priv->model = NULL;	
	priv->window = NULL;
	priv->treeview = NULL;
	priv->column = NULL;
	priv->cells = NULL;
	
	priv->editing_started = FALSE;
	priv->editing_canceled = FALSE;
}

static void 
cg_combo_flags_finalize (GObject *object)
{
	CgComboFlags *combo_flags;
	CgComboFlagsPrivate *priv;

	combo_flags = CG_COMBO_FLAGS (object);
	priv = CG_COMBO_FLAGS_PRIVATE (combo_flags);

	if (priv->window != NULL)
		cg_combo_flags_popdown (combo_flags);

	G_OBJECT_CLASS (cg_combo_flags_parent_class)->finalize (object);
}

static void
cg_combo_flags_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	CgComboFlags *combo_flags;
	CgComboFlagsPrivate *priv;

	g_return_if_fail(CG_IS_COMBO_FLAGS (object));

	combo_flags = CG_COMBO_FLAGS (object);
	priv = CG_COMBO_FLAGS_PRIVATE (combo_flags);

	switch (prop_id)
	{
	case PROP_MODEL:
		if (priv->model != NULL) g_object_unref (G_OBJECT (priv->model));
		priv->model = GTK_TREE_MODEL (g_value_dup_object (value));

		if (priv->treeview != NULL)
		{
			gtk_tree_view_set_model (GTK_TREE_VIEW (priv->treeview),
			                         priv->model);
		}

		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cg_combo_flags_get_property (GObject *object,
                             guint prop_id,
                             GValue *value, 
                             GParamSpec *pspec)
{
	CgComboFlags *combo_flags;
	CgComboFlagsPrivate *priv;

	g_return_if_fail (CG_IS_COMBO_FLAGS (object));

	combo_flags = CG_COMBO_FLAGS (object);
	priv = CG_COMBO_FLAGS_PRIVATE (combo_flags);

	switch (prop_id)
	{
	case PROP_MODEL:
		g_value_set_object (value, G_OBJECT (priv->model));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cg_combo_flags_class_init (CgComboFlagsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CgComboFlagsPrivate));

	object_class->finalize = cg_combo_flags_finalize;
	object_class->set_property = cg_combo_flags_set_property;
	object_class->get_property = cg_combo_flags_get_property;

	g_object_class_install_property(object_class,
	                                PROP_MODEL,
	                                g_param_spec_object("model",
	                                                    "Model",
	                                                    "The model used by the CgComboFlags widget",
	                                                    GTK_TYPE_TREE_MODEL,
	                                                    G_PARAM_READWRITE));

	combo_flags_signals[SELECTED] =
		g_signal_new("selected",
		             G_OBJECT_CLASS_TYPE(object_class),
		             G_SIGNAL_RUN_LAST,
		             0, /* no default handler */
		             NULL, NULL,
		             anjuta_cclosure_marshal_VOID__BOXED_ENUM,
		             G_TYPE_NONE,
		             2,
		             GTK_TYPE_TREE_ITER,
	                     CG_TYPE_COMBO_FLAGS_SELECTION_TYPE);
}

static void
cg_combo_flags_cell_layout_init (GtkCellLayoutIface *iface)
{
	iface->pack_start = cg_combo_flags_cell_layout_pack_start;
	iface->pack_end = cg_combo_flags_cell_layout_pack_end;
	iface->reorder = cg_combo_flags_cell_layout_reorder;
	iface->clear = cg_combo_flags_cell_layout_clear;
	iface->add_attribute = cg_combo_flags_cell_layout_add_attribute;
	iface->set_cell_data_func = cg_combo_flags_cell_layout_set_cell_data_func;
	iface->clear_attributes = cg_combo_flags_cell_layout_clear_attributes;
}

static void
cg_combo_flags_cell_editable_init (GtkCellEditableIface *iface)
{
	iface->start_editing = cg_combo_flags_cell_editable_start_editing;
}

static gboolean
cg_combo_flags_popup_idle (gpointer data)
{
	CgComboFlags *combo;
	CgComboFlagsPrivate *priv;
	GtkTreeSelection* selection;
	GtkWidget *toplevel;
	GtkWidget *scrolled;
	GdkWindow *window;
	GdkDeviceManager* device_manager;
	gint height, width, x, y;

	combo = CG_COMBO_FLAGS (data);
	priv = CG_COMBO_FLAGS_PRIVATE (combo);

	g_assert (priv->window == NULL);
	priv->window = gtk_window_new (GTK_WINDOW_POPUP);

	g_object_ref (G_OBJECT (priv->window));
	gtk_window_set_resizable (GTK_WINDOW (priv->window), FALSE);

	g_signal_connect (G_OBJECT (priv->window), "key_press_event",
	                  G_CALLBACK (cg_combo_flags_window_key_press_cb),
	                  combo);

	g_signal_connect (G_OBJECT (priv->window), "button_press_event",
	                  G_CALLBACK (cg_combo_flags_window_button_press_cb),
	                  combo);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (priv->window), scrolled);

	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
									     GTK_SHADOW_ETCHED_IN);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
  								    GTK_POLICY_NEVER, GTK_POLICY_NEVER);

	gtk_widget_show (scrolled);

	priv->treeview = gtk_tree_view_new_with_model (priv->model);
	gtk_widget_show (priv->treeview);
	gtk_container_add (GTK_CONTAINER (scrolled), priv->treeview);

	g_signal_connect (G_OBJECT (priv->treeview), "key_press_event",
	                  G_CALLBACK (cg_combo_flags_treeview_key_press_cb),
	                  combo);

	g_signal_connect (G_OBJECT (priv->treeview), "button_press_event",
	                  G_CALLBACK (cg_combo_flags_treeview_button_press_cb),
	                  combo);

	priv->column = gtk_tree_view_column_new ();
	g_object_ref (G_OBJECT (priv->column));
	cg_combo_flags_sync_cells (combo, GTK_CELL_LAYOUT (priv->column));
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview), priv->column);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (priv->treeview), FALSE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->treeview), FALSE);
	gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (priv->treeview), TRUE);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (combo));
	if (GTK_IS_WINDOW (toplevel))
	{
		gtk_window_group_add_window (gtk_window_get_group (
		                             GTK_WINDOW (toplevel)),
		                             GTK_WINDOW (priv->window));

		gtk_window_set_transient_for (GTK_WINDOW (priv->window),
		                              GTK_WINDOW (toplevel));

	}
	
	gtk_window_set_screen (GTK_WINDOW (priv->window),
                           gtk_widget_get_screen (GTK_WIDGET (combo)));

	cg_combo_flags_get_position (combo, &x, &y, &width, &height);
	gtk_widget_set_size_request (priv->window, width, height);
	gtk_window_move (GTK_WINDOW(priv->window), x, y);
	gtk_widget_show (priv->window);

	gtk_widget_grab_focus (priv->window);
	if (!gtk_widget_has_focus (priv->treeview))
		gtk_widget_grab_focus (priv->treeview);

	window = gtk_widget_get_window (priv->window);

	device_manager = gdk_display_get_device_manager (gdk_window_get_display (window));
	priv->pointer_device = gdk_device_manager_get_client_pointer (device_manager);
	priv->keyboard_device = gdk_device_get_associated_device (priv->pointer_device);

	gtk_grab_add (priv->window);

	gdk_device_grab (priv->pointer_device, window, GDK_OWNERSHIP_NONE, TRUE,
	                 GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
	                 NULL, GDK_CURRENT_TIME);

	gdk_device_grab (priv->keyboard_device, window, GDK_OWNERSHIP_NONE, TRUE,
	                 GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
	                 NULL, GDK_CURRENT_TIME);
	return FALSE;
}

static gboolean
cg_combo_flags_popdown_idle (gpointer data)
{
	gtk_widget_destroy (GTK_WIDGET (data));
	return FALSE;
}

GType
cg_combo_flags_selection_type_get_type (void)
{
	static GType our_type = 0;

	if(our_type == 0)
	{
		static const GEnumValue values[] =
		{
			{ CG_COMBO_FLAGS_SELECTION_NONE, "CG_COMBO_FLAGS_SELECTION_NONE", "none" },
			{ CG_COMBO_FLAGS_SELECTION_UNSELECT, "CG_COMBO_FLAGS_SELECTION_UNSELECT", "unselect" },
			{ CG_COMBO_FLAGS_SELECTION_SELECT, "CG_COMBO_FLAGS_SELECTION_SELECT", "select" },
			{ CG_COMBO_FLAGS_SELECTION_TOGGLE, "CG_COMBO_FLAGS_SELECTION_TOGGLE", "toggle" },
			{ 0, NULL, NULL }
		};

		our_type = g_enum_register_static("CgComboFlagsSelectionType", values);
	}

	return our_type;
}

GtkWidget *
cg_combo_flags_new (void)
{
	GObject *object;	
	object = g_object_new (CG_TYPE_COMBO_FLAGS, NULL);
	return GTK_WIDGET (object);
}

GtkWidget *
cg_combo_flags_new_with_model (GtkTreeModel *model)
{
	GObject *object;
	object = g_object_new (CG_TYPE_COMBO_FLAGS, "model", model, NULL);
	return GTK_WIDGET (object);
}

void
cg_combo_flags_popup(CgComboFlags *combo)
{
	g_idle_add(cg_combo_flags_popup_idle, combo);
}

void
cg_combo_flags_popdown(CgComboFlags *combo)
{
	CgComboFlagsPrivate *priv;
	priv = CG_COMBO_FLAGS_PRIVATE (combo);

	if (priv->window != NULL)
	{
		gtk_grab_remove (priv->window);
		gdk_device_ungrab (priv->pointer_device, GDK_CURRENT_TIME);
		gdk_device_ungrab (priv->keyboard_device, GDK_CURRENT_TIME);
		gtk_widget_hide (priv->window);

		g_object_unref (priv->column);
		g_idle_add (cg_combo_flags_popdown_idle, priv->window);

		priv->window = NULL;
		priv->treeview = NULL;
		priv->column = NULL;

		if (priv->editing_started)
		{
			priv->editing_started = FALSE;
			gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (combo));

			/* Seems like someone already calls _remove_widget when the
			 * cell renderer emits its edited signal (which we rely on if
			 * the editing was not canceled). */
			if (priv->editing_canceled)
				gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (combo));
		}
	}
}

gboolean
cg_combo_flags_editing_canceled (CgComboFlags *combo)
{
	return CG_COMBO_FLAGS_PRIVATE (combo)->editing_canceled;
}
