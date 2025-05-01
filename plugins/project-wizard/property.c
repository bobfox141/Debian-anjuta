/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    property.c
    Copyright (C) 2004 Sebastien Granjoux

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

/*
 * Project properties, used in middle pages
 *
 *---------------------------------------------------------------------------*/

#include <config.h>

#include "property.h"

#include <glib.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-utils.h>
#include <libanjuta/anjuta-pkg-config-chooser.h>

/*---------------------------------------------------------------------------*/

struct _NPWPage
{
	GList* properties;
	GHashTable* values;
	gchar* name;
	gchar* label;
	gchar* description;
	gint language;
	GtkWidget *widget;
};

struct _NPWProperty {
	NPWPropertyType type;
	NPWPropertyType restriction;
	NPWPropertyOptions options;
	gdouble range[3];
	gchar* label;
	gchar* description;
	gchar* defvalue;
	gint language;
	const gchar *name;
	GHashTable *values;
	GtkWidget* widget;
	GSList* items;
};

struct _NPWItem {
	gchar* name;
	gchar* label;
	gint language;
};

static const gchar* NPWPropertyTypeString[] = {
	"hidden",
	"boolean",
	"integer",
	"string",
	"list",
	"directory",
	"file",
	"icon",
	"package",
};

static const gchar* NPWPropertyRestrictionString[] = {
	"filename",
	"directory",
	"printable"
};

/* Item object
 *---------------------------------------------------------------------------*/

static NPWItem*
npw_item_new (const gchar *name, const gchar *label, gint language)
{
	NPWItem *item;

	item = g_slice_new (NPWItem);
	item->name = g_strdup (name);
	item->label = g_strdup (label);
	item->language = language;

	return item;
}

static void
npw_item_free (NPWItem *item)
{
	g_free (item->name);
	g_free (item->label);
	g_slice_free (NPWItem, item);
}

static gint
npw_item_compare (const NPWItem *a, const NPWItem *b)
{
	return g_strcmp0 (a->name, b->name);
}

static const gchar *
npw_item_get_label (const NPWItem *item)
{
	return item->language == 0 ? _(item->label) : item->label;
}

/* Property object
 *---------------------------------------------------------------------------*/

static NPWPropertyType
npw_property_type_from_string (const gchar* type)
{
	gint i;

	for (i = 0; i < NPW_LAST_PROPERTY; i++)
	{
		if (strcmp (NPWPropertyTypeString[i], type) == 0)
		{
			return (NPWPropertyType)(i + 1);
		}
	}

	return NPW_UNKNOWN_PROPERTY;
}

static NPWPropertyRestriction
npw_property_restriction_from_string (const gchar* restriction)
{

	if (restriction != NULL)
	{
		gint i;

		for (i = 0; i < NPW_LAST_RESTRICTION; i++)
		{
			if (strcmp (NPWPropertyRestrictionString[i], restriction) == 0)
			{
				return (NPWPropertyRestriction)(i + 1);
			}
		}
	}

	return NPW_NO_RESTRICTION;
}

static gint
npw_property_compare (const NPWProperty *a, const NPWProperty *b)
{
	return g_strcmp0 (a->name, b->name);
}

NPWProperty*
npw_property_new (void)
{
	NPWProperty* prop;

	prop = g_slice_new0(NPWProperty);
	prop->type = NPW_UNKNOWN_PROPERTY;
	prop->restriction = NPW_NO_RESTRICTION;
	/* value is set to NULL */

	return prop;
}

void
npw_property_free (NPWProperty* prop)
{
	if (prop->items != NULL)
	{
		g_slist_foreach (prop->items, (GFunc)npw_item_free, NULL);
		g_slist_free (prop->items);
	}
	g_free (prop->label);
	g_free (prop->description);
	g_free (prop->defvalue);
	g_slice_free (NPWProperty, prop);
}

void
npw_property_set_language (NPWProperty* prop, gint language)
{
	prop->language = language;
}

void
npw_property_set_type (NPWProperty* prop, NPWPropertyType type)
{
	prop->type = type;
}

void
npw_property_set_string_type (NPWProperty* prop, const gchar* type)
{
	npw_property_set_type (prop, npw_property_type_from_string (type));
}


NPWPropertyType
npw_property_get_type (const NPWProperty* prop)
{
	return prop->type;
}

