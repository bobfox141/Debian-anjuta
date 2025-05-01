/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*  window.c
 *	Copyright (C) 2006 Armin Burgmeier
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *	GNU Library General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "window.h"
#include "transform.h"
#include "validator.h"
#include "element-editor.h"

#include <libanjuta/anjuta-plugin.h>
#include <libanjuta/interfaces/ianjuta-project-chooser.h>
#include <stdlib.h>
#include <glib.h>

#include <ctype.h>

#define BUILDER_FILE PACKAGE_DATA_DIR "/glade/anjuta-class-gen-plugin.ui"

#define CC_HEADER_TEMPLATE PACKAGE_DATA_DIR "/class-templates/cc-header.tpl"
#define CC_SOURCE_TEMPLATE PACKAGE_DATA_DIR "/class-templates/cc-source.tpl"

#define GO_HEADER_TEMPLATE PACKAGE_DATA_DIR "/class-templates/go-header.tpl"
#define GO_SOURCE_TEMPLATE PACKAGE_DATA_DIR "/class-templates/go-source.tpl"

#define PY_SOURCE_TEMPLATE PACKAGE_DATA_DIR "/class-templates/py-source.tpl"

#define JS_SOURCE_TEMPLATE PACKAGE_DATA_DIR "/class-templates/js-source.tpl"

#define VALA_SOURCE_TEMPLATE PACKAGE_DATA_DIR "/class-templates/vala-source.tpl"

typedef struct _CgWindowPrivate CgWindowPrivate;
struct _CgWindowPrivate
{
	GtkBuilder *bxml;
	GtkWidget *window;

	CgElementEditor *editor_cc;

	CgElementEditor *editor_go_members;
	CgElementEditor *editor_go_properties;
	CgElementEditor *editor_go_signals;

	CgElementEditor *editor_py_methods;
	CgElementEditor *editor_py_constvars;

	CgElementEditor *editor_js_methods;
	CgElementEditor *editor_js_variables;
	CgElementEditor *editor_js_imports;

	CgElementEditor *editor_vala_methods;
	CgElementEditor *editor_vala_properties;
	CgElementEditor *editor_vala_signals;

	CgValidator *validator;
};

#define CG_WINDOW_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE( \
		(o), \
		CG_TYPE_WINDOW, \
		CgWindowPrivate \
	))

enum {
	PROP_0,

	/* Construct only */
	PROP_BUILDER_XML
};

static GObjectClass *parent_class = NULL;

static const gchar *CC_SCOPE_LIST[] =
{
	"public",
	"protected",
	"private",
	NULL
};

static const gchar *CC_IMPLEMENTATION_LIST[] =
{
	"normal",
	"static",
	"virtual",
	NULL
};

static const gchar *GO_SCOPE_LIST[] =
{
	"public",
	"private",
	NULL
};

static const gchar *GO_PARAMSPEC_LIST[] =
{
	N_("Guess from type"),
	"g_param_spec_object",
	"g_param_spec_pointer",
	"g_param_spec_enum",
	"g_param_spec_flags",
	"g_param_spec_boxed",
	NULL
};

static const CgElementEditorFlags GO_PROPERTY_FLAGS[] =
{
	{ "G_PARAM_READABLE", "R" },
	{ "G_PARAM_WRITABLE", "W" },
	{ "G_PARAM_CONSTRUCT", "C" },
	{ "G_PARAM_CONSTRUCT_ONLY", "CO" },
	{ "G_PARAM_LAX_VALIDATION", "LV" },
	{ "G_PARAM_STATIC_NAME", "SNA" },
	{ "G_PARAM_STATIC_NICK", "SNI" },
	{ "G_PARAM_STATIC_BLURB", "SBL" },
	{ NULL, NULL }
};

static const CgElementEditorFlags GO_SIGNAL_FLAGS[] =
{
	{ "G_SIGNAL_RUN_FIRST", "RF" },
	{ "G_SIGNAL_RUN_LAST", "RL" },
	{ "G_SIGNAL_RUN_CLEANUP", "RC" },
	{ "G_SIGNAL_NO_RECURSE", "NR" },
	{ "G_SIGNAL_DETAILED", "D" },
	{ "G_SIGNAL_ACTION", "A" },
	{ "G_SIGNAL_NO_HOOKS", "NH" },
	{ NULL, NULL }
};

static const gchar *VALA_BOOLEAN_LIST[] =
{
	"Yes",
	"No",
	NULL
};

static const gchar *VALA_METHSIG_SCOPE_LIST[] =
{
	"public",
	"private",
	"protected",
	NULL
}
;
static const gchar *VALA_PROP_SCOPE_LIST[] =
{
	"public",
	"protected",
	"internal",
	NULL
};

#if 0
static void
cg_window_browse_button_clicked_cb (GtkButton *button,
                                    gpointer user_data)
{
	GtkWidget *entry;
	GtkFileChooserDialog *dialog;
	const gchar *text;
	gchar *filename;

	entry = GTK_WIDGET (user_data);

	dialog = GTK_FILE_CHOOSER_DIALOG (
		gtk_file_chooser_dialog_new (
			"Select A File", /* TODO: Better context for caption */
			GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL));

	gtk_file_chooser_set_do_overwrite_confirmation (
		GTK_FILE_CHOOSER(dialog), TRUE);

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	if (text != NULL)
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), text);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	{
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		gtk_entry_set_text (GTK_ENTRY (entry), filename);
		g_free (filename);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}
#endif

static gchar *
cg_window_fetch_string (CgWindow *window,
                        const gchar *id)
{
	GtkWidget *widget;
	CgWindowPrivate *priv;

	priv = CG_WINDOW_PRIVATE (window);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->bxml, id));

	g_return_val_if_fail (widget != NULL, NULL);

	if (GTK_IS_ENTRY (widget))
		return g_strdup (gtk_entry_get_text(GTK_ENTRY(widget)));
	else if (GTK_IS_COMBO_BOX (widget))
	{
 		GtkTreeIter iter;

		if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter))
		{
      		GtkTreeModel *model;
			gchar *text;

			model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
			g_return_val_if_fail (GTK_IS_LIST_STORE (model), NULL);

			gtk_tree_model_get (model, &iter, 0, &text, -1);

			return text;
		}
		else
		{
			return NULL;
		}
	}
	else
		return NULL;
}

