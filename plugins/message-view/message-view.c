/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*  (c) Johannes Schmid 2003
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

#include <libanjuta/anjuta-utils.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-preferences.h>
#include <libanjuta/interfaces/ianjuta-message-view.h>

#include "message-view.h"
#define MESSAGE_TYPE message_get_type()

#define PREFERENCES_SCHEMA "org.gnome.anjuta.plugins.message-manager"
#define COLOR_ERROR "color-error"
#define COLOR_WARNING "color-warning"

struct _MessageViewPrivate
{
	//guint num_messages;
	gchar *line_buffer;

	GtkWidget *tree_view;
	GtkTreeModel *model;
	GtkTreeModel *filter;

	GtkWidget *popup_menu;

	gint adj_chgd_hdlr;

	/* Messages filter */
	MessageViewFlags flags;
	gint normal_count;
	gint warn_count;
	gint error_count;
	gint info_count;

	/* Properties */
	gchar *label;
	gchar *pixmap;
	gboolean highlite;

	GSettings* settings;
};

typedef struct
{
	IAnjutaMessageViewType type;
	gchar *summary;
	gchar *details;

} Message;

enum
{
	COLUMN_COLOR = 0,
	COLUMN_SUMMARY,
	COLUMN_MESSAGE,
	COLUMN_PIXBUF,
	N_COLUMNS
};

enum
{
	MV_PROP_ID = 0,
	MV_PROP_LABEL,
	MV_PROP_PIXMAP,
	MV_PROP_HIGHLITE
};

static gpointer parent_class;

static void prefs_init (MessageView *mview);
static void prefs_finalize (MessageView *mview);

static gboolean
message_view_tree_view_filter (GtkTreeModel *model,
							   GtkTreeIter  *iter,
							   gpointer      data);

/* Ask the user for an uri name */
static gchar *
ask_user_for_save_uri (GtkWindow* parent)
{
	GtkWidget* dialog;
	gchar* uri;

       	dialog = gtk_file_chooser_dialog_new (_("Save file as"), parent,
		GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);

	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
	}
	else
	{
		uri = NULL;
	}

	gtk_widget_destroy(dialog);

	return uri;
}

/* Message object creation, copy and freeing */
static Message*
message_new (IAnjutaMessageViewType type, const gchar *summary,
			 const gchar *details)
{
	/* DEBUG_PRINT ("%s", "Creating message"); */
	Message *message = g_new0 (Message, 1);
	message->type = type;
	if (summary)
		message->summary = g_strdup (summary);
	if (details)
		message->details = g_strdup (details);
	return message;
}

static Message*
message_copy (const Message *src)
{
	return message_new (src->type, src->summary, src->details);
}

static void
message_free (Message *message)
{
	/* DEBUG_PRINT ("%s", "Freeing message"); */
	g_free (message->summary);
	g_free (message->details);
	g_free (message);
}

static gboolean
message_serialize (Message *message, AnjutaSerializer *serializer)
{
	if (!anjuta_serializer_write_int (serializer, "type",
									  message->type))
		return FALSE;
	if (!anjuta_serializer_write_string (serializer, "summary",
										 message->summary))
		return FALSE;
	if (!anjuta_serializer_write_string (serializer, "details",
										 message->details))
		return FALSE;
	return TRUE;
}

static gboolean
message_deserialize (Message *message, AnjutaSerializer *serializer)
{
	gint type;
	if (!anjuta_serializer_read_int (serializer, "type",
									 &type))
		return FALSE;
	message->type = type;
	if (!anjuta_serializer_read_string (serializer, "summary",
										&message->summary, TRUE))
		return FALSE;
	if (!anjuta_serializer_read_string (serializer, "details",
										&message->details, TRUE))
		return FALSE;
	return TRUE;
}

static GType
message_get_type (void)
{
	static GType type = 0;
	if (!type)
	{
		type = g_boxed_type_register_static ("MessageViewMessage",
											 (GBoxedCopyFunc) message_copy,
											 (GBoxedFreeFunc) message_free);
	}
	return type;
}

/* Utility functions */
/* Adds the char c to the string str */
static void
add_char(gchar** str, gchar c)
{
	gchar* buffer;

	g_return_if_fail(str != NULL);

	buffer = g_strdup_printf("%s%c", *str, c);
	g_free(*str);
	*str = buffer;
}

