/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    plugin.c
    Copyright (C) 2000 Naba Kumar

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

#include <config.h>
#include <libanjuta/interfaces/libanjuta-interfaces.h>
#include <libanjuta/anjuta-profile-manager.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-status.h>
#include <libanjuta/anjuta-project.h>

#include "gbf-project-util.h"

#include "plugin.h"

#define UI_FILE PACKAGE_DATA_DIR"/ui/anjuta-project-manager.xml"
#define PREFS_GLADE PACKAGE_DATA_DIR"/glade/anjuta-project-manager-plugin.ui"
#define ICON_FILE "anjuta-project-manager-plugin-48.png"
#define DEFAULT_PROFILE "file://"PACKAGE_DATA_DIR"/profiles/default.profile"
#define PROJECT_PROFILE_NAME "project"

typedef struct _PmPropertiesDialogInfo PmPropertiesDialogInfo;

typedef enum _PmPropertiesType
{
	PM_PROJECT_PROPERTIES,
	PM_TARGET_PROPERTIES,
	PM_GROUP_PROPERTIES
} PmPropertiesType;

struct _PmPropertiesDialogInfo
{
	PmPropertiesType type;
	AnjutaProjectNode* id;
	GtkWidget* dialog;
};

static gpointer parent_class;

static gboolean uri_is_inside_project (ProjectManagerPlugin *plugin,
									   const gchar *uri);
static void project_manager_plugin_close (ProjectManagerPlugin *plugin);

static GtkWindow*
get_plugin_parent_window (ProjectManagerPlugin *plugin)
{
	GtkWindow *win;
	GtkWidget *toplevel;
	
	toplevel = gtk_widget_get_toplevel (plugin->scrolledwindow);
	if (toplevel && GTK_IS_WINDOW (toplevel))
		win = GTK_WINDOW (toplevel);
	else
		win = GTK_WINDOW (ANJUTA_PLUGIN (plugin)->shell);
	return win;
}

static void
update_title (ProjectManagerPlugin* plugin, const gchar *project_uri)
{
	AnjutaStatus *status;
	status = anjuta_shell_get_status (ANJUTA_PLUGIN (plugin)->shell, NULL);
	if (project_uri)
	{
		GFile *file;
		GFileInfo *file_info;

		file = g_file_new_for_uri (project_uri);
		file_info = g_file_query_info (file,
			G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
			G_FILE_QUERY_INFO_NONE,
			NULL,
			NULL);
		if (file_info)
		{
			gchar *dispname;
			gchar *ext;
			
			dispname = g_strdup (
				g_file_info_get_display_name (file_info));
			ext = strrchr (dispname, '.');	
			if (ext)
				*ext = '\0';
			anjuta_status_set_title (status, dispname);
			g_free (dispname);
			g_object_unref (file_info);
		}

		g_object_unref (file);
	}
	else
	{
		anjuta_status_set_title (status, NULL);
	}
}

static gchar*
get_session_dir (ProjectManagerPlugin *plugin)
{
	gchar *session_dir = NULL;
	gchar *local_dir;
	
	g_return_val_if_fail (plugin->project_root_uri, NULL);
	
	local_dir = anjuta_util_get_local_path_from_uri (plugin->project_root_uri);
	if (local_dir)
	{
		session_dir = g_build_filename (local_dir, ".anjuta", "session",
										NULL);
	}
	g_free (local_dir);
	
	return session_dir;
}

static void
project_manager_save_session (ProjectManagerPlugin *plugin)
{
	gchar *session_dir;
	session_dir = get_session_dir (plugin);
	g_return_if_fail (session_dir != NULL);
	
	plugin->session_by_me = TRUE;
	anjuta_shell_session_save (ANJUTA_PLUGIN (plugin)->shell,
							   session_dir, NULL);
	plugin->session_by_me = FALSE;
	g_free (session_dir);
}

static void
on_session_save (AnjutaShell *shell, AnjutaSessionPhase phase,
				 AnjutaSession *session, ProjectManagerPlugin *plugin)
{
	GList *files;

	if (phase != ANJUTA_SESSION_PHASE_NORMAL)
		return;
	
	/*
	 * When a project session is being saved (session_by_me == TRUE),
	 * we should not save the current project uri, because project 
	 * sessions are loaded when the project has already been loaded.
	 */
	if (plugin->project_uri && !plugin->session_by_me)
	{
		files = anjuta_session_get_string_list (session,
												"File Loader",
												"Files");
		files = g_list_append (files, g_strdup (plugin->project_uri));
		anjuta_session_set_string_list (session, "File Loader",
										"Files", files);
		g_list_foreach (files, (GFunc)g_free, NULL);
		g_list_free (files);
	}
}

static void
on_shell_exiting (AnjutaShell *shell, ProjectManagerPlugin *plugin)
{
	if (plugin->project_uri)
	{
		/* Also make sure we save the project session also */
		project_manager_save_session (plugin);
	}
}

static gboolean
on_close_project_idle (gpointer plugin)
{
	project_manager_plugin_close (ANJUTA_PLUGIN_PROJECT_MANAGER (plugin));
	ANJUTA_PLUGIN_PROJECT_MANAGER(plugin)->close_project_idle = -1;
	
	return FALSE;
}

static void
on_close_project (GtkAction *action, ProjectManagerPlugin *plugin)
{
	if (plugin->project_uri)
		plugin->close_project_idle = g_idle_add (on_close_project_idle, plugin);
}

static GList *
find_missing_uris (GList *pre, GList *post)
{
	GHashTable *hash;
	GList *ret = NULL;
	GList *node;
	
	hash = g_hash_table_new (g_str_hash, g_str_equal);
	node = pre;
	while (node)
	{
		g_hash_table_insert (hash, node->data, node->data);
		node = g_list_next (node);
	}
	
	node = post;
	while (node)
	{
		if (g_hash_table_lookup (hash, node->data) == NULL)
			ret = g_list_prepend (ret, node->data);
		node = g_list_next (node);
	}
	g_hash_table_destroy (hash);
	return g_list_reverse (ret);
}

static void
update_operation_emit_signals (ProjectManagerPlugin *plugin, GList *pre,
							   GList *post)
{
	GList *missing_uris, *node;
	
	missing_uris = find_missing_uris (pre, post);
	node = missing_uris;
	while (node)
	{
		DEBUG_PRINT ("URI added emitting: %s", (char*)node->data);
		g_signal_emit_by_name (G_OBJECT (plugin), "element_added",
							   node->data);
		node = g_list_next (node);
	}
	g_list_free (missing_uris);
	
	missing_uris = find_missing_uris (post, pre);
	node = missing_uris;
	while (node)
	{
		DEBUG_PRINT ("URI removed emitting: %s", (char*)node->data);
		g_signal_emit_by_name (G_OBJECT (plugin), "element_removed",
							   node->data);
		node = g_list_next (node);
	}
	g_list_free (missing_uris);
}

static void
update_operation_end (ProjectManagerPlugin *plugin, gboolean emit_signal)
{
	if (emit_signal)
	{
		if (plugin->pre_update_sources)
		{
			GList *post_update_sources =
			ianjuta_project_manager_get_elements (IANJUTA_PROJECT_MANAGER (plugin),
												  ANJUTA_PROJECT_SOURCE,
												  NULL);
			update_operation_emit_signals (plugin, plugin->pre_update_sources,
										   post_update_sources);
			if (post_update_sources)
			{
				g_list_foreach (post_update_sources, (GFunc)g_free, NULL);
				g_list_free (post_update_sources);
			}
		}
		if (plugin->pre_update_targets)
		{
			GList *post_update_targets =
			ianjuta_project_manager_get_elements (IANJUTA_PROJECT_MANAGER (plugin),
												  ANJUTA_PROJECT_TARGET,
												  NULL);
			update_operation_emit_signals (plugin, plugin->pre_update_targets,
										   post_update_targets);
			if (post_update_targets)
			{
				g_list_foreach (post_update_targets, (GFunc)g_free, NULL);
				g_list_free (post_update_targets);
			}
		}
		if (plugin->pre_update_groups)
		{
			GList *post_update_groups =
			ianjuta_project_manager_get_elements (IANJUTA_PROJECT_MANAGER (plugin),
												  ANJUTA_PROJECT_GROUP,
												  NULL);
			update_operation_emit_signals (plugin, plugin->pre_update_groups,
										   post_update_groups);
			if (post_update_groups)
			{
				g_list_foreach (post_update_groups, (GFunc)g_free, NULL);
				g_list_free (post_update_groups);
			}
		}
	}
	if (plugin->pre_update_sources)
	{
		g_list_foreach (plugin->pre_update_sources, (GFunc)g_free, NULL);
		g_list_free (plugin->pre_update_sources);
		plugin->pre_update_sources = NULL;
	}
	if (plugin->pre_update_targets)
	{
		g_list_foreach (plugin->pre_update_targets, (GFunc)g_free, NULL);
		g_list_free (plugin->pre_update_targets);
		plugin->pre_update_targets = NULL;
	}
	if (plugin->pre_update_groups)
	{
		g_list_foreach (plugin->pre_update_groups, (GFunc)g_free, NULL);
		g_list_free (plugin->pre_update_groups);
		plugin->pre_update_groups = NULL;
	}
}

static void
update_operation_begin (ProjectManagerPlugin *plugin)
{
	update_operation_end (plugin, FALSE);
	plugin->pre_update_sources =
	ianjuta_project_manager_get_elements (IANJUTA_PROJECT_MANAGER (plugin),
										  ANJUTA_PROJECT_SOURCE,
										  NULL);
	plugin->pre_update_targets =
	ianjuta_project_manager_get_elements (IANJUTA_PROJECT_MANAGER (plugin),
										  ANJUTA_PROJECT_TARGET,
										  NULL);
	plugin->pre_update_groups =
	ianjuta_project_manager_get_elements (IANJUTA_PROJECT_MANAGER (plugin),
										  ANJUTA_PROJECT_GROUP,
										  NULL);
}