static gint
cg_window_fetch_integer (CgWindow *window,
                         const gchar *id)
{
	GtkWidget *widget;
	CgWindowPrivate *priv;

	priv = CG_WINDOW_PRIVATE(window);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->bxml, id));

	g_return_val_if_fail(widget != NULL, 0);

	if (GTK_IS_SPIN_BUTTON(widget))
		return gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( widget));
	else if (GTK_IS_ENTRY (widget))
		return strtol (gtk_entry_get_text (GTK_ENTRY (widget)), NULL, 0);
	else if (GTK_IS_COMBO_BOX (widget))
		return gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	else
		return 0;
}

static gboolean
cg_window_fetch_boolean (CgWindow *window,
                         const gchar *id)
{
	GtkWidget *widget;
	CgWindowPrivate *priv;

	priv = CG_WINDOW_PRIVATE (window);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->bxml, id));

	g_return_val_if_fail (widget != NULL, FALSE);

	if (GTK_IS_TOGGLE_BUTTON (widget))
		return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	else
		return FALSE;
}

static void
cg_window_set_heap_value (CgWindow *window,
                          GHashTable *values,
                          GType type,
                          const gchar *name,
                          const gchar *id)
{
	gint int_value;
	gchar *text;

	switch (type)
	{
	case G_TYPE_STRING:
		text = cg_window_fetch_string (window, id);
		g_hash_table_insert (values, (gchar*)name, text);
		break;
	case G_TYPE_INT:
		int_value = cg_window_fetch_integer (window, id);
		g_hash_table_insert (values, (gchar*)name, g_strdup_printf ("%d", int_value));
		break;
	case G_TYPE_BOOLEAN:
		text = g_strdup (cg_window_fetch_boolean (window, id) ? "1" : "0");
		g_hash_table_insert (values, (gchar*)name, text);
		break;
	default:
		break;
	}
}

static void
cg_window_validate_cc (CgWindow *window)
{
	CgWindowPrivate *priv;
	priv = CG_WINDOW_PRIVATE(window);

	if (priv->validator != NULL) g_object_unref (G_OBJECT (priv->validator));

	priv->validator = cg_validator_new (
		GTK_WIDGET (gtk_builder_get_object (priv->bxml, "create_button")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "cc_name")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "header_file")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "source_file")), NULL);
}

static void
cg_window_validate_go (CgWindow *window)
{
	CgWindowPrivate *priv;
	priv = CG_WINDOW_PRIVATE (window);

	if (priv->validator != NULL) g_object_unref (G_OBJECT (priv->validator));

	priv->validator = cg_validator_new (
		GTK_WIDGET (gtk_builder_get_object (priv->bxml, "create_button")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "go_name")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "go_prefix")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "go_type")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "go_func_prefix")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "header_file")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "source_file")), NULL);
}

static void
cg_window_validate_py (CgWindow *window)
{
	CgWindowPrivate *priv;
	priv = CG_WINDOW_PRIVATE (window);

	if (priv->validator != NULL) g_object_unref (G_OBJECT (priv->validator));

	priv->validator = cg_validator_new (
		GTK_WIDGET (gtk_builder_get_object (priv->bxml, "create_button")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "py_name")), NULL);
}

static void
cg_window_validate_js (CgWindow *window)
{
	CgWindowPrivate *priv;
	priv = CG_WINDOW_PRIVATE (window);

	if (priv->validator != NULL) g_object_unref (G_OBJECT (priv->validator));

	priv->validator = cg_validator_new (
		GTK_WIDGET (gtk_builder_get_object (priv->bxml, "create_button")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "js_name")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "source_file")), NULL);
}

static void
cg_window_validate_vala (CgWindow *window)
{
	CgWindowPrivate *priv;
	priv = CG_WINDOW_PRIVATE (window);

	if (priv->validator != NULL) g_object_unref (G_OBJECT (priv->validator));

	priv->validator = cg_validator_new (
		GTK_WIDGET (gtk_builder_get_object (priv->bxml, "create_button")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "vala_name")),
		GTK_ENTRY (gtk_builder_get_object (priv->bxml, "source_file")), NULL);
}


static void
cg_window_header_file_entry_set_sensitive (gpointer user_data, gboolean sensitive)
{
	CgWindow *window;
	CgWindowPrivate *priv;

	GtkWidget *file_header;

	window = CG_WINDOW (user_data);
	priv = CG_WINDOW_PRIVATE (window);

	file_header = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "header_file"));

	gtk_widget_set_sensitive (file_header, sensitive);
}

static void
cg_window_top_notebook_switch_page_cb (G_GNUC_UNUSED GtkNotebook *notebook,
                                       G_GNUC_UNUSED GtkWidget *page,
                                       guint page_num,
                                       gpointer user_data)
{
	CgWindow *window;
	window = CG_WINDOW(user_data);

	switch(page_num)
	{
	case 0:
		cg_window_header_file_entry_set_sensitive (user_data, TRUE);
		cg_window_validate_cc (window);
		break;
	case 1:
		cg_window_header_file_entry_set_sensitive (user_data, TRUE);
		cg_window_validate_go (window);
		break;
	case 2: /* Python */
		cg_window_header_file_entry_set_sensitive (user_data, FALSE);
		cg_window_validate_py (window);
		break;
	case 3: /* JavaScript */
		cg_window_header_file_entry_set_sensitive (user_data, FALSE);
		cg_window_validate_js (window);
		break;
	case 4: /* Vala */
		cg_window_header_file_entry_set_sensitive (user_data, FALSE);
		cg_window_validate_vala (window);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static gchar *
cg_window_class_name_to_file_name (const gchar *class_name)
{
	return cg_transform_custom_c_type (class_name, FALSE, '-');
}

static void
cg_window_add_project_parent_changed_cb (GtkWidget *project_combo, gpointer user_data)
{
	CgWindow *window;
	CgWindowPrivate *priv;
	gboolean active = TRUE;
	GtkWidget *widget;

	window = CG_WINDOW (user_data);
	priv = CG_WINDOW_PRIVATE (window);

	widget = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "add_project"));
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
	{
		GFile *file;
		widget = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "add_project_parent"));

		file = ianjuta_project_chooser_get_selected (IANJUTA_PROJECT_CHOOSER (widget), NULL);
		active = file != NULL;
	}

	widget = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "create_button"));
	gtk_widget_set_sensitive (widget, active);
}

static void
cg_window_add_project_toggled_cb (GtkToggleButton *button,
                                  gpointer user_data)
{
	CgWindow *window;
	CgWindowPrivate *priv;
	GtkWidget *widget;
	gboolean sensitive;

	window = CG_WINDOW (user_data);
	priv = CG_WINDOW_PRIVATE (window);

	sensitive = gtk_toggle_button_get_active (button);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "add_repository"));
	gtk_widget_set_sensitive (widget, sensitive);
	if (!sensitive)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget),
		                              FALSE);
	}
	widget = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "add_project_parent"));
	gtk_widget_set_sensitive (widget, sensitive);

	cg_window_add_project_parent_changed_cb (NULL, window);
}

