/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * action-callbacks.c
 * Copyright (C) 2003 Naba Kumar
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <config.h>
#include <libanjuta/anjuta-ui.h>
#include <libanjuta/anjuta-utils.h>

#include <libanjuta/interfaces/ianjuta-markable.h>
#include <libanjuta/interfaces/ianjuta-editor.h>
#include <libanjuta/interfaces/ianjuta-editor-selection.h>
#include <libanjuta/interfaces/ianjuta-editor-convert.h>
#include <libanjuta/interfaces/ianjuta-editor-line-mode.h>
#include <libanjuta/interfaces/ianjuta-editor-view.h>
#include <libanjuta/interfaces/ianjuta-editor-folds.h>
#include <libanjuta/interfaces/ianjuta-file.h>
#include <libanjuta/interfaces/ianjuta-file-savable.h>
#include <libanjuta/interfaces/ianjuta-print.h>
#include <libanjuta/interfaces/ianjuta-editor-comment.h>
#include <libanjuta/interfaces/ianjuta-editor-zoom.h>
#include <libanjuta/interfaces/ianjuta-editor-goto.h>
#include <libanjuta/interfaces/ianjuta-editor-language.h>
#include <libanjuta/interfaces/ianjuta-editor-assist.h>

#include <sys/wait.h>
#include <sys/stat.h>

#include "anjuta-docman.h"
#include "action-callbacks.h"
#include "plugin.h"
#include "file_history.h"
#include "search-box.h"
#include "search-files.h"
#include "anjuta-bookmarks.h"

static IAnjutaDocument *
get_current_document (gpointer user_data)
{
	AnjutaDocman *docman;
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);

	return anjuta_docman_get_current_document (docman);
}

static GtkWidget *
get_current_focus_widget (gpointer user_data)
{
	AnjutaDocman *docman;
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);

	return anjuta_docman_get_current_focus_widget (docman);
}

static gboolean
get_current_popup_active (gpointer user_data)
{
	GtkWidget *widget;
	AnjutaDocman *docman;
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);

	widget = anjuta_docman_get_current_popup (docman);
	if (widget)
	{
		widget = gtk_widget_get_toplevel (widget);
		if (gtk_widget_is_toplevel (widget))
			return gtk_window_has_toplevel_focus (GTK_WINDOW (widget));
	}
	return FALSE;
}

void
on_open_activate (GtkAction *action, gpointer user_data)
{
	AnjutaDocman *docman;
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);
	anjuta_docman_open_file (docman);
}

void
on_save_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;
	AnjutaDocman *docman;
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);

	doc = anjuta_docman_get_current_document (docman);
	if (doc)
		anjuta_docman_save_document (docman, doc, NULL);
}

void
on_save_as_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;
	AnjutaDocman *docman;
	DocmanPlugin *plugin;

	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);

	doc = anjuta_docman_get_current_document (docman);
	if (doc)
		anjuta_docman_save_document_as (docman, doc, NULL);
}

void
on_save_all_activate (GtkAction *action, gpointer user_data)
{
	GList *buffers, *node;
	AnjutaDocman *docman;
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);

	buffers = anjuta_docman_get_all_doc_widgets (docman);
	if (buffers)
	{
		for (node = buffers; node != NULL; node = g_list_next (node))
		{
			IAnjutaDocument *doc;
			doc = IANJUTA_DOCUMENT (node->data);
			if (doc)
			{
				/* known-uri test occurs downstream */
				/* CHECKME test here and perform save-as when appropriate ? */
				if (ianjuta_file_savable_is_dirty (IANJUTA_FILE_SAVABLE (doc), NULL))
				{
					ianjuta_file_savable_save (IANJUTA_FILE_SAVABLE (doc), NULL);
				}
			}
		}
		g_list_free (buffers);
	}
}