/* Properties dialogs functions
 *---------------------------------------------------------------------------*/

static void
on_properties_dialog_response (GtkDialog *win,
							   gint id,
							   GtkWidget **dialog)
{
	gtk_widget_destroy (*dialog);
	*dialog = NULL;
}

static void
project_manager_create_properties_dialog (ProjectManagerPlugin *plugin,
    									  GtkWidget **dialog,
    								      const gchar *title,
    									  GtkWidget *properties)
{
	*dialog = gtk_dialog_new_with_buttons (title,
							   GTK_WINDOW (ANJUTA_PLUGIN(plugin)->shell),
							   GTK_DIALOG_DESTROY_WITH_PARENT,
							   GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);

	g_signal_connect (*dialog, "response",
					  G_CALLBACK (on_properties_dialog_response),
					  dialog);

	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG(*dialog))),
			   properties);
	gtk_window_set_default_size (GTK_WINDOW (*dialog), 450, -1);
	gtk_widget_show (*dialog);
}

/* Display properties dialog. These dialogs are not modal, so a pointer on each
 * dialog is kept with in node data to be able to destroy them if the node is
 * removed. It is useful to put the dialog at the top if the same target is
 * selected while the corresponding dialog already exist instead of creating
 * two times the same dialog.
 * The project properties dialog is display if the node iterator is NULL. */

static void
project_manager_show_node_properties_dialog (ProjectManagerPlugin *plugin,
    									GbfTreeData *data)
{
	if (data == NULL) return;

	if (data->is_shortcut) data = data->shortcut;
	
	if (data->properties_dialog != NULL)
	{
		/* Show already existing dialog */
		gtk_window_present (GTK_WINDOW (data->properties_dialog));
	}
	else
	{
		GtkWidget *properties = NULL;
		const char *title;
		AnjutaProjectNode *node;

		switch (data->type)
		{
		case GBF_TREE_NODE_GROUP:
			title = _("Group properties");
			node = gbf_tree_data_get_node (data, plugin->project);
			if (node != NULL)
			{
				properties = ianjuta_project_configure_node (plugin->project, node, NULL);

				if (properties == NULL)
				{
					anjuta_util_dialog_info (GTK_WINDOW (ANJUTA_PLUGIN(plugin)->shell),
								 _("No properties available for this group"));
				}
			}
			break;
		case GBF_TREE_NODE_TARGET:
			title = _("Target properties");
			node = gbf_tree_data_get_node (data, plugin->project);
			if (node != NULL)
			{
				properties = ianjuta_project_configure_node (plugin->project, node, NULL);

				if (properties == NULL)
				{
					anjuta_util_dialog_info (GTK_WINDOW (ANJUTA_PLUGIN(plugin)->shell),
							 _("No properties available for this target"));
				}
			}
			break;
		default:
			break;
		}

		if (properties)
		{
			project_manager_create_properties_dialog(plugin,
			    &data->properties_dialog,
		    	title,
		    	properties);
		}
	}
}

static void
project_manager_show_project_properties_dialog (ProjectManagerPlugin *plugin)
{
	/* Project configuration dialog */
	
	if (plugin->properties_dialog != NULL)
	{
		/* Show already existing dialog */
		gtk_window_present (GTK_WINDOW (plugin->properties_dialog));
	}
	else
	{
		project_manager_create_properties_dialog(plugin,
		    &plugin->properties_dialog,
		    _("Project properties"),
		    ianjuta_project_configure (plugin->project, NULL));
	}
}


/* GUI callbacks
 *---------------------------------------------------------------------------*/

static gboolean
on_refresh_idle (gpointer user_data)
{
	ProjectManagerPlugin *plugin;
	AnjutaStatus *status;
	GError *err = NULL;
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (user_data);
	
	status = anjuta_shell_get_status (ANJUTA_PLUGIN (plugin)->shell, NULL);
	anjuta_status_push (status, _("Refreshing symbol tree…"));
	anjuta_status_busy_push (status);
	
	ianjuta_project_refresh (IANJUTA_PROJECT (plugin->project), &err);
	if (err)
	{
		anjuta_util_dialog_error (get_plugin_parent_window (plugin),
								  _("Failed to refresh project: %s"),
								  err->message);
		g_error_free (err);
	}
	anjuta_status_busy_pop (status);
	anjuta_status_pop (status);
	return FALSE;
}

static void
on_refresh (GtkAction *action, ProjectManagerPlugin *plugin)
{
	g_idle_add (on_refresh_idle, plugin);
}

static void
on_properties (GtkAction *action, ProjectManagerPlugin *plugin)
{
	project_manager_show_project_properties_dialog (plugin); 
}

static void
on_add_group (GtkAction *action, ProjectManagerPlugin *plugin)
{
	gchar *group_uri, *default_group_uri = NULL;
	if (plugin->current_editor_uri)
		default_group_uri = g_path_get_dirname (plugin->current_editor_uri);
	else
		default_group_uri = g_strdup ("");
	group_uri =
		ianjuta_project_manager_add_group (IANJUTA_PROJECT_MANAGER (plugin),
										   "", default_group_uri,
										   NULL);
	g_free (group_uri);
	g_free (default_group_uri);
}

static void
on_add_target (GtkAction *action, ProjectManagerPlugin *plugin)
{
	gchar *target_uri, *default_group_uri = NULL;
	if (plugin->current_editor_uri)
		default_group_uri = g_path_get_dirname (plugin->current_editor_uri);
	else
		default_group_uri = g_strdup ("");
	target_uri =
		ianjuta_project_manager_add_target (IANJUTA_PROJECT_MANAGER (plugin),
											"", default_group_uri,
											NULL);
	g_free (target_uri);
	g_free (default_group_uri);
}

static void
on_add_source (GtkAction *action, ProjectManagerPlugin *plugin)
{
	gchar *default_group_uri = NULL, *default_source_uri;
	gchar* source_uri;
	
	if (plugin->current_editor_uri)
	{
		default_group_uri = g_path_get_dirname (plugin->current_editor_uri);
		default_source_uri = plugin->current_editor_uri;
	}
	else
	{
		default_group_uri = g_strdup ("");
		default_source_uri = "";
	}
	source_uri =
		ianjuta_project_manager_add_source (IANJUTA_PROJECT_MANAGER (plugin),
											default_source_uri,
											default_group_uri, NULL);
	g_free (source_uri);
	g_free (default_group_uri);
}

static void
on_popup_properties (GtkAction *action, ProjectManagerPlugin *plugin)
{
	GbfTreeData *data;

	data = gbf_project_view_get_selected (GBF_PROJECT_VIEW (plugin->view));
	if (data)
	{
		project_manager_show_node_properties_dialog (plugin, data);
	}
}

static void
on_popup_add_group (GtkAction *action, ProjectManagerPlugin *plugin)
{
	GbfTreeData *selected_group;
	AnjutaProjectGroup *new_group;
	
	update_operation_begin (plugin);
	selected_group = gbf_project_view_get_selected (GBF_PROJECT_VIEW (plugin->view));
	
	new_group = gbf_project_util_new_group (plugin->model,
										   get_plugin_parent_window (plugin),
										   selected_group, NULL);
	update_operation_end (plugin, TRUE);
}

static void
on_popup_add_target (GtkAction *action, ProjectManagerPlugin *plugin)
{
	GbfTreeData *selected_group;
	AnjutaProjectTarget *new_target;

	update_operation_begin (plugin);
	selected_group = gbf_project_view_get_selected (GBF_PROJECT_VIEW (plugin->view));

	new_target = gbf_project_util_new_target (plugin->model,
											 get_plugin_parent_window (plugin),
											 selected_group, NULL);
	
	update_operation_end (plugin, TRUE);
}

static void
on_popup_add_source (GtkAction *action, ProjectManagerPlugin *plugin)
{
	GbfTreeData *selected_target;
	AnjutaProjectSource *new_source;
	
	update_operation_begin (plugin);
	selected_target = gbf_project_view_get_selected (GBF_PROJECT_VIEW (plugin->view));

	new_source = gbf_project_util_add_source (plugin->model,
											 get_plugin_parent_window (plugin),
											 selected_target, NULL);

	update_operation_end (plugin, TRUE);
}

static gboolean
confirm_removal (ProjectManagerPlugin *plugin, AnjutaProjectNode *node)
{
	gboolean answer;
	gchar *mesg;
	gchar* question;
	gchar* full_mesg;
	gchar* name;


	name = anjuta_project_node_get_name (node);
	switch (anjuta_project_node_get_type (node))
	{
		case ANJUTA_PROJECT_GROUP:
			question = _("Are you sure you want to remove the following group from the project?\n\n");
			mesg = _("Group: %s\n\nThe group will not be deleted from the file system.");
			break;
		case ANJUTA_PROJECT_TARGET:
			question = _("Are you sure you want to remove the following target from the project?\n\n");
			mesg = _("Target: %s");
			break;
		case ANJUTA_PROJECT_SOURCE:
			question = _("Are you sure you want to remove the following source file from the project?\n\n");
			mesg = _("Source: %s\n\nThe source file will not be deleted from the file system.");
			break;
		default:
			g_warning ("Unknown node");
			return FALSE;
	}
	full_mesg = g_strconcat (question, mesg, NULL);
	answer =
		anjuta_util_dialog_boolean_question (get_plugin_parent_window (plugin),
											 full_mesg, name);
	g_free (name);
	g_free (full_mesg);
	return answer;
}

