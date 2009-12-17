/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) James Liggett 2007 <jrliggett@cox.net>
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

#ifndef GIT_UI_UTILS_H
#define GIT_UI_UTILS_H

#include <libanjuta/anjuta-vcs-status-tree-view.h>
#include <libanjuta/interfaces/libanjuta-interfaces.h>
#include "plugin.h"
#include "git-status-command.h"
#include "git-diff-command.h"
#include "git-branch-list-command.h"
#include "git-stash-list-command.h"

typedef struct
{
	GtkBuilder *bxml;
	Git* plugin;
} GitUIData;

typedef struct
{
	Git *plugin;
	gint last_progress;
	gchar *text;
} GitProgressData;

GitUIData* git_ui_data_new (Git* plugin, GtkBuilder *bxml);
void git_ui_data_free (GitUIData* data);
GitProgressData *git_progress_data_new (Git *plugin, const gchar *text);
void git_progress_data_free (GitProgressData *data);
void git_create_message_view (Git* plugin);
gboolean git_check_input (GtkWidget *parent, GtkWidget *widget,  
						  const gchar *input, const gchar *error_message);
gboolean git_get_selected_stash (GtkTreeSelection *selection, gchar **stash);
gboolean git_check_branches (GtkComboBox *combo_box);
gchar *git_get_log_from_textview (GtkWidget* textview);
guint git_status_bar_progress_pulse (Git *plugin, gchar *text);
void git_clear_status_bar_progress_pulse (guint timer_id);
void git_pulse_progress_bar (GtkProgressBar *progress_bar);
void git_report_errors (AnjutaCommand *command, guint return_code);
gchar *git_get_filename_from_full_path (gchar *path);
const gchar *git_get_relative_path (const gchar *path, 
									const gchar *working_directory);

/* Stock signal handlers */
void on_git_command_finished (AnjutaCommand *command, guint return_code, 
							  gpointer user_data);
void on_git_status_command_data_arrived (AnjutaCommand *command, 
										 AnjutaVcsStatusTreeView *tree_view);
void on_git_command_info_arrived (AnjutaCommand *command, Git *plugin);
void on_git_command_progress (AnjutaCommand *command, gfloat progress, 
							  GitProgressData *data);
void on_git_list_branch_combo_command_data_arrived (AnjutaCommand *command,
                                                    GtkListStore *branch_combo_model);
void on_git_list_branch_command_data_arrived (AnjutaCommand *command,
											  GtkListStore *branch_list_model);
void on_git_list_branch_combo_command_finished (AnjutaCommand *command,
                                                guint return_code,
                                                GtkComboBox *combo_box);
void on_git_list_tag_command_data_arrived (AnjutaCommand *command, 
									   GtkListStore *tag_list_model);
void on_git_list_stash_command_data_arrived (AnjutaCommand *command,
											 GtkListStore *stash_list_model);
void on_git_stash_save_command_finished (AnjutaCommand *command, 
										 guint return_code, Git *plugin);
void on_git_stash_apply_command_finished (AnjutaCommand *command,
										  guint return_code, Git *plugin);
void on_git_remote_list_command_data_arrived (AnjutaCommand *command,
                                              GtkListStore *remote_list_model);
void on_git_notebook_button_toggled (GtkToggleButton *toggle_button,
                                     GtkNotebook *notebook);
void git_select_all_status_items (GtkButton *select_all_button, 
								  AnjutaVcsStatusTreeView *tree_view);
void git_clear_all_status_selections (GtkButton *clear_button,
									  AnjutaVcsStatusTreeView *tree_view);
void on_git_origin_check_toggled (GtkToggleButton *button, GtkWidget *widget);
void git_init_whole_project (Git *plugin, GtkWidget* project,
							 gboolean active);
void on_git_whole_project_toggled (GtkToggleButton* project, Git *plugin);
void on_git_diff_command_finished (AnjutaCommand *command, guint return_code, 
								   Git *plugin);
void git_send_raw_command_output_to_editor (AnjutaCommand *command, 
											IAnjutaEditor *editor);
void git_set_log_view_column_label (GtkTextBuffer *text_buffer, 
                                    GtkTextIter *location, 
                                    GtkTextMark *mark, GtkLabel *column_label);
void git_stop_status_bar_progress_pulse (AnjutaCommand *command, 
										 guint return_code, gpointer timer_id);
void git_hide_pulse_progress_bar (AnjutaCommand *command, guint return_code,
								  GtkProgressBar *progress_bar);
void git_disconnect_data_arrived_signals (AnjutaCommand *command, GObject *object);
void git_cancel_data_arrived_signal_disconnect (AnjutaCommand *command, 
												guint return_code,
												GObject *signal_target);
gboolean git_get_selected_refs (GtkTreeModel *model, GtkTreePath *path, 
							    GtkTreeIter *iter, GList **selected_list);
void on_git_selected_column_toggled (GtkCellRendererToggle *renderer,
									 gchar *path, GtkListStore *list_store);
#endif
