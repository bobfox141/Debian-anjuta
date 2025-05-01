/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) James Liggett 2010 <jrliggett@cox.net>
 * 
 * anjuta is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * anjuta is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "git-merge-pane.h"

struct _GitMergePanePriv
{
	GtkBuilder *builder;
};

G_DEFINE_TYPE (GitMergePane, git_merge_pane, GIT_TYPE_PANE);

static void
on_ok_action_activated (GtkAction *action, GitMergePane *self)
{
	Git *plugin;
	GtkEditable *merge_revision_entry;
	GtkToggleAction *no_commit_action;
	GtkToggleAction *squash_action;
	GtkToggleButton *use_custom_log_check;
	gchar *revision;
	gchar *log;
	AnjutaColumnTextView *merge_log_view;
	GitMergeCommand *merge_command;

	plugin = ANJUTA_PLUGIN_GIT (anjuta_dock_pane_get_plugin (ANJUTA_DOCK_PANE (self)));
	merge_revision_entry = GTK_EDITABLE (gtk_builder_get_object (self->priv->builder,
	                                                   			 "merge_revision_entry"));
	no_commit_action = GTK_TOGGLE_ACTION (gtk_builder_get_object (self->priv->builder, 
	                                                              "no_commit_action"));
	squash_action = GTK_TOGGLE_ACTION (gtk_builder_get_object (self->priv->builder,
	                                                           "squash_action"));
	use_custom_log_check = GTK_TOGGLE_BUTTON (gtk_builder_get_object (self->priv->builder,
	                                                                  "use_custom_log_check"));
	revision = gtk_editable_get_chars (merge_revision_entry, 0, -1);
	log = NULL;

	if (!git_pane_check_input (GTK_WIDGET (ANJUTA_PLUGIN (plugin)->shell),
	                           GTK_WIDGET (merge_revision_entry), revision,
	                           _("Please enter a revision.")))
	{
		g_free (revision);
		return;
	}

	if (gtk_toggle_button_get_active (use_custom_log_check))
	{
		merge_log_view = ANJUTA_COLUMN_TEXT_VIEW (gtk_builder_get_object (self->priv->builder,
		                                                        		  "merge_log_view"));
		log = anjuta_column_text_view_get_text (merge_log_view);

		if (!git_pane_check_input (GTK_WIDGET (ANJUTA_PLUGIN (plugin)->shell),
		                           GTK_WIDGET (merge_log_view), log,
		                           _("Please enter a log message.")))
		{
			g_free (revision);
			g_free (log);
			return;
		}
	}

	merge_command = git_merge_command_new (plugin->project_root_directory, 
	                                       revision, log,
	                                       gtk_toggle_action_get_active (no_commit_action),
	                                       gtk_toggle_action_get_active (squash_action));

	g_free (revision);
	g_free (log);

	git_pane_create_message_view (plugin);

	g_signal_connect (G_OBJECT (merge_command), "data-arrived",
	                  G_CALLBACK (git_pane_on_command_info_arrived),
	                  plugin);

	g_signal_connect (G_OBJECT (merge_command), "command-finished",
	                  G_CALLBACK (git_pane_report_errors),
	                  plugin);

	g_signal_connect (G_OBJECT (merge_command), "command-finished",
	                  G_CALLBACK (g_object_unref),
	                  NULL);

	anjuta_command_start (ANJUTA_COMMAND (merge_command));
	                                       

	git_pane_remove_from_dock (GIT_PANE (self));
}

static void
on_use_custom_log_check_toggled (GtkToggleButton *button, GitMergePane *self)
{
	GtkWidget *merge_log_view;
	gboolean active;

	merge_log_view = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
	                                               "merge_log_view"));
	active = gtk_toggle_button_get_active (button);

	gtk_widget_set_sensitive (merge_log_view, active);
}

static void
git_merge_pane_init (GitMergePane *self)
{
	gchar *objects[] = {"merge_pane",
						"ok_action",
						"cancel_action",
						"no_commit_action",
						"squash_action",
						NULL};
	GError *error = NULL;
	GtkAction *ok_action;
	GtkAction *cancel_action;
	GtkWidget *use_custom_log_check;

	self->priv = g_new0 (GitMergePanePriv, 1);
	self->priv->builder = gtk_builder_new ();

	if (!gtk_builder_add_objects_from_file (self->priv->builder, BUILDER_FILE, 
	                                        objects, 
	                                        &error))
	{
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}

	ok_action = GTK_ACTION (gtk_builder_get_object (self->priv->builder,
	                                                "ok_action"));
	cancel_action = GTK_ACTION (gtk_builder_get_object (self->priv->builder,
	                                                    "cancel_action"));
	use_custom_log_check = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
	                                                       	   "use_custom_log_check"));

	g_signal_connect (G_OBJECT (ok_action), "activate",
	                  G_CALLBACK (on_ok_action_activated),
	                  self);

	g_signal_connect_swapped (G_OBJECT (cancel_action), "activate",
	                          G_CALLBACK (git_pane_remove_from_dock),
	                          self);

	g_signal_connect (G_OBJECT (use_custom_log_check), "toggled",
	                  G_CALLBACK (on_use_custom_log_check_toggled),
	                  self);
}

static void
git_merge_pane_finalize (GObject *object)
{
	GitMergePane *self;

	self = GIT_MERGE_PANE (object);

	g_object_unref (self->priv->builder);
	g_free (self->priv);

	G_OBJECT_CLASS (git_merge_pane_parent_class)->finalize (object);
}

static GtkWidget *
git_merge_pane_get_widget (AnjutaDockPane *pane)
{
	GitMergePane *self;

	self = GIT_MERGE_PANE (pane);

	return GTK_WIDGET (gtk_builder_get_object (self->priv->builder, 
	                                           "merge_pane"));
}

static void
git_merge_pane_class_init (GitMergePaneClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	AnjutaDockPaneClass *pane_class = ANJUTA_DOCK_PANE_CLASS (klass);

	object_class->finalize = git_merge_pane_finalize;
	pane_class->get_widget = git_merge_pane_get_widget;
	pane_class->refresh = NULL;
}


AnjutaDockPane *
git_merge_pane_new (Git *plugin)
{
	return g_object_new (GIT_TYPE_MERGE_PANE, "plugin", plugin, NULL);
}

void
on_merge_button_clicked (GtkAction *action, Git *plugin)
{
	AnjutaDockPane *pane;

	pane = git_merge_pane_new (plugin);

	anjuta_dock_replace_command_pane (ANJUTA_DOCK (plugin->dock), "Merge", 
	                                  _("Merge"), NULL, pane, GDL_DOCK_BOTTOM, NULL, 0,
	                                  NULL);
}
