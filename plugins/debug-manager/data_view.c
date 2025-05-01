/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    data_view.c
    Copyright (C) 2006 Sebastien Granjoux

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

#include "data_view.h"

#include "chunk_view.h"

#include "anjuta-marshal.h"

#include <glib/gi18n.h>

#include <gdk/gdkkeysyms.h>

#include <stdlib.h>

/* Size in pixel of border between text view widget */
#define ADDRESS_BORDER 4
#define ASCII_BORDER 2
#define SCROLLBAR_SPACING 4

struct _DmaDataView
{
	GtkContainer parent;
	
	GtkWidget *address;
	GtkWidget *data;
	GtkWidget *ascii;
	GtkWidget *range;
	
	GtkWidget *goto_window;
	GtkWidget *goto_entry;
	
	guint16 shadow_type;
	GtkAllocation frame;

	GtkTextBuffer *adr_buffer;
	GtkTextBuffer *data_buffer;
	GtkTextBuffer *ascii_buffer;

	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	
	GtkAdjustment* view_range;
	GtkAdjustment* buffer_range;
	
	DmaDataBuffer *buffer;
	
	gulong start;
	
	guint bytes_by_line;
	guint line_by_page;
	guint char_by_byte;
	guint char_height;
	guint char_width;
};

struct _DmaDataViewClass
{
	GtkContainerClass parent_class;

	GtkCssProvider *css;
};

enum {
  PROP_0, PROP_BUFFER, N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (DmaDataView, dma_data_view, GTK_TYPE_CONTAINER);

/* Helper functions
 *---------------------------------------------------------------------------*/

#if 0
static void
get_widget_char_size (GtkWidget *widget, gint *width, gint *height)
{
	PangoFontMetrics *metrics;
	PangoContext *context;
	PangoFontDescription *font_desc;

	font_desc = pango_font_description_from_string ("Monospace 10");
	
	context = gtk_widget_get_pango_context (widget);
	metrics = pango_context_get_metrics (context,
				       /*widget->style->font_desc,*/
					   font_desc,
				       pango_context_get_language (context));

	*height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
							   pango_font_metrics_get_descent (metrics));
	*width = (pango_font_metrics_get_approximate_char_width (metrics) + PANGO_SCALE - 1) / PANGO_SCALE;
	pango_font_metrics_unref (metrics);
	pango_font_description_free (font_desc);
}
#endif

static void
get_css_padding_and_border (GtkWidget *widget,
                            GtkBorder *border)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder tmp;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, border);
  gtk_style_context_get_border (context, state, &tmp);
  border->top += tmp.top;
  border->right += tmp.right;
  border->bottom += tmp.bottom;
  border->left += tmp.left;
}

/* Goto address window
 *---------------------------------------------------------------------------*/

static void
dma_data_view_goto_window_hide (DmaDataView *view)
{
  gtk_widget_hide (view->goto_window);
//  gtk_entry_set_text (GTK_ENTRY (view->goto_entry), "");
}

static gboolean
dma_data_view_goto_delete_event (GtkWidget *widget,
								 GdkEventAny *event,
								 DmaDataView *view)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	dma_data_view_goto_window_hide (view);

	return TRUE;
}

static gboolean
dma_data_view_goto_key_press_event (GtkWidget *widget,
									GdkEventKey *event,
									DmaDataView *view)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (IS_DMA_DATA_VIEW (view), FALSE);

	/* Close window */
	if (event->keyval == GDK_KEY_Escape ||
		event->keyval == GDK_KEY_Tab ||
		event->keyval == GDK_KEY_KP_Tab ||
		event->keyval == GDK_KEY_ISO_Left_Tab)
    {
		dma_data_view_goto_window_hide (view);
		return TRUE;
    }

	/* Goto to address and close window */
	if (event->keyval == GDK_KEY_Return ||
		event->keyval == GDK_KEY_ISO_Enter ||
		event->keyval == GDK_KEY_KP_Enter)
	{
		gulong adr;
		const gchar *text;
		gchar *end;
		
		text = gtk_entry_get_text (GTK_ENTRY (view->goto_entry));
		adr = strtoul (text, &end, 0);
		
		if ((*text != '\0') && (*end == '\0'))
		{
			/* Valid input goto to address */
			gtk_adjustment_set_value (view->buffer_range, adr);
		}
		
		dma_data_view_goto_window_hide (view);
		return TRUE;
	}
	
	return FALSE;
}