static void
cg_window_cc_name_changed_cb (GtkEntry *entry,
                              gpointer user_data)
{
	CgWindow *window;
	CgWindowPrivate *priv;

	GtkWidget *file_header;
	GtkWidget *file_source;
	gchar* str_filebase;
	gchar* str_fileheader;
	gchar* str_filesource;

	window = CG_WINDOW (user_data);
	priv = CG_WINDOW_PRIVATE (window);

	file_header = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "header_file"));
	file_source = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "source_file"));

	str_filebase = cg_window_class_name_to_file_name (
		gtk_entry_get_text (GTK_ENTRY (entry)));

	str_fileheader = g_strconcat (str_filebase, ".h", NULL);
	str_filesource = g_strconcat (str_filebase, ".cc", NULL);
	g_free (str_filebase);

	gtk_entry_set_text (GTK_ENTRY (file_header), str_fileheader);
	gtk_entry_set_text (GTK_ENTRY (file_source), str_filesource);

	g_free (str_fileheader);
	g_free (str_filesource);
}

static void
cg_window_go_name_changed_cb (GtkEntry *entry,
                              gpointer user_data)
{
	CgWindow *window;
	CgWindowPrivate *priv;

	GtkWidget *type_prefix;
	GtkWidget *type_name;
	GtkWidget *func_prefix;

	gchar *str_type_prefix;
	gchar *str_type_name;
	gchar *str_func_prefix;
	const gchar *name;

	GtkWidget *file_header;
	GtkWidget *file_source;
	gchar* str_filebase;
	gchar* str_fileheader;
	gchar* str_filesource;

	window = CG_WINDOW (user_data);
	priv = CG_WINDOW_PRIVATE (window);

	type_prefix = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "go_prefix"));
	type_name = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "go_type"));
	func_prefix = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "go_func_prefix"));

	file_header = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "header_file"));
	file_source = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "source_file"));

	name = gtk_entry_get_text (GTK_ENTRY (entry));
	cg_transform_custom_c_type_to_g_type (name, &str_type_prefix,
	                                      &str_type_name, &str_func_prefix);

	gtk_entry_set_text (GTK_ENTRY (type_prefix), str_type_prefix);
	gtk_entry_set_text (GTK_ENTRY (type_name), str_type_name);
	gtk_entry_set_text (GTK_ENTRY (func_prefix), str_func_prefix);

	g_free (str_type_prefix);
	g_free (str_type_name);
	g_free (str_func_prefix);

	str_filebase = cg_window_class_name_to_file_name (name);
	str_fileheader = g_strconcat (str_filebase, ".h", NULL);
	str_filesource = g_strconcat (str_filebase, ".c", NULL);
	g_free (str_filebase);

	gtk_entry_set_text (GTK_ENTRY (file_header), str_fileheader);
	gtk_entry_set_text (GTK_ENTRY (file_source), str_filesource);

	g_free (str_fileheader);
	g_free (str_filesource);
}

static void
cg_window_dynamic_name_changed_cb (GtkEntry *entry,
				   gpointer user_data,
				   const gchar *file_ending)
{
	CgWindow *window;
	CgWindowPrivate *priv;

	GtkWidget *file_header;
	GtkWidget *file_source;
	gchar* str_filebase;
	gchar* str_filesource;

	window = CG_WINDOW (user_data);
	priv = CG_WINDOW_PRIVATE (window);

	file_header = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "header_file"));
	file_source = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "source_file"));

	str_filebase = cg_window_class_name_to_file_name (
		gtk_entry_get_text (GTK_ENTRY (entry)));

	str_filesource = g_strconcat (str_filebase, file_ending, NULL);
	g_free (str_filebase);

	gtk_entry_set_text (GTK_ENTRY (file_header), str_filesource);
	gtk_entry_set_text (GTK_ENTRY (file_source), str_filesource);

	g_free (str_filesource);
}

static void
cg_window_py_name_changed_cb (GtkEntry *entry,
			      gpointer user_data)
{
	cg_window_dynamic_name_changed_cb (entry, user_data, ".py");
}

static void
cg_window_js_name_changed_cb (GtkEntry *entry,
			      gpointer user_data)
{
	cg_window_dynamic_name_changed_cb (entry, user_data, ".js");
}

static void
cg_window_js_is_subclass_changed_cb (GtkEntry *entry,
				     gpointer user_data)
{
	CgWindow *window;
	CgWindowPrivate *priv;

	GtkWidget *is_subclass;
	GtkWidget *entry_base_class;
	GtkWidget *label_base_class;

	window = CG_WINDOW (user_data);
	priv = CG_WINDOW_PRIVATE (window);

	is_subclass = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "js_is_subclass"));
	entry_base_class = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "js_base"));
	label_base_class = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "lbl_js_base"));

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (is_subclass)) == TRUE)
	{
		gtk_editable_set_editable (GTK_EDITABLE (entry_base_class), TRUE);
		gtk_widget_set_sensitive (label_base_class, TRUE);
	}
	else
	{
		gtk_editable_set_editable (GTK_EDITABLE (entry_base_class), FALSE);
		gtk_widget_set_sensitive (label_base_class, FALSE);
	}
}

static void
cg_window_vala_name_changed_cb (GtkEntry *entry,
			      gpointer user_data)
{
	cg_window_dynamic_name_changed_cb (entry, user_data, ".vala");
}

#if 0
static void
cg_window_associate_browse_button (GladeXML *xml,
                                   const gchar *button_id,
                                   const gchar *entry_id)
{
	GtkWidget *button;
	GtkWidget *entry;

	button = glade_xml_get_widget (xml, button_id);
	entry = glade_xml_get_widget (xml, entry_id);

	g_return_if_fail (GTK_IS_BUTTON (button) && GTK_IS_ENTRY (entry));

	g_signal_connect(G_OBJECT(button), "clicked",
	                 G_CALLBACK(cg_window_browse_button_clicked_cb), entry);
}
#endif

static void
cg_window_set_builder (CgWindow *window,
                         GtkBuilder *xml)
{
	CgWindowPrivate *priv;
	priv = CG_WINDOW_PRIVATE (window);

	priv->bxml = xml;
	g_object_ref (priv->bxml);

	priv->window = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "classgen_main"));