static gchar*
escape_string (const gchar *str)
{
	GString *gstr;
	const gchar *iter;

	gstr = g_string_new ("");
	iter = str;
	while (*iter != '\0')
	{
		if (*iter == '>')
			gstr = g_string_append (gstr, "&gt;");
		else if (*iter == '<')
			gstr = g_string_append (gstr, "&lt;");
		else if (*iter == '&')
			gstr = g_string_append (gstr, "&amp;");
		else
			gstr = g_string_append_c (gstr, *iter);
		iter++;
	}
	return g_string_free (gstr, FALSE);
}

static gboolean
message_view_query_tooltip (GtkWidget* widget, gint x, gint y, gboolean keyboard,
						 GtkTooltip* tooltip)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeModel *model;
	MessageView* view = MESSAGE_VIEW(widget);

	model = view->privat->model;

	if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW(view->privat->tree_view),
		x, y, &path, NULL, NULL, NULL))
	{
		Message *message;
		gchar *text, *title, *desc;

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter, COLUMN_MESSAGE, &message, -1);
		gtk_tree_path_free(path);

		if (!message->details || !message->summary ||
			strlen (message->details) <= 0 ||
			strlen (message->summary) <= 0)
			return FALSE;

		title = escape_string (message->summary);
		desc = escape_string (message->details);
		text = g_strdup_printf ("<b>%s</b>\n%s", title, desc);

		g_free (title);
		g_free (desc);

		gtk_tooltip_set_markup (tooltip, text);
		g_free (text);
		return TRUE;
	}
	return FALSE;
}

/* MessageView signal callbacks */
/* Send a signal if a message was double-clicked or ENTER or SPACE was pressed */
static gboolean
on_message_event (GObject* object, GdkEvent* event, gpointer data)
{
	g_return_val_if_fail(object != NULL, FALSE);
	g_return_val_if_fail(event != NULL, FALSE);
	g_return_val_if_fail(data != NULL, FALSE);

	MessageView* view = MESSAGE_VIEW(data);

	if (event == NULL)
		return FALSE;

	if (event->type == GDK_KEY_PRESS)
	{
		switch(((GdkEventKey *)event)->keyval)
		{
			case GDK_KEY_space:
			case GDK_KEY_Return:
			{
				const gchar* message =
					ianjuta_message_view_get_current_message(IANJUTA_MESSAGE_VIEW (view), NULL);
				if (message)
				{
					g_signal_emit_by_name (G_OBJECT (view), "message_clicked",
										   message);
					return TRUE;
				}
				break;
			}
			default:
				return FALSE;
		}
	}
	else if (event->type == GDK_2BUTTON_PRESS)
	{
		if (((GdkEventButton *) event)->button == 1)
		{
			const gchar* message =
				ianjuta_message_view_get_current_message(IANJUTA_MESSAGE_VIEW (view), NULL);
			if (message)
			{
				g_signal_emit_by_name (G_OBJECT (view), "message_clicked",
									   message);
				return TRUE;
			}
		}
		return FALSE;
	}
	else if (event->type == GDK_BUTTON_PRESS)
	{
		if (((GdkEventButton *) event)->button == 3)
		{
			gtk_menu_popup (GTK_MENU (view->privat->popup_menu), NULL, NULL, NULL, NULL,
					((GdkEventButton *) event)->button,
					((GdkEventButton *) event)->time);
			return TRUE;
		}
	}
	return FALSE;
}

static void
on_adjustment_changed (GtkAdjustment* adj, gpointer data)
{
	gtk_adjustment_set_value (adj,
				  gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj));
}

static void
on_adjustment_value_changed (GtkAdjustment* adj, gpointer data)
{
	MessageView *self = MESSAGE_VIEW (data);
	if (gtk_adjustment_get_value (adj) > (gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj)) - 0.1)
	{
		if (!self->privat->adj_chgd_hdlr)
		{
			self->privat->adj_chgd_hdlr =
				g_signal_connect (G_OBJECT (adj), "changed",
								  G_CALLBACK (on_adjustment_changed), NULL);
		}
	}
	else
	{
		if (self->privat->adj_chgd_hdlr)
		{
			g_signal_handler_disconnect (G_OBJECT (adj), self->privat->adj_chgd_hdlr);
			self->privat->adj_chgd_hdlr = 0;
		}
	}
}