static void
dma_data_view_goto_position_func (DmaDataView *view)
{
	gint x, y;
	gint win_x, win_y;
	GdkWindow *window = gtk_widget_get_window (GTK_WIDGET (view));
	GdkScreen *screen = gdk_window_get_screen (window);
	gint monitor_num;
	GdkRectangle monitor;

	monitor_num = gdk_screen_get_monitor_at_window (screen, window);
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	gtk_widget_realize (view->goto_window);

	gdk_window_get_origin (window, &win_x, &win_y);
	x = MAX(12, win_x + 12);
	y = MAX(12, win_y + 12);
	
	gtk_window_move (GTK_WINDOW (view->goto_window), x, y);
}

/* Cut and paste from gtkwindow.c */
static void
send_focus_change (GtkWidget *widget,
                   gboolean   in)
{
	GdkEvent *fevent = gdk_event_new (GDK_FOCUS_CHANGE);

	fevent->focus_change.type = GDK_FOCUS_CHANGE;
	fevent->focus_change.window = g_object_ref (gtk_widget_get_window (widget));
	fevent->focus_change.in = in;

	gtk_widget_send_focus_change (widget, fevent);
	gdk_event_free (fevent);
}

static void
dma_data_view_goto_activate (GtkWidget   *menu_item,
			     DmaDataView *view)
{
	GtkWidget *toplevel;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWindowGroup *toplevel_group;
	GtkWindowGroup *goto_window_group;
	
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
	toplevel_group = gtk_window_get_group (GTK_WINDOW (toplevel));
	goto_window_group = gtk_window_get_group (GTK_WINDOW (view->goto_window));
	
	if (view->goto_window != NULL)
	{
		if (toplevel_group)
			gtk_window_group_add_window (toplevel_group,
						     GTK_WINDOW (view->goto_window));
		else if (goto_window_group)
			gtk_window_group_remove_window (goto_window_group,
							GTK_WINDOW (view->goto_window));
	
	}
	else
	{
		view->goto_window = gtk_window_new (GTK_WINDOW_POPUP);

		if (toplevel_group)
			gtk_window_group_add_window (toplevel_group,
						     GTK_WINDOW (view->goto_window));

		gtk_window_set_modal (GTK_WINDOW (view->goto_window), TRUE);
		g_signal_connect (view->goto_window, "delete_event",
						  G_CALLBACK (dma_data_view_goto_delete_event),
						  view);
		g_signal_connect (view->goto_window, "key_press_event",
						  G_CALLBACK (dma_data_view_goto_key_press_event),
						  view);
  
		frame = gtk_frame_new (NULL);
		gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
		gtk_widget_show (frame);
		gtk_container_add (GTK_CONTAINER (view->goto_window), frame);
	
		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_show (vbox);
		gtk_container_add (GTK_CONTAINER (frame), vbox);
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

		/* add entry */
		view->goto_entry = gtk_entry_new ();
		gtk_entry_set_icon_from_stock (GTK_ENTRY (view->goto_entry),
									   GTK_ENTRY_ICON_PRIMARY,
									   GTK_STOCK_JUMP_TO);
		gtk_widget_show (view->goto_entry);
		gtk_container_add (GTK_CONTAINER (vbox),
						   view->goto_entry);
					   
		gtk_widget_realize (view->goto_entry);
	}
	
	dma_data_view_goto_position_func (view);	
	gtk_entry_set_text (GTK_ENTRY (view->goto_entry), "0x");
	gtk_widget_show (view->goto_window);
	
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view->address), FALSE);
    gtk_widget_grab_focus (view->goto_entry);
    send_focus_change (view->goto_entry, TRUE);
	gtk_editable_set_position (GTK_EDITABLE (view->goto_entry), -1);
}

/* Private functions
 *---------------------------------------------------------------------------*/

