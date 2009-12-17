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
#include <libanjuta/anjuta-shell.h>
#include <libanjuta/resources.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/interfaces/libanjuta-interfaces.h>

//#include <libgtodo/main.h>
#include "main.h"
#include "plugin.h"

#define UI_FILE PACKAGE_DATA_DIR"/ui/anjuta-gtodo.ui"

static gpointer parent_class;

static void
on_hide_completed_action_activate (GtkAction *action, GTodoPlugin *plugin)
{
	gboolean state;
	state = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	gtodo_set_hide_done (state);
}

static void
on_hide_due_date_action_activate (GtkAction *action, GTodoPlugin *plugin)
{
	gboolean state;
	state = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	gtodo_set_hide_due (state);
}

static void
on_hide_end_date_action_activate (GtkAction *action, GTodoPlugin *plugin)
{
	gboolean state;
	state = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	gtodo_set_hide_nodate (state);
}

static GtkActionEntry actions_todo_view[] = {
	{
		"ActionMenuViewTodo",
		NULL,
		N_("_Tasks"),
		NULL, NULL, NULL,
	},
};

static GtkToggleActionEntry actions_view[] = {
	{
		"ActionViewTodoHideCompleted",
		NULL,
		N_("Hide _Completed Items"),
		NULL,
		N_("Hide completed to-do items"),
		G_CALLBACK (on_hide_completed_action_activate),
		FALSE
	},
	{
		"ActionViewTodoHideDueDate",
		NULL,
		N_("Hide Items Past _Due Date"),
		NULL,
		N_("Hide items that are past due date"),
		G_CALLBACK (on_hide_due_date_action_activate),
		FALSE
	},
	{
		"ActionViewTodoHideEndDate",
		NULL,
		N_("Hide Items Without _End Date"),
		NULL,
		N_("Hide items without an end date"),
		G_CALLBACK (on_hide_end_date_action_activate),
		FALSE
	}
};

static gboolean
create_gui (GTodoPlugin *gtodo_plugin)
{
	GtkWidget *wid;
	AnjutaUI *ui;
	GtkAction* hide_due;
	GtkAction* hide_nodate;
	GtkAction* hide_done;

	if (gtodo_plugin->widget) return TRUE;

	wid = gui_create_todo_widget();
	if (!wid) return FALSE;
	
	gtk_widget_show_all (wid);
	gtodo_plugin->widget = wid;
		
	ui = anjuta_shell_get_ui (ANJUTA_PLUGIN (gtodo_plugin)->shell, NULL);
	
	/* Add all our editor actions */
	gtodo_plugin->action_group = 
		anjuta_ui_add_action_group_entries (ui, "ActionGroupTodoView",
											_("Task manager"),
											actions_todo_view,
											G_N_ELEMENTS (actions_todo_view),
											GETTEXT_PACKAGE, FALSE, gtodo_plugin);
	gtodo_plugin->action_group2 = 
		anjuta_ui_add_toggle_action_group_entries (ui, "ActionGroupTodoViewOps",
												_("Task manager view"),
												actions_view,
												G_N_ELEMENTS (actions_view),
												GETTEXT_PACKAGE, TRUE, gtodo_plugin);
	gtodo_plugin->uiid = anjuta_ui_merge (ui, UI_FILE);
	anjuta_shell_add_widget (ANJUTA_PLUGIN (gtodo_plugin)->shell, wid,
							 "AnjutaTodoPlugin", _("Tasks"),
							 "gtodo", /* Icon stock */
							 ANJUTA_SHELL_PLACEMENT_CENTER, NULL);
	
	hide_done = anjuta_ui_get_action (ui, "ActionGroupTodoViewOps",
								   "ActionViewTodoHideCompleted");
	g_object_set(G_OBJECT(hide_done), "active",
				 gtodo_get_hide_done(), NULL);
	hide_due = anjuta_ui_get_action (ui, "ActionGroupTodoViewOps",
								   "ActionViewTodoHideDueDate");
	g_object_set(G_OBJECT(hide_due), "active",
				 gtodo_get_hide_due(), NULL);
	hide_nodate = anjuta_ui_get_action (ui, "ActionGroupTodoViewOps",
								   "ActionViewTodoHideEndDate");
	g_object_set(G_OBJECT(hide_nodate), "active",
				 gtodo_get_hide_nodate(), NULL);
	
	return TRUE;
}

static gboolean
remove_gui (GTodoPlugin *gtodo_plugin)
{
	AnjutaUI *ui = anjuta_shell_get_ui (ANJUTA_PLUGIN (gtodo_plugin)->shell, NULL);

	if (!gtodo_plugin->widget) return FALSE;

	/* Container holds the last ref to this widget so it will be destroyed as
	 * soon as removed. No need to separately destroy it. */
	anjuta_shell_remove_widget (ANJUTA_PLUGIN (gtodo_plugin)->shell, gtodo_plugin->widget,
								NULL);
	anjuta_ui_unmerge (ui, gtodo_plugin->uiid);
	anjuta_ui_remove_action_group (ui, gtodo_plugin->action_group2);
	anjuta_ui_remove_action_group (ui, gtodo_plugin->action_group);
	
	gtodo_plugin->uiid = 0;
	gtodo_plugin->widget = NULL;
	gtodo_plugin->action_group = NULL;
	gtodo_plugin->action_group2 = NULL;
	
	return TRUE;
}