static void
message_view_set_property (GObject * object,
			   guint property_id,
			   const GValue * value, GParamSpec * pspec)
{
	MessageView *self = MESSAGE_VIEW (object);
	g_return_if_fail(value != NULL);
	g_return_if_fail(pspec != NULL);

	switch (property_id)
	{
	case MV_PROP_LABEL:
	{
		g_free (self->privat->label);
		self->privat->label = g_value_dup_string (value);
		break;
	}
	case MV_PROP_PIXMAP:
	{
		g_free (self->privat->pixmap);
		self->privat->pixmap = g_value_dup_string (value);
		break;
	}
	case MV_PROP_HIGHLITE:
	{
		self->privat->highlite = g_value_get_boolean (value);
		break;
	}
	default:
	{
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
	}
}

static void
message_view_get_property (GObject * object,
			   guint property_id,
			   GValue * value, GParamSpec * pspec)
{
	MessageView *self = MESSAGE_VIEW (object);

	g_return_if_fail(value != NULL);
	g_return_if_fail(pspec != NULL);

	switch (property_id)
	{
		case MV_PROP_LABEL:
		{
			g_value_set_string (value, self->privat->label);
			break;
		}
		case MV_PROP_PIXMAP:
		{
			g_value_set_string (value, self->privat->pixmap);
			break;
		}
		case MV_PROP_HIGHLITE:
		{
			g_value_set_boolean (value, self->privat->highlite);
			break;
		}
		default:
		{
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
		}
	}
}

static void
message_view_dispose (GObject *obj)
{
	MessageView *mview = MESSAGE_VIEW (obj);
	prefs_finalize (mview);
	if (mview->privat->tree_view)
	{
		mview->privat->tree_view = NULL;
	}
	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
message_view_finalize (GObject *obj)
{
	MessageView *mview = MESSAGE_VIEW (obj);
	g_free (mview->privat->line_buffer);
	g_free (mview->privat->label);
	g_free (mview->privat->pixmap);
	g_free (mview->privat);
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
message_view_instance_init (MessageView * self)
{
	GtkWidget *scrolled_win;
	GtkCellRenderer *renderer;
	GtkCellRenderer *renderer_pixbuf;
	GtkTreeViewColumn *column;
	GtkTreeViewColumn *column_pixbuf;
	GtkTreeSelection *select;
	GtkListStore *model;
	GtkAdjustment* adj;

	g_return_if_fail(self != NULL);
	self->privat = g_new0 (MessageViewPrivate, 1);

	/* Init private data */
	self->privat->line_buffer = g_strdup("");
	self->privat->flags = 0xF;

	/* Create the tree widget */
	model = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING,
								G_TYPE_STRING, MESSAGE_TYPE,  G_TYPE_STRING);
	self->privat->model = GTK_TREE_MODEL (model);

	/* message filter */
	self->privat->filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (model), NULL);
	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (self->privat->filter),
											message_view_tree_view_filter,
											self, NULL);

	self->privat->tree_view =
		gtk_tree_view_new_with_model (GTK_TREE_MODEL (self->privat->filter));
	gtk_widget_show (self->privat->tree_view);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW
									   (self->privat->tree_view), FALSE);

	/* Create pixbuf column */
	renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
	g_object_set (G_OBJECT(renderer_pixbuf), "stock-size", GTK_ICON_SIZE_MENU, NULL);
	column_pixbuf = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column_pixbuf, _("Icon"));
	gtk_tree_view_column_pack_start (column_pixbuf, renderer_pixbuf, TRUE);
	gtk_tree_view_column_add_attribute
		(column_pixbuf, renderer_pixbuf, "stock-id", COLUMN_PIXBUF);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->privat->tree_view),
								 column_pixbuf);
	/* Create columns to hold text and color of a line, this
	 * columns are invisible of course. */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "yalign", 0.0, "wrap-mode", PANGO_WRAP_WORD,
				  "wrap-width", 1000, NULL);
	column = gtk_tree_view_column_new ();

	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_title (column, _("Messages"));
	gtk_tree_view_column_add_attribute
		(column, renderer, "foreground", COLUMN_COLOR);
	gtk_tree_view_column_add_attribute
		(column, renderer, "markup", COLUMN_SUMMARY);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->privat->tree_view),
								 column);

	/* Adjust selection */
	select = gtk_tree_view_get_selection
		(GTK_TREE_VIEW (self->privat->tree_view));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_BROWSE);

	/* Add tree view to a scrolled window */
	scrolled_win = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scrolled_win),
			   self->privat->tree_view);
	gtk_widget_show (scrolled_win);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
									GTK_POLICY_AUTOMATIC,
									GTK_POLICY_AUTOMATIC);
	adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW
											   (scrolled_win));
	self->privat->adj_chgd_hdlr = g_signal_connect(G_OBJECT(adj), "changed",
									 G_CALLBACK (on_adjustment_changed), self);
	g_signal_connect(G_OBJECT(adj), "value_changed",
					 G_CALLBACK(on_adjustment_value_changed), self);

	gtk_container_add (GTK_CONTAINER (self), scrolled_win);

	/* Connect signals */
	g_signal_connect (G_OBJECT(self->privat->tree_view), "event",
					  G_CALLBACK (on_message_event), self);

	g_object_set (G_OBJECT(self), "has-tooltip", TRUE, NULL);

}

