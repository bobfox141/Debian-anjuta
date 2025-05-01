/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*  anjuta-msgman.c (c) 2004 Johannes Schmid
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

#include <gtk/gtk.h>
#include <libanjuta/anjuta-utils.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-shell.h>
#include <libanjuta/anjuta-tabber.h>
#include <libanjuta/anjuta-close-button.h>
#include <libanjuta/resources.h>

#include "message-view.h"
#include "anjuta-msgman.h"

struct _AnjutaMsgmanPriv
{
	AnjutaPreferences *preferences;
	GtkWidget* popup_menu;
	GtkWidget* tab_popup;
	GList *views;

	GtkWidget* tabber;
	GSList* button_group;
};

struct _AnjutaMsgmanPage
{
	GtkWidget *widget;
	GtkWidget *pixmap;
	GtkWidget *label;
	GtkWidget *box;
	GtkWidget *close_button;
};

enum
{
	VIEW_CHANGED,
	LAST_SIGNAL
};

guint msgman_signal[LAST_SIGNAL];

typedef struct _AnjutaMsgmanPage AnjutaMsgmanPage;

static void
on_msgman_close_page (GtkButton* button,
									AnjutaMsgman *msgman)
{
	MessageView *view = MESSAGE_VIEW (g_object_get_data (G_OBJECT (button),
														 "message_view"));
	anjuta_msgman_remove_view (msgman, view);
}

static void
on_msgman_close_all(GtkMenuItem* item, AnjutaMsgman* msgman)
{

	anjuta_msgman_remove_all_views(msgman);
}

