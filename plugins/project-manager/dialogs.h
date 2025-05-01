/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4; coding: utf-8 -*- */
/* am-dialogs.h
 *
 * Copyright (C) 2009  Sébastien Granjoux
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _AM_DIALOGS_H_
#define _AM_DIALOGS_H_

#include <gtk/gtk.h>
#include <libanjuta/interfaces/ianjuta-project.h>
#include "plugin.h"

G_BEGIN_DECLS

AnjutaProjectNode* anjuta_pm_project_new_group (ProjectManagerPlugin *plugin, GtkWindow *parent, GtkTreeIter *default_parent, const gchar *name);
AnjutaProjectNode* anjuta_pm_project_new_target (ProjectManagerPlugin *plugin, GtkWindow *parent, GtkTreeIter *default_group, const gchar *name);
AnjutaProjectNode* anjuta_pm_project_new_source (ProjectManagerPlugin *plugin, GtkWindow *parent, GtkTreeIter *default_parent, const gchar *name);
GList* anjuta_pm_add_source_dialog (ProjectManagerPlugin *plugin, GtkWindow *parent, GtkTreeIter *default_target, GFile *default_source);
GList* anjuta_pm_project_new_multiple_source (ProjectManagerPlugin *plugin, GtkWindow *parent, GtkTreeIter *default_parent, GList *name);
GList* anjuta_pm_project_new_module (ProjectManagerPlugin *plugin, GtkWindow *parent, GtkTreeIter *default_target, const gchar *default_module);
GList* anjuta_pm_project_new_package (ProjectManagerPlugin *plugin, GtkWindow *parent, GtkTreeIter *default_module, GList *packages_to_add);

gboolean anjuta_pm_project_show_properties_dialog (ProjectManagerPlugin *plugin, GtkTreeIter *selected);


G_END_DECLS

#endif /* _AM_DIALOGS_H_ */
