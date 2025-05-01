/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) James Liggett 2007 <jrliggett@cox.net>
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

#ifndef SUBVERSION_LOG_DIALOG_H
#define SUBVERSION_LOG_DIALOG_H

#include "subversion-ui-utils.h"
#include "svn-log-command.h"
#include "svn-diff-command.h"
#include "svn-cat-command.h"
#include <libanjuta/interfaces/ianjuta-document-manager.h>
#include <libanjuta/interfaces/ianjuta-editor.h>

void on_menu_subversion_log (GtkAction* action, Subversion* plugin);
void on_fm_subversion_log (GtkAction *action, Subversion *plugin);

GtkWidget *subversion_log_window_create (Subversion *plugin);
void subversion_log_set_whole_project_sensitive (GtkBuilder *log_bxml,
												 gboolean sensitive);

#endif
