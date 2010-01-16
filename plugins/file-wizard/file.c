/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */

/***************************************************************************
 *            file.c
 *
 *  Sun Nov 30 17:46:54 2003
 *  Copyright  2003  Jean-Noel Guiheneuf
 *  jnoel@lotuscompounds.com
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <time.h>

#include <libanjuta/interfaces/libanjuta-interfaces.h>

#include "plugin.h"
#include "file.h"

#define BUILDER_FILE_FILE PACKAGE_DATA_DIR"/glade/anjuta-file-wizard.ui"
#define NEW_FILE_DIALOG "dialog.new.file"
#define NEW_FILE_ENTRY "new.file.entry"
#define NEW_FILE_TYPE "new.file.type"
#define NEW_FILE_TYPE_STORE "new.file.type.store"
#define NEW_FILE_TEMPLATE "new.file.template"
#define NEW_FILE_HEADER "new.file.header"
#define NEW_FILE_LICENSE "new.file.license"
#define NEW_FILE_MENU_LICENSE "new.file.menu.license"
#define NEW_FILE_MENU_LICENSE_STORE "new.file.menu.license.store"
#define NEW_FILE_ADD_TO_PROJECT "add_to_project"
#define NEW_FILE_ADD_TO_REPOSITORY "add_to_repository"

typedef struct _NewFileGUI
{
	GtkBuilder *bxml;
	GtkWidget *dialog;
	GtkWidget *add_to_project;
	GtkWidget *add_to_repository;
	gboolean showing;
	AnjutaFileWizardPlugin *plugin;
} NewFileGUI;


typedef struct _NewfileType
{
	gchar *name;
	gchar *ext;
	gint header;
	gboolean gpl;
	gboolean template;
	Cmt comment;
	Lge type;
} NewfileType;



NewfileType new_file_type[] = {
	{N_("C Source File"), ".c", LGE_HC, TRUE, TRUE, CMT_C, LGE_C},
	{N_("C/C++ Header File"), ".h", -1, TRUE, TRUE, CMT_C, LGE_HC},
	{N_("C++ Source File"), ".cxx", LGE_HC, TRUE, TRUE, CMT_CPP, LGE_CPLUS},
	{N_("C# Source File"), ".c#", LGE_HC, FALSE, TRUE, CMT_CPP, LGE_CSHARP},
	{N_("Java Source File"), ".java", -1, FALSE, FALSE, CMT_CPP, LGE_JAVA},
	{N_("Perl Source File"), ".pl", -1, TRUE, TRUE, CMT_P, LGE_PERL},
	{N_("Python Source File"), ".py", -1, TRUE, FALSE, CMT_P, LGE_PYTHON},
	{N_("Shell Script File"), ".sh", -1, TRUE, TRUE, CMT_P, LGE_SHELL},
	{N_("Other"), NULL, -1, FALSE, FALSE, CMT_C, LGE_C}
};


typedef struct _NewlicenseType
{
	gchar *name;
	const gchar* type;
} NewlicenseType;

NewlicenseType new_license_type[] = {
	{N_("General Public License (GPL)"), "GPL"},
	{N_("Lesser General Public License (LGPL)"), "LGPL"},
	{N_("BSD Public License"), "BSD"}
};

static NewFileGUI *nfg = NULL;


static gboolean create_new_file_dialog(IAnjutaDocumentManager *docman);
static void insert_notice(IAnjutaMacro* macro, const gchar* license_type, gint comment_type);
static void insert_header(IAnjutaMacro* macro, gint source_type);

static void
on_add_to_project_toggled (GtkWidget* toggle_button, NewFileGUI *gui)
{
	gboolean status = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(toggle_button));
	gtk_widget_set_sensitive (gui->add_to_repository, status);
	if (!status)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gui->add_to_repository),
		                              FALSE);
}

void
display_new_file(AnjutaFileWizardPlugin *plugin,
				 IAnjutaDocumentManager *docman)
{
	IAnjutaProjectCapabilities caps =
		IANJUTA_PROJECT_CAN_ADD_NONE;
	
	if (!nfg)
		if (!create_new_file_dialog (docman))
			return;
	
	nfg->plugin = plugin;
	
	/* check whether we have a loaded project or not */
	if (plugin->top_dir) {
		IAnjutaProjectManager *manager =
			anjuta_shell_get_interface (ANJUTA_PLUGIN (plugin)->shell,
										IAnjutaProjectManager, NULL);
       if (manager)
			caps = ianjuta_project_manager_get_capabilities (manager, NULL);
	}

	g_signal_connect (nfg->add_to_project, "toggled", 
	                  G_CALLBACK(on_add_to_project_toggled),
	                  nfg);
	
	if ((caps & IANJUTA_PROJECT_CAN_ADD_SOURCE) == FALSE) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (nfg->add_to_project),
									  FALSE);
		gtk_widget_set_sensitive (nfg->add_to_project, FALSE);
	}
	else
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (nfg->add_to_project),
									  TRUE);
		gtk_widget_set_sensitive (nfg->add_to_project, TRUE);
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (nfg->add_to_repository),
								  FALSE);
	
	if (nfg && !(nfg->showing))
	{
		gtk_window_present (GTK_WINDOW (nfg->dialog));
		nfg->showing = TRUE;
	}
}

