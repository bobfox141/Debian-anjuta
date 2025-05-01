/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) James Liggett 2008 <jrliggett@cox.net>
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

#include "git-bisect-state-command.h"

struct _GitBisectStateCommandPriv
{
	GitBisectState state;
	gchar *revision;
};

G_DEFINE_TYPE (GitBisectStateCommand, git_bisect_state_command, 
			   GIT_TYPE_COMMAND);

static void
git_bisect_state_command_init (GitBisectStateCommand *self)
{
	self->priv = g_new0 (GitBisectStateCommandPriv, 1);
}

static void
git_bisect_state_command_finalize (GObject *object)
{
	GitBisectStateCommand *self;
	
	self = GIT_BISECT_STATE_COMMAND (object);
	
	g_free (self->priv->revision);
	g_free (self->priv);

	G_OBJECT_CLASS (git_bisect_state_command_parent_class)->finalize (object);
}

static guint
git_bisect_state_command_run (AnjutaCommand *command)
{
	GitBisectStateCommand *self;
	
	self = GIT_BISECT_STATE_COMMAND (command);
	
	git_command_add_arg (GIT_COMMAND (command), "bisect");
	
	switch (self->priv->state)
	{
		case GIT_BISECT_STATE_GOOD:
			git_command_add_arg (GIT_COMMAND (command), "good");
			break;
		case GIT_BISECT_STATE_BAD:
			git_command_add_arg (GIT_COMMAND (command), "bad");
			break;
		default:
			break;
	}
	
	if (self->priv->revision)
		git_command_add_arg (GIT_COMMAND (command), self->priv->revision);
	
	return 0;
}

static void
git_bisect_state_command_class_init (GitBisectStateCommandClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	GitCommandClass* parent_class = GIT_COMMAND_CLASS (klass);
	AnjutaCommandClass* command_class = ANJUTA_COMMAND_CLASS (klass);

	object_class->finalize = git_bisect_state_command_finalize;
	parent_class->output_handler = git_command_send_output_to_info;
	command_class->run = git_bisect_state_command_run;
}


GitBisectStateCommand *
git_bisect_state_command_new (const gchar *working_directory, 
							  GitBisectState state, const gchar *revision)
{
	GitBisectStateCommand *self;
	
	self = g_object_new (GIT_TYPE_BISECT_STATE_COMMAND,
						 "working-directory", working_directory,
						 "single-line-output", TRUE,
						 NULL);
	
	self->priv->state = state;
	self->priv->revision = g_strdup (revision);
	
	return self;
}