static void
dma_data_view_address_size_request (DmaDataView *view,
									GtkRequisition *requisition)
{
	PangoLayout *layout;
	char text[] = "0";
	int width;
	int height;
	
	layout = gtk_widget_create_pango_layout(view->address, text);
	pango_layout_get_pixel_size(layout, &(requisition->width), &(requisition->height));
	do
	{
		text[0]++;
		pango_layout_get_pixel_size(layout, &width, &height);
		if (width > requisition->width) requisition->width = width;
		if (height > requisition->height) requisition->height = height;
	} while (text[0] == '9');
	for (text[0] = 'A'; text[0] != 'G'; text[0]++)
	{
		pango_layout_get_pixel_size(layout, &width, &height);
		if (width > requisition->width) requisition->width = width;
		if (height > requisition->height) requisition->height = height;
	}
	g_object_unref(G_OBJECT(layout));
	
	requisition->width *= sizeof(view->start) * 2;
}

static void
dma_data_view_data_size_request (DmaDataView *view,
									GtkRequisition *requisition)
{
	PangoContext *context;
	PangoFontDescription *font_desc;
	PangoFontMetrics *metrics;

	context = gtk_widget_get_pango_context (view->data);
	gtk_style_context_get (gtk_widget_get_style_context (view->data),
	                       gtk_widget_get_state_flags (view->data),
	                       "font", &font_desc, NULL);
	metrics = pango_context_get_metrics (context, font_desc,
	                                     pango_context_get_language (context));

	requisition->height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
							   pango_font_metrics_get_descent (metrics));
	requisition->width = (pango_font_metrics_get_approximate_char_width (metrics) + PANGO_SCALE - 1) / PANGO_SCALE;
	pango_font_metrics_unref (metrics);
}

static void
dma_data_view_ascii_size_request (DmaDataView *view,
									GtkRequisition *requisition)
{
	PangoContext *context;
	PangoFontDescription *font_desc;
	PangoFontMetrics *metrics;

	context = gtk_widget_get_pango_context (view->data);
	gtk_style_context_get (gtk_widget_get_style_context (view->data),
	                       gtk_widget_get_state_flags (view->data),
	                       "font", &font_desc, NULL);
	metrics = pango_context_get_metrics (context, font_desc,
	                                     pango_context_get_language (context));

	requisition->height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
							   pango_font_metrics_get_descent (metrics));
	requisition->width = (pango_font_metrics_get_approximate_char_width (metrics) + PANGO_SCALE - 1) / PANGO_SCALE;
	
	pango_font_metrics_unref (metrics);
}

static void
dma_data_view_value_changed (GtkAdjustment *adj,
                             DmaDataView   *view)
{
	gdouble value;

	value = gtk_adjustment_get_value (adj);
	view->start = ((gulong) value) - (((gulong) value) % view->bytes_by_line);
	dma_data_view_refresh (view);
}

static void
dma_data_view_populate_popup (GtkTextView *widget,
							  GtkMenu *menu,
							  DmaDataView *view)
{
	GtkWidget *menu_item;
	
	/* separator */
	menu_item = gtk_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);

	/* create goto menu_item. */
	menu_item = gtk_menu_item_new_with_mnemonic (_("_Go to address"));
	g_signal_connect (G_OBJECT (menu_item), "activate",
			  G_CALLBACK (dma_data_view_goto_activate), view);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
}

static void
dma_data_view_get_preferred_width (GtkWidget *widget,
                                   gint *minimum_width, gint *natural_width)
{
	DmaDataView *view = DMA_DATA_VIEW (widget);
	gint width;
	GtkRequisition child_requisition;
	GtkBorder css_border;

	gtk_widget_get_preferred_width (view->range, &width, NULL);

	dma_data_view_address_size_request (view, &child_requisition);
	width += child_requisition.width;
	width += ADDRESS_BORDER;

	dma_data_view_data_size_request (view, &child_requisition);
	width += child_requisition.width * view->char_by_byte;
	width += ASCII_BORDER;

	dma_data_view_ascii_size_request (view, &child_requisition);
	width += child_requisition.width;

	width += SCROLLBAR_SPACING;

	get_css_padding_and_border (widget, &css_border);
	width += css_border.left + css_border.right;

	*minimum_width = *natural_width = width;
}

