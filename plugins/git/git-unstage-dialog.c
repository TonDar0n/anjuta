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

#include "git-unstage-dialog.h"

static void
on_reset_files_command_finished (AnjutaCommand *command, guint return_code,
								 Git *plugin)
{
	AnjutaStatus *status;
	
	status = anjuta_shell_get_status (ANJUTA_PLUGIN (plugin)->shell,
									  NULL);
	
	anjuta_status (status, _("Git: Files unstaged."), 5);
	
	git_report_errors (command, return_code);
	
	g_object_unref (command);
}


static void
on_unstage_dialog_response (GtkDialog *dialog, gint response_id, 
							GitUIData *data)
{
	GtkWidget *unstage_status_view;
	GList *selected_paths;
	GitResetFilesCommand *reset_files_command;
	
	if (response_id == GTK_RESPONSE_OK)
	{	
		unstage_status_view = GTK_WIDGET (gtk_builder_get_object (data->bxml, 
		                                                          "unstage_status_view"));
		selected_paths = anjuta_vcs_status_tree_view_get_selected (ANJUTA_VCS_STATUS_TREE_VIEW (unstage_status_view));
		reset_files_command = git_reset_files_command_new (data->plugin->project_root_directory,
														   GIT_RESET_FILES_HEAD,
														   selected_paths);
		
		git_command_free_string_list (selected_paths);
		
		g_signal_connect (G_OBJECT (reset_files_command), "command-finished",
						  G_CALLBACK (on_reset_files_command_finished),
						  data->plugin);
		
		anjuta_command_start (ANJUTA_COMMAND (reset_files_command));
	}
	
	gtk_widget_destroy (GTK_WIDGET (dialog));
	git_ui_data_free (data);
}

static void
unstage_dialog (Git *plugin)
{
	GtkBuilder *bxml;
	GError *error;
	GtkWidget *dialog;
	GtkWidget *unstage_select_all_button;
	GtkWidget *unstage_clear_button;
	GtkWidget *unstage_status_view;
	GtkWidget *unstage_status_progress_bar;
	GitStatusCommand *status_command;
	GitUIData *data;
	
	bxml = gtk_builder_new ();
	error = NULL;

	if (!gtk_builder_add_from_file (bxml, BUILDER_FILE, &error))
	{
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}
	
	dialog = GTK_WIDGET (gtk_builder_get_object (bxml, "unstage_dialog"));
	unstage_select_all_button = GTK_WIDGET (gtk_builder_get_object (bxml, 
	                                                                "unstage_select_all_button"));
	unstage_clear_button = GTK_WIDGET (gtk_builder_get_object (bxml, 
	                                                           "unstage_clear_button"));
	unstage_status_view = GTK_WIDGET (gtk_builder_get_object (bxml, 
	                                                          "unstage_status_view"));
	unstage_status_progress_bar = GTK_WIDGET (gtk_builder_get_object (bxml, 
	                                                                  "unstage_status_progress_bar"));
	
	status_command = git_status_command_new (plugin->project_root_directory,
											 GIT_STATUS_SECTION_COMMIT);
	
	g_signal_connect (G_OBJECT (unstage_select_all_button), "clicked",
					  G_CALLBACK (git_select_all_status_items),
					  unstage_status_view);
	
	g_signal_connect (G_OBJECT (unstage_clear_button), "clicked",
					  G_CALLBACK (git_clear_all_status_selections),
					  unstage_status_view);
	
	git_pulse_progress_bar (GTK_PROGRESS_BAR (unstage_status_progress_bar));
	
	g_signal_connect (G_OBJECT (status_command), "command-finished",
					  G_CALLBACK (git_cancel_data_arrived_signal_disconnect),
					  unstage_status_view);
	
	g_signal_connect (G_OBJECT (status_command), "command-finished",
					  G_CALLBACK (git_hide_pulse_progress_bar),
					  unstage_status_progress_bar);
	
	g_signal_connect (G_OBJECT (status_command), "command-finished",
					  G_CALLBACK (on_git_command_finished),
					  NULL);
	
	g_signal_connect (G_OBJECT (status_command), "data-arrived",
					  G_CALLBACK (on_git_status_command_data_arrived),
					  unstage_status_view);
	
	g_object_weak_ref (G_OBJECT (unstage_status_view),
					   (GWeakNotify) git_disconnect_data_arrived_signals,
					   status_command);
	
	anjuta_command_start (ANJUTA_COMMAND (status_command));
	
	data = git_ui_data_new (plugin, bxml);
	
	g_signal_connect(G_OBJECT (dialog), "response", 
					 G_CALLBACK (on_unstage_dialog_response), 
					 data);
	
	gtk_widget_show_all (dialog);
}

void
on_menu_git_unstage (GtkAction *action, Git *plugin)
{
	unstage_dialog (plugin);
}