static void
project_root_added (AnjutaPlugin *plugin, const gchar *name,
					const GValue *value, gpointer user_data)
{
	const gchar *root_uri;

	root_uri = g_value_get_string (value);
	
	if (root_uri)
	{
		GFile *file;
		gchar *todo_file;
		GError *error = NULL;
		
		todo_file = g_strconcat (root_uri, "/TODO.tasks", NULL);
		file = g_file_parse_name (todo_file);
		if (!gtodo_client_load (cl, file, &error))
		{
			remove_gui (ANJUTA_PLUGIN_GTODO (plugin));
			anjuta_util_dialog_error (GTK_WINDOW (plugin->shell), "Unable to load TODO file: %s", error->message);
			g_error_free (error);
			error = NULL;
		}
		else
		{
			create_gui (ANJUTA_PLUGIN_GTODO (plugin));
		}
		g_free (todo_file);
		g_object_unref (file);
	}
}

static void
project_root_removed (AnjutaPlugin *plugin, const gchar *name,
					  gpointer user_data)
{
	const gchar * home;
	gchar *default_todo;
	GFile* file;
	GError *error = NULL;
	
	home = g_get_home_dir ();
	default_todo = g_strconcat ("file://", home, "/.gtodo/todos", NULL);
	file = g_file_new_for_uri (default_todo);
	
	if (!gtodo_client_load (cl, file, &error))
	{
		remove_gui (ANJUTA_PLUGIN_GTODO (plugin));
		anjuta_util_dialog_error (GTK_WINDOW (plugin->shell), "Unable to load TODO file: %s", error->message);
		g_error_free (error);
		error = NULL;
	}
	else
	{
		create_gui (ANJUTA_PLUGIN_GTODO (plugin));
	}
	g_free (default_todo);
	g_object_unref (file);
}

static gboolean
activate_plugin (AnjutaPlugin *plugin)
{
	GTodoPlugin *gtodo_plugin;
	static gboolean initialized;
	
	DEBUG_PRINT ("%s", "GTodoPlugin: Activating Task manager plugin ...");
	gtodo_plugin = ANJUTA_PLUGIN_GTODO (plugin);
	
	if (!initialized)
	{
		gtodo_load_settings();
	}
	
	create_gui (ANJUTA_PLUGIN_GTODO (plugin));
	
	/* set up project directory watch */
	gtodo_plugin->root_watch_id = anjuta_plugin_add_watch (plugin,
													IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI,
													project_root_added,
													project_root_removed, NULL);
	
	initialized = TRUE;													
	return TRUE;
}

static gboolean
deactivate_plugin (AnjutaPlugin *plugin)
{
	GTodoPlugin *gplugin = ANJUTA_PLUGIN_GTODO (plugin);
	
	DEBUG_PRINT ("%s", "GTodoPlugin: Dectivating Tasks manager plugin ...");
	
	anjuta_plugin_remove_watch (plugin, gplugin->root_watch_id, TRUE);

	remove_gui (gplugin);
	
	gplugin->root_watch_id = 0;
	
	return TRUE;
}

static void
finalize (GObject *obj)
{
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
dispose (GObject *obj)
{
	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
gtodo_plugin_instance_init (GObject *obj)
{
	GTodoPlugin *plugin = ANJUTA_PLUGIN_GTODO (obj);
	plugin->uiid = 0;
}

static void
gtodo_plugin_class_init (GObjectClass *klass) 
{
	AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	plugin_class->activate = activate_plugin;
	plugin_class->deactivate = deactivate_plugin;
	klass->dispose = dispose;
	klass->finalize = finalize;
}

static void
itodo_load (IAnjutaTodo *plugin, GFile* file, GError **err)
{
	g_return_if_fail (file != NULL);
	if (!gtodo_client_load (cl, file, err))
	{
		remove_gui (ANJUTA_PLUGIN_GTODO (plugin));
	}
	else
	{
		create_gui (ANJUTA_PLUGIN_GTODO (plugin));
	}
}

static void
itodo_iface_init(IAnjutaTodoIface *iface)
{
	iface->load = itodo_load;
}

static void
ipreferences_merge(IAnjutaPreferences* ipref, AnjutaPreferences* prefs, GError** e)
{
	/* Add preferences */
	GTodoPlugin* gtodo_plugin = ANJUTA_PLUGIN_GTODO (ipref); 
	GdkPixbuf *pixbuf;
	
	pixbuf = gdk_pixbuf_new_from_file (PACKAGE_PIXMAPS_DIR"/"ICON_FILE, NULL);
	gtodo_plugin->prefs = preferences_widget();
	anjuta_preferences_dialog_add_page (ANJUTA_PREFERENCES_DIALOG (anjuta_preferences_get_dialog (prefs)), 
										"GTodo",
										_("To-do Manager"),
										pixbuf, gtodo_plugin->prefs);
	g_object_unref (pixbuf);
}

static void
ipreferences_unmerge(IAnjutaPreferences* ipref, AnjutaPreferences* prefs, GError** e)
{
	preferences_remove_signals();
	anjuta_preferences_remove_page(prefs, _("To-do Manager"));
}


static void
ipreferences_iface_init(IAnjutaPreferencesIface* iface)
{
	iface->merge = ipreferences_merge;
	iface->unmerge = ipreferences_unmerge;	
}


ANJUTA_PLUGIN_BEGIN (GTodoPlugin, gtodo_plugin);
ANJUTA_PLUGIN_ADD_INTERFACE (itodo, IANJUTA_TYPE_TODO);
ANJUTA_PLUGIN_ADD_INTERFACE(ipreferences, IANJUTA_TYPE_PREFERENCES);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (GTodoPlugin, gtodo_plugin);
