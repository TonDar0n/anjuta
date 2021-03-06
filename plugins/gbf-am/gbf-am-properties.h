/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8; coding: utf-8 -*- */
/* gbf-am-properties.h
 *
 * Copyright (C) 2005  Naba Kumar
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Naba Kumar
 */

#ifndef _GBF_AM_PROPERTIES_H_
#define _GBF_AM_PROPERTIES_H_

#include <gtk/gtkwidget.h>
#include "gbf-am-project.h"

GtkWidget *gbf_am_properties_get_widget (GbfAmProject *project, GError **error);
GtkWidget *gbf_am_properties_get_group_widget (GbfAmProject *project,
					       const gchar *group_id,
					       GError **error);
GtkWidget *gbf_am_properties_get_target_widget (GbfAmProject *project,
						const gchar *target_id,
						GError **error);

#endif