void
npw_property_set_restriction (NPWProperty* prop, NPWPropertyRestriction restriction)
{
	prop->restriction = restriction;
}

void
npw_property_set_string_restriction (NPWProperty* prop, const gchar* restriction)
{
	npw_property_set_restriction (prop, npw_property_restriction_from_string (restriction));
}

NPWPropertyRestriction
npw_property_get_restriction (const NPWProperty* prop)
{
	return prop->restriction;
}

gboolean
npw_property_is_valid_restriction (const NPWProperty* prop)
{
	const gchar *value;

	switch (prop->restriction)
	{
	case NPW_FILENAME_RESTRICTION:
		value = npw_property_get_value (prop);

		/* First character should be letters, digit or "#$:%+,.=@^_`~" */
		if (value == NULL) return TRUE;
		if (!isalnum (*value) && (strchr ("#$:%+,.=@^_`~", *value) == NULL))
			return FALSE;

		/* Following characters should be letters, digit or "#$:%+,-.=@^_`~"
		 * or '-' or '.' */
		for (value++; *value != '\0'; value++)
		{
			if (!isalnum (*value) && (strchr ("#$:%+,-.=@^_`~", *value) == NULL))
				return FALSE;
		}
		break;
	case NPW_DIRECTORY_RESTRICTION:
		value = npw_property_get_value (prop);

		/* First character should be letters, digit or "#$:%+,.=@^_`~" or
		 * directory separator */
		if (value == NULL) return TRUE;
		if (!isalnum (*value) && (strchr ("#$:%+,.=@^_`~", *value) == NULL) && (*value != G_DIR_SEPARATOR))
			return FALSE;

		/* Following characters should be letters, digit or "#$:%+,-.=@^_`~"
		 * or directory separator */
		for (value++; *value != '\0'; value++)
		{
			if (!isalnum (*value)
			    && (strchr ("#$:%+,-.=@^_`~", *value) == NULL)
			    && (*value != G_DIR_SEPARATOR))
				return FALSE;
		}
		break;
	case NPW_PRINTABLE_RESTRICTION:
		value = npw_property_get_value (prop);

		if (value == NULL) return TRUE;

		/* All characters should be ASCII printable character */
		for (value++; *value != '\0'; value++)
		{
			if (!g_ascii_isprint (*value)) return FALSE;
		}
		break;
	default:
		break;
	}

	return TRUE;
}

void
npw_property_set_name (NPWProperty* prop, const gchar* name, NPWPage *page)
{
	gchar *key;

	prop->values = page->values;
	if (g_hash_table_lookup_extended (prop->values, name, (gpointer *)&key, NULL))
	{
		prop->name = key;
	}
	else
	{
		prop->name = g_strdup (name);
		g_hash_table_insert (prop->values, (gpointer)prop->name, NULL);
	}
}

const gchar*
npw_property_get_name (const NPWProperty* prop)
{
	return prop->name;
}

void
npw_property_set_label (NPWProperty* prop, const gchar* label)
{
	g_free (prop->label);
	prop->label = g_strdup (label);
}

const gchar*
npw_property_get_label (const NPWProperty* prop)
{
	return prop->language == 0 ? _(prop->label) : prop->label;
}

void
npw_property_set_description (NPWProperty* prop, const gchar* description)
{
	g_free (prop->description);
	prop->description = g_strdup (description);
}

const gchar*
npw_property_get_description (const NPWProperty* prop)
{
	return prop->language == 0 ? _(prop->description) : prop->description;
}

static void
cb_browse_button_clicked (GtkButton *button, NPWProperty* prop)
{
	GtkWidget *dialog;
	gchar *path;

	switch (prop->type)
	{
	case NPW_DIRECTORY_PROPERTY:
		dialog = gtk_file_chooser_dialog_new (_("Select directory"),
												 GTK_WINDOW (gtk_widget_get_ancestor (prop->widget, GTK_TYPE_WINDOW)),
												 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
												 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      							 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      							 NULL);
		path = g_strdup(gtk_entry_get_text(GTK_ENTRY(prop->widget)));
		while (g_file_test(path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR) == FALSE)
		{
			char* tmp = g_path_get_dirname(path);
			g_free(path);
			path = tmp;
		}
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), path);
		g_free(path);
		break;
	case NPW_FILE_PROPERTY:
		dialog = gtk_file_chooser_dialog_new (_("Select file"),
												 GTK_WINDOW (gtk_widget_get_ancestor (prop->widget, GTK_TYPE_WINDOW)),
												 GTK_FILE_CHOOSER_ACTION_SAVE,
												 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      							 GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				      							 NULL);
		break;
	default:
		g_return_if_reached ();
	}

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gchar* name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		gtk_entry_set_text (GTK_ENTRY (prop->widget), name);
		g_free (name);
	}
	gtk_widget_destroy (dialog);
}