static void
message_view_class_init (MessageViewClass * klass)
{
	GParamSpec *message_view_spec_label;
	GParamSpec *message_view_spec_pixmap;
	GParamSpec *message_view_spec_highlite;
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass* widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	gobject_class->set_property = message_view_set_property;
	gobject_class->get_property = message_view_get_property;
	gobject_class->finalize = message_view_finalize;
	gobject_class->dispose = message_view_dispose;

	widget_class->query_tooltip = message_view_query_tooltip;


	message_view_spec_label = g_param_spec_string ("label",
						       "Label of the view",
						       "Used to decorate the view,"
						       "translateable",
						       "no label",
						       G_PARAM_READWRITE);
	g_object_class_install_property (gobject_class,
					 MV_PROP_LABEL,
					 message_view_spec_label);

	message_view_spec_pixmap = g_param_spec_string ("pixmap",
						       "Pixmap of the view",
						       "Used to decorate the view tab,"
						       "translateable",
						       "no label",
						       G_PARAM_READWRITE);
	g_object_class_install_property (gobject_class,
					 MV_PROP_PIXMAP,
					 message_view_spec_pixmap);

	message_view_spec_highlite = g_param_spec_boolean ("highlite",
							   "Highlite build messages",
							   "If TRUE, specify colors",
							   FALSE,
							   G_PARAM_READWRITE);
	g_object_class_install_property (gobject_class,
					 MV_PROP_HIGHLITE,
					 message_view_spec_highlite);
}

/* Returns a new message-view instance */
GtkWidget *
message_view_new (GtkWidget* popup_menu)
{
	MessageView * mv = MESSAGE_VIEW (g_object_new (message_view_get_type (), NULL));
	mv->privat->popup_menu = popup_menu;
	prefs_init (mv);
	return GTK_WIDGET(mv);
}

gboolean
message_view_serialize (MessageView *view, AnjutaSerializer *serializer)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (view != NULL && MESSAGE_IS_VIEW (view), FALSE);

	if (!anjuta_serializer_write_string (serializer, "label",
										 view->privat->label))
		return FALSE;
	if (!anjuta_serializer_write_string (serializer, "pixmap",
										 view->privat->pixmap))
		return FALSE;
	if (!anjuta_serializer_write_int (serializer, "highlite",
									  view->privat->highlite))
		return FALSE;

	/* Serialize individual messages */
	model = view->privat->model;

	if (!anjuta_serializer_write_int (serializer, "messages",
									  gtk_tree_model_iter_n_children (model, NULL)))
		return FALSE;

	if (gtk_tree_model_get_iter_first (model, &iter))
	{
		do
		{
			Message *message;
			gtk_tree_model_get (model, &iter, COLUMN_MESSAGE, &message, -1);
			if (message)
			{
				if (!message_serialize (message, serializer))
					return FALSE;
			}
		}
		while (gtk_tree_model_iter_next (model, &iter));
	}
	return TRUE;
}

gboolean
message_view_deserialize (MessageView *view, AnjutaSerializer *serializer)
{
	GtkTreeModel *model;
	gint messages, i;

	g_return_val_if_fail (view != NULL && MESSAGE_IS_VIEW (view), FALSE);

	if (!anjuta_serializer_read_string (serializer, "label",
										&view->privat->label, TRUE))
		return FALSE;
	if (!anjuta_serializer_read_string (serializer, "pixmap",
										&view->privat->pixmap, TRUE))
		return FALSE;
	if (!anjuta_serializer_read_int (serializer, "highlite",
									 &view->privat->highlite))
		return FALSE;

	/* Create individual messages */
	model = view->privat->model;
	gtk_list_store_clear (GTK_LIST_STORE (model));

	if (!anjuta_serializer_read_int (serializer, "messages", &messages))
		return FALSE;

	for (i = 0; i < messages; i++)
	{
		Message *message;

		message = message_new (0, NULL, NULL);
		if (!message_deserialize (message, serializer))
		{
			message_free (message);
			return FALSE;
		}
		ianjuta_message_view_append (IANJUTA_MESSAGE_VIEW (view), message->type,
									 message->summary, message->details, NULL);
		message_free (message);
	}
	return TRUE;
}