#if 0
	cg_window_associate_browse_button (priv->gxml, "browse_header",
	                                   "header_file");

	cg_window_associate_browse_button (priv->gxml, "browse_source",
	                                   "source_file");
#endif

	priv->editor_cc = cg_element_editor_new (
		GTK_TREE_VIEW (gtk_builder_get_object (priv->bxml, "cc_elements")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "cc_elements_add")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "cc_elements_remove")),
		5,
		_("Scope"), CG_ELEMENT_EDITOR_COLUMN_LIST, CC_SCOPE_LIST,
		_("Implementation"), CG_ELEMENT_EDITOR_COLUMN_LIST, CC_IMPLEMENTATION_LIST,
		_("Type"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Name"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Arguments"), CG_ELEMENT_EDITOR_COLUMN_ARGUMENTS);

	priv->editor_go_members = cg_element_editor_new (
		GTK_TREE_VIEW (gtk_builder_get_object (priv->bxml, "go_members")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "go_members_add")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "go_members_remove")),
		4,
		_("Scope"), CG_ELEMENT_EDITOR_COLUMN_LIST, GO_SCOPE_LIST,
		_("Type"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Name"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Arguments"), CG_ELEMENT_EDITOR_COLUMN_ARGUMENTS);

	priv->editor_go_properties = cg_element_editor_new(
		GTK_TREE_VIEW (gtk_builder_get_object (priv->bxml, "go_properties")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "go_properties_add")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "go_properties_remove")),
		7,
		_("Name"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Nick"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Blurb"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("GType"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("ParamSpec"), CG_ELEMENT_EDITOR_COLUMN_LIST, GO_PARAMSPEC_LIST,
		_("Default"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Flags"), CG_ELEMENT_EDITOR_COLUMN_FLAGS, GO_PROPERTY_FLAGS);

	priv->editor_go_signals = cg_element_editor_new(
		GTK_TREE_VIEW (gtk_builder_get_object (priv->bxml, "go_signals")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "go_signals_add")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "go_signals_remove")),
		5,
		_("Type"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Name"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Arguments"), CG_ELEMENT_EDITOR_COLUMN_ARGUMENTS, /* Somehow redundant with marshaller, but required for default handler */
		_("Flags"), CG_ELEMENT_EDITOR_COLUMN_FLAGS, GO_SIGNAL_FLAGS,
		_("Marshaller"), CG_ELEMENT_EDITOR_COLUMN_STRING);

	priv->editor_py_methods = cg_element_editor_new (
		GTK_TREE_VIEW (gtk_builder_get_object (priv->bxml, "py_methods")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "py_methods_add")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "py_methods_remove")),
		2,
		_("Name"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Arguments"), CG_ELEMENT_EDITOR_COLUMN_ARGUMENTS);

	priv->editor_py_constvars = cg_element_editor_new (
		GTK_TREE_VIEW (gtk_builder_get_object (priv->bxml, "py_constvars")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "py_constvars_add")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "py_constvars_remove")),
		2,
		_("Name"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Value"), CG_ELEMENT_EDITOR_COLUMN_STRING);

	priv->editor_js_methods = cg_element_editor_new (
		GTK_TREE_VIEW (gtk_builder_get_object (priv->bxml, "js_methods")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "js_methods_add")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "js_methods_remove")),
		2,
		_("Name"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Arguments"), CG_ELEMENT_EDITOR_COLUMN_ARGUMENTS);

	priv->editor_js_variables = cg_element_editor_new (
		GTK_TREE_VIEW (gtk_builder_get_object (priv->bxml, "js_variables")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "js_variables_add")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "js_variables_remove")),
		2,
		_("Name"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Value"), CG_ELEMENT_EDITOR_COLUMN_STRING);

	priv->editor_js_imports = cg_element_editor_new (
		GTK_TREE_VIEW (gtk_builder_get_object (priv->bxml, "js_imports")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "js_imports_add")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "js_imports_remove")),
		2,
		_("Name"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Module"), CG_ELEMENT_EDITOR_COLUMN_STRING);

	priv->editor_vala_methods = cg_element_editor_new (
		GTK_TREE_VIEW (gtk_builder_get_object (priv->bxml, "vala_methods")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "vala_methods_add")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "vala_methods_remove")),
		4,
		_("Scope"), CG_ELEMENT_EDITOR_COLUMN_LIST, VALA_METHSIG_SCOPE_LIST,
		_("Type"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Name"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Arguments"), CG_ELEMENT_EDITOR_COLUMN_ARGUMENTS);

	priv->editor_vala_properties = cg_element_editor_new (
		GTK_TREE_VIEW (gtk_builder_get_object (priv->bxml, "vala_properties")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "vala_properties_add")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "vala_properties_remove")),
		7,
		_("Scope"), CG_ELEMENT_EDITOR_COLUMN_LIST, VALA_PROP_SCOPE_LIST,
		_("Type"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Name"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Automatic"), CG_ELEMENT_EDITOR_COLUMN_LIST, VALA_BOOLEAN_LIST,
		_("Getter"), CG_ELEMENT_EDITOR_COLUMN_LIST, VALA_BOOLEAN_LIST,
		_("Setter"), CG_ELEMENT_EDITOR_COLUMN_LIST, VALA_BOOLEAN_LIST,
		_("Value"), CG_ELEMENT_EDITOR_COLUMN_STRING);

	priv->editor_vala_signals = cg_element_editor_new (
		GTK_TREE_VIEW (gtk_builder_get_object (priv->bxml, "vala_signals")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "vala_signals_add")),
		GTK_BUTTON (gtk_builder_get_object (priv->bxml, "vala_signals_remove")),
		3,
		_("Scope"), CG_ELEMENT_EDITOR_COLUMN_LIST, VALA_METHSIG_SCOPE_LIST,
		_("Name"), CG_ELEMENT_EDITOR_COLUMN_STRING,
		_("Arguments"), CG_ELEMENT_EDITOR_COLUMN_ARGUMENTS);

	/* Active item property in glade cannot be set because no GtkTreeModel
	 * is assigned. */
	gtk_combo_box_set_active (
		GTK_COMBO_BOX (gtk_builder_get_object (priv->bxml, "license")), 0);

	gtk_combo_box_set_active (
		GTK_COMBO_BOX (gtk_builder_get_object (priv->bxml, "cc_inheritance")),
		0);
	g_signal_connect (
		G_OBJECT (gtk_builder_get_object (priv->bxml, "py_name")), "changed",
		G_CALLBACK (cg_window_py_name_changed_cb), window);

	g_signal_connect (
		G_OBJECT (gtk_builder_get_object (priv->bxml, "vala_name")), "changed",
		G_CALLBACK (cg_window_vala_name_changed_cb), window);

	gtk_combo_box_set_active (
		GTK_COMBO_BOX (gtk_builder_get_object (priv->bxml, "vala_class_scope")),
		0);

	/* This revalidates the appropriate validator */
	g_signal_connect (
		G_OBJECT (gtk_builder_get_object (priv->bxml, "top_notebook")),
		"switch-page", G_CALLBACK (cg_window_top_notebook_switch_page_cb),
		window);

	g_signal_connect (
		G_OBJECT (gtk_builder_get_object (priv->bxml, "go_name")), "changed",
		G_CALLBACK (cg_window_go_name_changed_cb), window);

	g_signal_connect (
		G_OBJECT (gtk_builder_get_object (priv->bxml, "cc_name")), "changed",
		G_CALLBACK (cg_window_cc_name_changed_cb), window);

	g_signal_connect (
		G_OBJECT (gtk_builder_get_object (priv->bxml, "js_name")), "changed",
		G_CALLBACK (cg_window_js_name_changed_cb), window);

	g_signal_connect (
		G_OBJECT (gtk_builder_get_object (priv->bxml, "js_is_subclass")), "toggled",
		G_CALLBACK (cg_window_js_is_subclass_changed_cb), window);

	g_signal_connect (
		G_OBJECT (gtk_builder_get_object (priv->bxml, "add_project")), "toggled",
		G_CALLBACK (cg_window_add_project_toggled_cb), window);

	g_signal_connect (
		G_OBJECT (gtk_builder_get_object (priv->bxml, "add_project_parent")), "changed",
		G_CALLBACK (cg_window_add_project_parent_changed_cb), window);

	cg_window_add_project_toggled_cb (GTK_TOGGLE_BUTTON (
		gtk_builder_get_object (priv->bxml, "add_project")), window);

	/* Selected page is CC */
	cg_window_validate_cc (window);
}

