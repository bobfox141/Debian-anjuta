/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) Johannes Schmid 2007 <jhs@gnome.org>
 * 
 * anjuta is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * anjuta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with anjuta.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _SEARCH_BOX_H_
#define _SEARCH_BOX_H_

#include <gtk/gtk.h>
#include "anjuta-docman.h"

G_BEGIN_DECLS

#define SEARCH_TYPE_BOX             (search_box_get_type ())
#define SEARCH_BOX(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEARCH_TYPE_BOX, SearchBox))
#define SEARCH_BOX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SEARCH_TYPE_BOX, SearchBoxClass))
#define SEARCH_IS_BOX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEARCH_TYPE_BOX))
#define SEARCH_IS_BOX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SEARCH_TYPE_BOX))
#define SEARCH_BOX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SEARCH_TYPE_BOX, SearchBoxClass))

typedef struct _SearchBoxClass SearchBoxClass;
typedef struct _SearchBox SearchBox;
typedef struct _SearchBoxPrivate SearchBoxPrivate;

struct _SearchBoxClass
{
	GtkBoxClass parent_class;
};

struct _SearchBox
{
	GtkBox parent_instance;

	SearchBoxPrivate* priv;
};

GType search_box_get_type (void);
GtkWidget* search_box_new (AnjutaDocman* docman);

void search_box_fill_search_focus (SearchBox* search_box, gboolean on_replace);
void search_box_grab_line_focus (SearchBox* search_box);
void search_box_hide (SearchBox* search_box);
void search_box_set_replace (SearchBox* object, gboolean replace);
gboolean search_box_incremental_search (SearchBox* search_box,
                                        gboolean search_forward,
                                        gboolean search_next,
                                        gboolean wrap);
void search_box_highlight_all (SearchBox * search_box);

void search_box_clear_highlight (SearchBox * search_box);
void search_box_toggle_highlight (SearchBox * search_box, gboolean status);
void search_box_toggle_case_sensitive (SearchBox * search_box, gboolean status);
void search_box_toggle_regex (SearchBox * search_box, gboolean status);

const gchar* search_box_get_search_string (SearchBox* search_box);
void search_box_set_search_string (SearchBox* search_box, const gchar* search);
const gchar* search_box_get_replace_string (SearchBox* search_box);
void search_box_set_replace_string (SearchBox* search_box, const gchar* replace);

void search_box_session_load (SearchBox* search_box, AnjutaSession* session);
void search_box_session_save (SearchBox* search_box, AnjutaSession* session);

G_END_DECLS

#endif /* _SEARCH_BOX_H_ */