void message_view_next(MessageView* view)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *select;

	g_return_if_fail (view != NULL && MESSAGE_IS_VIEW (view));

	model = view->privat->model;
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW
					      (view->privat->tree_view));

	if (!gtk_tree_selection_get_selected (select, &model, &iter))
	{
		if (gtk_tree_model_get_iter_first (model, &iter))
			gtk_tree_selection_select_iter (select, &iter);
	}
	while (gtk_tree_model_iter_next (model, &iter))
	{
		Message *message;
		gtk_tree_model_get (model, &iter, COLUMN_MESSAGE,
							&message, -1);
		if (message->type == IANJUTA_MESSAGE_VIEW_TYPE_WARNING
			|| message->type == IANJUTA_MESSAGE_VIEW_TYPE_ERROR)
		{
			const gchar* message;
			gtk_tree_selection_select_iter (select, &iter);
			message =
				ianjuta_message_view_get_current_message(IANJUTA_MESSAGE_VIEW (view), NULL);
			if (message)
			{
				GtkTreePath *path;
				path = gtk_tree_model_get_path (model, &iter);
				gtk_tree_view_set_cursor (GTK_TREE_VIEW
											  (view->privat->tree_view),
											  path, NULL, FALSE);
				gtk_tree_path_free (path);
				g_signal_emit_by_name (G_OBJECT (view), "message_clicked",
									   message);
				break;
			}
		}
	}
}

void message_view_previous(MessageView* view)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *select;
	GtkTreePath *path;

	g_return_if_fail (view != NULL && MESSAGE_IS_VIEW (view));

	model = view->privat->model;
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW
					      (view->privat->tree_view));

	if (!gtk_tree_selection_get_selected (select, &model, &iter))
	{
		if (gtk_tree_model_get_iter_first (model, &iter))
			gtk_tree_selection_select_iter (select, &iter);
	}

	/* gtk_tree_model_iter_previous does not exist, use path */
	path = gtk_tree_model_get_path (model, &iter);

	while (gtk_tree_path_prev(path))
	{
		Message *message;
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get (model, &iter, COLUMN_MESSAGE,
							&message, -1);
		if (message->type == IANJUTA_MESSAGE_VIEW_TYPE_WARNING
			|| message->type == IANJUTA_MESSAGE_VIEW_TYPE_ERROR)
		{
			const gchar* message;

			gtk_tree_selection_select_iter (select, &iter);
			message =
				ianjuta_message_view_get_current_message(IANJUTA_MESSAGE_VIEW (view), NULL);
			if (message)
			{
				GtkTreePath *path;
				path = gtk_tree_model_get_path (model, &iter);
				gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW
											  (view->privat->tree_view),
											  path, NULL, FALSE, 0, 0);
				gtk_tree_path_free (path);
				g_signal_emit_by_name (G_OBJECT (view), "message_clicked",
									   message);
				break;
			}
		}
	}
	gtk_tree_path_free (path);
}

static gboolean message_view_save_as(MessageView* view, gchar* uri)
{
	GFile *file;
	GOutputStream *os;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean ok;

	g_return_val_if_fail (view != NULL && MESSAGE_IS_VIEW (view), FALSE);

	if (uri == NULL)
		return FALSE;

	file = g_file_new_for_uri (uri);
	os = G_OUTPUT_STREAM (
			g_file_replace (file, NULL,
				FALSE,
				G_FILE_CREATE_NONE,
				NULL,
				NULL));
	/* Create file */
	if (os == NULL)
	{
		g_object_unref (file);
		return FALSE;
	}

	/* Save all lines of message view */
	model = view->privat->model;

	ok = TRUE;
	gtk_tree_model_get_iter_first (model, &iter);
	do
	{
		Message *message;

		gtk_tree_model_get (model, &iter, COLUMN_MESSAGE, &message, -1);
		if (message)
		{
			if (message->details && (strlen (message->details) > 0))
			{
				if (g_output_stream_write (os, message->details, strlen (message->details), NULL, NULL) < 0)
				{
					ok = FALSE;
				}
			}
			else
			{
				if (g_output_stream_write (os, message->summary, strlen (message->summary), NULL, NULL) < 0)
				{
					ok = FALSE;
				}
			}
			if (g_output_stream_write (os, "\n", 1, NULL, NULL) < 0)
			{
				ok = FALSE;
			}
		}
	} while (gtk_tree_model_iter_next (model, &iter));
	g_output_stream_close (os, NULL, NULL);
	g_object_unref (os);
	g_object_unref (file);

	return ok;
}

