/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * gprof-profile-data.c
 * Copyright (C) James Liggett 2006 <jrliggett@cox.net>
 * 
 * gprof-profile-data.c is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2, or (at your option) any later version.
 * 
 * plugin.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with plugin.c.  See the file "COPYING".  If not,
 * write to:  The Free Software Foundation, Inc.,
 *            59 Temple Place - Suite 330,
 *            Boston,  MA  02111-1307, USA.
 */

#include <libgnomevfs/gnome-vfs-mime.h>
#include "gprof-profile-data.h"

struct _GProfProfileDataPriv
{
	GProfFlatProfile *flat_profile;
	GProfCallGraph *call_graph;
};

static void 
gprof_profile_data_init (GProfProfileData *self)
{
	self->priv = g_new0 (GProfProfileDataPriv, 1);
}

static void
gprof_profile_data_finalize (GObject *obj)
{
	GProfProfileData *self;
	
	self = (GProfProfileData *) obj;
	
	if (self->priv->flat_profile)
		gprof_flat_profile_free (self->priv->flat_profile);
	
	if (self->priv->call_graph)
		gprof_call_graph_free (self->priv->call_graph);
	
	g_free (self->priv);
}

static void
gprof_profile_data_class_init (GProfProfileDataClass *klass)
{
	GObjectClass *object_class;
	
	object_class = (GObjectClass *) klass;
	
	object_class->finalize = gprof_profile_data_finalize;
}

GType
gprof_profile_data_get_type ()
{
	static GType obj_type = 0;
	
	if (!obj_type)
	{
		static const GTypeInfo obj_info = 
		{
			sizeof (GProfProfileDataClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gprof_profile_data_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,           /* class_self */
			sizeof (GProfProfileData),
			0,              /* n_preallocs */
			(GInstanceInitFunc) gprof_profile_data_init,
			NULL            /* value_table */
		};
		obj_type = g_type_register_static (G_TYPE_OBJECT,
		                                   "GProfProfileData", &obj_info, 0);
	}
	return obj_type;
}

GProfProfileData *
gprof_profile_data_new ()
{
	return g_object_new (GPROF_PROFILE_DATA_TYPE, NULL);;
}

void
gprof_profile_data_free (GProfProfileData *self)
{
	g_object_unref (self);
}

gboolean
gprof_profile_data_init_profile (GProfProfileData *self, gchar *path,
								 GPtrArray *options)
{
	gint stdout_pipe;
	gint i;
	FILE *stdout_stream;
	gchar *program_dir;
	gchar *gmon_out_path;
	GPtrArray *gprof_args;
	gchar *mime_type;
	gboolean is_libtool_target = FALSE;
	GPid gprof_pid;
	gint gprof_status;
	
	/* Determine target mime type */
	mime_type = gnome_vfs_get_mime_type (path);
	if (strcmp (mime_type, "application/x-shellscript") == 0)
		is_libtool_target = TRUE;
	g_free (mime_type);
	
	/* Run gprof with -b given the path to a program run with profiling */
	
	gprof_args = g_ptr_array_sized_new ((options->len - 1) + 7);
	if (is_libtool_target)
	{
		g_ptr_array_add (gprof_args, "libtool");
		g_ptr_array_add (gprof_args, "--mode=execute");
	}
	g_ptr_array_add (gprof_args, "gprof");
	g_ptr_array_add (gprof_args, "-b");
	
	/* Add options */
	
	for (i = 0; i < options->len - 1; i++)
		g_ptr_array_add (gprof_args, g_ptr_array_index (options, i));
	
	g_ptr_array_add (gprof_args, path);
	
	/* Also give the path of the gmon.out file */
	
	program_dir = g_path_get_dirname (path);
	gmon_out_path = g_build_filename (program_dir, "gmon.out", NULL);
	g_ptr_array_add (gprof_args, gmon_out_path);
	g_ptr_array_add (gprof_args, NULL);
	
	g_spawn_async_with_pipes (NULL, (gchar **) gprof_args->pdata, 
							  NULL, 
							  G_SPAWN_SEARCH_PATH | 
							  G_SPAWN_DO_NOT_REAP_CHILD |
							  G_SPAWN_STDERR_TO_DEV_NULL, 
							  NULL, NULL, &gprof_pid, NULL, &stdout_pipe, 
							  NULL, NULL);
	
	g_ptr_array_free (gprof_args, TRUE);
	g_free (gmon_out_path);
	g_free (program_dir);

	stdout_stream = fdopen (stdout_pipe, "r");

	if (self->priv->flat_profile)
		gprof_flat_profile_free (self->priv->flat_profile);

	self->priv->flat_profile = gprof_flat_profile_new (stdout_stream);

	if (self->priv->call_graph)
		gprof_call_graph_free (self->priv->call_graph);

	self->priv->call_graph = gprof_call_graph_new (stdout_stream, 
												   self->priv->flat_profile);	
	
	fclose (stdout_stream);
	close (stdout_pipe);
	
	waitpid (gprof_pid, &gprof_status, 0);
	g_spawn_close_pid (gprof_pid);

	if (WIFEXITED (gprof_status) && WEXITSTATUS (gprof_status) != 0)
		return FALSE;
	
	return TRUE;
}

GProfFlatProfile *
gprof_profile_data_get_flat_profile (GProfProfileData *self)
{
	return self->priv->flat_profile;
}

GProfCallGraph *
gprof_profile_data_get_call_graph (GProfProfileData *self)
{
	return self->priv->call_graph;
}

gboolean
gprof_profile_data_has_data (GProfProfileData *self)
{
	return (self->priv->flat_profile != NULL) &&
		   (self->priv->call_graph != NULL);
}