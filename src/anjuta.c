/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta.c
 * Copyright (C) 2000 Naba Kumar  <naba@gnome.org>
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA 
 */

#include <string.h>
#include <libanjuta/anjuta-shell.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-utils.h>
#include <libanjuta/anjuta-save-prompt.h>
#include <libanjuta/anjuta-plugin-manager.h>
#include <libanjuta/interfaces/libanjuta-interfaces.h>

#include "anjuta.h"

#define ANJUTA_REMEMBERED_PLUGINS "anjuta.remembered.plugins"
#define ANJUTA_SESSION_SKIP_LAST "anjuta.session.skip.last"
#define ANJUTA_SESSION_SKIP_LAST_FILES "anjuta.session.skip.last.files"
#define USER_SESSION_PATH_NEW (anjuta_util_get_user_cache_file_path ("session/", NULL))

#define DEFAULT_PROFILE "file://"PACKAGE_DATA_DIR"/profiles/default.profile"
#define USER_PROFILE_NAME "user"

static gchar *system_restore_session = NULL;

static gboolean
on_anjuta_delete_event (AnjutaApp *app, GdkEvent *event, gpointer data)
{
	AnjutaPluginManager *plugin_manager;
	AnjutaProfileManager *profile_manager;
	AnjutaProfile *current_profile;
	AnjutaSavePrompt *save_prompt;
	gchar *remembered_plugins;
	
	DEBUG_PRINT ("%s", "AnjutaApp delete event");
	
	plugin_manager = anjuta_shell_get_plugin_manager (ANJUTA_SHELL (app), NULL);
	profile_manager = anjuta_shell_get_profile_manager (ANJUTA_SHELL (app), NULL);
	
	/* Save remembered plugins */
	remembered_plugins =
		anjuta_plugin_manager_get_remembered_plugins (plugin_manager);
	anjuta_preferences_set (app->preferences,
							ANJUTA_REMEMBERED_PLUGINS,
							remembered_plugins);
	g_free (remembered_plugins);
	
	/* Check for unsaved data */
	save_prompt = anjuta_save_prompt_new (GTK_WINDOW (app));
	anjuta_shell_save_prompt (ANJUTA_SHELL (app), save_prompt, NULL);

	if (anjuta_save_prompt_get_items_count (save_prompt) > 0)
	{
		switch (gtk_dialog_run (GTK_DIALOG (save_prompt)))
		{
			case ANJUTA_SAVE_PROMPT_RESPONSE_CANCEL:
				gtk_widget_destroy (GTK_WIDGET (save_prompt));
				/* Do not exit now */
				return TRUE;
			case ANJUTA_SAVE_PROMPT_RESPONSE_DISCARD:
			case ANJUTA_SAVE_PROMPT_RESPONSE_SAVE_CLOSE:
				/* exit now */
				break;
		}
	}
	/* Wait for files to be really saved (asyncronous operation) */
	if (app->save_count > 0)
	{
		g_message ("Waiting for %d file(s) to be saved!", app->save_count);
		while (app->save_count > 0)
		{
			g_main_context_iteration (NULL, TRUE);
		}
	}
	
	/* If current active profile is "user", save current session as
	 * default session
	 */
	current_profile = anjuta_profile_manager_get_current (profile_manager);
	if (strcmp (anjuta_profile_get_name (current_profile), "user") == 0)
	{
		gchar *session_dir;
		session_dir = USER_SESSION_PATH_NEW;
		anjuta_shell_session_save (ANJUTA_SHELL (app), session_dir, NULL);
		g_free (session_dir);
	}
	
	anjuta_shell_notify_exit (ANJUTA_SHELL (app), NULL);
	
	gtk_widget_destroy (GTK_WIDGET (save_prompt));
	
	/* Shutdown */
	if (g_object_get_data (G_OBJECT (app), "__proper_shutdown"))
	{
		gtk_widget_hide (GTK_WIDGET (app));
		anjuta_plugin_manager_unload_all_plugins
			(anjuta_shell_get_plugin_manager (ANJUTA_SHELL (app), NULL));
	}
	return FALSE;
}

static void
on_anjuta_destroy (GtkWidget * w, gpointer data)
{
	DEBUG_PRINT ("%s", "AnjutaApp destroy event");
	
	gtk_widget_hide (w);
	gtk_main_quit ();
}