static void
dma_data_view_get_preferred_height (GtkWidget *widget,
                                    gint *minimum_height, gint *natural_height)
{
	DmaDataView *view = DMA_DATA_VIEW (widget);
	gint height;
	GtkRequisition child_requisition;
	GtkBorder css_border;

	gtk_widget_get_preferred_height (view->range, &height, NULL);

	dma_data_view_address_size_request (view, &child_requisition);
	height = MAX (height, child_requisition.height);

	dma_data_view_data_size_request (view, &child_requisition);
	height = MAX (height, child_requisition.height);

	dma_data_view_ascii_size_request (view, &child_requisition);
	height = MAX (height, child_requisition.height);

	get_css_padding_and_border (widget, &css_border);
	height += css_border.top + css_border.bottom;

	*minimum_height = *natural_height = height;
}

static void
dma_data_view_size_allocate (GtkWidget *widget,
                             GtkAllocation *allocation)
{
	DmaDataView *view = DMA_DATA_VIEW (widget);
	GtkAllocation child_allocation;
	GtkRequisition range_requisition;
	GtkRequisition address_requisition;
	GtkRequisition data_requisition;
	GtkRequisition ascii_requisition;
	GtkBorder css_border;
	gint width;
	gint height;
	gint bytes_by_line;
	gint step;
	gboolean need_fill = FALSE;
	
	gtk_widget_set_allocation (widget, allocation);

	gtk_widget_get_preferred_size (view->range, &range_requisition, NULL);
	dma_data_view_address_size_request (view, &address_requisition);
	dma_data_view_data_size_request (view, &data_requisition);
	dma_data_view_ascii_size_request (view, &ascii_requisition);
	get_css_padding_and_border (widget, &css_border);
	
	/* Compute number of byte per line */
	width = allocation->width
	        - (css_border.left + css_border.right)
	        - ADDRESS_BORDER
	        - ASCII_BORDER
	        - SCROLLBAR_SPACING
	        - range_requisition.width
	        - address_requisition.width
	        - data_requisition.width * view->char_by_byte
	        - ascii_requisition.width;

	step = (ascii_requisition.width + data_requisition.width * (view->char_by_byte + 1));
	for (bytes_by_line = 1; width >= step * bytes_by_line; bytes_by_line *= 2)
	{
		width -=  step * bytes_by_line;
	}
	
	if (bytes_by_line != view->bytes_by_line)
	{
		need_fill = TRUE;
		view->bytes_by_line = bytes_by_line;
	}

	/* Compute number of line by page */
	height = allocation->height - (css_border.top + css_border.bottom);
	
	if (view->line_by_page != (height / address_requisition.height))
	{
		need_fill = TRUE;
		view->line_by_page = (height / address_requisition.height);
	}

	child_allocation.y = allocation->y + css_border.top;
	child_allocation.height = MAX (1, allocation->height
	                               - (css_border.top + css_border.bottom));

	/* Scroll bar */
	child_allocation.x = allocation->x + allocation->width
		- css_border.right - range_requisition.width;
	child_allocation.width = range_requisition.width;
	gtk_widget_size_allocate (view->range, &child_allocation);

	child_allocation.x = allocation->x + css_border.right;

	/* Address */
	child_allocation.width = address_requisition.width;
	gtk_widget_size_allocate (view->address, &child_allocation);
	child_allocation.x += child_allocation.width;

	child_allocation.x += ADDRESS_BORDER;

	/* Data */

	child_allocation.width = (view->bytes_by_line * (view->char_by_byte + 1) - 1) * data_requisition.width;
	gtk_widget_size_allocate (view->data, &child_allocation);
	child_allocation.x += child_allocation.width;

	child_allocation.x += ASCII_BORDER;

	/* Ascii */
	child_allocation.width = view->bytes_by_line * ascii_requisition.width;
	gtk_widget_size_allocate (view->ascii, &child_allocation);
	child_allocation.x += child_allocation.width;

	if (need_fill)
	{
		gdouble page_increment, page_size, upper;

		page_increment = view->bytes_by_line * (view->line_by_page - 1);
		upper = gtk_adjustment_get_upper (view->buffer_range);
                page_size = (gulong) upper % view->bytes_by_line + page_increment;

		gtk_adjustment_set_step_increment (view->buffer_range, view->bytes_by_line);
		gtk_adjustment_set_page_increment (view->buffer_range, page_increment);
		gtk_adjustment_set_page_size (view->buffer_range, page_size);

		if (view->start + page_size > upper)
		{
			view->start = upper - page_size + view->bytes_by_line - 1;
			view->start -= view->start % view->bytes_by_line;
		}
		dma_data_view_refresh (view);
	}

}