static gboolean
on_save_prompt_save_editor (AnjutaSavePrompt *save_prompt,
							gpointer item, gpointer user_data)
{
	AnjutaDocman *docman;

	docman = ANJUTA_DOCMAN (user_data);
	return anjuta_docman_save_document (docman, IANJUTA_DOCUMENT (item),
										GTK_WIDGET (save_prompt));
}

void
on_close_file_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;
	AnjutaDocman *docman;
	DocmanPlugin *plugin;

	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);
	doc = anjuta_docman_get_current_document (docman);
	if (doc == NULL)
		return;

	if (ianjuta_file_savable_is_dirty(IANJUTA_FILE_SAVABLE (doc), NULL))
	{
		GtkWidget *parent;
		gchar *uri;
		GFile* file;
		AnjutaSavePrompt *save_prompt;

		parent = gtk_widget_get_toplevel (GTK_WIDGET (doc));
		/* Prompt for unsaved data */
		save_prompt = anjuta_save_prompt_new (GTK_WINDOW (parent));
		file = ianjuta_file_get_file (IANJUTA_FILE (doc), NULL);
		if (file)
		{
			uri = g_file_get_uri (file);
			g_object_unref (file);
		}
		else
			uri = NULL;
		/* NULL uri ok */
		anjuta_save_prompt_add_item (save_prompt,
									 ianjuta_document_get_filename (doc, NULL),
									 uri, doc, on_save_prompt_save_editor,
									 docman);
		g_free (uri);

		switch (gtk_dialog_run (GTK_DIALOG (save_prompt)))
		{
			case ANJUTA_SAVE_PROMPT_RESPONSE_CANCEL:
				/* Do not close */
				break;
			case ANJUTA_SAVE_PROMPT_RESPONSE_DISCARD:
			case ANJUTA_SAVE_PROMPT_RESPONSE_SAVE_CLOSE:
				/* Close it */
				anjuta_docman_remove_document (docman, doc);
				break;
		}
		gtk_widget_destroy (GTK_WIDGET (save_prompt));
	}
	else
		anjuta_docman_remove_document (docman, doc);
}

void
on_close_all_file_activate (GtkAction *action, gpointer user_data)
{
	GList *buffers;
	AnjutaDocman *docman;
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);

	/* Close all 'saved' files */
	buffers = anjuta_docman_get_all_doc_widgets (docman);
	if (buffers)
	{
		GList *node;
		node = buffers;
		while (node)
		{
			IAnjutaDocument *doc;
			GList* next;
			doc = IANJUTA_DOCUMENT (node->data);
			next = g_list_next (node); /* grab it now, as we may change it. */
			if (doc)
			{
				if (!ianjuta_file_savable_is_dirty (IANJUTA_FILE_SAVABLE (doc), NULL))
				{
					anjuta_docman_remove_document (docman, doc);
				}
			}
			node = next;
		}
		g_list_free (buffers);
	}
}

void
on_close_other_file_activate (GtkAction *action, gpointer user_data)
{
	GList *buffers;
	AnjutaDocman *docman;
	IAnjutaDocument *curr_doc;
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);

	/* Close all 'saved' files */
	buffers = anjuta_docman_get_all_doc_widgets (docman);
	if (buffers)
	{
		GList *node;
		node = buffers;

		curr_doc = anjuta_docman_get_current_document (docman);
		while (node)
		{
			IAnjutaDocument *doc;
			GList* next;
			doc = IANJUTA_DOCUMENT (node->data);
			next = g_list_next (node); /* grab it now, as we may change it. */
			if (doc && doc != curr_doc)
			{
				if (!ianjuta_file_savable_is_dirty (IANJUTA_FILE_SAVABLE (doc), NULL))
				{
					anjuta_docman_remove_document (docman, doc);
				}
			}
			node = next;
		}
		g_list_free (buffers);
	}
}

