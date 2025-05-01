/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4; coding: utf-8 -*- */
/* am-writer.h
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

#ifndef _AM_WRITER_H_
#define _AM_WRITER_H_

#include "am-project.h"

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

gboolean amp_project_update_am_property (AmpProject *project, AnjutaProjectNode *node, AnjutaProjectProperty *property); 

gboolean amp_group_node_create_token (AmpProject *project, AmpGroupNode *group, GError **error);
gboolean amp_group_node_delete_token (AmpProject  *project, AmpGroupNode *group, GError **error);

gboolean amp_target_node_create_token (AmpProject  *project, AmpTargetNode *target, GError **error);
gboolean amp_target_node_delete_token (AmpProject  *project, AmpTargetNode *target, GList *tokens, GError **error);

gboolean amp_source_node_create_token (AmpProject  *project, AmpSourceNode *source, GError **error);
gboolean amp_source_node_delete_token (AmpProject  *project, AmpSourceNode *source, GError **error);

G_END_DECLS

#endif /* _AM_WRITER_H_ */