static gboolean
create_new_file_dialog(IAnjutaDocumentManager *docman)
{
	GtkComboBox *optionmenu;
	GtkListStore *store;
	gint i;

	nfg = g_new0(NewFileGUI, 1);
	nfg->bxml =  gtk_builder_new ();
	if (!gtk_builder_add_from_file (nfg->bxml, BUILDER_FILE_FILE, NULL))
	{
		anjuta_util_dialog_error(NULL, _("Unable to build user interface for New File"));
		g_free(nfg);
		nfg = NULL;
		return FALSE;
	}
	nfg->dialog = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_DIALOG));
	nfg->add_to_project = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_ADD_TO_PROJECT));
	nfg->add_to_repository = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_ADD_TO_REPOSITORY));
	nfg->showing = FALSE;
	
	store = GTK_LIST_STORE (gtk_builder_get_object (nfg->bxml, NEW_FILE_TYPE_STORE));
	for (i=0; i < (sizeof(new_file_type) / sizeof(NewfileType)); i++)
	{
		GtkTreeIter iter;
		
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, new_file_type[i].name, -1);
	}
	optionmenu = GTK_COMBO_BOX (gtk_builder_get_object (nfg->bxml, NEW_FILE_TYPE));
	gtk_combo_box_set_active (optionmenu, 0);

	store = GTK_LIST_STORE (gtk_builder_get_object (nfg->bxml, NEW_FILE_MENU_LICENSE_STORE));
	for (i=0; i < (sizeof(new_license_type) / sizeof(NewlicenseType)); i++)
	{
		GtkTreeIter iter;
		
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, new_license_type[i].name, -1);
	}
	optionmenu = GTK_COMBO_BOX (gtk_builder_get_object (nfg->bxml, NEW_FILE_MENU_LICENSE));
	gtk_combo_box_set_active (optionmenu, 0);
	
	g_object_set_data (G_OBJECT (nfg->dialog), "IAnjutaDocumentManager", docman);
	gtk_builder_connect_signals (nfg->bxml, NULL);
	g_signal_emit_by_name(G_OBJECT (optionmenu), "changed");
	
	return TRUE;
}

gboolean
on_new_file_cancelbutton_clicked(GtkWidget *window, GdkEvent *event,
			                     gboolean user_data)
{
	if (nfg->showing)
	{
		gtk_widget_hide(nfg->dialog);
		nfg->showing = FALSE;
	}
	return TRUE;
}

