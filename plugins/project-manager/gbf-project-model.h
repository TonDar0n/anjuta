/*  -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * Copyright (C) 2002 Dave Camp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA. 
 * 
 * Author: Dave Camp <dave@ximian.com> 
 */

#ifndef GBF_PROJECT_MODEL_H
#define GBF_PROJECT_MODEL_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <libanjuta/interfaces/libanjuta-interfaces.h>
#include <libanjuta/anjuta-project.h>
#include "gbf-tree-data.h"

#define GBF_TYPE_PROJECT_MODEL            (gbf_project_model_get_type ())
#define GBF_PROJECT_MODEL(obj)	          (G_TYPE_CHECK_INSTANCE_CAST ((obj), GBF_TYPE_PROJECT_MODEL, GbfProjectModel))
#define GBF_PROJECT_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GBF_TYPE_PROJECT_MODEL, GbfProjectModelClass))
#define GBF_IS_PROJECT_MODEL(obj)	      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GBF_TYPE_PROJECT_MODEL))
#define GBF_IS_PROJECT_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GBF_TYPE_PROJECT_MODEL))

typedef struct _GbfProjectModel        GbfProjectModel;
typedef struct _GbfProjectModelClass   GbfProjectModelClass;
typedef struct _GbfProjectModelPrivate GbfProjectModelPrivate;

enum {
	GBF_PROJECT_MODEL_COLUMN_DATA,
	GBF_PROJECT_MODEL_NUM_COLUMNS
};

struct _GbfProjectModel {
	GtkTreeStore parent;
	GbfProjectModelPrivate *priv;
};

struct _GbfProjectModelClass {
	GtkTreeStoreClass parent_class;
};

GType            gbf_project_model_get_type          (void); 
GbfProjectModel *gbf_project_model_new               (IAnjutaProject    *project);

void             gbf_project_model_set_project       (GbfProjectModel   *model,
                                                      IAnjutaProject    *project);
IAnjutaProject  *gbf_project_model_get_project       (GbfProjectModel   *model);

GtkTreePath     *gbf_project_model_get_project_root  (GbfProjectModel   *model);
gboolean         gbf_project_model_find_id           (GbfProjectModel   *model,
                                                      GtkTreeIter       *iter,
                                                      GbfTreeData       *data);
GbfTreeData     *gbf_project_model_find_uri          (GbfProjectModel   *model,
                                                      const gchar       *uri,
                                                      GbfTreeNodeType   type);
AnjutaProjectNode *gbf_project_model_get_node        (GbfProjectModel *model,
                                                      GtkTreeIter     *iter);

#endif