void message_view_save(MessageView* view)
{
	GtkWindow* parent;
	gchar* uri;

	g_return_if_fail (view != NULL && MESSAGE_IS_VIEW (view));

	parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view)));

	uri = ask_user_for_save_uri (parent);
	if (uri)
	{
		if (message_view_save_as (view, uri) == FALSE)
		{
			anjuta_util_dialog_error(parent, _("Error writing %s"), uri);
		}
		g_free (uri);
	}
}

void message_view_copy(MessageView* view)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *select;

	g_return_if_fail (view != NULL && MESSAGE_IS_VIEW (view));

	model = view->privat->model;
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW
					      (view->privat->tree_view));

	if (gtk_tree_selection_get_selected (select, &model, &iter))
	{
		Message *message;
		const gchar *text;
		GtkClipboard *clipboard;

		gtk_tree_model_get (model, &iter, COLUMN_MESSAGE, &message, -1);

		if (message->details && (*message->details != '\0'))
		{
			text = message->details;
		}
		else if (message->summary && (*message->summary != '\0'))
		{
			text = message->summary;
		}
		else
		{
			/* No message */
			return;
		}

		clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view), GDK_SELECTION_CLIPBOARD);

		gtk_clipboard_set_text (clipboard, text, -1);
	}
}

/* Preferences notifications */
static void
pref_change_color (MessageView *mview, IAnjutaMessageViewType type,
				   const gchar *color_pref_key)
{
	gchar* color;
	GtkListStore *store;
	GtkTreeIter iter;
	gboolean success;

	color = g_settings_get_string (mview->privat->settings, color_pref_key);
	store = GTK_LIST_STORE (mview->privat->model);
	success = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);
	while (success)
	{
		Message *message;
		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COLUMN_MESSAGE,
							&message, -1);
		if (message && message->type == type)
		{
			gtk_list_store_set (store, &iter, COLUMN_COLOR, color, -1);
		}
		success = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
	}
	g_free(color);
}


static void
on_notify_color (GSettings* settings, const gchar* key,
                 gpointer user_data)
{
	if (g_str_equal (key, COLOR_ERROR))
	    pref_change_color (MESSAGE_VIEW (user_data),
	                       IANJUTA_MESSAGE_VIEW_TYPE_ERROR,
	                       key);
	else
	    pref_change_color (MESSAGE_VIEW (user_data),
	                       IANJUTA_MESSAGE_VIEW_TYPE_WARNING,
	                       key);
}

static void
prefs_init (MessageView *mview)
{
	mview->privat->settings = g_settings_new (PREFERENCES_SCHEMA);
	g_signal_connect (mview->privat->settings, "changed::" COLOR_ERROR,
	                  G_CALLBACK (on_notify_color), mview);
	g_signal_connect (mview->privat->settings, "changed::" COLOR_WARNING,
	                  G_CALLBACK (on_notify_color), mview);
}

static void
prefs_finalize (MessageView *mview)
{
	if (mview->privat->settings)
		g_object_unref (mview->privat->settings);
	mview->privat->settings = NULL;
}

/* IAnjutaMessageView interface implementation */

/* Appends the text in buffer. Flushes the buffer where a newline is found.
 * by emiiting buffer_flushed signal. The string is expected to be utf8.
 */
static void
imessage_view_buffer_append (IAnjutaMessageView * message_view,
									const gchar * message, GError ** e)
{
	MessageView *view;
	gint cur_char;
	int len;

	g_return_if_fail (MESSAGE_IS_VIEW (message_view));

	if (!message)
		return;

	len = strlen(message);

	view = MESSAGE_VIEW (message_view);

	/* Check if message contains newlines */
	for (cur_char = 0; cur_char < len; cur_char++)
	{
		/* Is newline => print line */
		if (message[cur_char] != '\n')
		{
			add_char(&view->privat->line_buffer, message[cur_char]);
		}
		else
		{
			g_signal_emit_by_name (G_OBJECT (view), "buffer_flushed",
								   view->privat->line_buffer);
			g_free(view->privat->line_buffer);
			view->privat->line_buffer = g_strdup("");
		}
	}
}