static void
cb_preview_update (GtkFileChooser *fc,
		   GtkImage *preview)
{
	char *filename;
	GdkPixbuf *pixbuf;

	filename = gtk_file_chooser_get_preview_filename (fc);
	if (filename) {
		pixbuf = gdk_pixbuf_new_from_file (filename, NULL);

		gtk_file_chooser_set_preview_widget_active (fc, pixbuf != NULL);

		if (pixbuf) {
			gtk_image_set_from_pixbuf (preview, pixbuf);
			g_object_unref (pixbuf);
		}

		g_free (filename);
	}
}

static void
cb_icon_button_clicked (GtkButton *button, NPWProperty* prop)
{
	GtkWidget *dialog;
	GtkFileFilter *filter;
	GtkWidget *preview;
	int res;

	dialog = gtk_file_chooser_dialog_new (_("Select an Image File"),
											GTK_WINDOW (gtk_widget_get_ancestor (prop->widget, GTK_TYPE_WINDOW)),
					      					GTK_FILE_CHOOSER_ACTION_OPEN,
					      					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					      					NULL);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
	    									PACKAGE_PIXMAPS_DIR);
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pixbuf_formats (filter);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

	preview = gtk_image_new ();
	gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (dialog),
	    								    preview);
	g_signal_connect (dialog, "update-preview",
			  								G_CALLBACK (cb_preview_update), preview);

	res = gtk_dialog_run (GTK_DIALOG (dialog));

	if (res == GTK_RESPONSE_ACCEPT) {
    	gchar *filename;

    	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		gtk_image_set_from_file (GTK_IMAGE (gtk_button_get_image (GTK_BUTTON (prop->widget))), filename);
		gtk_button_set_label (GTK_BUTTON (prop->widget), filename == NULL ? _("Choose Icon") : NULL);
	}

	gtk_widget_destroy (dialog);
}