void
on_reload_file_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;
	gchar *msg;
	GtkWidget *dialog;
	GtkWidget *parent;
	gint reply = GTK_RESPONSE_YES;

	doc = get_current_document (user_data);
	if (doc == NULL)
		return;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (doc));
	if (IANJUTA_IS_FILE_SAVABLE (doc) && ianjuta_file_savable_is_dirty (IANJUTA_FILE_SAVABLE (doc), NULL))
	{
		msg = g_strdup_printf (
		_("Are you sure you want to reload '%s'?\nAny unsaved changes will be lost."),
								ianjuta_document_get_filename (doc, NULL));

		dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
										 GTK_DIALOG_DESTROY_WITH_PARENT,
										 GTK_MESSAGE_QUESTION,
										 GTK_BUTTONS_NONE, "%s", msg);
		gtk_dialog_add_button (GTK_DIALOG (dialog),
							   GTK_STOCK_CANCEL, GTK_RESPONSE_NO);
		anjuta_util_dialog_add_button (GTK_DIALOG (dialog), _("_Reload"),
								  GTK_STOCK_REVERT_TO_SAVED,
								  GTK_RESPONSE_YES);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
										 GTK_RESPONSE_NO);
		reply = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_free (msg);
	}

	if (reply == GTK_RESPONSE_YES)
	{
		GFile* file;
		file = ianjuta_file_get_file (IANJUTA_FILE (doc), NULL);
		if (file)
		{
			ianjuta_file_open (IANJUTA_FILE (doc), file, NULL);
			g_object_unref(file);
		}
	}
}

void
on_print_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_print_print (IANJUTA_PRINT (doc), NULL);
}

void
on_print_preview_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_print_print_preview (IANJUTA_PRINT (doc), NULL);
}

void
on_editor_command_upper_case_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
	{
		if (ianjuta_editor_selection_has_selection (IANJUTA_EDITOR_SELECTION (doc),
													NULL))
		{
			IAnjutaIterable *start, *end;
			start = ianjuta_editor_selection_get_start (IANJUTA_EDITOR_SELECTION (doc),
														NULL);
			end = ianjuta_editor_selection_get_end (IANJUTA_EDITOR_SELECTION (doc), NULL);
			ianjuta_editor_convert_to_upper (IANJUTA_EDITOR_CONVERT (doc),
											 start, end, NULL);
			g_object_unref (start);
			g_object_unref (end);
		}
	}
}

void
on_editor_command_lower_case_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
	{
		if (ianjuta_editor_selection_has_selection (IANJUTA_EDITOR_SELECTION (doc), NULL))
		{
			IAnjutaIterable *start, *end;
			start = ianjuta_editor_selection_get_start (IANJUTA_EDITOR_SELECTION (doc), NULL);
			end = ianjuta_editor_selection_get_end (IANJUTA_EDITOR_SELECTION (doc), NULL);
			ianjuta_editor_convert_to_lower (IANJUTA_EDITOR_CONVERT (doc),
											 start, end, NULL);
			g_object_unref (start);
			g_object_unref (end);
		}
	}
}

void
on_editor_command_eol_crlf_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_line_mode_convert (IANJUTA_EDITOR_LINE_MODE (doc),
										  IANJUTA_EDITOR_LINE_MODE_CRLF, NULL);
}

void
on_editor_command_eol_lf_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_line_mode_convert (IANJUTA_EDITOR_LINE_MODE (doc),
										  IANJUTA_EDITOR_LINE_MODE_LF, NULL);
}

void
on_editor_command_eol_cr_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_line_mode_convert (IANJUTA_EDITOR_LINE_MODE (doc),
										  IANJUTA_EDITOR_LINE_MODE_CR, NULL);
}

