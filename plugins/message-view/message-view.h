/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*  (c) 2003 Johannes Schmid
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

#ifndef MESSAGE_VIEW_H
#define MESSAGE_VIEW_H

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <libanjuta/anjuta-preferences.h>
#include <libanjuta/anjuta-serializer.h>

/* Message View Properties:
Property |				Description
--------------------------------------------------
"label"			(gchararray) The label of the view, can be translated

"truncate"		(gboolean) Truncate messages
"highlight"		(gboolean) Highlite error messages
*/

G_BEGIN_DECLS

#define MESSAGE_VIEW_TYPE        (message_view_get_type ())
#define MESSAGE_VIEW(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), MESSAGE_VIEW_TYPE, MessageView))
#define MESSAGE_VIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), MESSAGE_VIEW_TYPE, MessageViewClass))
#define MESSAGE_IS_VIEW(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), MESSAGE_VIEW_TYPE))
#define MESSAGE_IS_VIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), MESSAGE_VIEW_TYPE))

typedef struct _MessageView MessageView;
typedef struct _MessageViewClass MessageViewClass;
typedef struct _MessageViewPrivate MessageViewPrivate;

struct _MessageView
{
	GtkBox parent;

	/* private */
	MessageViewPrivate* privat;
};

struct _MessageViewClass
{
	GtkBoxClass parent;
};

typedef enum
{
	MESSAGE_VIEW_SHOW_NORMAL = 1 << 0,
	MESSAGE_VIEW_SHOW_INFO = 1 << 1,
	MESSAGE_VIEW_SHOW_WARNING = 1 << 2,
	MESSAGE_VIEW_SHOW_ERROR = 1 << 3,
} MessageViewFlags;

/* Note: MessageView implements IAnjutaMessageView interface */
GType message_view_get_type (void);
GtkWidget* message_view_new (GtkWidget* popup_menu);

void message_view_next(MessageView* view);
void message_view_previous(MessageView* view);
void message_view_save(MessageView* view);
void message_view_copy(MessageView* view);
gboolean message_view_serialize (MessageView *view,
								 AnjutaSerializer *serializer);
gboolean message_view_deserialize (MessageView *view,
								   AnjutaSerializer *serializer);

MessageViewFlags message_view_get_flags (MessageView* view);
void message_view_set_flags (MessageView* view, MessageViewFlags flags);
gint message_view_get_count (MessageView* view, MessageViewFlags flags);

G_END_DECLS

#endif