GtkWidget*
npw_property_create_widget (NPWProperty* prop)
{
	GtkWidget* widget = NULL;
	GtkWidget* entry;
	const gchar* value;

	value = npw_property_get_value (prop);
	switch (prop->type)
	{
	case NPW_BOOLEAN_PROPERTY:
		entry = gtk_check_button_new ();
		if (value)
		{
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (entry),
			                              (gboolean)atoi (value));
		}
		break;
	case NPW_INTEGER_PROPERTY:
		if (prop->range[1] == 0) prop->range[1] = 10000;
		if (prop->range[2] == 0) prop->range[2] = 1;
		entry = gtk_spin_button_new_with_range (prop->range[0], prop->range[1], prop->range[2]);
		if (value)
		{
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (entry), atoi (value));
		}
		break;
	case NPW_STRING_PROPERTY:
		entry = gtk_entry_new ();
		if (value) gtk_entry_set_text (GTK_ENTRY (entry), value);
		break;
	case NPW_DIRECTORY_PROPERTY:
	case NPW_FILE_PROPERTY:
		if ((prop->options & NPW_EXIST_SET_OPTION) && !(prop->options & NPW_EXIST_OPTION))
		{
			GtkWidget *button;

			// Use an entry box and a browse button as GtkFileChooserButton
			// allow to select only existing file
			widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);

			entry = gtk_entry_new ();
			if (value) gtk_entry_set_text (GTK_ENTRY (entry), value);
			gtk_widget_set_hexpand (entry, TRUE);
			gtk_container_add (GTK_CONTAINER (widget), entry);

			button = gtk_button_new_from_stock (GTK_STOCK_OPEN);
			g_signal_connect (button, "clicked", G_CALLBACK (cb_browse_button_clicked), prop);
			gtk_container_add (GTK_CONTAINER (widget), button);
			gtk_box_set_child_packing (GTK_BOX (widget), button, FALSE, TRUE, 0, GTK_PACK_END);
		}
		else
		{
			if (prop->type == NPW_DIRECTORY_PROPERTY)
			{
				entry = gtk_file_chooser_button_new (_("Choose directory"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
			}
			else
			{
				entry = gtk_file_chooser_button_new (_("Choose file"),
												 GTK_FILE_CHOOSER_ACTION_OPEN);
			}

			if (value)
			{
				GFile *file = g_file_parse_name (value);
				gchar* uri = g_file_get_uri (file);
				gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (entry), uri);
				g_free (uri);
				g_object_unref (file);
			}
		}
		break;
	case NPW_ICON_PROPERTY:
		{
			GtkWidget *image;

			image = gtk_image_new ();
			entry = gtk_button_new ();
			if (value)
			{
				gtk_image_set_from_file (GTK_IMAGE (image), value);
			}
			else
			{
				gtk_button_set_label (GTK_BUTTON(entry), _("Choose Icon"));
			}
			gtk_button_set_image (GTK_BUTTON (entry), image);
			g_signal_connect (entry, "clicked", G_CALLBACK (cb_icon_button_clicked), prop);
		}
		break;
	case NPW_LIST_PROPERTY:
	{
		GtkWidget *child;
		GSList* node;
		gboolean get_value = FALSE;

		entry = gtk_combo_box_text_new_with_entry ();
		for (node = prop->items; node != NULL; node = node->next)
		{
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (entry), npw_item_get_label((NPWItem *)node->data));
			if ((value != NULL) && !get_value && (strcmp (value, ((NPWItem *)node->data)->name) == 0))
			{
				value = npw_item_get_label ((NPWItem *)node->data);
				get_value = TRUE;
			}
		}
		child = gtk_bin_get_child (GTK_BIN (entry));
		if (!(prop->options & NPW_EDITABLE_OPTION))
		{
			gtk_editable_set_editable (GTK_EDITABLE (child), FALSE);
		}
		if (value) gtk_entry_set_text (GTK_ENTRY (child), value);
		break;
	}
	case NPW_PACKAGE_PROPERTY:
	{
		GtkWidget *scroll_window;
		scroll_window = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll_window), GTK_SHADOW_IN);

		entry = anjuta_pkg_config_chooser_new ();
		anjuta_pkg_config_chooser_show_active_column (ANJUTA_PKG_CONFIG_CHOOSER (entry), TRUE);
		gtk_container_add (GTK_CONTAINER (scroll_window), entry);

		widget = scroll_window;
		break;
	}
	default:
		return NULL;
	}
	prop->widget = entry;


	return widget == NULL ? entry : widget;
}

void
npw_property_set_widget (NPWProperty* prop, GtkWidget* widget)
{
	prop->widget = widget;
}

GtkWidget*
npw_property_get_widget (const NPWProperty* prop)
{
	return prop->widget;
}

void
npw_property_set_default (NPWProperty* prop, const gchar* value)
{
	/* Check if the default property is valid */
	if (value && (prop->options & NPW_EXIST_SET_OPTION) && !(prop->options & NPW_EXIST_OPTION))
	{
		gchar *expand_value = anjuta_util_shell_expand (value);
		/* a file or directory with the same name shouldn't exist */
		if (g_file_test (expand_value, G_FILE_TEST_EXISTS))
		{
			char* buffer;
			guint i;

			/* Allocate memory for the string and a decimal number */
			buffer = g_new (char, strlen(value) + 8);
			/* Give up after 1000000 tries */
			for (i = 1; i < 1000000; i++)
			{
				sprintf(buffer,"%s%d",value, i);
				if (!g_file_test (buffer, G_FILE_TEST_EXISTS)) break;
			}
			g_free (prop->defvalue);
			prop->defvalue = buffer;
			g_free (expand_value);

			return;
		}
		g_free (expand_value);
	}
	/* This function could be used with value = defvalue to only check
	 * the default property */
	if (prop->defvalue != value)
	{
		g_free (prop->defvalue);
		prop->defvalue = (value == NULL) ? NULL : g_strdup (value);
	}
}

gboolean
npw_property_set_range (NPWProperty* prop, NPWPropertyRangeMark mark, const gchar* value)
{
	char *end;
	double d;
	gboolean ok;

	d = strtod (value,  &end);
	ok = (*end == ':')  || (*end == '\0');
	if (ok) prop->range[mark] = d;

	return ok;
}