void
on_editor_command_select_all_activate (GtkAction *action, gpointer user_data)
{
	GtkWidget *widget;

	widget = get_current_focus_widget (user_data);

	if (widget && GTK_IS_EDITABLE (widget))
	{
		gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
	}
	else
	{
		if (widget /* editor text is focused */
			|| get_current_popup_active (user_data))
		{
			IAnjutaDocument *doc;

			doc = get_current_document (user_data);
			if (doc)
				ianjuta_editor_selection_select_all
				(IANJUTA_EDITOR_SELECTION (doc), NULL);
	}
	}
}

void
on_editor_command_select_block_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_selection_select_block (IANJUTA_EDITOR_SELECTION (doc), NULL);
}

void
on_editor_command_match_brace_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_goto_matching_brace (IANJUTA_EDITOR_GOTO (doc), NULL);
}

void
on_editor_command_undo_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_document_undo (doc, NULL);
}

void
on_editor_command_redo_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_document_redo (doc, NULL);
}

void
on_editor_command_cut_activate (GtkAction *action, gpointer user_data)
{
	GtkWidget *widget;

	widget = get_current_focus_widget (user_data);

	if (widget && GTK_IS_EDITABLE (widget))
	{
		gtk_editable_cut_clipboard (GTK_EDITABLE (widget));
	}
	else
	{
		if (widget /* editor text is focused */
			|| get_current_popup_active (user_data))
		{
			IAnjutaDocument *doc;

			doc = get_current_document (user_data);
			if (doc)
				ianjuta_document_cut (doc, NULL);
		}
	}
}

void
on_editor_command_paste_activate (GtkAction *action, gpointer user_data)
{
	GtkWidget *widget;

	widget = get_current_focus_widget (user_data);

	if (widget && GTK_IS_EDITABLE (widget))
	{
		gtk_editable_paste_clipboard (GTK_EDITABLE (widget));
	}
	else
	{
		if (widget	/* editor text is focused */
			|| get_current_popup_active (user_data))
		{
			IAnjutaDocument *doc;

			doc = get_current_document (user_data);
			if (doc)
				ianjuta_document_paste (doc, NULL);
		}
	}
}

void
on_editor_command_copy_activate (GtkAction *action, gpointer user_data)
{
	GtkWidget *widget;

	widget = get_current_focus_widget (user_data);

	if (widget && GTK_IS_EDITABLE (widget))
	{
		gtk_editable_copy_clipboard (GTK_EDITABLE (widget));
	}
	else
	{
		if (widget /* editor text is focused */
			|| get_current_popup_active (user_data))
		{
			IAnjutaDocument *doc;

			doc = get_current_document (user_data);
			if (doc)
				ianjuta_document_copy (doc, NULL);
		}
	}
}

void
on_editor_command_clear_activate (GtkAction *action, gpointer user_data)
{
	GtkWidget *widget;

	widget = get_current_focus_widget (user_data);

	if (widget && GTK_IS_EDITABLE (widget))
	{
		gint start, end;
		if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), &start, &end))
		{
			start = gtk_editable_get_position (GTK_EDITABLE (widget));
			end = start + 1;
		}
		gtk_editable_delete_text (GTK_EDITABLE (widget), start, end);
	}
	else if (widget	/* editor text is focused */
		|| get_current_popup_active (user_data))
	{
		IAnjutaDocument *doc;

		doc = get_current_document (user_data);
		if (doc)
			ianjuta_document_clear (doc, NULL);
	}
}

/* fold funcs are for scintilla only */
void
on_editor_command_close_folds_all_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_folds_close_all (IANJUTA_EDITOR_FOLDS (doc), NULL);
}

void
on_editor_command_open_folds_all_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_folds_open_all (IANJUTA_EDITOR_FOLDS (doc), NULL);
}

void
on_editor_command_toggle_fold_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_folds_toggle_current (IANJUTA_EDITOR_FOLDS (doc), NULL);
}

void
on_transform_eolchars1_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_line_mode_fix (IANJUTA_EDITOR_LINE_MODE (doc), NULL);
}

void
on_prev_history (GtkAction *action, gpointer user_data)
{
	AnjutaDocman *docman;
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);
	an_file_history_back (docman);
}