static void
cg_window_cc_transform_func (GHashTable *table,
                             G_GNUC_UNUSED gpointer user_data)
{
	cg_transform_arguments (table, "Arguments", FALSE);
}

static void
cg_window_go_members_transform_func (GHashTable *table,
                                     gpointer user_data)
{
	CgWindow *window;
	gchar *name;
	gchar *func_prefix;

	/* Strip of func prefix of members if they contain any, the prefix
	 * is added by the autogen template. */
	window = CG_WINDOW (user_data);
	name = g_hash_table_lookup (table, "Name");
	func_prefix = cg_window_fetch_string (window, "go_func_prefix");

	if (g_str_has_prefix (name, func_prefix))
	{
		name = g_strdup (name + strlen (func_prefix) + 1);
		g_hash_table_insert (table, "Name", name);
	}

	g_free (func_prefix);
	cg_transform_arguments (table, "Arguments", TRUE);
}

static void
cg_window_go_properties_transform_func (GHashTable *table,
                                        G_GNUC_UNUSED gpointer user_data)
{
	gchar *paramspec;

	cg_transform_string (table, "Name");
	cg_transform_string (table, "Nick");
	cg_transform_string (table, "Blurb");

	cg_transform_guess_paramspec (table, "ParamSpec",
	                              "Type", N_(GO_PARAMSPEC_LIST[0]));

	cg_transform_flags (table, "Flags", GO_PROPERTY_FLAGS);

	paramspec = g_hash_table_lookup (table, "ParamSpec");
	if (paramspec && (strcmp (paramspec, "g_param_spec_string") == 0))
		cg_transform_string (table, "Default");
}

static void
cg_window_go_signals_transform_func (GHashTable *table,
                                     gpointer user_data)
{
	CgWindow *window;

	gchar *type;
	guint arg_count;

	gchar *gtype_prefix;
	gchar *gtype_suffix;
	gchar *name;
	gchar *self_type;

	window = CG_WINDOW (user_data);

	cg_transform_string (table, "Name");

	/* Provide GType of return type */
	type = g_hash_table_lookup (table, "Type");
	if (type != NULL)
	{
		cg_transform_c_type_to_g_type (type, &gtype_prefix, &gtype_suffix);
		g_hash_table_insert (table, "GTypePrefix", gtype_prefix);
		g_hash_table_insert (table, "GTypeSuffix", gtype_suffix);
	}

	cg_transform_arguments (table, "Arguments", TRUE);

	/* Add self as signal's first argument */
	name = cg_window_fetch_string (window, "go_name");
	self_type = g_strconcat (name, "*", NULL);
	g_free (name);

	cg_transform_first_argument (table, "Arguments", self_type);
	g_free (self_type);

	/* Provide GTypes and amount of arguments */
	arg_count = cg_transform_arguments_to_gtypes (table, "Arguments",
	                                              "ArgumentGTypes");

	g_hash_table_insert (table, "ArgumentCount",
	                     g_strdup_printf ("%u", arg_count));

	cg_transform_flags (table, "Flags", GO_SIGNAL_FLAGS);
}

static void
cg_window_py_methods_transform_func (GHashTable *table,
			     G_GNUC_UNUSED gpointer user_data)
{
	cg_transform_python_arguments (table, "Arguments");
}

static void
cg_window_py_constvars_transform_func (GHashTable *table,
                                       G_GNUC_UNUSED gpointer user_data)
{
	cg_transform_string (table, "Value");
}

static void
cg_window_js_methods_transform_func (GHashTable *table,
                                     G_GNUC_UNUSED gpointer user_data)
{
	cg_transform_arguments (table, "Arguments", FALSE);
}

static void
cg_window_js_variables_transform_func (GHashTable *table,
                                     G_GNUC_UNUSED gpointer user_data)
{

	cg_transform_string (table, "Name");
}

static void
cg_window_js_imports_transform_func (GHashTable *table,
                                     G_GNUC_UNUSED gpointer user_data)
{
	cg_transform_string (table, "Name");
}

static void
vala_transform_scope_func (GHashTable *table,
                           G_GNUC_UNUSED gpointer user_data)
{
	gchar *scope;

	scope = g_hash_table_lookup (table, "Scope");
	if (scope == NULL)
	{
		g_hash_table_insert (table, "Scope",
				     g_strdup_printf("public"));
	}
}

static void
cg_window_vala_methods_transform_func (GHashTable *table,
                                       G_GNUC_UNUSED gpointer user_data)
{
	cg_transform_string (table, "Name");
	cg_transform_arguments (table, "Arguments", FALSE);

	vala_transform_scope_func (table, user_data);
}

