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

#ifndef _SUBVERSION_VCS_INTERFACE_H_
#define _SUBVERSION_VCS_INTERFACE_H_

#include <libanjuta/anjuta-utils.h>
#include <libanjuta/interfaces/ianjuta-vcs.h>

#include "svn-add-command.h"
#include "svn-checkout-command.h"
#include "svn-diff-command.h"
#include "svn-status-command.h"
#include "svn-remove-command.h"

#include "subversion-ui-utils.h"

void subversion_ivcs_iface_init (IAnjutaVcsIface *iface);
void subversion_ivcs_add (IAnjutaVcs *obj, GList *files, 
						  AnjutaAsyncNotify *notify, GError **err);
void subversion_ivcs_checkout (IAnjutaVcs *obj, 
							   const gchar *repository_location, GFile *dest,
							   GCancellable *cancel,
							   AnjutaAsyncNotify *notify, GError **err);
void subversion_ivcs_diff (IAnjutaVcs *obj, GFile* file,  
						   IAnjutaVcsDiffCallback callback, gpointer user_data,  
						   GCancellable* cancel, AnjutaAsyncNotify *notify,
						   GError **err);
void subversion_ivcs_query_status (IAnjutaVcs *obj, GFile *file, 
								   IAnjutaVcsStatusCallback callback,
								   gpointer user_data, GCancellable *cancel,
								   AnjutaAsyncNotify *notify, GError **err);
void subversion_ivcs_remove (IAnjutaVcs *obj, GList *files, 
							 AnjutaAsyncNotify *notify, GError **err);


#endif 
 