static void
imessage_view_append (IAnjutaMessageView *message_view,
					  IAnjutaMessageViewType type,
					  const gchar *summary,
					  const gchar *details,
					  GError ** e)
{
	gchar* color;
	GtkListStore *store;
	GtkTreeIter iter;
	gboolean highlite;
	gchar *utf8_msg;
	gchar *escaped_str;
	gchar* stock_id = NULL;

	MessageView *view;
	Message *message;

	g_return_if_fail (MESSAGE_IS_VIEW (message_view));

	view = MESSAGE_VIEW (message_view);

	message = message_new (type, summary, details);

	g_object_get (G_OBJECT (view), "highlite", &highlite, NULL);
	color = NULL;
	if (highlite)
	{
		switch (message->type)
		{
			case IANJUTA_MESSAGE_VIEW_TYPE_INFO:
				view->privat->info_count++;
				stock_id = GTK_STOCK_INFO;
				break;
			case IANJUTA_MESSAGE_VIEW_TYPE_WARNING:
				color = g_settings_get_string (view->privat->settings, COLOR_WARNING);
				/* FIXME: There is no GTK_STOCK_WARNING which would fit better here */
				view->privat->warn_count++;
				stock_id = GTK_STOCK_DIALOG_WARNING;
				break;
			case IANJUTA_MESSAGE_VIEW_TYPE_ERROR:
				color = g_settings_get_string (view->privat->settings, COLOR_ERROR);
				view->privat->error_count++;
				stock_id = GTK_STOCK_STOP;
				break;
			default:
				color = NULL;
				view->privat->normal_count++;
		}
	}

	/* Add the message to the tree */
	store = GTK_LIST_STORE (view->privat->model);
	gtk_list_store_append (store, &iter);

	/*
	 * Must be normalized to compose representation to be
	 * displayed correctly (Bug in gtk_list???)
	 */
	utf8_msg = g_utf8_normalize (message->summary, -1,
								 G_NORMALIZE_DEFAULT_COMPOSE);
	if (message->details && strlen (message->details) > 0)
	{
		gchar *summary;
		summary = escape_string (message->summary);
		escaped_str = g_strdup_printf ("<b>%s</b>", summary);
		g_free (summary);
	} else {
		escaped_str = escape_string (message->summary);
	}
	gtk_list_store_set (store, &iter,
							COLUMN_SUMMARY, escaped_str,
							COLUMN_MESSAGE, message,
							-1);
	if (color)
	{
		gtk_list_store_set (store, &iter,
							COLUMN_COLOR, color, -1);
	}
	if (stock_id)
	{
		gtk_list_store_set (store, &iter,
							COLUMN_PIXBUF, stock_id, -1);
	}
	g_free(color);
	message_free (message);
	g_free (utf8_msg);
	g_free (escaped_str);
}

/* Clear all messages from the message view */
static void
imessage_view_clear (IAnjutaMessageView *message_view, GError **e)
{
	GtkListStore *store;
	MessageView *view;

	g_return_if_fail (MESSAGE_IS_VIEW (message_view));
	view = MESSAGE_VIEW (message_view);

	/* filter settings restart */
	view->privat->normal_count = 0;
	view->privat->info_count = 0;
	view->privat->warn_count = 0;
	view->privat->error_count = 0;

	store = GTK_LIST_STORE (view->privat->model);
	gtk_list_store_clear (store);
}

/* Move the selection to the next line. */
static void
imessage_view_select_next (IAnjutaMessageView * message_view,
						   GError ** e)
{
	MessageView* view = MESSAGE_VIEW(message_view);
	message_view_next(view);
}

/* Move the selection to the previous line. */
static void
imessage_view_select_previous (IAnjutaMessageView * message_view,
							   GError ** e)
{
	MessageView *view = MESSAGE_VIEW(message_view);
	message_view_previous(view);
}

/* Return the currently selected messages or the first message if no
 * message is selected or NULL if no messages are availible. The
 * returned message must not be freed.
 */
