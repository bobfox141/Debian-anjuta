/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    execute.h
    Copyright (C) 2005 Sebastien Granjoux

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef __EXECUTE_H__
#define __EXECUTE_H__

#include "tool.h"

#include <glib.h>

typedef struct _ATPContextList
{
	GList* list;
} ATPContextList;

void atp_user_tool_execute (GtkMenuItem *item, ATPUserTool* this);

ATPContextList *atp_context_list_construct (ATPContextList *this);
void atp_context_list_destroy (ATPContextList *this);

#endif