gboolean
on_new_file_okbutton_clicked(GtkWidget *window, GdkEvent *event,
			                 gboolean user_data)
{
	GtkWidget *entry;
	GtkWidget *checkbutton;
	GtkWidget *optionmenu;
	const gchar *name;
	gchar *header_name = NULL;
	gint sel;
	const gchar* license_type;
	gint comment_type;
	gint source_type;
	IAnjutaDocumentManager *docman;
	GtkWidget *toplevel;
	IAnjutaMacro* macro;
	IAnjutaEditor *te = NULL;
	IAnjutaEditor *teh = NULL;
	gboolean ok = TRUE;
	
	toplevel= gtk_widget_get_toplevel (window);
	docman = IANJUTA_DOCUMENT_MANAGER (g_object_get_data (G_OBJECT(toplevel),
										"IAnjutaDocumentManager"));
	macro = anjuta_shell_get_interface (ANJUTA_PLUGIN(docman)->shell, 
		                       IAnjutaMacro, NULL);
	entry = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_ENTRY));
	name = gtk_entry_get_text(GTK_ENTRY(entry));

	/* Create main file */
	if (name && strlen (name) > 0)
		te = ianjuta_document_manager_add_buffer (docman, name, NULL, NULL);
	else
		te = ianjuta_document_manager_add_buffer (docman, "", NULL, NULL);
	
	if (te == NULL)
		return FALSE;
	
	/* Create header file */
	optionmenu = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_TYPE));
	source_type = gtk_combo_box_get_active(GTK_COMBO_BOX(optionmenu));
	
	checkbutton = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_HEADER));
	if (gtk_widget_get_sensitive (checkbutton) &&
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton)))
	{
		if (name && strlen (name) > 0)
		{
			const gchar *old_ext = strrchr (name,'.');
			const gchar *new_ext =  new_file_type[new_file_type[source_type].header].ext;
			
			if (old_ext == NULL)
			{
				header_name = g_strconcat (name, new_ext, NULL);
			}
			else
			{
				header_name = g_strndup (name, old_ext - name + strlen(new_ext));
				strcpy(&header_name[old_ext - name], new_ext);
			}
			teh = ianjuta_document_manager_add_buffer (docman, header_name, NULL, NULL);
		}
		else
		{
			teh = ianjuta_document_manager_add_buffer (docman, "", NULL, NULL);
		}
		ianjuta_document_manager_set_current_document (docman, IANJUTA_DOCUMENT(te), NULL);		
	}

	checkbutton = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_TEMPLATE));
	if (gtk_widget_get_sensitive (checkbutton) &&
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton)))
	{
		insert_header(macro, source_type);
		if (teh != NULL)
		{
			ianjuta_document_manager_set_current_document (docman, IANJUTA_DOCUMENT(teh), NULL);		
			insert_header (macro, new_file_type[source_type].header);
			ianjuta_document_manager_set_current_document (docman, IANJUTA_DOCUMENT(te), NULL);		
		}
	}
	
	checkbutton = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_LICENSE));
	if (gtk_widget_get_sensitive (checkbutton) &&
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton)))
	{
		optionmenu = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_MENU_LICENSE));
		sel = gtk_combo_box_get_active(GTK_COMBO_BOX(optionmenu));
		license_type = new_license_type[sel].type;
		comment_type = new_file_type[source_type].comment;
		                                  
		insert_notice(macro, license_type, comment_type);		
		if (teh != NULL)
		{
			comment_type = new_file_type[new_file_type[source_type].header].comment;
			ianjuta_document_manager_set_current_document (docman, IANJUTA_DOCUMENT(teh), NULL);		
			insert_notice(macro, license_type, comment_type);		
			ianjuta_document_manager_set_current_document (docman, IANJUTA_DOCUMENT(te), NULL);		
		}
	}
	
	/* Add file to project */
	if (nfg->plugin->top_dir &&
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (nfg->add_to_project)))
	{
		IAnjutaProjectManager *pm;
		GFile* file;
		GList *names = NULL;
		GList *uri_list;
		
		pm = anjuta_shell_get_interface (ANJUTA_PLUGIN(docman)->shell, 
										 IAnjutaProjectManager, NULL);
		g_return_val_if_fail (pm != NULL, FALSE);

		if (teh) names = g_list_prepend (names, header_name);
		names = g_list_prepend (names, (gpointer) name);
		uri_list = ianjuta_project_manager_add_sources (pm, names, NULL, NULL);
		g_list_free (names);
		
		if (uri_list)
		{
			GList* node;

			/* Save main file */
			file = g_file_new_for_uri ((const gchar *)uri_list->data);
			ianjuta_file_savable_save_as (IANJUTA_FILE_SAVABLE (te), file, NULL);		
			g_object_unref (file);
			
			if (uri_list->next)
			{
				/* Save header file */
				file = g_file_new_for_uri ((const gchar *)uri_list->next->data);
				ianjuta_file_savable_save_as (IANJUTA_FILE_SAVABLE (teh), file, NULL);		
				g_object_unref (file);
			}		


			/* Add to repository */
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (nfg->add_to_repository)))
			{
				IAnjutaVcs* ivcs = anjuta_shell_get_interface (ANJUTA_PLUGIN(docman)->shell, 
										 IAnjutaVcs, NULL);
				if (ivcs)
				{					
					GList* files = NULL;
					AnjutaAsyncNotify* notify = anjuta_async_notify_new();
					for (node = uri_list; node != NULL; node = g_list_next (node))
					{
						files = g_list_append (files, g_file_new_for_uri (node->data));
					}
					ianjuta_vcs_add (ivcs, files, notify, NULL);
					g_list_foreach (files, (GFunc) g_object_unref, NULL);
				}
			}

			/* Re emit element_added for symbol-db */
			for (node = uri_list; node != NULL; node = g_list_next (node))
			{
				g_signal_emit_by_name (G_OBJECT (pm), "element_added", node->data);
			}

			g_list_foreach (uri_list, (GFunc)g_free, NULL);
			g_list_free (uri_list);
		}
		else
		{
			ok = FALSE;
		}
	}
	g_free (header_name);
	
	gtk_widget_hide (nfg->dialog);
	nfg->showing = FALSE;
	
	return ok;
}