static void
on_profile_scoped (AnjutaProfileManager *profile_manager,
				   AnjutaProfile *profile, AnjutaApp *app)
{
	gchar *session_dir;
	static gboolean first_time = TRUE;
	
	DEBUG_PRINT ("Profile scoped: %s", anjuta_profile_get_name (profile));
	if (strcmp (anjuta_profile_get_name (profile), USER_PROFILE_NAME) != 0)
		return;
	
	DEBUG_PRINT ("%s", "User profile loaded; Restoring user session");
	
	/* If profile scoped to "user", restore user session */
	if (system_restore_session)
	{
		session_dir = system_restore_session;
		system_restore_session = NULL;
	}
	else
	{
		session_dir = USER_SESSION_PATH_NEW;
	}
	
	if (first_time)
	{
		first_time = FALSE;
	}
	else
	{
		/* Clear the files list since we don't want to load them later */
		AnjutaSession *session;
		session = anjuta_session_new (session_dir);
		anjuta_session_set_string_list (session, "File Loader",
										"Files", NULL);
		anjuta_session_sync (session);
		g_object_unref (session);
	}
	
	/* Restore session */
	anjuta_shell_session_load (ANJUTA_SHELL (app), session_dir, NULL);
	g_free (session_dir);
}

static void
on_profile_descoped (AnjutaProfileManager *profile_manager,
					 AnjutaProfile *profile, AnjutaApp *app)
{
	gchar *session_dir;
	
	DEBUG_PRINT ("Profile descoped: %s", anjuta_profile_get_name (profile));
	if (strcmp (anjuta_profile_get_name (profile), USER_PROFILE_NAME) != 0)
		return;
	
	DEBUG_PRINT ("%s", "User profile descoped; Saving user session");
	
	/* If profile descoped from is "user", save user session */
	session_dir = USER_SESSION_PATH_NEW;
	
	/* Save current session */
	anjuta_shell_session_save (ANJUTA_SHELL (app), session_dir, NULL);
	g_free (session_dir);
}

/* This extracts the project URI saved in the session so that it could be
 * loaded sequencially. Returned string must be freed.
 */
static gchar*
extract_project_from_session (const gchar* session_dir)
{
	AnjutaSession *session;
	GList *node, *files;
	gchar *project_uri = NULL;
	
	session = anjuta_session_new (session_dir);
	
	files = anjuta_session_get_string_list (session, "File Loader", "Files");
	g_object_unref (session);
	
	/* Find project file */
	node = files;
	for (node = files; node != NULL; node = g_list_next (node))
	{
		gchar *uri = node->data;
		if (uri)
		{
			if (anjuta_util_is_project_file (uri))
			{
				g_free (project_uri);
				project_uri = uri;
			}
			else
			{
				g_free (uri);
			}
		}
	}
	g_list_free (files);
	
	return project_uri;
}