static void
cg_window_vala_signals_transform_func (GHashTable *table,
                                       G_GNUC_UNUSED gpointer user_data)
{
	cg_transform_string (table, "Name");
	cg_transform_arguments (table, "Arguments", FALSE);

	vala_transform_scope_func (table, user_data);
}

static void
cg_window_vala_properties_transform_func (GHashTable *table,
                                          G_GNUC_UNUSED gpointer user_data)
{
	cg_transform_string (table, "Name");
	cg_transform_string (table, "Value");

	vala_transform_scope_func (table, user_data);
}

#if 0
static gboolean
cg_window_scope_condition_func (const gchar **elements,
                                gpointer user_data)
{
	/* Matches all members in the given scope */
	if (elements[0] == NULL) return FALSE;
	if (strcmp (elements[0], (const gchar *) user_data) == 0) return TRUE;
	return FALSE;
}
#endif

static gboolean
cg_window_scope_with_args_condition_func (const gchar **elements,
                                          gpointer user_data)
{
	/* Matches all members in the given scope that have arguments set */
	if (elements[0] == NULL) return FALSE;
	if (elements[3] == NULL || *(elements[3]) == '\0') return FALSE;
	if (strcmp (elements[0], (const gchar *) user_data) != 0) return FALSE;
	return TRUE;
}

static gboolean
cg_window_scope_without_args_condition_func (const gchar **elements,
                                             gpointer user_data)
{
	/* Matches all members in the given scope that have no arguments set */
	if (elements[0] == NULL) return FALSE;
	if (elements[3] != NULL && *(elements[3]) != '\0') return FALSE;
	if (strcmp(elements[0], (const gchar *) user_data) != 0) return FALSE;
	return TRUE;
}

static void
cg_window_init (CgWindow *window)
{
	CgWindowPrivate *priv;
	priv = CG_WINDOW_PRIVATE (window);

	priv->bxml = NULL;
	priv->window = NULL;

	priv->editor_cc = NULL;
	priv->editor_go_members = NULL;
	priv->editor_go_properties = NULL;
	priv->editor_go_signals = NULL;
	priv->editor_py_methods = NULL;
	priv->editor_py_constvars = NULL;
	priv->editor_js_methods = NULL;
	priv->editor_js_variables = NULL;
	priv->editor_js_imports = NULL;
	priv->editor_vala_methods = NULL;
	priv->editor_vala_properties = NULL;
	priv->editor_vala_signals = NULL;

	priv->validator = NULL;
}