static gboolean
npw_property_set_value_from_widget (NPWProperty* prop)
{
	gchar* alloc_value = NULL;
	const gchar* value = NULL;
	GList* packages;
	GList* node;
	GString* str_value;
	gboolean change;
	const gchar *old_value;

	switch (prop->type)
	{
	case NPW_INTEGER_PROPERTY:
		alloc_value = g_strdup_printf("%d", gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (prop->widget)));
		value = alloc_value;
		break;
	case NPW_BOOLEAN_PROPERTY:
		alloc_value = g_strdup_printf("%d", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prop->widget)));
		value = alloc_value;
		break;
	case NPW_STRING_PROPERTY:
		value = gtk_entry_get_text (GTK_ENTRY (prop->widget));
		break;
	case NPW_DIRECTORY_PROPERTY:
	case NPW_FILE_PROPERTY:
		if ((prop->options & NPW_EXIST_SET_OPTION) && !(prop->options & NPW_EXIST_OPTION))
		{
			/* a GtkEntry is used in this case*/
			value = gtk_entry_get_text (GTK_ENTRY (prop->widget));
			/* Expand ~ and environment variable */
			alloc_value = anjuta_util_shell_expand (value);
			value = alloc_value;
		}
		else
		{
			alloc_value = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (prop->widget));
			value = alloc_value;
		}
		break;
	case NPW_ICON_PROPERTY:
		g_object_get (G_OBJECT (gtk_button_get_image (GTK_BUTTON (prop->widget))), "file", &alloc_value, NULL);
		value = alloc_value;
		break;
	case NPW_LIST_PROPERTY:
	{
		GSList* node;

		value = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (prop->widget))));
		for (node = prop->items; node != NULL; node = node->next)
		{
			if (strcmp (value, npw_item_get_label((NPWItem *)node->data)) == 0)
			{
				value = ((NPWItem *)node->data)->name;
				break;
			}
		}
		break;
	}
	case NPW_PACKAGE_PROPERTY:
		packages =
			anjuta_pkg_config_chooser_get_active_packages (ANJUTA_PKG_CONFIG_CHOOSER (prop->widget));
		str_value = NULL;
		for (node = packages; node != NULL; node = g_list_next (node))
		{
			if (str_value)
			{
				g_string_append_printf (str_value, " %s", (gchar*) node->data);
			}
			else
				str_value = g_string_new (node->data);
		}
		if (str_value != NULL)
		{
			value = str_value->str;
			g_string_free (str_value, FALSE);
		}
		g_list_foreach (packages, (GFunc) g_free, NULL);
		g_list_free (packages);
		break;
	default:
		/* Hidden property */
		value = prop->defvalue;
		break;
	}

	/* Update value */
	old_value = g_hash_table_lookup (prop->values, prop->name);
	change = g_strcmp0 (value, old_value) != 0;
	if (change) g_hash_table_insert (prop->values, g_strdup (prop->name), g_strdup (value));
	if (alloc_value != NULL) g_free (alloc_value);

	return change;
}

gboolean
npw_property_update_value_from_widget (NPWProperty* prop)
{
	return npw_property_set_value_from_widget (prop);
}

gboolean
npw_property_save_value_from_widget (NPWProperty* prop)
{
	return npw_property_set_value_from_widget (prop);
}