static void
dma_data_view_paint (GtkWidget    *widget,
                     cairo_t *cr)
{
	gtk_render_frame (gtk_widget_get_style_context (widget), cr, 0, 0,
	                  gtk_widget_get_allocated_width (widget),
	                  gtk_widget_get_allocated_height (widget));
}

static gint
dma_data_view_draw (GtkWidget *widget,
                    cairo_t *cr)
{
	dma_data_view_paint (widget, cr);

	(* GTK_WIDGET_CLASS (dma_data_view_parent_class)->draw) (widget, cr);
	return FALSE;
}

static void
dma_data_view_changed_notify (DmaDataBuffer* buffer, gulong lower, gulong upper, DmaDataView *view)
{
	if ((upper >= view->start) && (lower < (view->start + view->line_by_page * view->bytes_by_line)))
	{
		dma_data_view_refresh (view);
	}
}

/* Public functions
 *---------------------------------------------------------------------------*/

void
dma_data_view_goto_address (DmaDataView *view, const void *address)
{
	gtk_adjustment_set_value (view->view_range, (gdouble)((gulong)address));
}

GtkWidget *
dma_data_view_get_address (DmaDataView *view)
{
	return view->address;
}
	
GtkWidget *
dma_data_view_get_data (DmaDataView *view)
{
	return view->data;
}

GtkWidget *
dma_data_view_get_ascii (DmaDataView *view)
{
	return view->ascii;
}

void
dma_data_view_refresh (DmaDataView *view)
{
	gchar *data = "";
	gint offset;
	GtkTextIter cur;
	GtkTextMark *mark;
	GtkTextBuffer *buffer;

	/* Save all cursor offset */
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view->address));
    mark = gtk_text_buffer_get_insert (buffer);
    gtk_text_buffer_get_iter_at_mark (buffer, &cur, mark);
    offset = gtk_text_iter_get_offset (&cur);

	data = dma_data_buffer_get_address (view->buffer, view->start, view->line_by_page * view->bytes_by_line, view->bytes_by_line, sizeof(view->start) * 2);
	gtk_text_buffer_set_text (buffer, data, -1);
	g_free (data);
	
    mark = gtk_text_buffer_get_insert (buffer);
    gtk_text_buffer_get_iter_at_mark (buffer, &cur, mark);
	gtk_text_iter_set_offset (&cur, offset);
	gtk_text_buffer_move_mark_by_name (buffer, "insert", &cur);
	gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &cur);

	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view->data));
    mark = gtk_text_buffer_get_insert (buffer);
    gtk_text_buffer_get_iter_at_mark (buffer, &cur, mark);
    offset = gtk_text_iter_get_offset (&cur);

	data = dma_data_buffer_get_data (view->buffer, view->start, view->line_by_page * view->bytes_by_line, view->bytes_by_line, DMA_HEXADECIMAL_BASE);
	gtk_text_buffer_set_text (buffer, data, -1);
	g_free (data);
	
    mark = gtk_text_buffer_get_insert (buffer);
    gtk_text_buffer_get_iter_at_mark (buffer, &cur, mark);
	gtk_text_iter_set_offset (&cur, offset);
	gtk_text_buffer_move_mark_by_name (buffer, "insert", &cur);
	gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &cur);
	
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view->ascii));
    mark = gtk_text_buffer_get_insert (buffer);
    gtk_text_buffer_get_iter_at_mark (buffer, &cur, mark);
    offset = gtk_text_iter_get_offset (&cur);

	data = dma_data_buffer_get_data (view->buffer, view->start, view->line_by_page * view->bytes_by_line, view->bytes_by_line, DMA_ASCII_BASE);
	gtk_text_buffer_set_text (buffer, data, -1);
	g_free (data);
	
    mark = gtk_text_buffer_get_insert (buffer);
    gtk_text_buffer_get_iter_at_mark (buffer, &cur, mark);
	gtk_text_iter_set_offset (&cur, offset);
	gtk_text_buffer_move_mark_by_name (buffer, "insert", &cur);
	gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &cur);
	
}

/* GtkContainer functions
 *---------------------------------------------------------------------------*/