static void
on_popup_remove (GtkAction *action, ProjectManagerPlugin *plugin)
{
	AnjutaProjectNode *node;
	
	node = gbf_project_view_find_selected (GBF_PROJECT_VIEW (plugin->view),
										   ANJUTA_PROJECT_SOURCE);
	if (node == NULL)
	{
		node = gbf_project_view_find_selected (GBF_PROJECT_VIEW (plugin->view),
											   ANJUTA_PROJECT_TARGET);
	}
	if (node == NULL)
	{
		node = gbf_project_view_find_selected (GBF_PROJECT_VIEW (plugin->view),
											   ANJUTA_PROJECT_GROUP);
	}
	if (node)
	{
		if (confirm_removal (plugin, node))
		{
			GError *err = NULL;
			update_operation_begin (plugin);
			ianjuta_project_remove_node (plugin->project, node, &err);
			update_operation_end (plugin, TRUE);
			if (err)
			{
				gchar *name = anjuta_project_node_get_name (node);
				anjuta_util_dialog_error (get_plugin_parent_window (plugin),
										  _("Failed to remove '%s':\n%s"),
										  name, err->message);
				g_free (name);
				g_error_free (err);
			}
		}
	}
}

static void
on_popup_add_to_project (GtkAction *action, ProjectManagerPlugin *plugin)
{
	GtkWindow *win;
	GFile *file;
	GFileInfo *file_info;
	gchar *parent_directory, *filename;
	GError *error = NULL;
	
	win = get_plugin_parent_window (plugin);

	file = g_file_new_for_uri (plugin->fm_current_uri);
	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_STANDARD_TYPE,
								   G_FILE_QUERY_INFO_NONE,
								   NULL,
								   &error);
	if (file_info != NULL)
	{
		parent_directory = g_path_get_dirname (plugin->fm_current_uri);
		if (!parent_directory)
			parent_directory = g_strdup ("");
		
		filename = g_path_get_basename (plugin->fm_current_uri);
		if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY)
		{
			gchar *new_uri =
			ianjuta_project_manager_add_group (IANJUTA_PROJECT_MANAGER (plugin),
											   filename, parent_directory,
											   NULL);
			g_free (new_uri);
		}
		else
		{
			gchar *new_uri =
				ianjuta_project_manager_add_source (IANJUTA_PROJECT_MANAGER
													(plugin),
													plugin->fm_current_uri,
													parent_directory,
													NULL);
			g_free (new_uri);
		}
		g_object_unref (file_info);
		g_free (filename);
		g_free (parent_directory);
	}
	else
	{
		anjuta_util_dialog_error (win, _("Failed to retrieve URI info of %s: %s"),
					  plugin->fm_current_uri, error->message);
		g_error_free (error);
	}
}

static void
on_uri_activated (GtkWidget *widget, const gchar *uri,
				  ProjectManagerPlugin *plugin)
{
	IAnjutaFileLoader *loader;
	GFile* file = g_file_new_for_uri (uri);
	
	loader = anjuta_shell_get_interface (ANJUTA_PLUGIN (plugin)->shell,
										 IAnjutaFileLoader, NULL);
	if (loader)
		ianjuta_file_loader_load (loader, file, FALSE, NULL);
	g_object_unref (file);
}

static void
on_target_activated (GtkWidget *widget, const gchar *target_id,
					 ProjectManagerPlugin *plugin)
{
	on_popup_properties (NULL, plugin);
}

static void
on_group_activated (GtkWidget *widget, const gchar *group_id,
					 ProjectManagerPlugin *plugin)
{
	on_popup_properties (NULL, plugin);
}

static GtkActionEntry pm_actions[] = 
{
	{
		"ActionMenuProject", NULL,
		N_("_Project"), NULL, NULL, NULL
	},
	{
		"ActionProjectProperties", GTK_STOCK_PROPERTIES,
		N_("_Properties"), NULL, N_("Project properties"),
		G_CALLBACK (on_properties)
	},
	{
		"ActionProjectRefresh", GTK_STOCK_REFRESH,
		N_("_Refresh"), NULL, N_("Refresh project manager tree"),
		G_CALLBACK (on_refresh)
	},
	{
		"ActionProjectAddGroup", GTK_STOCK_ADD,
		N_("Add _Group…"), NULL, N_("Add a group to project"),
		G_CALLBACK (on_add_group)
	},
	{
		"ActionProjectAddTarget", GTK_STOCK_ADD,
		N_("Add _Target…"), NULL, N_("Add a target to project"),
		G_CALLBACK (on_add_target)
	},
	{
		"ActionProjectAddSource", GTK_STOCK_ADD,
		N_("Add _Source File…"), NULL, N_("Add a source file to project"),
		G_CALLBACK (on_add_source)
	},
	{
		"ActionFileCloseProject", NULL,
		N_("Close Pro_ject"), NULL, N_("Close project"),
		G_CALLBACK (on_close_project)
	},
};

static GtkActionEntry popup_actions[] = 
{
	{
		"ActionPopupProjectProperties", GTK_STOCK_PROPERTIES,
		N_("_Properties"), NULL, N_("Properties of group/target/source"),
		G_CALLBACK (on_popup_properties)
	},
	{
		"ActionPopupProjectAddToProject", GTK_STOCK_ADD,
		N_("_Add to Project"), NULL, N_("Add a source file to project"),
		G_CALLBACK (on_popup_add_to_project)
	},
	{
		"ActionPopupProjectAddGroup", GTK_STOCK_ADD,
		N_("Add _Group"), NULL, N_("Add a group to project"),
		G_CALLBACK (on_popup_add_group)
	},
	{
		"ActionPopupProjectAddTarget", GTK_STOCK_ADD,
		N_("Add _Target"), NULL, N_("Add a target to project"),
		G_CALLBACK (on_popup_add_target)
	},
	{
		"ActionPopupProjectAddSource", GTK_STOCK_ADD,
		N_("Add _Source File"), NULL, N_("Add a source file to project"),
		G_CALLBACK (on_popup_add_source)
	},
	{
		"ActionPopupProjectRemove", GTK_STOCK_REMOVE,
		N_("Re_move"), NULL, N_("Remove from project"),
		G_CALLBACK (on_popup_remove)
	}
};

static void
update_ui (ProjectManagerPlugin *plugin)
{
	AnjutaUI *ui;
	gint j;
	GtkAction *action;
	IAnjutaProjectCapabilities caps = IANJUTA_PROJECT_CAN_ADD_NONE;
	
	if (plugin->project)
		caps = ianjuta_project_get_capabilities (plugin->project, NULL);
	
	ui = anjuta_shell_get_ui (ANJUTA_PLUGIN (plugin)->shell, NULL);
	for (j = 0; j < G_N_ELEMENTS (pm_actions); j++)
	{
		action = anjuta_ui_get_action (ui, "ActionGroupProjectManager",
									   pm_actions[j].name);
		if (pm_actions[j].callback &&
			strcmp (pm_actions[j].name, "ActionFileCloseProject") != 0)
		{
			/* 'close' menuitem is never disabled */
			g_object_set (G_OBJECT (action), "sensitive",
						  (plugin->project != NULL), NULL);
		}
	}
	
	/* Main menu */
	action = anjuta_ui_get_action (ui, "ActionGroupProjectManager",
								   "ActionProjectAddGroup");
	g_object_set (G_OBJECT (action), "sensitive",
				  ((plugin->project != NULL) &&
				   (caps & IANJUTA_PROJECT_CAN_ADD_GROUP)), NULL);
	
	action = anjuta_ui_get_action (ui, "ActionGroupProjectManager",
								   "ActionProjectAddTarget");
	g_object_set (G_OBJECT (action), "sensitive",
				  ((plugin->project != NULL) &&
				   (caps & IANJUTA_PROJECT_CAN_ADD_TARGET)), NULL);

	action = anjuta_ui_get_action (ui, "ActionGroupProjectManager",
								   "ActionProjectAddSource");
	g_object_set (G_OBJECT (action), "sensitive",
				  ((plugin->project != NULL) &&
				   (caps & IANJUTA_PROJECT_CAN_ADD_SOURCE)), NULL);

	/* Popup menus */
	for (j = 0; j < G_N_ELEMENTS (popup_actions); j++)
	{
		action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
									   popup_actions[j].name);
		if (popup_actions[j].callback)
		{
			g_object_set (G_OBJECT (action), "sensitive",
						  (plugin->project != NULL), NULL);
		}
	}
}

static void
on_treeview_selection_changed (GtkTreeSelection *sel,
							   ProjectManagerPlugin *plugin)
{
	AnjutaUI *ui;
	GtkAction *action;
	AnjutaProjectNode *node;
	gchar *selected_uri;
	IAnjutaProjectCapabilities caps = IANJUTA_PROJECT_CAN_ADD_NONE;
	
	ui = anjuta_shell_get_ui (ANJUTA_PLUGIN (plugin)->shell, NULL);
	/* Popup menu */
	action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
								   "ActionPopupProjectAddGroup");
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
	action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
								   "ActionPopupProjectAddTarget");
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
	action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
								   "ActionPopupProjectAddSource");
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
	action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
								   "ActionPopupProjectRemove");
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
	
	if (plugin->project)
		caps = ianjuta_project_get_capabilities (plugin->project, NULL);
	node = gbf_project_view_find_selected (GBF_PROJECT_VIEW (plugin->view),
										   ANJUTA_PROJECT_SOURCE);
	if (node && anjuta_project_node_get_type (node) == ANJUTA_PROJECT_SOURCE)
	{
		if (caps & IANJUTA_PROJECT_CAN_ADD_SOURCE)
		{
			action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
										   "ActionPopupProjectAddSource");
			g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);
		}
		action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
									   "ActionPopupProjectRemove");
		g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);
		goto finally;
	}
	
	node = gbf_project_view_find_selected (GBF_PROJECT_VIEW (plugin->view),
										   ANJUTA_PROJECT_TARGET);
	if (node && anjuta_project_node_get_type (node) == ANJUTA_PROJECT_TARGET)
	{
		if (caps & IANJUTA_PROJECT_CAN_ADD_SOURCE)
		{
			action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
										   "ActionPopupProjectAddSource");
			g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);
		}
		action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
									   "ActionPopupProjectRemove");
		g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);
		goto finally;
	}
	
	node = gbf_project_view_find_selected (GBF_PROJECT_VIEW (plugin->view),
										   ANJUTA_PROJECT_GROUP);
	if (node && anjuta_project_node_get_type (node) == ANJUTA_PROJECT_GROUP)
	{
		if (caps & IANJUTA_PROJECT_CAN_ADD_GROUP)
		{
			action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
										   "ActionPopupProjectAddGroup");
			g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);
		}
		if (caps & IANJUTA_PROJECT_CAN_ADD_TARGET)
		{
			action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
										   "ActionPopupProjectAddTarget");
			g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);
		}
		action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
									   "ActionPopupProjectRemove");
		g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);
		goto finally;
	}