void
on_next_history (GtkAction *action, gpointer user_data)
{
	AnjutaDocman *docman;
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);
	an_file_history_forward (docman);
}

void
on_comment_block (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_comment_block (IANJUTA_EDITOR_COMMENT (doc), NULL);
}

void on_comment_box (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_comment_box (IANJUTA_EDITOR_COMMENT (doc), NULL);
}

void on_comment_stream (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_comment_stream (IANJUTA_EDITOR_COMMENT (doc), NULL);
}

void
on_goto_line_no1_activate (GtkAction *action, gpointer user_data)
{
	DocmanPlugin *plugin;

	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);

	gtk_widget_show (plugin->search_box);
	search_box_grab_line_focus (SEARCH_BOX (plugin->search_box));
}

void
on_goto_block_start1_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_goto_start_block (IANJUTA_EDITOR_GOTO (doc), NULL);
}

void
on_goto_block_end1_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_goto_end_block (IANJUTA_EDITOR_GOTO (doc), NULL);
}

void
on_zoom_in_text_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_zoom_in (IANJUTA_EDITOR_ZOOM (doc), NULL);
}

void
on_zoom_out_text_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_zoom_out (IANJUTA_EDITOR_ZOOM (doc), NULL);
}

void
on_force_hilite_activate (GtkWidget *menuitem, gpointer user_data)
{
	IAnjutaDocument *doc;

	doc = get_current_document (user_data);
	if (doc)
	{
		const gchar *language_code;
		language_code = g_object_get_data (G_OBJECT (menuitem), "language_code");
		if (language_code && IANJUTA_IS_EDITOR_LANGUAGE (doc))
			ianjuta_editor_language_set_language (IANJUTA_EDITOR_LANGUAGE (doc),
												  language_code, NULL);
	}
}

void
on_show_search (GtkAction *action, gpointer user_data)
{
	DocmanPlugin *plugin;
	GtkWidget *search_box;

	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);

	search_box = plugin->search_box;

	search_box_fill_search_focus (SEARCH_BOX (search_box), FALSE);
	gtk_widget_show (search_box);
}

void
on_repeat_quicksearch (GtkAction *action, gpointer user_data)
{
	DocmanPlugin *plugin;
	GtkWidget *search_box;

	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);

	search_box = plugin->search_box;

	gtk_widget_show (search_box);
	search_box_incremental_search (SEARCH_BOX (search_box), TRUE, TRUE, TRUE);
}

void
on_search_previous (GtkAction *action, gpointer user_data)
{
	DocmanPlugin *plugin;
	GtkWidget *search_box;

	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);

	search_box = plugin->search_box;

	gtk_widget_show (search_box);
	search_box_incremental_search (SEARCH_BOX (search_box), FALSE, TRUE, TRUE);
}

void
on_search_and_replace (GtkAction *action, gpointer user_data)
{
	DocmanPlugin *plugin;
	GtkWidget *search_box;

	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);

	search_box = plugin->search_box;

	search_box_fill_search_focus (SEARCH_BOX (search_box), TRUE);
	gtk_widget_show (search_box);
}

void
on_search_popup_case_sensitive_toggle (GtkAction *action, gpointer user_data)
{
	DocmanPlugin *plugin;
	GtkWidget *search_box;

	gboolean case_sensitive_status =
		gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);

	search_box = plugin->search_box;

	gtk_widget_show (search_box);
	search_box_toggle_case_sensitive (SEARCH_BOX (search_box), case_sensitive_status);
}

void
on_search_popup_highlight_toggle (GtkAction *action, gpointer user_data)
{
	DocmanPlugin *plugin;
	GtkWidget *search_box;

	gboolean highlight_status = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);

	search_box = plugin->search_box;

	gtk_widget_show (search_box);
	search_box_toggle_highlight (SEARCH_BOX (search_box), highlight_status);
}

