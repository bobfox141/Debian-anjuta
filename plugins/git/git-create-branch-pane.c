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

#include "git-create-branch-pane.h"

struct _GitCreateBranchPanePriv
{
	GtkBuilder *builder;
};

G_DEFINE_TYPE (GitCreateBranchPane, git_create_branch_pane, GIT_TYPE_PANE);

static void
on_ok_action_activated (GtkAction *action, GitCreateBranchPane *self)
{
	Git *plugin;
	GtkEntry *branch_name_entry;
	AnjutaEntry *branch_revision_entry;
	GtkToggleButton *checkout_check;
	gchar *name;
	const gchar *revision;
	GitBranchCreateCommand *create_command;

	plugin = ANJUTA_PLUGIN_GIT (anjuta_dock_pane_get_plugin (ANJUTA_DOCK_PANE (self)));
	branch_name_entry = GTK_ENTRY (gtk_builder_get_object (self->priv->builder,
	                                            		   "branch_name_entry"));
	branch_revision_entry = ANJUTA_ENTRY (gtk_builder_get_object (self->priv->builder,
	                                                   			  "branch_revision_entry"));
	checkout_check = GTK_TOGGLE_BUTTON (gtk_builder_get_object (self->priv->builder,
	                                                            "checkout_check"));
	name = gtk_editable_get_chars (GTK_EDITABLE (branch_name_entry), 0, -1);

	if (!git_pane_check_input (GTK_WIDGET (ANJUTA_PLUGIN (plugin)->shell),
	                           GTK_WIDGET (branch_name_entry), name,
	                           _("Please enter a branch name.")))
	{
		g_free (name);

		return;
	}

	revision = anjuta_entry_get_text (branch_revision_entry);

	if (g_utf8_strlen (revision, -1) == 0)
		revision = NULL;
	
	create_command = git_branch_create_command_new (plugin->project_root_directory,
	                                                name,
	                                                revision,
	                                                gtk_toggle_button_get_active (checkout_check));

	g_signal_connect (G_OBJECT (create_command), "command-finished",
	                  G_CALLBACK (git_pane_report_errors),
	                  plugin);


	g_signal_connect (G_OBJECT (create_command), "command-finished",
	                  G_CALLBACK (g_object_unref),
	                  NULL);

	anjuta_command_start (ANJUTA_COMMAND (create_command));


	g_free (name);

	git_pane_remove_from_dock (GIT_PANE (self));
}

static void
git_create_branch_pane_init (GitCreateBranchPane *self)
{
	gchar *objects[] = {"create_branch_pane",
						"ok_action",
						"cancel_action",
						NULL};
	GError *error = NULL;
	GtkAction *ok_action;
	GtkAction *cancel_action;

	self->priv = g_new0 (GitCreateBranchPanePriv, 1);
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

	g_signal_connect (G_OBJECT (ok_action), "activate",
	                  G_CALLBACK (on_ok_action_activated),
	                  self);

	g_signal_connect_swapped (G_OBJECT (cancel_action), "activate",
	                          G_CALLBACK (git_pane_remove_from_dock),
	                          self);
}

static void
git_create_branch_pane_finalize (GObject *object)
{
	GitCreateBranchPane *self;

	self = GIT_CREATE_BRANCH_PANE (object);

	g_object_unref (self->priv->builder);
	g_free (self->priv);

	G_OBJECT_CLASS (git_create_branch_pane_parent_class)->finalize (object);
}

static GtkWidget *
git_create_branch_pane_get_widget (AnjutaDockPane *pane)
{
	GitCreateBranchPane *self;

	self = GIT_CREATE_BRANCH_PANE (pane);

	return GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
	                                           "create_branch_pane"));
}

static void
git_create_branch_pane_class_init (GitCreateBranchPaneClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	AnjutaDockPaneClass *pane_class = ANJUTA_DOCK_PANE_CLASS (klass);

	object_class->finalize = git_create_branch_pane_finalize;
	pane_class->get_widget = git_create_branch_pane_get_widget;
	pane_class->refresh = NULL;
}


AnjutaDockPane *
git_create_branch_pane_new (Git *plugin)
{
	return g_object_new (GIT_TYPE_CREATE_BRANCH_PANE, "plugin", plugin, NULL);
}

void
on_create_branch_button_clicked (GtkAction *action, Git *plugin)
{
	AnjutaDockPane *pane;

	pane = git_create_branch_pane_new (plugin);

	anjuta_dock_replace_command_pane (ANJUTA_DOCK (plugin->dock), "CreateBranch", 
	                                  "Create Branch", NULL, pane, GDL_DOCK_BOTTOM, NULL, 0,
	                                  NULL);
}