void
on_new_file_entry_changed (GtkEditable *entry, gpointer user_data)
{
	char *name;
	gint sel;
	static gint last_length = 0;
	gint length;
	GtkWidget *optionmenu;
	
	name = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	length = strlen(name);
	
	if (last_length != 2 && length == 1)
	{
		optionmenu = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_TYPE));
		sel = gtk_combo_box_get_active(GTK_COMBO_BOX(optionmenu));
		name = g_strconcat (name, new_file_type[sel].ext, NULL);
		gtk_entry_set_text (GTK_ENTRY(entry), name);
	}
	last_length = length;
	
	g_free(name);
}

void
on_new_file_type_changed (GtkComboBox   *optionmenu, gpointer user_data)
{
	gint sel;
	char *name, *tmp;
	GtkWidget *widget;
	GtkWidget *entry;
	
	sel = gtk_combo_box_get_active(optionmenu);
	
	widget = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_HEADER));
	gtk_widget_set_sensitive(widget, new_file_type[sel].header >= 0);
	widget = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_LICENSE));
	gtk_widget_set_sensitive(widget, new_file_type[sel].gpl);
	widget = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_TEMPLATE));
	gtk_widget_set_sensitive(widget, new_file_type[sel].template);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
	
	entry = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_ENTRY));
	name = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	if (strlen(name) > 0)
	{
		tmp = strrchr(name, '.');
		if (tmp)
			name = g_strndup(name, tmp - name);
		name =  g_strconcat (name, new_file_type[sel].ext, NULL);
		gtk_entry_set_text (GTK_ENTRY(entry), name);
	}
	g_free(name);
}

void
on_new_file_license_toggled(GtkToggleButton *button, gpointer user_data)
{
	GtkWidget *widget;
	
	widget = GTK_WIDGET (gtk_builder_get_object (nfg->bxml, NEW_FILE_MENU_LICENSE));
	gtk_widget_set_sensitive(widget, gtk_toggle_button_get_active(button));
}

static void
insert_notice(IAnjutaMacro* macro, const gchar* license_type, gint comment_type)
{
	const gchar *template;
	gchar *name;

	switch (comment_type)
	{
		case CMT_C:
			template = "/* %s */";
			break;
		case CMT_CPP:
			template = "// %s";
			break;
		case CMT_P:
			template = "# %s";
			break;
		default:
			template = "/* %s */";
			break;
	}

	name = g_strdup_printf(template, license_type);
	ianjuta_macro_insert(macro, name, NULL);
	g_free (name);
}

static void
insert_header(IAnjutaMacro* macro, gint source_type)
{
	switch (source_type)
	{
		case  LGE_C: case LGE_HC:
			ianjuta_macro_insert(macro, "Header_c", NULL);
			break;
		case  LGE_CPLUS: case LGE_JAVA:
			ianjuta_macro_insert(macro, "Header_cpp", NULL);
			break;
		case  LGE_CSHARP:
			ianjuta_macro_insert(macro, "Header_csharp", NULL);
			break;
		case LGE_PERL:
			ianjuta_macro_insert(macro, "Header_perl", NULL);
			break;
		case LGE_SHELL:
			ianjuta_macro_insert(macro, "Header_shell", NULL);
			break;
		default:
			break;
	}
}