/* FIXME: Clean this mess */
AnjutaApp*
anjuta_new (gchar *prog_name, gchar **files, gboolean no_splash,
			gboolean no_session, gboolean no_files,
			const gchar *im_file,
			gboolean proper_shutdown, const gchar *geometry)
{
	AnjutaPluginManager *plugin_manager;
	AnjutaProfileManager *profile_manager;
	AnjutaApp *app;
	AnjutaStatus *status;
	AnjutaProfile *profile;
	GFile *session_profile;
	gchar *remembered_plugins;
	gchar *project_uri = NULL;
	gchar *profile_name = NULL;
	GError *error = NULL;
	
	/* Initialize application */
	app = ANJUTA_APP (anjuta_app_new ());
	status = anjuta_shell_get_status (ANJUTA_SHELL (app), NULL);
	anjuta_status_progress_add_ticks (status, 1);
	
	if (im_file)
		anjuta_status_set_splash (status, im_file, 100);
	if (no_splash)
		anjuta_status_disable_splash (status, no_splash);

	if (proper_shutdown)
	{
		g_object_set_data (G_OBJECT (app), "__proper_shutdown", "1");
	}
	g_signal_connect (G_OBJECT (app), "delete_event",
					  G_CALLBACK (on_anjuta_delete_event), NULL);
	g_signal_connect (G_OBJECT (app), "destroy",
					  G_CALLBACK (on_anjuta_destroy), NULL);
	plugin_manager  = anjuta_shell_get_plugin_manager (ANJUTA_SHELL (app),
													   NULL);
	profile_manager = anjuta_shell_get_profile_manager (ANJUTA_SHELL (app),
														NULL);
	
	/* Restore remembered plugins */
	remembered_plugins =
		anjuta_preferences_get (app->preferences, ANJUTA_REMEMBERED_PLUGINS);
	if (remembered_plugins)
		anjuta_plugin_manager_set_remembered_plugins (plugin_manager,
													  remembered_plugins);
	g_free (remembered_plugins);
	
	/* Prepare profile */
	profile = anjuta_profile_new (USER_PROFILE_NAME, plugin_manager);
	session_profile = g_file_new_for_uri (DEFAULT_PROFILE);
	anjuta_profile_add_plugins_from_xml (profile, session_profile,
										 TRUE, &error);
	if (error)
	{
		anjuta_util_dialog_error (GTK_WINDOW (app), "%s", error->message);
		g_error_free (error);
		error = NULL;
	}
	g_object_unref (session_profile);
	
	/* Load user session profile */
	profile_name = g_path_get_basename (DEFAULT_PROFILE);
	session_profile = anjuta_util_get_user_cache_file (profile_name, NULL);
	if (g_file_query_exists (session_profile, NULL))
	{
		anjuta_profile_add_plugins_from_xml (profile, session_profile,
											 FALSE, &error);
		if (error)
		{
			anjuta_util_dialog_error (GTK_WINDOW (app), "%s", error->message);
			g_error_free (error);
			error = NULL;
		}
	}
	anjuta_profile_set_sync_file (profile, session_profile); 
	g_object_unref (session_profile);
	g_free (profile_name);
	
	/* Load profile */
	anjuta_profile_manager_freeze (profile_manager);
	anjuta_profile_manager_push (profile_manager, profile, &error);
	if (error)
	{
		anjuta_util_dialog_error (GTK_WINDOW (app), "%s", error->message);
		g_error_free (error);
		error = NULL;
	}
	
	if (files || geometry)
	{                 
		gchar *session_dir;
		AnjutaSession *session;
		gchar **node;
		GList *files_load = NULL;
		
		/* Reset default session */
		session_dir = USER_SESSION_PATH_NEW;
		
		project_uri = extract_project_from_session (session_dir);
		
		session = anjuta_session_new (session_dir);
		anjuta_session_clear (session);
		if (geometry)
			anjuta_session_set_string (session, "Anjuta", "Geometry",
									   geometry);
		
		/* Identify non-project files and set them for loading in session */
		for (node = files; *node != NULL; node++)
		{
			if (anjuta_util_is_project_file (*node))
			{
				/* Pick up the first project file for loading later */
				g_free (project_uri);
				project_uri = g_strdup (*node);
			}
			else
			{
				files_load = g_list_prepend (files_load, *node);
			}
		}
		if (files_load)
		{
			anjuta_session_set_string_list (session, "File Loader", "Files",
											files_load);
			g_list_foreach (files_load, (GFunc)g_free, NULL);
			g_list_free (files_load);
		}
		anjuta_session_sync (session);
		g_object_unref (session);
		g_free (session_dir);
	}
	else
	{
		gchar *session_dir;
		AnjutaSession *session;
		
		/* Load user session */
		session_dir = USER_SESSION_PATH_NEW;
		
		/* If preferences is set to not load last session, clear it */
		if (no_session ||
			anjuta_preferences_get_bool (app->preferences,
										ANJUTA_SESSION_SKIP_LAST))
		{
			/* Reset default session */
			session = anjuta_session_new (session_dir);
			anjuta_session_clear (session);
			g_object_unref (session);
		}
		/* If preferences is set to not load last project, clear it */
		else if (no_files ||
				 anjuta_preferences_get_bool (app->preferences,
											 ANJUTA_SESSION_SKIP_LAST_FILES))
		{
			session = anjuta_session_new (session_dir);
			anjuta_session_set_string_list (session, "File Loader",
											"Files", NULL);
			anjuta_session_sync (session);
			g_object_unref (session);
		}
		/* Otherwise, load session normally */
		else
		{
			project_uri = extract_project_from_session (session_dir);
		}
		g_free (session_dir);
	}
	
	/* Prepare for session save and load on profile change */
	g_signal_connect (profile_manager, "profile-scoped",
					  G_CALLBACK (on_profile_scoped), app);
	/* Load project file */
	if (project_uri)
	{
		GFile* file = g_file_new_for_uri (project_uri);
		IAnjutaFileLoader *loader;
		loader = anjuta_shell_get_interface (ANJUTA_SHELL (app),
											 IAnjutaFileLoader, NULL);
		ianjuta_file_loader_load (loader, file, FALSE, NULL);
		g_free (project_uri);
		g_object_unref (file);
	}
	anjuta_profile_manager_thaw (profile_manager, &error);
	
	if (error)
	{
		anjuta_util_dialog_error (GTK_WINDOW (app), "%s", error->message);
		g_error_free (error);
		error = NULL;
	}
	g_signal_connect (profile_manager, "profile-descoped",
					  G_CALLBACK (on_profile_descoped), app);
	
	anjuta_status_progress_tick (status, NULL, _("Loaded Session…"));
	anjuta_status_disable_splash (status, TRUE);
	return app;
}