finally:
	selected_uri =
		ianjuta_project_manager_get_selected (IANJUTA_PROJECT_MANAGER (plugin),
											  NULL);
	if (selected_uri)
	{
		GValue *value;
		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, selected_uri);
		anjuta_shell_add_value (ANJUTA_PLUGIN(plugin)->shell,
								IANJUTA_PROJECT_MANAGER_CURRENT_URI,
								value, NULL);
		g_signal_emit_by_name (G_OBJECT (plugin), "element_selected",
							   selected_uri);
		g_free (selected_uri);
	} else {
		anjuta_shell_remove_value (ANJUTA_PLUGIN(plugin)->shell,
								   IANJUTA_PROJECT_MANAGER_CURRENT_URI, NULL);
	}
}

static gboolean
on_treeview_event  (GtkWidget *widget,
					 GdkEvent  *event,
					 ProjectManagerPlugin *plugin)
{
	AnjutaUI *ui;
	
	ui = anjuta_shell_get_ui (ANJUTA_PLUGIN (plugin)->shell, NULL);
	if (event->type == GDK_BUTTON_PRESS) {
		GdkEventButton *e = (GdkEventButton *) event;
		if (e->button == 3) {
			GtkWidget *popup;
			popup = gtk_ui_manager_get_widget (GTK_UI_MANAGER (ui),
											   "/PopupProjectManager");
			g_return_val_if_fail (GTK_IS_WIDGET (popup), FALSE);
			gtk_menu_popup (GTK_MENU (popup),
					NULL, NULL, NULL, NULL,
					((GdkEventButton *) event)->button,
					((GdkEventButton *) event)->time);
			return TRUE;
		}
	}
	return FALSE;
}

static void
register_stock_icons (AnjutaPlugin *plugin)
{
	static gboolean registered = FALSE;

	if (registered)
		return;
	registered = TRUE;

	/* Register stock icons */
	BEGIN_REGISTER_ICON(plugin);
	REGISTER_ICON (ICON_FILE,
				   "project-manager-plugin-icon");
	END_REGISTER_ICON;
}

static void
value_added_fm_current_file (AnjutaPlugin *plugin, const char *name,
							const GValue *value, gpointer data)
{
	AnjutaUI *ui;
	GtkAction *action;
	gchar *uri;
	ProjectManagerPlugin *pm_plugin;
	
	GFile* file = g_value_get_object (value);
	uri = g_file_get_uri (file);

	pm_plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (plugin);
	ui = anjuta_shell_get_ui (plugin->shell, NULL);
	
	if (pm_plugin->fm_current_uri)
		g_free (pm_plugin->fm_current_uri);
	pm_plugin->fm_current_uri = g_strdup (uri);
	
	action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
								   "ActionPopupProjectAddToProject");
	g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);
	g_free (uri);
}

static void
value_removed_fm_current_file (AnjutaPlugin *plugin,
							  const char *name, gpointer data)
{
	AnjutaUI *ui;
	GtkAction *action;
	ProjectManagerPlugin *pm_plugin;

	pm_plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (plugin);
	
	if (pm_plugin->fm_current_uri)
		g_free (pm_plugin->fm_current_uri);
	pm_plugin->fm_current_uri = NULL;
	
	ui = anjuta_shell_get_ui (plugin->shell, NULL);
	action = anjuta_ui_get_action (ui, "ActionGroupProjectManagerPopup",
								   "ActionPopupProjectAddToProject");
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
}

static void
value_added_current_editor (AnjutaPlugin *plugin, const char *name,
							const GValue *value, gpointer data)
{
	GObject *editor;
	ProjectManagerPlugin *pm_plugin;
	GFile* file;
	
	editor = g_value_get_object (value);
	if (!IANJUTA_IS_EDITOR(editor))
		return;
	
	pm_plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (plugin);
	
	if (pm_plugin->current_editor_uri)
		g_free (pm_plugin->current_editor_uri);
	file = ianjuta_file_get_file (IANJUTA_FILE (editor), NULL);
	if (file)
	{
		pm_plugin->current_editor_uri = g_file_get_uri (file);
		g_object_unref (file);
	}
	else
		pm_plugin->current_editor_uri = NULL;

}

static void
value_removed_current_editor (AnjutaPlugin *plugin,
							  const char *name, gpointer data)
{
	ProjectManagerPlugin *pm_plugin;
	
	pm_plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (plugin);
	
	if (pm_plugin->current_editor_uri)
		g_free (pm_plugin->current_editor_uri);
	pm_plugin->current_editor_uri = NULL;
}

static void
project_manager_load_gbf (ProjectManagerPlugin *pm_plugin)
{
	AnjutaPluginManager *plugin_manager;
	AnjutaStatus *status;
	gchar *dirname;
	GFile *dirfile;
	gchar *basename;
	const gchar *root_uri;
	GError *error = NULL;
	GList *desc;
	IAnjutaProjectBackend *backend;
	gint found = 0;
	
	root_uri = pm_plugin->project_root_uri;
	
	dirname = anjuta_util_get_local_path_from_uri (root_uri);
	dirfile = g_file_new_for_uri (root_uri);
	
	g_return_if_fail (dirname != NULL);
	
	if (pm_plugin->project != NULL)
			g_object_unref (pm_plugin->project);
	
	DEBUG_PRINT ("loading gbf backend…\n");
	plugin_manager = anjuta_shell_get_plugin_manager (ANJUTA_PLUGIN(pm_plugin)->shell, NULL);

	if (!anjuta_plugin_manager_is_active_plugin (plugin_manager, "IAnjutaProjectBackend"))
	{
		GList *descs = NULL;
		
		descs = anjuta_plugin_manager_query (plugin_manager,
											 "Anjuta Plugin",
											 "Interfaces",
											 "IAnjutaProjectBackend",
											 NULL);
		backend = NULL;
		for (desc = g_list_first (descs); desc != NULL; desc = g_list_next (desc)) {
			AnjutaPluginDescription *backend_desc;
			gchar *location = NULL;
			IAnjutaProjectBackend *plugin;
			gint backend_val;
				
			backend_desc = (AnjutaPluginDescription *)desc->data;
			anjuta_plugin_description_get_string (backend_desc, "Anjuta Plugin", "Location", &location);
			plugin = (IAnjutaProjectBackend *)anjuta_plugin_manager_get_plugin_by_id (plugin_manager, location);
			g_message ("search plugin %s", location);
			g_free (location);

			backend_val = ianjuta_project_backend_probe (plugin, dirfile, NULL);
			if (backend_val > found)
			{
				/* Backend found */;
				backend = plugin;
				found = backend_val;
			}
		}
		g_list_free (descs);
	}
	else
	{
		/* A backend is already loaded, use it */
		backend = IANJUTA_PROJECT_BACKEND (anjuta_shell_get_object (ANJUTA_PLUGIN (pm_plugin)->shell,
                                        "IAnjutaProjectBackend", NULL));

        g_object_ref (backend);
	}
	
	if (!backend)
	{
		/* FIXME: Set err */
		g_warning ("no backend available for this project\n");
		g_free (dirname);
		g_object_unref (dirfile);
		return;
	}
	
	DEBUG_PRINT ("%s", "Creating new gbf project\n");
	pm_plugin->project = ianjuta_project_backend_new_project (backend, NULL);
	if (!pm_plugin->project)
	{
		/* FIXME: Set err */
		g_warning ("project creation failed\n");
		g_free (dirname);
		g_object_unref (dirfile);
		return;
	}
	
	status = anjuta_shell_get_status (ANJUTA_PLUGIN (pm_plugin)->shell, NULL);
	anjuta_status_progress_add_ticks (status, 1);
	basename = g_path_get_basename (dirname);
	anjuta_status_push (status, _("Loading project: %s"), basename);
	anjuta_status_busy_push (status);
	
	DEBUG_PRINT ("loading project %s\n\n", dirname);
	/* FIXME: use the error parameter to determine if the project
	 * was loaded successfully */
	ianjuta_project_load (pm_plugin->project, dirfile, &error);
	
	anjuta_status_progress_tick (status, NULL, _("Created project view…"));
	
	if (error)
	{
		GtkWidget *toplevel;
		GtkWindow *win;
		
		toplevel = gtk_widget_get_toplevel (pm_plugin->scrolledwindow);
		if (toplevel && GTK_IS_WINDOW (toplevel))
			win = GTK_WINDOW (toplevel);
		else
			win = GTK_WINDOW (ANJUTA_PLUGIN (pm_plugin)->shell);
		
		anjuta_util_dialog_error (win, _("Failed to parse project (the project is opened, but there will be no project view) %s: %s\n"
										 ""),
								  dirname, error->message);
		/* g_propagate_error (err, error); */
		g_object_unref (pm_plugin->project);
		pm_plugin->project = NULL;
		g_free (basename);
		g_free (dirname);
		g_object_unref (dirfile);
		/* gtk_widget_destroy (progress_win); */
		anjuta_status_pop (status);
		anjuta_status_busy_pop (status);
		return;
	}
	g_object_set (G_OBJECT (pm_plugin->model), "project",
				  pm_plugin->project, NULL);
	
	update_ui (pm_plugin);
	anjuta_shell_present_widget (ANJUTA_PLUGIN (pm_plugin)->shell,
								 pm_plugin->scrolledwindow,
								 NULL);
	
	anjuta_status_set_default (status, _("Project"), basename);
	anjuta_status_pop (status);
	anjuta_status_busy_pop (status);
	g_free (basename);
	g_free (dirname);
	g_object_unref (dirfile);
}