gboolean
npw_property_remove_value (NPWProperty* prop)
{
	if (g_hash_table_lookup (prop->values, prop->name) != NULL)
	{
		g_hash_table_insert (prop->values, g_strdup(prop->name), NULL);

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

const char*
npw_property_get_value (const NPWProperty* prop)
{
	const gchar *value;

	value = g_hash_table_lookup (prop->values, prop->name);
	if (value == NULL)
	{
		value = prop->defvalue;
	}

	return value;
}

gboolean
npw_property_add_list_item (NPWProperty* prop, const gchar* name, const gchar* label, gint language)
{
	NPWItem* item;
	GSList *find;

	item = npw_item_new (name, label, language);
	find = g_slist_find_custom (prop->items, item, (GCompareFunc)npw_item_compare);
	if (find != NULL)
	{
		NPWItem* old_item = (NPWItem *)find->data;

		if (old_item->language <= item->language)
		{
			npw_item_free ((NPWItem *)find->data);
			find->data = item;
		}
		else
		{
			npw_item_free (item);
		}
	}
	else
	{
		prop->items = g_slist_append (prop->items, item);
	}

	return TRUE;
}

void
npw_property_set_mandatory_option (NPWProperty* prop, gboolean value)
{
	if (value)
	{
		prop->options |= NPW_MANDATORY_OPTION;
	}
	else
	{
		prop->options &= ~NPW_MANDATORY_OPTION;
	}
}

void
npw_property_set_summary_option (NPWProperty* prop, gboolean value)
{
	if (value)
	{
		prop->options |= NPW_SUMMARY_OPTION;
	}
	else
	{
		prop->options &= ~NPW_SUMMARY_OPTION;
	}
}

void
npw_property_set_editable_option (NPWProperty* prop, gboolean value)
{
	if (value)
	{
		prop->options |= NPW_EDITABLE_OPTION;
	}
	else
	{
		prop->options &= ~NPW_EDITABLE_OPTION;
	}
}

NPWPropertyOptions
npw_property_get_options (const NPWProperty* prop)
{
	return prop->options;
}

void
npw_property_set_exist_option (NPWProperty* prop, NPWPropertyBooleanValue value)
{
	switch (value)
	{
	case NPW_TRUE:
		prop->options |= NPW_EXIST_OPTION | NPW_EXIST_SET_OPTION;
		break;
	case NPW_FALSE:
		prop->options &= ~NPW_EXIST_OPTION;
		prop->options |= NPW_EXIST_SET_OPTION;
		npw_property_set_default (prop, prop->defvalue);
		break;
	case NPW_DEFAULT:
		prop->options &= ~(NPW_EXIST_OPTION | NPW_EXIST_SET_OPTION);
		break;
	}
}

NPWPropertyBooleanValue
npw_property_get_exist_option (const NPWProperty* prop)
{
	return prop->options & NPW_EXIST_SET_OPTION ? (prop->options & NPW_EXIST_OPTION ? NPW_TRUE : NPW_FALSE) : NPW_DEFAULT;
}

/* Page object = list of properties
 *---------------------------------------------------------------------------*/

NPWPage*
npw_page_new (GHashTable* value)
{
	NPWPage* page;

	page = g_new0(NPWPage, 1);
	page->values = value;

	return page;
}

void
npw_page_free (NPWPage* page)
{
	g_return_if_fail (page != NULL);

	g_free (page->name);
	g_free (page->label);
	g_free (page->description);
	g_list_foreach (page->properties, (GFunc)npw_property_free, NULL);
	g_list_free (page->properties);
	g_free (page);
}

void
npw_page_set_name (NPWPage* page, const gchar* name)
{
	g_free (page->name);
	page->name = g_strdup (name);
}

const gchar*
npw_page_get_name (const NPWPage* page)
{
	return page->name;
}

gboolean
npw_page_set_language (NPWPage *page, gint language)
{
	if (page->language <= language)
	{
		page->language = language;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void
npw_page_set_label (NPWPage* page, const gchar* label)
{
	g_free (page->label);
	page->label = g_strdup (label);
}

const gchar*
npw_page_get_label (const NPWPage* page)
{
	return page->language == 0 ? _(page->label) : page->label;
}

void
npw_page_set_description (NPWPage* page, const gchar* description)
{
	g_free (page->description);
	page->description = g_strdup (description);
}

const gchar*
npw_page_get_description (const NPWPage* page)
{
	return page->language == 0 ? _(page->description) : page->description;
}

void
npw_page_set_widget (NPWPage* page, GtkWidget *widget)
{
	page->widget = widget;
}

GtkWidget*
npw_page_get_widget (const NPWPage* page)
{
	return page->widget;
}

void
npw_page_foreach_property (const NPWPage* page, GFunc func, gpointer data)
{
	g_list_foreach (page->properties, func, data);
}

NPWProperty *
npw_page_add_property (NPWPage* page, NPWProperty *prop)
{
	GList *find;

	find = g_list_find_custom (page->properties, prop, (GCompareFunc)npw_property_compare);
	if (find == NULL)
	{
		page->properties = g_list_append (page->properties, prop);
	}
	else
	{
		NPWProperty* old_prop = (NPWProperty *)find->data;

		if (old_prop->language <= prop->language)
		{
			npw_property_free (old_prop);
			find->data = prop;
		}
		else
		{
			npw_property_free (prop);
			prop = old_prop;
		}
	}

	return prop;
}