static GType
dma_data_view_child_type (GtkContainer *container)
{
	return G_TYPE_NONE;
}

static void
dma_data_view_forall (GtkContainer *container,
                gboolean      include_internals,
                GtkCallback   callback,
                gpointer      callback_data)
{
	DmaDataView *view = DMA_DATA_VIEW (container);

	g_return_if_fail (callback != NULL);

	if (include_internals)
	{
		callback (view->address, callback_data);
		callback (view->data, callback_data);
		callback (view->ascii, callback_data);
		callback (view->range, callback_data);
	}
		
}

/* GtkObject functions
 *---------------------------------------------------------------------------*/

static void
dma_data_view_destroy (GtkWidget *object)
{
	DmaDataView *view;

	g_return_if_fail (IS_DMA_DATA_VIEW (object));

	view = DMA_DATA_VIEW (object);

	gtk_widget_unparent (view->address);
	gtk_widget_destroy (view->address);
	gtk_widget_unparent (view->data);
	gtk_widget_destroy (view->data);
	gtk_widget_unparent (view->ascii);
	gtk_widget_destroy (view->ascii);
	gtk_widget_unparent (view->range);
	gtk_widget_destroy (view->range);

	if (view->goto_window)
	{
		gtk_widget_destroy (view->goto_window);
		view->goto_window = NULL;
		view->goto_entry = NULL;
	}
	
	GTK_WIDGET_CLASS (dma_data_view_parent_class)->destroy (object);
}


/* GObject functions
 *---------------------------------------------------------------------------*/

/* dispose is the first destruction step. It is used to unref object created
 * with instance_init in order to break reference counting cycles. This
 * function could be called several times. All function should still work
 * after this call. It has to called its parents.*/

static void
dma_data_view_dispose (GObject *object)
{
	DmaDataView *view = DMA_DATA_VIEW (object);

	if (view->buffer != NULL)
	{
		g_signal_handlers_disconnect_by_func (view->buffer,
						      dma_data_view_changed_notify,
						      view);
		g_object_unref (view->buffer);
		view->buffer = NULL;
	}
	
	G_OBJECT_CLASS (dma_data_view_parent_class)->dispose (object);
}

/* finalize is the last destruction step. It must free all memory allocated
 * with instance_init. It is called only one time just before releasing all
 * memory */

static void
dma_data_view_finalize (GObject *object)
{
	DmaDataView *view = DMA_DATA_VIEW (object);
	
	g_object_unref (view->address);
	g_object_unref (view->data);
	g_object_unref (view->ascii);
	g_object_unref (view->range);

	G_OBJECT_CLASS (dma_data_view_parent_class)->finalize (object);
}

static void
dma_data_view_constructed (GObject *object)
{
	DmaDataView *view = DMA_DATA_VIEW (object);
	GtkWidget* wid;

	g_signal_connect (G_OBJECT (view->buffer), 
	                  "changed_notify", 
	                  G_CALLBACK (dma_data_view_changed_notify), 
	                  view);

	wid = GTK_WIDGET (view);

	gtk_widget_set_has_window (wid, FALSE);
	gtk_widget_set_can_focus (wid, TRUE);
	gtk_widget_set_redraw_on_allocate (wid, FALSE); 	
	
	view->char_by_byte = 2;
	view->bytes_by_line = 16;
	view->line_by_page = 16;
	
	view->hadjustment = NULL;
	view->vadjustment = NULL;

	view->goto_window = NULL;
	view->goto_entry = NULL;

	view->buffer_range = GTK_ADJUSTMENT (gtk_adjustment_new (0,
											 dma_data_buffer_get_lower (view->buffer),
											 dma_data_buffer_get_upper (view->buffer)
											 ,1,4,4));
	g_signal_connect (view->buffer_range, "value_changed",
                        G_CALLBACK (dma_data_view_value_changed), view);

	wid = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, view->buffer_range);
	g_object_ref (wid);
	view->range = wid;
	gtk_widget_set_parent (wid, GTK_WIDGET (view));
	gtk_widget_show (wid);

	wid = dma_chunk_view_new ();
	g_object_ref (wid);
	gtk_widget_set_parent (wid, GTK_WIDGET (view));
	gtk_widget_set_size_request (wid, -1, 0);
	gtk_widget_show (wid);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (wid), FALSE);
	view->ascii = wid;
	view->ascii_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (wid));
	dma_chunk_view_set_scroll_adjustment (DMA_CHUNK_VIEW (wid), view->buffer_range);
	g_signal_connect (wid, "populate_popup",
                        G_CALLBACK (dma_data_view_populate_popup), view);
	
	wid = dma_chunk_view_new ();
	g_object_ref (wid);
	gtk_widget_set_parent (wid, GTK_WIDGET (view));
	gtk_widget_set_size_request (wid, -1, 0);
	gtk_widget_show (wid);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (wid), FALSE);
	view->data = wid;
	view->data_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (wid));
	dma_chunk_view_set_scroll_adjustment (DMA_CHUNK_VIEW (wid), view->buffer_range);
	g_signal_connect (wid, "populate_popup",
                        G_CALLBACK (dma_data_view_populate_popup), view);

	wid = dma_chunk_view_new ();
	g_object_ref (wid);
	gtk_widget_set_parent (wid, GTK_WIDGET (view));
	gtk_widget_set_size_request (wid, -1, 0);
	gtk_widget_show (wid);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (wid), FALSE);
	view->address = wid;
	view->adr_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (wid));
	dma_chunk_view_set_scroll_adjustment (DMA_CHUNK_VIEW (wid), view->buffer_range);
	g_signal_connect (wid, "populate_popup",
                        G_CALLBACK (dma_data_view_populate_popup), view);
}