static void
project_manager_unload_gbf (ProjectManagerPlugin *pm_plugin)
{
	AnjutaStatus *status;
	
	if (pm_plugin->project)
	{
		IAnjutaDocumentManager *docman;
		
		/* Close files that belong to this project (that are saved) */
		docman = anjuta_shell_get_interface (ANJUTA_PLUGIN (pm_plugin)->shell,
											 IAnjutaDocumentManager, NULL);
		if (docman)
		{
			GList *to_remove = NULL;
			GList *editors;
			GList *node;
			
			editors =
				ianjuta_document_manager_get_doc_widgets (docman, NULL);
			node = editors;
			while (node)
			{
				if (!IANJUTA_IS_EDITOR(node->data))
				{
					node = g_list_next(node);
					continue;
				}
				GFile* editor_file = ianjuta_file_get_file (IANJUTA_FILE (node->data), NULL);
				gchar *editor_uri = g_file_get_uri (editor_file);
				g_object_unref (editor_file);
					
				
				/* Only remove if it does not have unsaved data */
				if (editor_uri && (!IANJUTA_IS_FILE_SAVABLE (node->data) ||
								   !ianjuta_file_savable_is_dirty
										(IANJUTA_FILE_SAVABLE (node->data),
										 NULL)))
				{
					if (strncmp (editor_uri, pm_plugin->project_root_uri,
								 strlen (pm_plugin->project_root_uri)) == 0 &&
						editor_uri[strlen (pm_plugin->project_root_uri)] == '/')
					{
						to_remove = g_list_prepend (to_remove, node->data);
					}
				}
				g_free (editor_uri);
				node = g_list_next (node);
			}
			node = to_remove;
			while (node)
			{
				ianjuta_document_manager_remove_document (docman,
												IANJUTA_DOCUMENT (node->data),
														FALSE,
														NULL);
				node = g_list_next (node);
			}
			if (editors)
				g_list_free (editors);
			if (to_remove)
				g_list_free (to_remove);
		}
		
		/* Remove project properties dialogs */
		if (pm_plugin->properties_dialog != NULL) gtk_widget_destroy (pm_plugin->properties_dialog);
		pm_plugin->properties_dialog = NULL;
		
		/* Release project */
		g_object_unref (pm_plugin->project);
		pm_plugin->project = NULL;
		g_object_set (G_OBJECT (pm_plugin->model), "project", NULL, NULL);
		update_ui (pm_plugin);
		status = anjuta_shell_get_status (ANJUTA_PLUGIN (pm_plugin)->shell,
										  NULL);
		anjuta_status_set_default (status, _("Project"), NULL);
	}
}

static void
on_profile_scoped (AnjutaProfileManager *profile_manager,
				   AnjutaProfile *profile, ProjectManagerPlugin *plugin)
{
	GValue *value;
	gchar *session_dir;
	DEBUG_PRINT ("Profile scoped: %s", anjuta_profile_get_name (profile));
	if (strcmp (anjuta_profile_get_name (profile), PROJECT_PROFILE_NAME) != 0)
		return;
	
	DEBUG_PRINT ("%s", "Project profile loaded; Restoring project session");
	
	/* Load gbf project */
	project_manager_load_gbf (plugin);
	
	/* Export project */
	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, plugin->project_root_uri);
	
	update_title (plugin, plugin->project_root_uri);
	anjuta_shell_add_value (ANJUTA_PLUGIN(plugin)->shell,
							IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI,
							value, NULL);
	
	/* If profile scoped to "project", restore project session */
	session_dir = get_session_dir (plugin);
	g_return_if_fail (session_dir != NULL);
	
	/*
	 * If there is a session load already in progress (that is this
	 * project is being opened in session restoration), our session
	 * load would be ignored. Good thing.
	 */
	plugin->session_by_me = TRUE;
	anjuta_shell_session_load (ANJUTA_PLUGIN (plugin)->shell,
							   session_dir, NULL);
	plugin->session_by_me = FALSE;
	g_free (session_dir);
}

static void
on_profile_descoped (AnjutaProfileManager *profile_manager,
					 AnjutaProfile *profile, ProjectManagerPlugin *plugin)
{
	DEBUG_PRINT ("Profile descoped: %s", anjuta_profile_get_name (profile));
	
	if (strcmp (anjuta_profile_get_name (profile), PROJECT_PROFILE_NAME) != 0)
		return;
	
	DEBUG_PRINT ("%s", "Project profile descoped; Saving project session");
	
	/* Save current project session */
	g_return_if_fail (plugin->project_root_uri != NULL);
	
	/* Save project session */
	project_manager_save_session (plugin);
	
	/* Close current project */
	project_manager_unload_gbf (plugin);
		
	g_free (plugin->project_root_uri);
	g_free (plugin->project_uri);
	plugin->project_uri = NULL;
	plugin->project_root_uri = NULL;
	
	update_title (ANJUTA_PLUGIN_PROJECT_MANAGER (plugin), NULL);
	anjuta_shell_remove_value (ANJUTA_PLUGIN (plugin)->shell,
						  IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI, NULL);
}

static void
project_manager_plugin_close (ProjectManagerPlugin *plugin)
{
	AnjutaProfileManager *profile_manager;
	GError *error = NULL;
	
	/* Remove project profile */
	profile_manager = 
		anjuta_shell_get_profile_manager (ANJUTA_PLUGIN (plugin)->shell, NULL);
	anjuta_profile_manager_pop (profile_manager, PROJECT_PROFILE_NAME, &error);
	if (error)
	{
		anjuta_util_dialog_error (get_plugin_parent_window (plugin),
								  _("Error closing project: %s"),
								  error->message);
		g_error_free (error);
	}
}

static gboolean
project_manager_plugin_activate_plugin (AnjutaPlugin *plugin)
{
	AnjutaProfileManager *profile_manager;
	GtkWidget *view, *scrolled_window;
	GbfProjectModel *model;
	static gboolean initialized = FALSE;
	GtkTreeSelection *selection;
	/* GladeXML *gxml; */
	ProjectManagerPlugin *pm_plugin;
	
	DEBUG_PRINT ("ProjectManagerPlugin: Activating Project Manager plugin %p…", plugin);
	
	if (!initialized)
		register_stock_icons (plugin);
	
	pm_plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (plugin);
	pm_plugin->ui = anjuta_shell_get_ui (plugin->shell, NULL);
	pm_plugin->prefs = anjuta_shell_get_preferences (plugin->shell, NULL);

	/* create model & view and bind them */
	model = gbf_project_model_new (NULL);
	view = gbf_project_view_new ();
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (view),
							 GTK_TREE_MODEL (model));
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	g_signal_connect (view, "uri-activated",
					  G_CALLBACK (on_uri_activated), plugin);
	g_signal_connect (view, "target-selected",
					  G_CALLBACK (on_target_activated), plugin);
	g_signal_connect (view, "group-selected",
					  G_CALLBACK (on_group_activated), plugin);
	g_signal_connect (selection, "changed",
					  G_CALLBACK (on_treeview_selection_changed), plugin);
	g_signal_connect (view, "event",
					  G_CALLBACK (on_treeview_event), plugin);
	
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
									GTK_POLICY_AUTOMATIC,
									GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
										 GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (scrolled_window), view);
	gtk_widget_show (view);
	gtk_widget_show (scrolled_window);
	
	pm_plugin->scrolledwindow = scrolled_window;
	pm_plugin->view = view;
	pm_plugin->model = model;
	pm_plugin->properties_dialog = NULL;
	
	/* Action groups */
	pm_plugin->pm_action_group = 
		anjuta_ui_add_action_group_entries (pm_plugin->ui,
											"ActionGroupProjectManager",
											_("Project manager actions"),
											pm_actions,
											G_N_ELEMENTS(pm_actions),
											GETTEXT_PACKAGE, TRUE,
											plugin);
	pm_plugin->popup_action_group = 
		anjuta_ui_add_action_group_entries (pm_plugin->ui,
											"ActionGroupProjectManagerPopup",
											_("Project manager popup actions"),
											popup_actions,
											G_N_ELEMENTS (popup_actions),
											GETTEXT_PACKAGE, TRUE,
											plugin);
	/* Merge UI */
	pm_plugin->merge_id = 
		anjuta_ui_merge (pm_plugin->ui, UI_FILE);
	
	update_ui (pm_plugin);
	
	/* Added widget in shell */
	anjuta_shell_add_widget (plugin->shell, pm_plugin->scrolledwindow,
							 "AnjutaProjectManager", _("Project"),
							 "project-manager-plugin-icon",
							 ANJUTA_SHELL_PLACEMENT_LEFT, NULL);
#if 0
	/* Add preferences page */
	gxml = glade_xml_new (PREFS_GLADE, "dialog.project.manager", NULL);
	
	anjuta_preferences_add_page (pm_plugin->prefs,
								gxml, "Project Manager", ICON_FILE);
	preferences_changed(pm_plugin->prefs, pm_plugin);
	g_object_unref (G_OBJECT (gxml));
