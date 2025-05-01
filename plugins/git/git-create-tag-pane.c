/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * git-shell-test
 * Copyright (C) James Liggett 2010 <jrliggett@cox.net>
 * 
 * git-shell-test is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * git-shell-test is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "git-create-tag-pane.h"

struct _GitCreateTagPanePriv
{
	GtkBuilder *builder;
};

G_DEFINE_TYPE (GitCreateTagPane, git_create_tag_pane, GIT_TYPE_PANE);

static void
on_ok_action_activated (GtkAction *action, GitCreateTagPane *self)
{
	Git *plugin;
	GtkEntry *tag_name_entry;
	AnjutaEntry *tag_revision_entry;
	GtkToggleAction *force_action;
	GtkToggleButton *sign_check;
	GtkToggleButton *annotate_check;
	AnjutaColumnTextView *tag_log_view;
	gchar *name;
	const gchar *revision;
	gchar *log;
	GitTagCreateCommand *create_command;

	plugin = ANJUTA_PLUGIN_GIT (anjuta_dock_pane_get_plugin (ANJUTA_DOCK_PANE (self)));
	tag_name_entry = GTK_ENTRY (gtk_builder_get_object (self->priv->builder,
	                                            		"tag_name_entry"));
	tag_revision_entry = ANJUTA_ENTRY (gtk_builder_get_object (self->priv->builder,
	                                                   		   "tag_revision_entry"));
	force_action = GTK_TOGGLE_ACTION (gtk_builder_get_object (self->priv->builder,
	                                                         "force_action"));
	sign_check = GTK_TOGGLE_BUTTON (gtk_builder_get_object (self->priv->builder,
	                                                        "sign_check"));
	annotate_check = GTK_TOGGLE_BUTTON (gtk_builder_get_object (self->priv->builder,
	                                                            "annotate_check"));
	tag_log_view = ANJUTA_COLUMN_TEXT_VIEW (gtk_builder_get_object (self->priv->builder,
	                                                        	    "tag_log_view"));
	name = gtk_editable_get_chars (GTK_EDITABLE (tag_name_entry), 0, -1);
	revision = anjuta_entry_get_text (tag_revision_entry);
	log = NULL;

	if (!git_pane_check_input (GTK_WIDGET (ANJUTA_PLUGIN (plugin)->shell),
	                           GTK_WIDGET (tag_name_entry), name,
	                           _("Please enter a tag name.")))
	{
		g_free (name);

		return;
	}

	if (g_utf8_strlen (revision, -1) == 0)
		revision = NULL;

	if (gtk_toggle_button_get_active (annotate_check))
	{
		log = anjuta_column_text_view_get_text (tag_log_view);

		if (!git_pane_check_input (GTK_WIDGET (ANJUTA_PLUGIN (plugin)->shell),
		                           GTK_WIDGET (tag_log_view), log,
		                           _("Please enter a log message.")))
		{
			g_free (name);
			g_free (log);

			return;
		}
	}

	create_command = git_tag_create_command_new (plugin->project_root_directory,
	                                             name,
	                                             revision,
	                                             log,
	                                             gtk_toggle_button_get_active (sign_check),
	                                             gtk_toggle_action_get_active (force_action));

	g_signal_connect (G_OBJECT (create_command), "command-finished",
	                  G_CALLBACK (git_pane_report_errors),
	                  plugin);


	g_signal_connect (G_OBJECT (create_command), "command-finished",
	                  G_CALLBACK (g_object_unref),
	                  NULL);

	anjuta_command_start (ANJUTA_COMMAND (create_command));


	g_free (name);
	g_free (log);

	git_pane_remove_from_dock (GIT_PANE (self));
}

static void
set_widget_sensitive (GtkToggleButton *button, GtkWidget *widget)
{
	gtk_widget_set_sensitive (widget,
	                          gtk_toggle_button_get_active (button));
}

static void
on_sign_check_toggled(GtkToggleButton *button, GtkToggleButton *annotate_check)
{
	gboolean active;

	active = gtk_toggle_button_get_active (button);

	gtk_toggle_button_set_active (annotate_check, active);
	gtk_widget_set_sensitive (GTK_WIDGET (annotate_check), !active);
}

static void
git_create_tag_pane_init (GitCreateTagPane *self)
{
	gchar *objects[] = {"create_tag_pane",
						"ok_action",
						"cancel_action",
						"force_action",
						NULL};
	GError *error = NULL;
	GtkAction *ok_action;
	GtkAction *cancel_action;
	GtkWidget *annotate_check;
	GtkWidget *sign_check;
	GtkWidget *tag_log_view;
	

	self->priv = g_new0 (GitCreateTagPanePriv, 1);
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
	annotate_check = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
	                                                     "annotate_check"));
	sign_check = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
	                                                 "sign_check"));
	tag_log_view = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
	                                                   "tag_log_view"));

	g_signal_connect (G_OBJECT (ok_action), "activate",
	                  G_CALLBACK (on_ok_action_activated),
	                  self);

	g_signal_connect_swapped (G_OBJECT (cancel_action), "activate",
	                          G_CALLBACK (git_pane_remove_from_dock),
	                          self);

	g_signal_connect (G_OBJECT (annotate_check), "toggled",
	                  G_CALLBACK (set_widget_sensitive),
	                  tag_log_view);

	g_signal_connect (G_OBJECT (sign_check), "toggled",
	              	  G_CALLBACK (on_sign_check_toggled),
	                  annotate_check);
}

static void
git_create_tag_pane_finalize (GObject *object)
{
	GitCreateTagPane *self;

	self = GIT_CREATE_TAG_PANE (object);

	g_object_unref (self->priv->builder);
	g_free (self->priv);

	G_OBJECT_CLASS (git_create_tag_pane_parent_class)->finalize (object);
}

static GtkWidget *
git_create_tag_pane_get_widget (AnjutaDockPane *pane)
{
	GitCreateTagPane *self;

	self = GIT_CREATE_TAG_PANE (pane);

	return GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
	                                           "create_tag_pane"));
}

static void
git_create_tag_pane_class_init (GitCreateTagPaneClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	AnjutaDockPaneClass *pane_class = ANJUTA_DOCK_PANE_CLASS (klass);

	object_class->finalize = git_create_tag_pane_finalize;
	pane_class->get_widget = git_create_tag_pane_get_widget;
	pane_class->refresh = NULL;
}


AnjutaDockPane *
git_create_tag_pane_new (Git *plugin)
{
	return g_object_new (GIT_TYPE_CREATE_TAG_PANE, "plugin", plugin, NULL);
}

void
on_create_tag_button_clicked (GtkAction *action, Git *plugin)
{
	AnjutaDockPane *pane;

	pane = git_create_tag_pane_new (plugin);

	anjuta_dock_replace_command_pane (ANJUTA_DOCK (plugin->dock), "CreateTag", 
	                                  _("Create Tag"), NULL, pane, GDL_DOCK_BOTTOM, NULL, 0,
	                                  NULL);
}