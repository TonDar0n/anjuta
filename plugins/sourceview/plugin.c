/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * plugin.c
 * Copyright (C) Johannes Schmid 2005 <jhs@gnome.org>
 * 
 * plugin.c is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * plugin.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with plugin.c.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <config.h>
#include <libanjuta/anjuta-shell.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-preferences.h>
#include <libanjuta/anjuta-status.h>
#include <libanjuta/interfaces/ianjuta-document-manager.h>
#include <libanjuta/interfaces/ianjuta-editor-factory.h>
#include <libanjuta/interfaces/ianjuta-editor.h>

#include "plugin.h"
#include "sourceview.h"

#define PREFS_GLADE PACKAGE_DATA_DIR"/glade/sourceview.glade"
#define ICON_FILE "sourceview.png"

static gpointer parent_class;

static gboolean
sourceview_plugin_activate (AnjutaPlugin *plugin)
{	
    /* Add preferences */
	AnjutaPreferences* prefs;
	AnjutaShell* shell;
	GladeXML* gxml;
	g_object_get(G_OBJECT(plugin), "shell", &shell, NULL);
	prefs = anjuta_shell_get_preferences(shell, NULL);
	gxml = glade_xml_new (PREFS_GLADE, "preferences_dialog", NULL);
	anjuta_preferences_add_page (prefs,
								 gxml, "Editor", ICON_FILE);
	DEBUG_PRINT ("SourceviewPlugin: Activating SourceviewPlugin plugin ...");

	return TRUE;
}

static gboolean
sourceview_plugin_deactivate (AnjutaPlugin *plugin)
{
	DEBUG_PRINT ("SourceviewPlugin: Dectivating SourceviewPlugin plugin ...");
	
	return TRUE;
}

static void
sourceview_plugin_finalize (GObject *obj)
{
	/* Finalization codes here */
	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (obj));
}

static void
sourceview_plugin_dispose (GObject *obj)
{
	/* Disposition codes */
	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (obj));
}

static void
sourceview_plugin_instance_init (GObject *obj)
{
	SourceviewPlugin *plugin = (SourceviewPlugin*)obj;
}

static void
sourceview_plugin_class_init (GObjectClass *klass) 
{
	AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	plugin_class->activate = sourceview_plugin_activate;
	plugin_class->deactivate = sourceview_plugin_deactivate;
	klass->finalize = sourceview_plugin_finalize;
	klass->dispose = sourceview_plugin_dispose;
}

static IAnjutaEditor*
ieditor_factory_new_editor(IAnjutaEditorFactory* factory, 
								const gchar* uri,
								const gchar* filename, 
								GError** error)
{
	AnjutaPlugin* plugin = ANJUTA_PLUGIN(factory);
	AnjutaShell* shell;
	AnjutaPreferences* prefs;
	g_object_get(G_OBJECT(plugin), "shell", &shell, NULL);
	prefs = anjuta_shell_get_preferences(shell, NULL);
	IAnjutaEditor* editor = IANJUTA_EDITOR(sourceview_new(uri, filename, prefs));
	return editor;
}

static void
ieditor_factory_iface_init (IAnjutaEditorFactoryIface *iface)
{
	iface->new_editor = ieditor_factory_new_editor;
}

ANJUTA_PLUGIN_BEGIN (SourceviewPlugin, sourceview_plugin);
ANJUTA_TYPE_ADD_INTERFACE(ieditor_factory, IANJUTA_TYPE_EDITOR_FACTORY);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (SourceviewPlugin, sourceview_plugin);