static void
dma_data_view_set_property (GObject *object, guint prop_id,
                            const GValue *value, GParamSpec *pspec)
{
	DmaDataView *view = DMA_DATA_VIEW (object);

	switch (prop_id)
	{
		case PROP_BUFFER:
			view->buffer = g_value_dup_object (value);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
dma_data_view_get_property (GObject *object, guint prop_id,
                            GValue *value, GParamSpec *pspec)
{
	DmaDataView *view = DMA_DATA_VIEW (object);

	switch (prop_id)
	{
		case PROP_BUFFER:
			g_value_set_object (value, view->buffer);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
dma_data_view_init (DmaDataView *view)
{
	GtkStyleContext *context;

	/* Setup styling */
	context = gtk_widget_get_style_context (GTK_WIDGET (view));
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (GET_DMA_DATA_VIEW_CLASS (view)->css),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	gtk_widget_set_has_window (GTK_WIDGET (view), FALSE);
}

/* class_init intialize the class itself not the instance */

static void
dma_data_view_class_init (DmaDataViewClass * klass)
{
	GObjectClass *gobject_class;
	GtkWidgetClass   *widget_class;
	GtkContainerClass *container_class;

	static const gchar data_view_style[] =
		"* {\n"
		"   font: Monospace 10;\n"
		"}";

	gobject_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	container_class = GTK_CONTAINER_CLASS (klass);

	gobject_class->set_property = dma_data_view_set_property;
	gobject_class->get_property = dma_data_view_get_property;
	gobject_class->constructed = dma_data_view_constructed;
	gobject_class->dispose = dma_data_view_dispose;
	gobject_class->finalize = dma_data_view_finalize;

	widget_class->destroy = dma_data_view_destroy;
	widget_class->get_preferred_width = dma_data_view_get_preferred_width;
	widget_class->get_preferred_height = dma_data_view_get_preferred_height;
	widget_class->size_allocate = dma_data_view_size_allocate;
	widget_class->draw = dma_data_view_draw;
	
	container_class->forall = dma_data_view_forall;
	container_class->child_type = dma_data_view_child_type;

	properties[PROP_BUFFER] =
		g_param_spec_object ("buffer", "Buffer", "Buffer",
		                     DMA_DATA_BUFFER_TYPE,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
	g_object_class_install_properties (gobject_class, N_PROPERTIES, properties);

	klass->css = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (klass->css, data_view_style, -1, NULL);
}

/* Creation and Destruction
 *---------------------------------------------------------------------------*/

GtkWidget*
dma_data_view_new_with_buffer (DmaDataBuffer* buffer)
{
	DmaDataView *view;

	view = g_object_new (DMA_DATA_VIEW_TYPE, "buffer", buffer, NULL);
	return GTK_WIDGET (view);
}