void
on_search_popup_clear_highlight (GtkAction *action, gpointer user_data)
{
	DocmanPlugin *plugin;
	GtkWidget *search_box;

	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);

	search_box = plugin->search_box;

	gtk_widget_show (search_box);
	search_box_clear_highlight (SEARCH_BOX (search_box));
}

void
on_search_popup_regex_search (GtkAction *action, gpointer user_data)
{
	DocmanPlugin *plugin;
	GtkWidget *search_box;

	gboolean regex_active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);

	search_box = plugin->search_box;

	gtk_widget_show (search_box);
	search_box_toggle_regex (SEARCH_BOX (search_box), regex_active);
}

void 
on_search_find_in_files (GtkAction *action, gpointer user_data)
{
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);

	if (!plugin->search_files)
	{
		plugin->search_files = G_OBJECT (search_files_new (ANJUTA_DOCMAN (plugin->docman),
		                                                   SEARCH_BOX(plugin->search_box)));
	}
	search_files_present (SEARCH_FILES(plugin->search_files));
}

void
on_editor_add_view_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;
	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_view_create (IANJUTA_EDITOR_VIEW (doc), NULL);
}

void
on_editor_remove_view_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;
	doc = get_current_document (user_data);
	if (doc)
		ianjuta_editor_view_remove_current (IANJUTA_EDITOR_VIEW (doc), NULL);
}

void
on_next_document (GtkAction *action, gpointer user_data)
{
	AnjutaDocman *docman;
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);

	anjuta_docman_next_page (docman);
}

void
on_previous_document (GtkAction *action, gpointer user_data)
{
	AnjutaDocman *docman;
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	docman = ANJUTA_DOCMAN (plugin->docman);

	anjuta_docman_previous_page (docman);
}

void
on_bookmark_toggle_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;
	DocmanPlugin *plugin;
	doc = get_current_document (user_data);
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	if (doc && IANJUTA_IS_EDITOR(doc))
	{
		IAnjutaEditor* editor = IANJUTA_EDITOR(doc);
		anjuta_bookmarks_toggle (ANJUTA_BOOKMARKS (plugin->bookmarks), editor,
							  ianjuta_editor_get_lineno (editor, NULL));
	}
}

void
on_bookmark_next_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;
	DocmanPlugin *plugin;
	doc = get_current_document (user_data);
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	if (doc && IANJUTA_IS_EDITOR(doc))
	{
		IAnjutaEditor* editor = IANJUTA_EDITOR(doc);
		anjuta_bookmarks_next (ANJUTA_BOOKMARKS (plugin->bookmarks), editor,
							  ianjuta_editor_get_lineno (editor, NULL));
	}
}

void
on_bookmark_prev_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;
	DocmanPlugin *plugin;
	doc = get_current_document (user_data);
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	if (doc && IANJUTA_IS_EDITOR(doc))
	{
		IAnjutaEditor* editor = IANJUTA_EDITOR(doc);
		anjuta_bookmarks_prev (ANJUTA_BOOKMARKS (plugin->bookmarks), editor,
							   ianjuta_editor_get_lineno (editor, NULL));
	}
}

void
on_bookmarks_clear_activate (GtkAction *action, gpointer user_data)
{
	DocmanPlugin *plugin;
	plugin = ANJUTA_PLUGIN_DOCMAN (user_data);
	anjuta_bookmarks_clear (ANJUTA_BOOKMARKS(plugin->bookmarks));
}

void
on_autocomplete_activate (GtkAction *action, gpointer user_data)
{
	IAnjutaDocument *doc;
	doc = get_current_document (user_data);
	if (doc && IANJUTA_IS_EDITOR_ASSIST(doc))
	{
		IAnjutaEditorAssist* assist = IANJUTA_EDITOR_ASSIST (doc);
		ianjuta_editor_assist_invoke (assist, NULL, NULL);
	}
}
