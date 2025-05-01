[+ autogen5 template +]
[+INCLUDE (string-append "licenses/" (get "License") ".tpl") \+]
[+INCLUDE (string-append "indent.tpl") \+]
/* [+INVOKE EMACS-MODELINE MODE="C" \+] */
[+INVOKE START-INDENT\+]
/*
 * plugin.c
 * Copyright (C) [+(shell "date +%Y")+] [+Author+] <[+Email+]>
 * 
[+INVOKE LICENSE-DESCRIPTION PFX=" * " PROGRAM=(get "Name") OWNER=(get "Author") \+]
 */

#include <config.h>
#include <libanjuta/anjuta-shell.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/interfaces/ianjuta-document-manager.h>

#include "plugin.h"

[+IF (=(get "HasUI") "1") +]
#define XML_FILE ANJUTA_DATA_DIR"/ui/[+NameHLower+].xml"
[+ENDIF+]
[+IF (=(get "HasGladeFile") "1") +]
#define UI_FILE ANJUTA_DATA_DIR"/glade/[+NameHLower+].ui"
#define TOP_WIDGET "top_widget"
[+ENDIF+]

static gpointer parent_class;

[+IF (=(get "HasUI") "1") +]
static void
on_sample_action_activate (GtkAction *action, [+PluginClass+] *plugin)
{
	GObject *obj;
	IAnjutaEditor *editor;
	IAnjutaDocumentManager *docman;
	
	/* Query for object implementing IAnjutaDocumentManager interface */
	obj = anjuta_shell_get_object (ANJUTA_PLUGIN (plugin)->shell,
									  "IAnjutaDocumentManager", NULL);
	docman = IANJUTA_DOCUMENT_MANAGER (obj);
	editor = IANJUTA_EDITOR (ianjuta_document_manager_get_current_document (docman, NULL));

	/* Do whatever with plugin */
	anjuta_util_dialog_info (GTK_WINDOW (ANJUTA_PLUGIN (plugin)->shell),
							 "Document manager pointer is: '0x%X'\n"
							 "Current Editor pointer is: 0x%X", docman,
							 editor);
}

static GtkActionEntry actions_file[] = {
	{
		"ActionFileSample",                       /* Action name */
		GTK_STOCK_NEW,                            /* Stock icon, if any */
		N_("_Sample action"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Sample action"),                      /* Tooltip */
		G_CALLBACK (on_sample_action_activate)    /* action callback */
	}
};
[+ENDIF+]

static gboolean
[+NameCLower+]_activate (AnjutaPlugin *plugin)
{
[+IF (=(get "HasUI") "1") +]
	AnjutaUI *ui;
[+ENDIF+]
[+IF (=(get "HasGladeFile") "1") +]
	GtkWidget *wid;
	GtkBuilder *builder;
	GError *error = NULL;
[+ENDIF+]
	[+PluginClass+] *[+NameCLower+];
	
	DEBUG_PRINT ("%s", "[+PluginClass+]: Activating [+PluginClass+] plugin ...");
	[+NameCLower+] = ([+PluginClass+]*) plugin;
[+IF (=(get "HasUI") "1") +]
	/* Add all UI actions and merge UI */
	ui = anjuta_shell_get_ui (plugin->shell, NULL);
	
	[+NameCLower+]->action_group = 
		anjuta_ui_add_action_group_entries (ui, "ActionGroup[+NameCClass+]",
											_("Sample file operations"),
											actions_file,
											G_N_ELEMENTS (actions_file),
											GETTEXT_PACKAGE, TRUE,
											plugin);
	[+NameCLower+]->uiid = anjuta_ui_merge (ui, XML_FILE);
[+ENDIF+]
[+IF (=(get "HasGladeFile") "1") +]
	/* Add plugin widgets to Shell */
	builder = gtk_builder_new ();
	if (!gtk_builder_add_from_file (builder, UI_FILE, &error))
	{
		g_critical ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	wid = gtk_builder_get_object (builder, TOP_WIDGET);
	if (!wid)
	{
		g_critical ("Widget \"%s\" is missing in file %s.",
				TOP_WIDGET, 
                                UI_FILE);
		return FALSE;
	}

	[+NameCLower+]->widget = wid;
	anjuta_shell_add_widget (plugin->shell, wid,
							 "[+PluginClass+]Widget", _("[+PluginClass+] widget"), NULL,
							 ANJUTA_SHELL_PLACEMENT_BOTTOM, NULL);
	g_object_unref (builder);
[+ENDIF+]
	return TRUE;
}

static gboolean
[+NameCLower+]_deactivate (AnjutaPlugin *plugin)
{
[+IF (=(get "HasUI") "1") +]
	AnjutaUI *ui;
[+ENDIF+]
	DEBUG_PRINT ("%s", "[+PluginClass+]: Dectivating [+PluginClass+] plugin ...");
[+IF (=(get "HasGladeFile") "1") +]
	anjuta_shell_remove_widget (plugin->shell, (([+PluginClass+]*)plugin)->widget,
								NULL);
[+ENDIF+]
[+IF (=(get "HasUI") "1") +]
	ui = anjuta_shell_get_ui (plugin->shell, NULL);
	anjuta_ui_remove_action_group (ui, (([+PluginClass+]*)plugin)->action_group);
	anjuta_ui_unmerge (ui, (([+PluginClass+]*)plugin)->uiid);
[+ENDIF+]	
	return TRUE;
}

static void
[+NameCLower+]_finalize (GObject *obj)
{
	/* Finalization codes here */
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
[+NameCLower+]_dispose (GObject *obj)
{
	/* Disposition codes */
	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
[+NameCLower+]_instance_init (GObject *obj)
{
	[+PluginClass+] *plugin = ([+PluginClass+]*)obj;
[+IF (=(get "HasUI") "1") +]
	plugin->uiid = 0;
[+ENDIF+]
[+IF (=(get "HasGladeFile") "1") +]
	plugin->widget = NULL;
[+ENDIF+]
}

static void
[+NameCLower+]_class_init (GObjectClass *klass) 
{
	AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	plugin_class->activate = [+NameCLower+]_activate;
	plugin_class->deactivate = [+NameCLower+]_deactivate;
	klass->finalize = [+NameCLower+]_finalize;
	klass->dispose = [+NameCLower+]_dispose;
}

ANJUTA_PLUGIN_BOILERPLATE ([+PluginClass+], [+NameCLower+]);
ANJUTA_SIMPLE_PLUGIN ([+PluginClass+], [+NameCLower+]);
[+INVOKE END-INDENT\+]