#endif
	
	/* Add watches */
	pm_plugin->fm_watch_id =
		anjuta_plugin_add_watch (plugin, IANJUTA_FILE_MANAGER_SELECTED_FILE,
								 value_added_fm_current_file,
								 value_removed_fm_current_file, NULL);
	pm_plugin->editor_watch_id = 
		anjuta_plugin_add_watch (plugin, IANJUTA_DOCUMENT_MANAGER_CURRENT_DOCUMENT,
								 value_added_current_editor,
								 value_removed_current_editor, NULL);
	/* Connect to save session */
	g_signal_connect (G_OBJECT (plugin->shell), "save_session",
					  G_CALLBACK (on_session_save), plugin);
	g_signal_connect (G_OBJECT (plugin->shell), "exiting",
					  G_CALLBACK (on_shell_exiting), plugin);
	profile_manager = anjuta_shell_get_profile_manager (plugin->shell, NULL);

	/* Connect to profile scoping */
	g_signal_connect (profile_manager, "profile-scoped",
					  G_CALLBACK (on_profile_scoped), plugin);
	g_signal_connect (profile_manager, "profile-descoped",
					  G_CALLBACK (on_profile_descoped), plugin);
	return TRUE;
}

static gboolean
project_manager_plugin_deactivate_plugin (AnjutaPlugin *plugin)
{
	AnjutaProfileManager *profile_manager;
	ProjectManagerPlugin *pm_plugin;
	pm_plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (plugin);

	DEBUG_PRINT ("ProjectManagerPlugin: Deactivate Project Manager plugin…");
	if (pm_plugin->close_project_idle > -1)
	{
		g_source_remove (pm_plugin->close_project_idle);
	}

	/* Close project if it's open */
	if (pm_plugin->project_root_uri)
		project_manager_plugin_close (pm_plugin);
	
	profile_manager = anjuta_shell_get_profile_manager (plugin->shell, NULL);

	/* Disconnect signals */
	g_signal_handlers_disconnect_by_func (G_OBJECT (plugin->shell),
										  G_CALLBACK (on_session_save),
										  plugin);
	g_signal_handlers_disconnect_by_func (G_OBJECT (plugin->shell),
										  G_CALLBACK (on_shell_exiting),
										  plugin);
	g_signal_handlers_disconnect_by_func (G_OBJECT (profile_manager),
										  G_CALLBACK (on_profile_descoped),
										  plugin);
	g_signal_handlers_disconnect_by_func (G_OBJECT (profile_manager),
										  G_CALLBACK (on_profile_scoped),
										  plugin);
	/* Remove watches */
	anjuta_plugin_remove_watch (plugin, pm_plugin->fm_watch_id, TRUE);
	anjuta_plugin_remove_watch (plugin, pm_plugin->editor_watch_id, TRUE);
	
	g_object_unref (G_OBJECT (pm_plugin->model));
	
	/* Widget is removed from the shell when destroyed */
	gtk_widget_destroy (pm_plugin->scrolledwindow);
	
	anjuta_ui_unmerge (pm_plugin->ui, pm_plugin->merge_id);
	anjuta_ui_remove_action_group (pm_plugin->ui, pm_plugin->pm_action_group);
	anjuta_ui_remove_action_group (pm_plugin->ui,
								   pm_plugin->popup_action_group);
	
	return TRUE;
}

static void
project_manager_plugin_finalize (GObject *obj)
{
	/* FIXME: */
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
project_manager_plugin_dispose (GObject *obj)
{
	/* FIXME: */
	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
project_manager_plugin_instance_init (GObject *obj)
{
	ProjectManagerPlugin *plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (obj);
	plugin->scrolledwindow = NULL;
	plugin->project = NULL;
	plugin->view = NULL;
	plugin->model = NULL;
	plugin->pre_update_sources = NULL;
	plugin->pre_update_targets = NULL;
	plugin->pre_update_groups = NULL;
	plugin->project_root_uri = NULL;
	plugin->project_uri = NULL;
	plugin->fm_current_uri = NULL;
	plugin->current_editor_uri = NULL;
	plugin->session_by_me = FALSE;
	plugin->close_project_idle = -1;
}

static void
project_manager_plugin_class_init (GObjectClass *klass) 
{
	AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	plugin_class->activate = project_manager_plugin_activate_plugin;
	plugin_class->deactivate = project_manager_plugin_deactivate_plugin;
	klass->dispose = project_manager_plugin_finalize;
	klass->dispose = project_manager_plugin_dispose;
}

/* IAnjutaProjectManager implementation */

static GFileType
get_uri_vfs_type (const gchar *uri)
{
	GFileType retval = G_FILE_TYPE_UNKNOWN;
	GFile *file;
	GFileInfo *file_info;

	file = g_file_new_for_uri (uri);
	file_info = g_file_query_info (file,
		G_FILE_ATTRIBUTE_STANDARD_TYPE,
		G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
		NULL, NULL);
	if (file_info)
	{
		retval = g_file_info_get_file_type (file_info);
		g_object_unref (file_info);
	}

	g_object_unref (file);
	return retval;
}

static gboolean
uri_is_inside_project (ProjectManagerPlugin *plugin, const gchar *uri)
{
	if (strncmp (uri, plugin->project_root_uri,
				 strlen (plugin->project_root_uri)) == 0)
		return TRUE;
	
	if (uri[0] == '/')
	{
		const gchar *project_root_path = strchr (plugin->project_root_uri, ':');
		if (project_root_path)
			project_root_path += 3;
		else
			project_root_path = plugin->project_root_uri;
		return (strncmp (uri, project_root_path,
						 strlen (project_root_path)) == 0);
	}
	return FALSE;
}

static gchar *
get_element_uri_from_id (ProjectManagerPlugin *plugin, AnjutaProjectNode *id, const gchar *root)
{
	const gchar *project_root = NULL;
	GFile *file = NULL;
	GFile *target_file = NULL;
	gchar *uri = NULL;
	
	if (!id)
		return NULL;
	
	anjuta_shell_get (ANJUTA_PLUGIN (plugin)->shell,
					  root, G_TYPE_STRING,
					  &project_root, NULL);
	if (project_root == NULL)
	{
		/* Perhaps missing build URI, use project URI instead */
		anjuta_shell_get (ANJUTA_PLUGIN (plugin)->shell,
					  IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI,
					  G_TYPE_STRING,
					  &project_root,
					  NULL);
	}

	switch (anjuta_project_node_get_type (id))
	{
		case ANJUTA_PROJECT_GROUP:
			file = anjuta_project_group_get_directory (id);
			break;
		case ANJUTA_PROJECT_TARGET:
			file = anjuta_project_group_get_directory (anjuta_project_node_parent (id));
			target_file = g_file_get_child (file, anjuta_project_target_get_name (id));
			file = target_file;
			break;
		case ANJUTA_PROJECT_SOURCE:
			file = anjuta_project_source_get_file (id);
			break;
		default:
			file = NULL;
			break;
	}

	if ((file != NULL) && (project_root != NULL))
	{
		gchar *rel_path;

		rel_path = g_file_get_relative_path (anjuta_project_group_get_directory (ianjuta_project_get_root (plugin->project, NULL)), file);

		if (rel_path)
		{
			GFile *node_file = NULL;
			GFile *root_file = NULL;
			root_file = g_file_new_for_uri (project_root);
			node_file = g_file_get_child (root_file, rel_path);
			g_object_unref (root_file);

			uri = g_file_get_uri (node_file);
			g_object_unref (node_file);
			g_free (rel_path);
		}
	}
		
	if (target_file != NULL) g_object_unref (target_file);
	
	return uri;
}

static const gchar *
get_element_relative_path (ProjectManagerPlugin *plugin, const gchar *uri, const gchar *root)
{
	const gchar *project_root = NULL;
	
	anjuta_shell_get (ANJUTA_PLUGIN (plugin)->shell,
					  root, G_TYPE_STRING,
					  &project_root, NULL);
	if (project_root == NULL)
	{
		/* Perhaps missing build URI, use project URI instead */
		anjuta_shell_get (ANJUTA_PLUGIN (plugin)->shell,
					  IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI,
					  G_TYPE_STRING,
					  &project_root,
					  NULL);
	}
	if (project_root)
	{
		if (uri[0] != '/')
		{
			return uri + strlen (project_root);
		}
		else
		{
			const gchar *project_root_path = strchr (project_root, ':');
			if (project_root_path)
				project_root_path += 3;
			else
				project_root_path = project_root;
			return uri + strlen (project_root_path);
		}
	}
	return NULL;
}

static AnjutaProjectNode*
get_node_from_file (AnjutaProjectNode *parent, GFile *file)
{
	AnjutaProjectNode *node;
	GFile *target_file = NULL;

	for (node = anjuta_project_node_first_child (parent); node != NULL; node = anjuta_project_node_next_sibling (node))
	{
		switch (anjuta_project_node_get_type (node))
		{
		case ANJUTA_PROJECT_GROUP:
			if (g_file_equal (anjuta_project_group_get_directory (node), file))
			{
				return node;
			}
			else
			{
				return get_node_from_file (node, file);
			}
			break;
		case ANJUTA_PROJECT_TARGET:
			target_file = g_file_get_child (anjuta_project_group_get_directory (parent), anjuta_project_target_get_name (node));
			if (g_file_equal (target_file, file))
			{
				g_object_unref (target_file);
				return node;
			}
			g_object_unref (target_file);
			break;
		case ANJUTA_PROJECT_SOURCE:
			if (g_file_equal (anjuta_project_source_get_file (node), file))
			{
				return node;
			}
			break;
		default:
			break;
		}
	}
				
	return NULL;		
}

static GbfTreeData*
get_tree_data_from_uri (ProjectManagerPlugin *plugin, const gchar *uri, GbfTreeNodeType type)
{
	return gbf_project_model_find_uri (plugin->model, uri, type);
}

static AnjutaProjectTarget*
get_target_from_uri (ProjectManagerPlugin *plugin, const gchar *uri)
{
	GbfTreeData *data;
	AnjutaProjectNode *node = NULL;

	data = get_tree_data_from_uri (plugin, uri, GBF_TREE_NODE_TARGET);
	if (data) node = gbf_tree_data_get_node (data, plugin->project);

	if (anjuta_project_node_get_type (node) == ANJUTA_PROJECT_TARGET)
	{
		return (AnjutaProjectTarget *)node;
	}
	else
	{
		return NULL;
	}
}

static AnjutaProjectNodeType
iproject_manager_get_element_type (IAnjutaProjectManager *project_manager,
								   const gchar *element_uri,
								   GError **err)
{
	GbfTreeData *data;
	AnjutaProjectNode *node = NULL;
	ProjectManagerPlugin *plugin;

	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager),
						  ANJUTA_PROJECT_UNKNOWN);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));

	data = get_tree_data_from_uri (plugin, element_uri, 0);
	if (data) node = gbf_tree_data_get_node (data, plugin->project);

	return node == NULL ? ANJUTA_PROJECT_UNKNOWN : anjuta_project_node_get_type (node);
}