static const gchar *
imessage_view_get_current_message (IAnjutaMessageView * message_view,
								   GError ** e)
{
	MessageView *view;
	GtkTreeIter iter;
	GtkTreeSelection *select;
	GtkTreeModel *model;
	const Message *message;

	g_return_val_if_fail (MESSAGE_IS_VIEW (message_view), NULL);

	view = MESSAGE_VIEW (message_view);
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW
									      (view->privat->tree_view));

	if (!gtk_tree_selection_get_selected (select, &model, &iter))
	{
		if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter))
		{
			gtk_tree_model_get (GTK_TREE_MODEL (model),
								&iter, COLUMN_MESSAGE, &message, -1);
			if (message)
			{
				if (message->details && strlen (message->details) > 0)
					return message->details;
				else
					return message->summary;
			}
		}
	}
	else
	{
		gtk_tree_model_get (GTK_TREE_MODEL (model),
						    &iter, COLUMN_MESSAGE, &message, -1);
		if (message)
		{
			if (message->details && strlen (message->details) > 0)
				return message->details;
			else
				return message->summary;
		}
	}
	return NULL;
}

/* Returns a GList which contains all messages, the GList itself
 * must be freed, the messages are managed by the message view and
 * must not be freed. NULL is return if no messages are availible.
 */
static GList *
imessage_view_get_all_messages (IAnjutaMessageView * message_view,
								GError ** e)
{
	MessageView *view;
	GtkListStore *store;
	GtkTreeIter iter;
	Message *message;
	GList *messages = NULL;

	g_return_val_if_fail (MESSAGE_IS_VIEW (message_view), NULL);

	view = MESSAGE_VIEW (message_view);
	store = GTK_LIST_STORE (view->privat->model);

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
	{
		do
		{
			gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
							    COLUMN_MESSAGE, &message);
			messages = g_list_prepend (messages, message->details);
		}
		while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
	}
	return messages;
}

static void
imessage_view_iface_init (IAnjutaMessageViewIface *iface)
{
	iface->buffer_append = imessage_view_buffer_append;
	iface->append = imessage_view_append;
	iface->clear = imessage_view_clear;
	iface->select_next = imessage_view_select_next;
	iface->select_previous = imessage_view_select_previous;
	iface->get_current_message = imessage_view_get_current_message;
	iface->get_all_messages = imessage_view_get_all_messages;
}

static gboolean
message_view_tree_view_filter (GtkTreeModel *model, GtkTreeIter  *iter,
							   gpointer      data)
{
	Message *msg;
	MessageView *msgview;

	msgview = MESSAGE_VIEW (data);
	gtk_tree_model_get (msgview->privat->model, iter, COLUMN_MESSAGE, &msg, -1);

	if (msg != NULL) {
		if (msg->type == IANJUTA_MESSAGE_VIEW_TYPE_NORMAL) {
			return msgview->privat->flags & MESSAGE_VIEW_SHOW_NORMAL;
		} else if (msg->type == IANJUTA_MESSAGE_VIEW_TYPE_INFO) {
			return msgview->privat->flags & MESSAGE_VIEW_SHOW_INFO;
		} else if (msg->type == IANJUTA_MESSAGE_VIEW_TYPE_WARNING) {
			return msgview->privat->flags & MESSAGE_VIEW_SHOW_WARNING;
		} else if (msg->type == IANJUTA_MESSAGE_VIEW_TYPE_ERROR) {
			return msgview->privat->flags & MESSAGE_VIEW_SHOW_ERROR;
		} else return TRUE;
	} else return FALSE;
}

MessageViewFlags
message_view_get_flags (MessageView* view)
{
	g_return_val_if_fail (view != NULL && MESSAGE_IS_VIEW (view), MESSAGE_VIEW_SHOW_NORMAL);

	return view->privat->flags;
}

void message_view_set_flags (MessageView* view, MessageViewFlags flags)
{
	g_return_if_fail (view != NULL && MESSAGE_IS_VIEW (view));

	view->privat->flags = flags;
	gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER(view->privat->filter));
}

gint message_view_get_count (MessageView* view, MessageViewFlags flags)
{
	g_return_val_if_fail (view != NULL && MESSAGE_IS_VIEW (view), 0);

	switch (flags)
	{
		case MESSAGE_VIEW_SHOW_NORMAL:
			return view->privat->normal_count;
		case MESSAGE_VIEW_SHOW_INFO:
			return view->privat->info_count;
		case MESSAGE_VIEW_SHOW_WARNING:
			return view->privat->warn_count;
		case MESSAGE_VIEW_SHOW_ERROR:
			return view->privat->error_count;
		default:
			g_assert_not_reached ();
	}
}

ANJUTA_TYPE_BEGIN(MessageView, message_view, GTK_TYPE_HBOX);
ANJUTA_TYPE_ADD_INTERFACE(imessage_view, IANJUTA_TYPE_MESSAGE_VIEW);
ANJUTA_TYPE_END;