static void
cg_window_finalize (GObject *object)
{
	CgWindow *window;
	CgWindowPrivate *priv;

	window = CG_WINDOW (object);
	priv = CG_WINDOW_PRIVATE (window);

	if (priv->editor_cc != NULL)
		g_object_unref (G_OBJECT (priv->editor_cc));
	if (priv->editor_go_members != NULL)
		g_object_unref (G_OBJECT (priv->editor_go_members));
	if (priv->editor_go_properties != NULL)
		g_object_unref (G_OBJECT (priv->editor_go_properties));
	if (priv->editor_go_signals != NULL)
		g_object_unref (G_OBJECT (priv->editor_go_signals));
	if (priv->editor_py_methods != NULL)
		g_object_unref (G_OBJECT (priv->editor_py_methods));
	if (priv->editor_py_constvars != NULL)
		g_object_unref (G_OBJECT (priv->editor_py_constvars));
	if (priv->editor_js_methods != NULL)
		g_object_unref (G_OBJECT (priv->editor_js_methods));
	if (priv->editor_js_variables != NULL)
		g_object_unref (G_OBJECT (priv->editor_js_variables));
	if (priv->editor_js_imports != NULL)
		g_object_unref (G_OBJECT (priv->editor_js_imports));
	if (priv->editor_vala_methods != NULL)
		g_object_unref (G_OBJECT (priv->editor_vala_methods));
	if (priv->editor_vala_properties != NULL)
		g_object_unref (G_OBJECT (priv->editor_vala_properties));
	if (priv->editor_vala_signals != NULL)
		g_object_unref (G_OBJECT (priv->editor_vala_signals));

	if (priv->validator != NULL)
		g_object_unref (G_OBJECT (priv->validator));

	if (priv->bxml != NULL)
		g_object_unref (G_OBJECT (priv->bxml));

	gtk_widget_destroy(priv->window);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
cg_window_set_property (GObject *object,
                        guint prop_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
	CgWindow *window;

	g_return_if_fail (CG_IS_WINDOW (object));

	window = CG_WINDOW (object);

	switch (prop_id)
	{
	case PROP_BUILDER_XML:
		cg_window_set_builder (window,
		                         GTK_BUILDER (g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cg_window_get_property (GObject *object,
                        guint prop_id,
                        GValue *value,
                        GParamSpec *pspec)
{
	CgWindow *window;
	CgWindowPrivate *priv;

	g_return_if_fail (CG_IS_WINDOW (object));

	window = CG_WINDOW (object);
	priv = CG_WINDOW_PRIVATE (window);

	switch (prop_id)
	{
	case PROP_BUILDER_XML:
		g_value_set_object (value, G_OBJECT (priv->bxml));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cg_window_class_init (CgWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	g_type_class_add_private (klass, sizeof (CgWindowPrivate));

	object_class->finalize = cg_window_finalize;
	object_class->set_property = cg_window_set_property;
	object_class->get_property = cg_window_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_BUILDER_XML,
	                                 g_param_spec_object ("builder-xml",
	                                                      "GtkBuilder",
	                                                      _("XML description of the user interface"),
	                                                      G_TYPE_OBJECT,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

GType
cg_window_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (CgWindowClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) cg_window_class_init,
			NULL,
			NULL,
			sizeof (CgWindow),
			0,
			(GInstanceInitFunc) cg_window_init,
			NULL
		};

		our_type = g_type_register_static (G_TYPE_OBJECT, "CgWindow",
		                                   &our_info, 0);
	}

	return our_type;
}

CgWindow *
cg_window_new (void)
{
	GtkBuilder *bxml = gtk_builder_new();
	GObject *window;
	GError* error = NULL;

	if (!gtk_builder_add_from_file (bxml, BUILDER_FILE, &error))
	{
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	window = g_object_new (CG_TYPE_WINDOW, "builder-xml", bxml, NULL);
	return CG_WINDOW (window);
}

GtkDialog *
cg_window_get_dialog (CgWindow *window)
{
	return GTK_DIALOG (CG_WINDOW_PRIVATE (window)->window);
}

GHashTable *
cg_window_create_value_heap (CgWindow *window)
{
	static const gchar *LICENSES[] = {
		"gpl",
		"lgpl",
		"bsd",
		"none"
	};

	CgWindowPrivate *priv;
	GHashTable *values;
	gint license_index;

	GtkNotebook *notebook;

	gchar *header_file;
	gchar *source_file;

	gchar *text;
	gchar *base_prefix;
	gchar *base_suffix;

	priv = CG_WINDOW_PRIVATE (window);
	notebook = GTK_NOTEBOOK (gtk_builder_get_object (priv->bxml,
	                                               "top_notebook"));

	values = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify)g_free);

	switch (gtk_notebook_get_current_page (notebook))
	{
	case 0: /* cc */
		cg_window_set_heap_value (window, values, G_TYPE_STRING,
		                          "ClassName", "cc_name");
		cg_window_set_heap_value (window, values, G_TYPE_STRING,
		                          "BaseClass", "cc_base");
		cg_window_set_heap_value (window, values, G_TYPE_STRING,
		                          "Inheritance", "cc_inheritance");
		cg_window_set_heap_value (window, values, G_TYPE_BOOLEAN,
		                          "Headings", "cc_headings");
		cg_window_set_heap_value (window, values, G_TYPE_BOOLEAN,
		                          "Inline", "cc_inline");

		cg_element_editor_set_values (priv->editor_cc, "Elements", values,
		                              cg_window_cc_transform_func, window,
		                              "Scope", "Implementation", "Type",
		                              "Name", "Arguments");

		break;
	case 1: /* GObject */
		cg_window_set_heap_value (window, values, G_TYPE_STRING,
		                          "ClassName", "go_name");
		cg_window_set_heap_value(window, values, G_TYPE_STRING,
		                         "BaseClass", "go_base");
		cg_window_set_heap_value(window, values, G_TYPE_STRING,
		                         "TypePrefix", "go_prefix");
		cg_window_set_heap_value(window, values, G_TYPE_STRING,
		                         "TypeSuffix", "go_type");

		/* Store GType of base class which is also required */
		text = cg_window_fetch_string (window, "go_base");
		cg_transform_custom_c_type_to_g_type (text, &base_prefix,
		                                      &base_suffix, NULL);

		g_free (text);

		g_hash_table_insert (values, "BaseTypePrefix", base_prefix);

		g_hash_table_insert (values, "BaseTypeSuffix", base_suffix);

		cg_window_set_heap_value (window, values, G_TYPE_STRING,
		                          "FuncPrefix", "go_func_prefix");

		cg_window_set_heap_value (window, values, G_TYPE_BOOLEAN,
		                          "Headings", "go_headings");

		cg_element_editor_set_values (priv->editor_go_members, "Members",
		                              values,
		                              cg_window_go_members_transform_func,
		                              window, "Scope", "Type", "Name",
		                              "Arguments");

		/* These count the amount of members that match certain conditions
		 * and that would be relatively hard to find out in the autogen
		 * file (at least with my limited autogen skills). These are the
		 * number of private respectively public member functions (arguments
		 * set) and variables (no arguments set). */
		cg_element_editor_set_value_count (priv->editor_go_members,
			"PrivateFunctionCount", values,
			cg_window_scope_with_args_condition_func, "private");

		cg_element_editor_set_value_count (priv->editor_go_members,
			"PrivateVariableCount", values,
			cg_window_scope_without_args_condition_func, "private");

		cg_element_editor_set_value_count (priv->editor_go_members,
			"PublicFunctionCount", values,
			cg_window_scope_with_args_condition_func, "public");

		cg_element_editor_set_value_count (priv->editor_go_members,
			"PublicVariableCount", values,
			cg_window_scope_without_args_condition_func, "public");

		cg_element_editor_set_values (priv->editor_go_properties, "Properties",
		                              values,
		                              cg_window_go_properties_transform_func,
		                              window, "Name", "Nick", "Blurb", "Type",
		                              "ParamSpec", "Default", "Flags");

		cg_element_editor_set_values (priv->editor_go_signals, "Signals",
		                              values,
		                              cg_window_go_signals_transform_func,
		                              window, "Type", "Name", "Arguments",
		                              "Flags", "Marshaller");

		break;
	case 2: /* Python */
		cg_window_set_heap_value (window, values, G_TYPE_STRING,
					  "ClassName", "py_name");
		cg_window_set_heap_value (window, values, G_TYPE_STRING,
					  "BaseClass", "py_base");
		cg_window_set_heap_value (window, values, G_TYPE_BOOLEAN,
					  "Headings", "py_headings");
		cg_element_editor_set_values (priv->editor_py_methods, "Methods", values,
					      cg_window_py_methods_transform_func,
                                              window, "Name", "Arguments");
		cg_element_editor_set_values (priv->editor_py_constvars, "Constvars",
					      values,
					      cg_window_py_constvars_transform_func,
					      window, "Name", "Value");
		break;
	case 3: /* JavaScript */
		cg_window_set_heap_value (window, values, G_TYPE_STRING,
					  "ClassName", "js_name");
		cg_window_set_heap_value (window, values, G_TYPE_STRING,
					  "BaseClass", "js_base");
		cg_window_set_heap_value (window, values, G_TYPE_STRING,
					  "Initargs", "js_initargs");
		cg_window_set_heap_value (window, values, G_TYPE_BOOLEAN,
					  "Headings", "js_headings");
		cg_element_editor_set_values (priv->editor_js_methods, "Methods", values,
					      cg_window_js_methods_transform_func,
                                              window, "Name", "Arguments");
		cg_element_editor_set_values (priv->editor_js_variables, "Variables",
					      values,
					      cg_window_js_variables_transform_func,
					      window, "Name", "Value");
		cg_element_editor_set_values (priv->editor_js_imports, "Imports",
					      values,
					      cg_window_js_imports_transform_func,
					      window, "Name", "Module");
		break;
	case 4: /* Vala */
		cg_window_set_heap_value (window, values, G_TYPE_STRING,
					  "ClassName", "vala_name");
		cg_window_set_heap_value (window, values, G_TYPE_STRING,
				  "BaseClass", "vala_base");
		cg_window_set_heap_value (window, values, G_TYPE_STRING,
		                          "ClassScope", "vala_class_scope");
		cg_window_set_heap_value (window, values, G_TYPE_BOOLEAN,
					  "Headings", "vala_headings");
		cg_element_editor_set_values (priv->editor_vala_methods, "Methods", values,
					      cg_window_vala_methods_transform_func,
					      window, "Scope", "Type", "Name", "Arguments");
		cg_element_editor_set_values (priv->editor_vala_properties, "Properties", values,
					      cg_window_vala_properties_transform_func,
					      window, "Scope", "Type", "Name", "Automatic",
					      "Getter", "Setter", "Value");
		cg_element_editor_set_values (priv->editor_vala_signals, "Signals", values,
					      cg_window_vala_signals_transform_func,
					      window, "Scope", "Name", "Arguments");
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	cg_window_set_heap_value (window, values, G_TYPE_STRING,
	                          "AuthorName", "author_name");

	cg_window_set_heap_value(window, values, G_TYPE_STRING,
	                         "AuthorEmail", "author_email");

	license_index = cg_window_fetch_integer (window, "license");
	g_hash_table_insert (values, "License", g_strdup (LICENSES[license_index]));

	header_file = cg_window_get_header_file (window) != NULL ? g_path_get_basename (cg_window_get_header_file (window)) : NULL;
	source_file = g_path_get_basename (cg_window_get_source_file (window));

	g_hash_table_insert (values, "HeaderFile", header_file);

	g_hash_table_insert (values, "SourceFile", source_file);

	return values;
}

const gchar *
cg_window_get_header_template (CgWindow *window)
{
	CgWindowPrivate *priv;
	GtkNotebook *notebook;

	priv = CG_WINDOW_PRIVATE (window);
	notebook = GTK_NOTEBOOK (gtk_builder_get_object (priv->bxml,
	                                               "top_notebook"));

	g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

	switch(gtk_notebook_get_current_page (notebook))
	{
	case 0:
		return CC_HEADER_TEMPLATE;
	case 1:
		return GO_HEADER_TEMPLATE;
	case 2: /* Python */
		return NULL;
	case 3: /* JavaScript */
		return NULL;
	case 4: /* Vala */
		return NULL;
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

const gchar *
cg_window_get_header_file(CgWindow *window)
{
	CgWindowPrivate *priv;
	GtkEntry *entry;

	priv = CG_WINDOW_PRIVATE (window);
	entry = GTK_ENTRY (gtk_builder_get_object (priv->bxml, "header_file"));

	g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
	return gtk_widget_get_sensitive (GTK_WIDGET (entry)) == TRUE ? gtk_entry_get_text (entry) : NULL;
}

const gchar *
cg_window_get_source_template(CgWindow *window)
{
	CgWindowPrivate *priv;
	GtkNotebook *notebook;

	priv = CG_WINDOW_PRIVATE (window);
	notebook = GTK_NOTEBOOK (gtk_builder_get_object (priv->bxml,
	                                               "top_notebook"));

	g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

	switch(gtk_notebook_get_current_page (notebook))
	{
	case 0:
		return CC_SOURCE_TEMPLATE;
	case 1:
		return GO_SOURCE_TEMPLATE;
	case 2:
		return PY_SOURCE_TEMPLATE;
	case 3: /* JavaScript */
		return JS_SOURCE_TEMPLATE;
	case 4: /* Vala */
		return VALA_SOURCE_TEMPLATE;
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

const gchar *
cg_window_get_source_file (CgWindow *window)
{
	CgWindowPrivate *priv;
	GtkEntry *entry;

	priv = CG_WINDOW_PRIVATE (window);
	entry = GTK_ENTRY (gtk_builder_get_object (priv->bxml, "source_file"));

	g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
	return gtk_entry_get_text (entry);
}

GFile *
cg_window_get_selected_target (CgWindow *window)
{
	CgWindowPrivate *priv;
	IAnjutaProjectChooser *chooser;

	priv = CG_WINDOW_PRIVATE (window);
	chooser = IANJUTA_PROJECT_CHOOSER (gtk_builder_get_object (priv->bxml, "add_project_parent"));

	return ianjuta_project_chooser_get_selected (chooser, NULL);
}

void
cg_window_set_add_to_project (CgWindow *window,
                              gboolean enable)
{
	CgWindowPrivate *priv;
	GtkCheckButton *button;

	priv = CG_WINDOW_PRIVATE (window);
	button = GTK_CHECK_BUTTON (gtk_builder_get_object (priv->bxml,
	                                                 "add_project"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), enable);
}

void
cg_window_set_add_to_repository (CgWindow *window,
                                 gboolean enable)
{
	CgWindowPrivate *priv;
	GtkCheckButton *button;

	priv = CG_WINDOW_PRIVATE (window);
	button = GTK_CHECK_BUTTON (gtk_builder_get_object (priv->bxml,
	                                                 "add_repository"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), enable);
}

gboolean
cg_window_get_add_to_project(CgWindow *window)
{
	return cg_window_fetch_boolean (window, "add_project");
}

gboolean
cg_window_get_add_to_repository (CgWindow *window)
{
	return cg_window_fetch_boolean (window, "add_repository");
}

void
cg_window_enable_add_to_project (CgWindow *window,
                                 gboolean enable)
{
	CgWindowPrivate *priv;
	GtkWidget *widget;

	priv = CG_WINDOW_PRIVATE (window);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "add_project"));
	gtk_widget_set_sensitive (widget, enable);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "add_project_parent"));
	gtk_widget_set_sensitive (widget, enable);
}

void
cg_window_set_project_model (CgWindow *window, IAnjutaProjectManager *manager)
{
	CgWindowPrivate *priv;
	GtkWidget *widget;

	priv = CG_WINDOW_PRIVATE (window);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "add_project_parent"));
	ianjuta_project_chooser_set_project_model (IANJUTA_PROJECT_CHOOSER (widget),
	                                           IANJUTA_PROJECT_MANAGER (manager),
	                                           ANJUTA_PROJECT_SOURCE,
	                                           NULL);
}

void
cg_window_enable_add_to_repository (CgWindow *window,
                                    gboolean enable)
{
	CgWindowPrivate *priv;
	GtkWidget *widget;

	priv = CG_WINDOW_PRIVATE (window);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->bxml, "add_repository"));

	gtk_widget_set_sensitive (widget, enable);
}

void
cg_window_set_author (CgWindow *window,
                      const gchar *author)
{
	CgWindowPrivate* priv;
	GtkEntry* entry;

	priv = CG_WINDOW_PRIVATE (window);
	entry = GTK_ENTRY(gtk_builder_get_object (priv->bxml, "author_name"));

	gtk_entry_set_text (entry, author);
}

void
cg_window_set_email (CgWindow *window,
                     const gchar *email)
{
	CgWindowPrivate* priv;
	GtkEntry* entry;

	priv = CG_WINDOW_PRIVATE (window);
	entry = GTK_ENTRY(gtk_builder_get_object (priv->bxml, "author_email"));

	gtk_entry_set_text (entry, email);
}