static GList*
iproject_manager_get_elements (IAnjutaProjectManager *project_manager,
							   AnjutaProjectNodeType element_type,
							   GError **err)
{
	ProjectManagerPlugin *plugin;
	
	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager), NULL);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	g_return_val_if_fail (IANJUTA_IS_PROJECT (plugin->project), NULL);

	return gbf_project_util_replace_by_uri (gbf_project_util_node_all (ianjuta_project_get_root (plugin->project, NULL), element_type)); 
}

static AnjutaProjectTargetClass
iproject_manager_get_target_type (IAnjutaProjectManager *project_manager,
								   const gchar *target_uri,
								   GError **err)
{
	ProjectManagerPlugin *plugin;
	AnjutaProjectTarget *target;
	
	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager),
						  ANJUTA_TARGET_UNKNOWN);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	g_return_val_if_fail (IANJUTA_IS_PROJECT (plugin->project),
						  ANJUTA_TARGET_UNKNOWN);
	
	g_return_val_if_fail (uri_is_inside_project (plugin, target_uri),
						  ANJUTA_TARGET_UNKNOWN);
	
	target = get_target_from_uri (plugin, target_uri);

	if (target != NULL)
	{
		AnjutaProjectTargetType type = anjuta_project_target_get_type (target);

		return anjuta_project_target_type_class (type);
	}
	else
	{
		return ANJUTA_TARGET_UNKNOWN;
	}
}

static GList*
iproject_manager_get_targets (IAnjutaProjectManager *project_manager,
							  AnjutaProjectTargetClass target_type,
							  GError **err)
{
	GList *targets, *node;
	ProjectManagerPlugin *plugin;
	
	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager), NULL);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	g_return_val_if_fail (IANJUTA_IS_PROJECT (plugin->project), NULL);

	/* Get all targets */
	targets = gbf_project_util_node_all (ianjuta_project_get_root (plugin->project, NULL), ANJUTA_PROJECT_TARGET);

	/* Remove all targets not in specified class */
	for (node = g_list_first (targets); node != NULL;)
	{
		AnjutaProjectTargetType type;

		type = anjuta_project_target_get_type (node->data);
		if (anjuta_project_target_type_class (type) != target_type)
		{
			GList *next = g_list_next (node);
			targets = g_list_delete_link (targets, node);
			node = next;
		}
		else
		{
			node = g_list_next (node);
		}
	}

	/* Replace all targets by their corresponding URI */
	for (node = g_list_first (targets); node != NULL; node = g_list_next (node))
	{
		node->data = get_element_uri_from_id (plugin, node->data, IANJUTA_BUILDER_ROOT_URI);
	}

	return targets;
}

static gchar*
iproject_manager_get_parent (IAnjutaProjectManager *project_manager,
							 const gchar *element_uri,
							 GError **err)
{
	AnjutaProjectNodeType type;
	ProjectManagerPlugin *plugin;
	
	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager), NULL);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	g_return_val_if_fail (IANJUTA_IS_PROJECT (plugin->project), NULL);
	
	type = ianjuta_project_manager_get_element_type (project_manager,
													 element_uri, NULL);
	/* FIXME: */
	return NULL;
}

static GList*
iproject_manager_get_children (IAnjutaProjectManager *project_manager,
							   const gchar *element_uri,
							   GError **err)
{
	ProjectManagerPlugin *plugin;
	
	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager), NULL);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	g_return_val_if_fail (IANJUTA_IS_PROJECT (plugin->project), NULL);
	/* FIXME: */
	return NULL;
}

static gchar*
iproject_manager_get_selected (IAnjutaProjectManager *project_manager,
							   GError **err)
{
	gchar *uri;
	AnjutaProjectNode *node;
	ProjectManagerPlugin *plugin;
	
	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager), NULL);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	if (plugin->project == NULL) return NULL;
	
	g_return_val_if_fail (IANJUTA_IS_PROJECT (plugin->project), NULL);
	
	node = gbf_project_view_find_selected (GBF_PROJECT_VIEW (plugin->view),
										   ANJUTA_PROJECT_SOURCE);
	if (node && anjuta_project_node_get_type (node) == ANJUTA_PROJECT_SOURCE)
	{
		uri = g_file_get_uri (anjuta_project_source_get_file (node));
		return uri;
	}

	node = gbf_project_view_find_selected (GBF_PROJECT_VIEW (plugin->view),
										   ANJUTA_PROJECT_TARGET);
	if (node && anjuta_project_node_get_type (node) == ANJUTA_PROJECT_TARGET)
	{
		uri = get_element_uri_from_id (plugin, node, IANJUTA_BUILDER_ROOT_URI);
		return uri;
	}

	node = gbf_project_view_find_selected (GBF_PROJECT_VIEW (plugin->view),
										   ANJUTA_PROJECT_GROUP);
	if (node && anjuta_project_node_get_type (node) == GBF_TREE_NODE_GROUP)
	{
		uri = g_file_get_uri (anjuta_project_group_get_directory (node));
		return uri;;
	}
	
	return NULL;
}

static gchar*
iproject_manager_get_selected_id (IAnjutaProjectManager *project_manager,
								  AnjutaProjectNodeType element_type,
								  GError **err)
{
	ProjectManagerPlugin *plugin;
	AnjutaProjectNode *node = NULL;
	gchar *uri;
	
	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager), NULL);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	g_return_val_if_fail (IANJUTA_IS_PROJECT (plugin->project), NULL);

	if (element_type == ANJUTA_PROJECT_UNKNOWN ||
		element_type == ANJUTA_PROJECT_SOURCE)
	{
		node = gbf_project_view_find_selected (GBF_PROJECT_VIEW (plugin->view),
		                                       ANJUTA_PROJECT_SOURCE);
		if (node)
		{
			uri = get_element_uri_from_id (plugin, node, IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI);
			return uri;
		}
	}

	if (element_type == ANJUTA_PROJECT_UNKNOWN ||
		element_type == ANJUTA_PROJECT_TARGET)
	{
		node = gbf_project_view_find_selected (GBF_PROJECT_VIEW (plugin->view),
											   ANJUTA_PROJECT_TARGET);
		if (node)
		{
			uri = get_element_uri_from_id (plugin, node, IANJUTA_BUILDER_ROOT_URI);
			return uri;
		}
	}

	if (element_type == ANJUTA_PROJECT_UNKNOWN ||
		element_type == ANJUTA_PROJECT_GROUP)
	{
		node = gbf_project_view_find_selected (GBF_PROJECT_VIEW (plugin->view),
											   ANJUTA_PROJECT_GROUP);
		if (node)
		{
			uri = get_element_uri_from_id (plugin, node, IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI);
			return uri;
		}
	}

	if (node)
	{
		uri = get_element_uri_from_id (plugin, node, IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI);
		return uri;
	}
	return NULL;
}

static IAnjutaProjectCapabilities
iproject_manager_get_capabilities (IAnjutaProjectManager *project_manager,
								   GError **err)
{
	ProjectManagerPlugin *plugin;
	
	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager),
						  IANJUTA_PROJECT_CAN_ADD_NONE);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	return ianjuta_project_get_capabilities (plugin->project, NULL);
}

static gchar*
iproject_manager_add_source (IAnjutaProjectManager *project_manager,
							 const gchar *source_uri_to_add,
							 const gchar *default_location_uri,
							 GError **err)
{
	ProjectManagerPlugin *plugin;
	GbfTreeData *location_data = NULL;
	AnjutaProjectSource *source_id;
	gchar* source_uri;
	
	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager), FALSE);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	g_return_val_if_fail (IANJUTA_IS_PROJECT (plugin->project), FALSE);

	update_operation_begin (plugin);
	if (default_location_uri != NULL)
	{
		location_data = get_tree_data_from_uri (plugin, default_location_uri, GBF_TREE_NODE_TARGET);
		if (location_data == NULL) get_tree_data_from_uri (plugin, default_location_uri, GBF_TREE_NODE_GROUP);
	}
	source_id = gbf_project_util_add_source (plugin->model,
										get_plugin_parent_window (plugin),
											 location_data,
											 source_uri_to_add);
	update_operation_end (plugin, TRUE);
	
	source_uri = get_element_uri_from_id(plugin, source_id, IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI);
	
	return source_uri;
}

static gchar*
iproject_manager_add_source_quiet (IAnjutaProjectManager *project_manager,
								   const gchar *source_uri_to_add,
								   const gchar *location_uri,
								   GError **err)
{
	ProjectManagerPlugin *plugin;
	AnjutaProjectSource *source_id;
	GFile *source_file;
	GbfTreeData *data;
	AnjutaProjectTarget *target = NULL;
	
	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager), FALSE);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	g_return_val_if_fail (IANJUTA_IS_PROJECT (plugin->project), FALSE);

	data = get_tree_data_from_uri (plugin, location_uri, GBF_TREE_NODE_TARGET);
	if (data == NULL) get_tree_data_from_uri (plugin, location_uri, GBF_TREE_NODE_GROUP);
	if (data != NULL) target = gbf_tree_data_get_node (data, plugin->project);
	source_file = g_file_new_for_uri (source_uri_to_add);
	update_operation_begin (plugin);
	source_id = ianjuta_project_add_source (plugin->project,
	    								target,
	    								source_file,
										err);
	update_operation_end (plugin, TRUE);
	g_object_unref (source_file);
	
	return get_element_uri_from_id (plugin, source_id, IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI);
}