static GtkWidget*
create_tab_popup_menu(AnjutaMsgman* msgman)
{
	GtkWidget* menu;
	GtkWidget* item_close_all;

	menu = gtk_menu_new();

	item_close_all = gtk_menu_item_new_with_label(_("Close all message tabs"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_close_all);
	g_signal_connect(G_OBJECT(item_close_all), "activate", G_CALLBACK(on_msgman_close_all),
					 msgman);
	gtk_widget_show_all(menu);

	gtk_menu_attach_to_widget(GTK_MENU(menu), GTK_WIDGET(msgman), NULL);
	return menu;
}

static void
on_msgman_popup_menu(GtkWidget* widget, AnjutaMsgman* msgman)
{
	gtk_menu_popup(GTK_MENU(msgman->priv->tab_popup),
				   NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

static gboolean
on_tab_button_press_event(GtkWidget* widget, GdkEventButton* event, AnjutaMsgman* msgman)
{
	if (event->type == GDK_BUTTON_PRESS)
	{
		if (event->button == 3)
		{
				gtk_menu_popup(GTK_MENU(msgman->priv->tab_popup), NULL, NULL, NULL, NULL,
							   event->button, event->time);
			return TRUE;
		}
	}
	return FALSE;
}

static AnjutaMsgmanPage *
anjuta_msgman_page_new (GtkWidget * view, const gchar * name,
			const gchar * pixmap, AnjutaMsgman * msgman)
{
	AnjutaMsgmanPage *page;

	g_return_val_if_fail (view != NULL, NULL);

	page = g_new0 (AnjutaMsgmanPage, 1);
	page->widget = GTK_WIDGET (view);

	page->label = gtk_label_new (name);
	gtk_label_set_ellipsize (GTK_LABEL(page->label), PANGO_ELLIPSIZE_END);
	page->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_spacing (GTK_BOX (page->box), 5);
	if (pixmap  && strlen(pixmap))
	{
		GtkStockItem unused;
		if (gtk_stock_lookup(pixmap, &unused))
		{
			page->pixmap = gtk_image_new_from_stock(pixmap, GTK_ICON_SIZE_MENU);
		}
		else
		{
			page->pixmap = anjuta_res_get_image_sized (pixmap, 16, 16);
		}
		gtk_box_pack_start (GTK_BOX (page->box), page->pixmap, FALSE, FALSE, 0);
	}
	gtk_box_pack_start (GTK_BOX (page->box), page->label, TRUE, TRUE, 0);

	page->close_button = anjuta_close_button_new ();

	g_object_set_data (G_OBJECT (page->close_button), "message_view", page->widget);
	g_signal_connect (page->close_button, "clicked",
						G_CALLBACK(on_msgman_close_page),
						msgman);

	gtk_box_pack_start (GTK_BOX(page->box), page->close_button, FALSE, FALSE, 0);

	gtk_widget_show_all (page->box);

	return page;
}

static void
anjuta_msgman_page_destroy (AnjutaMsgmanPage * page)
{
	g_free (page);
}

static AnjutaMsgmanPage *
anjuta_msgman_page_from_widget (AnjutaMsgman * msgman, MessageView * mv)
{
	GList *node;
	node = msgman->priv->views;
	while (node)
	{
		AnjutaMsgmanPage *page;
		page = node->data;
		g_assert (page);
		if (page->widget == GTK_WIDGET (mv))
			return page;
		node = g_list_next (node);
	}
	return NULL;
}

static gpointer parent_class;

static void
anjuta_msgman_dispose (GObject *obj)
{
	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
anjuta_msgman_finalize (GObject *obj)
{
	AnjutaMsgman *msgman = ANJUTA_MSGMAN (obj);
	gtk_widget_destroy(msgman->priv->tab_popup);
	if (msgman->priv)
	{
		g_free (msgman->priv);
		msgman->priv = NULL;
	}
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
anjuta_msgman_instance_init (AnjutaMsgman * msgman)
{
	msgman->priv = G_TYPE_INSTANCE_GET_PRIVATE (msgman, ANJUTA_TYPE_MSGMAN,
	                                            AnjutaMsgmanPriv);

	gtk_notebook_set_scrollable (GTK_NOTEBOOK (msgman), TRUE);
	msgman->priv->views = NULL;
	msgman->priv->tab_popup = create_tab_popup_menu(msgman);
	msgman->priv->tabber = anjuta_tabber_new (GTK_NOTEBOOK (msgman));
	msgman->priv->button_group = NULL;

	g_signal_connect(msgman, "popup-menu",
                       G_CALLBACK(on_msgman_popup_menu), msgman);
    g_signal_connect(msgman, "button-press-event",
                       G_CALLBACK(on_tab_button_press_event), msgman);
}

static void
anjuta_msgman_switch_page (GtkNotebook* notebook,
                           GtkWidget* page,
                           guint page_num)
{
	GTK_NOTEBOOK_CLASS (parent_class)->switch_page (notebook, page, page_num);

	g_signal_emit_by_name (ANJUTA_MSGMAN (notebook),
	                       "view-changed");
}

static void
anjuta_msgman_class_init (AnjutaMsgmanClass * klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkNotebookClass *notebook_class = GTK_NOTEBOOK_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	gobject_class->finalize = anjuta_msgman_finalize;
	gobject_class->dispose = anjuta_msgman_dispose;

	notebook_class->switch_page = anjuta_msgman_switch_page;

	g_type_class_add_private (klass, sizeof (AnjutaMsgmanPriv));


	/**
	 * AnjutaMsgMan::view-changed:
	 * @msgman: #AnjutaMsgMan
	 *
	 * Emitted when the current view changes
	 */

	msgman_signal[VIEW_CHANGED] = g_signal_new ("view-changed",
	                                            G_OBJECT_CLASS_TYPE (klass),
	                                            G_SIGNAL_RUN_LAST,
	                                            G_STRUCT_OFFSET (AnjutaMsgmanClass, view_changed),
	                                            NULL, NULL,
	                                            g_cclosure_marshal_VOID__VOID,
	                                            G_TYPE_NONE,
	                                            0);
}

static void
set_message_tab(GSettings* settings, GtkNotebook *msgman)
{
	gchar *tab_pos;
	GtkPositionType pos;

	tab_pos = g_settings_get_string (settings, MESSAGES_TABS_POS);
	pos = GTK_POS_TOP;
	if (tab_pos)
	{
		if (strcasecmp (tab_pos, "left") == 0)
			pos = GTK_POS_LEFT;
		else if (strcasecmp (tab_pos, "right") == 0)
			pos = GTK_POS_RIGHT;
		else if (strcasecmp (tab_pos, "bottom") == 0)
			pos = GTK_POS_BOTTOM;
		g_free (tab_pos);
	}
	gtk_notebook_set_tab_pos (msgman, pos);
}

void
on_notify_message_pref (GSettings *settings, const gchar* key,
                        gpointer user_data)
{
	set_message_tab(settings, GTK_NOTEBOOK (user_data));
}


GtkWidget*
anjuta_msgman_new (GtkWidget *popup_menu)
{
	GtkWidget *msgman = NULL;
	msgman = gtk_widget_new (ANJUTA_TYPE_MSGMAN, "show-tabs", FALSE, NULL);
	if (msgman)
	{
	    ANJUTA_MSGMAN (msgman)->priv->popup_menu = popup_menu;
	}
	return msgman;
}


ANJUTA_TYPE_BEGIN (AnjutaMsgman, anjuta_msgman, GTK_TYPE_NOTEBOOK);
ANJUTA_TYPE_END;

static void
on_message_view_destroy (MessageView *view, AnjutaMsgman *msgman)
{
	AnjutaMsgmanPage *page;

	page = anjuta_msgman_page_from_widget (msgman, view);

	g_signal_handlers_disconnect_by_func (G_OBJECT (view),
					  G_CALLBACK (on_message_view_destroy), msgman);

	msgman->priv->views = g_list_remove (msgman->priv->views, page);
	anjuta_msgman_page_destroy (page);

}

static void
anjuta_msgman_append_view (AnjutaMsgman * msgman, GtkWidget *mv,
						   const gchar * name, const gchar * pixmap)
{
	AnjutaMsgmanPage *page;

	g_return_if_fail (msgman != NULL);
	g_return_if_fail (mv != NULL);
	g_return_if_fail (name != NULL);

	gtk_widget_show_all (mv);
	page = anjuta_msgman_page_new (mv, name, pixmap, msgman);

	msgman->priv->views =
		g_list_prepend (msgman->priv->views, (gpointer) page);

	gtk_notebook_append_page (GTK_NOTEBOOK (msgman), mv, NULL);

	g_signal_emit_by_name (msgman, "view-changed");

	anjuta_tabber_add_tab (ANJUTA_TABBER(msgman->priv->tabber), page->box);

	g_signal_connect (G_OBJECT (mv), "destroy",
					  G_CALLBACK (on_message_view_destroy), msgman);
}

MessageView *
anjuta_msgman_add_view (AnjutaMsgman * msgman,
						const gchar * name, const gchar * pixmap)
{
	GtkWidget *mv;

	g_return_val_if_fail (msgman != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	mv = message_view_new (msgman->priv->popup_menu);
	g_return_val_if_fail (mv != NULL, NULL);
	g_object_set (G_OBJECT (mv), "highlite", TRUE, "label", name,
				  "pixmap", pixmap, NULL);
	anjuta_msgman_append_view (msgman, mv, name, pixmap);
	return MESSAGE_VIEW (mv);
}

void
anjuta_msgman_remove_view (AnjutaMsgman * msgman, MessageView *view)
{
	if (!view)
		view = anjuta_msgman_get_current_view (msgman);

	g_return_if_fail (view != NULL);
	gtk_widget_destroy (GTK_WIDGET (view));

	g_signal_emit_by_name (msgman, "view-changed");
}

void
anjuta_msgman_remove_all_views (AnjutaMsgman * msgman)
{
	GList *views, *node;
	AnjutaMsgmanPage *page;

	views = NULL;
	node = msgman->priv->views;
	while (node)
	{
		page = node->data;
		views = g_list_prepend (views, page->widget);
		node = g_list_next (node);
	}
	node = views;
	while (node)
	{
		gtk_widget_destroy (GTK_WIDGET (node->data));
		node = g_list_next (node);
	}

	g_list_free (msgman->priv->views);
	g_list_free (views);

	g_signal_emit_by_name (msgman, "view-changed");

	msgman->priv->views = NULL;
}

MessageView *
anjuta_msgman_get_current_view (AnjutaMsgman * msgman)
{
	gint page = gtk_notebook_get_current_page (GTK_NOTEBOOK(msgman));
	if (page != -1)
		return MESSAGE_VIEW (gtk_notebook_get_nth_page (GTK_NOTEBOOK(msgman), page));
	else
		return NULL;
}

MessageView *
anjuta_msgman_get_view_by_name (AnjutaMsgman * msgman, const gchar * name)
{
	GList *node;

	g_return_val_if_fail (msgman != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	node = msgman->priv->views;
	while (node)
	{
		AnjutaMsgmanPage *page;
		page = node->data;
		g_assert (page);
		if (strcmp (gtk_label_get_text (GTK_LABEL (page->label)),
			    name) == 0)
		{
			return MESSAGE_VIEW (page->widget);
		}
		node = g_list_next (node);
	}
	return NULL;
}

void
anjuta_msgman_set_current_view (AnjutaMsgman * msgman, MessageView * mv)
{
	g_return_if_fail (msgman != NULL);
	gint page_num;

	if (mv)
	{
		page_num =
			gtk_notebook_page_num (GTK_NOTEBOOK (msgman),
					       GTK_WIDGET (mv));
		gtk_notebook_set_current_page (GTK_NOTEBOOK (msgman), page_num);
	}
}

GList *
anjuta_msgman_get_all_views (AnjutaMsgman * msgman)
{
	return msgman->priv->views;
}

void
anjuta_msgman_set_view_title (AnjutaMsgman *msgman, MessageView *view,
							  const gchar *title)
{
	AnjutaMsgmanPage *page;

	g_return_if_fail (title != NULL);

	page = anjuta_msgman_page_from_widget (msgman, view);
	g_return_if_fail (page != NULL);

	gtk_label_set_text (GTK_LABEL (page->label), title);
}

void
anjuta_msgman_set_view_icon (AnjutaMsgman *msgman, MessageView *view,
							  GdkPixbufAnimation *icon)
{
	AnjutaMsgmanPage *page;

	g_return_if_fail (icon != NULL);

	page = anjuta_msgman_page_from_widget (msgman, view);
	g_return_if_fail (page != NULL);

	gtk_image_set_from_animation (GTK_IMAGE (page->pixmap), icon);
}

void
anjuta_msgman_set_view_icon_from_stock (AnjutaMsgman *msgman, MessageView *view,
							  const gchar *icon)
{
	AnjutaMsgmanPage *page;

	g_return_if_fail (icon != NULL);

	page = anjuta_msgman_page_from_widget (msgman, view);
	g_return_if_fail (page != NULL);

	gtk_image_set_from_stock (GTK_IMAGE (page->pixmap), icon,
							  GTK_ICON_SIZE_MENU);
}

gboolean
anjuta_msgman_serialize (AnjutaMsgman *msgman, AnjutaSerializer *serializer)
{
	GList *node;

	if (!anjuta_serializer_write_int (serializer, "views",
									  g_list_length (msgman->priv->views)))
		return FALSE;

	node = msgman->priv->views;
	while (node)
	{
		AnjutaMsgmanPage *page = (AnjutaMsgmanPage*)node->data;
		if (!message_view_serialize (MESSAGE_VIEW (page->widget), serializer))
			return FALSE;
		node = g_list_next (node);
	}
	return TRUE;
}

gboolean
anjuta_msgman_deserialize (AnjutaMsgman *msgman, AnjutaSerializer *serializer)
{
	gint views, i;

	if (!anjuta_serializer_read_int (serializer, "views", &views))
		return FALSE;

	for (i = 0; i < views; i++)
	{
		gchar *label, *pixmap;
		GtkWidget *view;
		view = message_view_new (msgman->priv->popup_menu);
		g_return_val_if_fail (view != NULL, FALSE);
		if (!message_view_deserialize (MESSAGE_VIEW (view), serializer))
		{
			gtk_widget_destroy (view);
			return FALSE;
		}
		g_object_get (view, "label", &label, "pixmap", &pixmap, NULL);
		anjuta_msgman_append_view (msgman, view, label, pixmap);
		g_free (label);
		g_free (pixmap);
	}
	return TRUE;
}

GtkWidget* anjuta_msgman_get_tabber (AnjutaMsgman* msgman)
{
	return msgman->priv->tabber;
}