static GList*
iproject_manager_add_source_multi (IAnjutaProjectManager *project_manager,
							 GList *source_add_uris,
							 const gchar *default_location_uri,
							 GError **err)
{
	ProjectManagerPlugin *plugin;
	GbfTreeData *location_data = NULL;
	GList* source_ids;
	GList* source_uris = NULL;
	
	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager), FALSE);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	g_return_val_if_fail (IANJUTA_IS_PROJECT (plugin->project), FALSE);

	update_operation_begin (plugin);
	if (default_location_uri != NULL)
	{
		location_data = get_tree_data_from_uri (plugin, default_location_uri, GBF_TREE_NODE_TARGET);
		if (location_data == NULL) get_tree_data_from_uri (plugin, default_location_uri, GBF_TREE_NODE_GROUP);
	}

	source_ids = gbf_project_util_add_source_multi (plugin->model,
										 get_plugin_parent_window (plugin),
										 location_data,
										 source_add_uris);
	update_operation_end (plugin, TRUE);
	
	while (source_ids)
	{
		source_uris = g_list_append (source_uris,
									 get_element_uri_from_id (plugin,
														  source_ids->data,
														  IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI));
		source_ids = g_list_delete_link (source_ids, source_ids);
	}

	return source_uris;
}

static gchar*
iproject_manager_add_target (IAnjutaProjectManager *project_manager,
							 const gchar *target_name_to_add,
							 const gchar *default_group_uri,
							 GError **err)
{
	ProjectManagerPlugin *plugin;
	gchar *target_uri = NULL;
	AnjutaProjectTarget *target_id;
	GbfTreeData *default_group_data;
	
	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager), FALSE);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	
	g_return_val_if_fail (IANJUTA_IS_PROJECT (plugin->project), FALSE);

	default_group_data = get_tree_data_from_uri (plugin, default_group_uri, GBF_TREE_NODE_GROUP);
	
	update_operation_begin (plugin);
	target_id = gbf_project_util_new_target (plugin->model,
											 get_plugin_parent_window (plugin),
											 default_group_data,
											 target_name_to_add);
	update_operation_end (plugin, TRUE);
	target_uri = get_element_uri_from_id (plugin, target_id, IANJUTA_BUILDER_ROOT_URI);

	return target_uri;
}

static gchar*
iproject_manager_add_group (IAnjutaProjectManager *project_manager,
							const gchar *group_name_to_add,
							const gchar *default_group_uri,
							GError **err)
{
	ProjectManagerPlugin *plugin;
	gchar *group_uri = NULL;
	AnjutaProjectGroup *group_id;
	GbfTreeData *default_group_data;
	
	g_return_val_if_fail (ANJUTA_IS_PLUGIN (project_manager), FALSE);
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	g_return_val_if_fail (IANJUTA_IS_PROJECT (plugin->project), FALSE);

	default_group_data = get_tree_data_from_uri (plugin, default_group_uri, GBF_TREE_NODE_GROUP);
	
	update_operation_begin (plugin);
	group_id = gbf_project_util_new_group (plugin->model,
										   get_plugin_parent_window (plugin),
										   default_group_data,
										   group_name_to_add);
	update_operation_end (plugin, TRUE);
	group_uri = get_element_uri_from_id (plugin, group_id, IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI);
	
	return group_uri;
}

static gboolean
iproject_manager_is_open (IAnjutaProjectManager *project_manager, GError **err)
{
	ProjectManagerPlugin *plugin;

	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));

	return IANJUTA_IS_PROJECT (plugin->project);
}

static GList*
iproject_manager_get_packages (IAnjutaProjectManager *project_manager, GError **err)
{
	ProjectManagerPlugin *plugin;

	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (G_OBJECT (project_manager));
	
	/* Check if a current project is opened */
	if (plugin->project == NULL) return NULL;

	return ianjuta_project_get_packages (plugin->project, NULL);
}

static void
iproject_manager_iface_init(IAnjutaProjectManagerIface *iface)
{
	iface->get_element_type = iproject_manager_get_element_type;
	iface->get_elements = iproject_manager_get_elements;
	iface->get_target_type = iproject_manager_get_target_type;
	iface->get_targets = iproject_manager_get_targets;
	iface->get_parent = iproject_manager_get_parent;
	iface->get_children = iproject_manager_get_children;
	iface->get_selected = iproject_manager_get_selected;
	iface->get_selected_id = iproject_manager_get_selected_id;
	iface->get_capabilities = iproject_manager_get_capabilities;
	iface->add_source = iproject_manager_add_source;
	iface->add_source_quiet = iproject_manager_add_source_quiet;
	iface->add_sources = iproject_manager_add_source_multi;
	iface->add_target = iproject_manager_add_target;
	iface->add_group = iproject_manager_add_group;
	iface->is_open = iproject_manager_is_open;
	iface->get_packages = iproject_manager_get_packages;
}

static void
ifile_open (IAnjutaFile *ifile, GFile* file, GError **e)
{
	AnjutaProfile *profile;
	AnjutaProfileManager *profile_manager;
	AnjutaPluginManager *plugin_manager;
	AnjutaStatus *status;
	gchar *session_profile_path, *profile_name;
	GFile *session_profile;
	GFile *default_profile;
	GFile *project_root;
	GFile *tmp;
	ProjectManagerPlugin *plugin;
	GError *error = NULL;

	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (ifile);
	
	/* If there is already a project loaded, load in separate anjuta window */
	if (plugin->project_root_uri)
	{
		gchar *uri = g_file_get_uri (file);
		gchar *quoted_uri = g_shell_quote (uri);
		gchar *cmd = g_strconcat ("anjuta --no-splash --no-client ", quoted_uri, NULL);
		g_free (quoted_uri);
		anjuta_util_execute_shell (NULL, cmd);
		g_free (cmd);
		g_free (uri);
		
		return;
	}
	
	plugin_manager  =
		anjuta_shell_get_plugin_manager (ANJUTA_PLUGIN (ifile)->shell, NULL);
	profile_manager =
		anjuta_shell_get_profile_manager (ANJUTA_PLUGIN (ifile)->shell, NULL);
	status = anjuta_shell_get_status (ANJUTA_PLUGIN (ifile)->shell, NULL);
	
	anjuta_status_progress_add_ticks (status, 2);
	/* Prepare profile */
	profile = anjuta_profile_new (PROJECT_PROFILE_NAME, plugin_manager);
	
	/* System default profile */
	default_profile = g_file_new_for_uri (DEFAULT_PROFILE);
	anjuta_profile_add_plugins_from_xml (profile, default_profile,
										 TRUE, &error);
	profile_name = g_file_get_basename (default_profile);
	g_object_unref (default_profile);
	if (error)
	{
		g_propagate_error (e, error);
		g_free (profile_name);
		g_object_unref (profile);
		
		return;
	}
	
	/* Project default profile */
	anjuta_profile_add_plugins_from_xml (profile, file, TRUE, &error);
	if (error)
	{
		g_propagate_error (e, error);
	
		g_free (profile_name);
		g_object_unref (profile);

		return;
	}
	
	/* Project session profile */
	project_root = g_file_get_parent (file);
	tmp = g_file_get_child (project_root, ".anjuta");
	session_profile = g_file_get_child (tmp, profile_name);
	g_object_unref (tmp);
	g_free (profile_name);
	
	session_profile_path = g_file_get_path (session_profile);
	DEBUG_PRINT ("Loading project session profile: %s", session_profile_path);
	if (g_file_query_exists (session_profile, NULL))
	{
		anjuta_profile_add_plugins_from_xml (profile, session_profile,
											 FALSE, &error);
		if (error)
		{
			g_propagate_error (e, error);
		
			g_free (session_profile_path);
			g_object_unref (project_root);
			g_object_unref (profile);
			g_object_unref (session_profile);
			
			return;
		}
	}
	anjuta_profile_set_sync_file (profile, session_profile); 
	g_free (session_profile_path);
	
	/* Set project uri */
	g_free (plugin->project_root_uri);
	g_free (plugin->project_uri);
	
	plugin->project_uri = g_file_get_uri (file);
	plugin->project_root_uri = g_file_get_uri (project_root);
	g_object_unref (project_root);
	
	/* Load profile */
	anjuta_profile_manager_push (profile_manager, profile, &error);
	if (error)
	{
		anjuta_util_dialog_error (GTK_WINDOW (ANJUTA_PLUGIN (ifile)->shell),
								  "%s", error->message);
		g_error_free (error);
		error = NULL;
	}
	
	anjuta_status_progress_tick (status, NULL,
								 _("Initializing Project…"));
	update_ui (plugin);

	anjuta_status_progress_tick (status, NULL, _("Project Loaded"));
}

static GFile*
ifile_get_file (IAnjutaFile *ifile, GError **e)
{
	ProjectManagerPlugin *plugin;
	
	plugin = ANJUTA_PLUGIN_PROJECT_MANAGER (ifile);
	if (plugin->project_root_uri)
		return g_file_new_for_uri (plugin->project_root_uri);
	else
		return NULL;
}

static void
ifile_iface_init(IAnjutaFileIface *iface)
{
	iface->open = ifile_open;
	iface->get_file = ifile_get_file;
}

ANJUTA_PLUGIN_BEGIN (ProjectManagerPlugin, project_manager_plugin);
ANJUTA_PLUGIN_ADD_INTERFACE (iproject_manager, IANJUTA_TYPE_PROJECT_MANAGER);
ANJUTA_PLUGIN_ADD_INTERFACE (ifile, IANJUTA_TYPE_FILE);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (ProjectManagerPlugin, project_manager_plugin);
