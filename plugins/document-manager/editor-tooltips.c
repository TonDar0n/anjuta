/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkstyle.h>

#include "editor-tooltips.h"
#include <libanjuta/anjuta-debug.h>


#define DEFAULT_DELAY 500	/* Default delay in ms */
#define STICKY_DELAY 0		/* Delay before popping up next tip
				 * if we are sticky
				 */
#define STICKY_REVERT_DELAY 1000	/* Delay before sticky tooltips revert
					 * to normal
					 */

static void editor_tooltips_class_init (EditorTooltipsClass * klass);
static void editor_tooltips_init (EditorTooltips * tooltips);
static void editor_tooltips_destroy (GtkObject * object);

static void editor_tooltips_event_handler (GtkWidget * widget,
					  GdkEvent * event);
static void editor_tooltips_widget_unmap (GtkWidget * widget,
					 gpointer data);
static void editor_tooltips_widget_remove (GtkWidget * widget,
					  gpointer data);
static void editor_tooltips_set_active_widget (EditorTooltips * tooltips,
					      GtkWidget * widget);
static gint editor_tooltips_timeout (gpointer data);

static gint editor_tooltips_paint_window (EditorTooltips * tooltips);
static void editor_tooltips_draw_tips (EditorTooltips * tooltips);
static void editor_tooltips_unset_tip_window (EditorTooltips * tooltips);

static gboolean get_keyboard_mode (GtkWidget * widget);

static GtkObjectClass *parent_class;
static const gchar *tooltips_data_key = "_EditorTooltipsData";

GType
editor_tooltips_get_type (void)
{
	static GType tooltips_type = 0;

	if (!tooltips_type) {
		static const GTypeInfo tooltips_info = {
			sizeof (EditorTooltipsClass),
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) editor_tooltips_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (EditorTooltips),
			0,	/* n_preallocs */
			(GInstanceInitFunc) editor_tooltips_init,
		};

		tooltips_type =
		    g_type_register_static (GTK_TYPE_OBJECT,
					    "EditorTooltips",
					    &tooltips_info, 0);
	}

	return tooltips_type;
}

static void
editor_tooltips_class_init (EditorTooltipsClass * class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = g_type_class_peek_parent (class);

	object_class->destroy = editor_tooltips_destroy;
}

static void
editor_tooltips_init (EditorTooltips * tooltips)
{
	tooltips->tip_window = NULL;
	tooltips->active_tips_data = NULL;
	tooltips->tips_data_list = NULL;

	tooltips->delay = DEFAULT_DELAY;
	tooltips->enabled = TRUE;
	tooltips->timer_tag = 0;
	tooltips->use_sticky_delay = FALSE;
	tooltips->last_popdown.tv_sec = -1;
	tooltips->last_popdown.tv_usec = -1;
}

EditorTooltips *
editor_tooltips_new (void)
{
	return g_object_new (EDITOR_TYPE_TOOLTIPS, NULL);
}

static void
editor_tooltips_destroy_data (EditorTooltipsData * tooltipsdata)
{
	g_free (tooltipsdata->tip_text);
	g_free (tooltipsdata->tip_private);

	g_signal_handlers_disconnect_by_func (tooltipsdata->widget,
					      editor_tooltips_event_handler,
					      tooltipsdata);
	g_signal_handlers_disconnect_by_func (tooltipsdata->widget,
					      editor_tooltips_widget_unmap,
					      tooltipsdata);
	g_signal_handlers_disconnect_by_func (tooltipsdata->widget,
					      editor_tooltips_widget_remove,
					      tooltipsdata);

	g_object_set_data (G_OBJECT (tooltipsdata->widget),
			   tooltips_data_key, NULL);
	g_object_unref (tooltipsdata->widget);
	g_free (tooltipsdata);
}

static void
tip_window_display_closed (GdkDisplay * display,
			   gboolean was_error, EditorTooltips * tooltips)
{
	editor_tooltips_unset_tip_window (tooltips);
}

static void
disconnect_tip_window_display_closed (EditorTooltips * tooltips)
{
	g_signal_handlers_disconnect_by_func (gtk_widget_get_display
					      (tooltips->tip_window),
					      (gpointer)
					      tip_window_display_closed,
					      tooltips);
}

static void
editor_tooltips_unset_tip_window (EditorTooltips * tooltips)
{
	if (tooltips->tip_window) {
		disconnect_tip_window_display_closed (tooltips);

		gtk_widget_destroy (tooltips->tip_window);
		tooltips->tip_window = NULL;
	}
}

static void
editor_tooltips_destroy (GtkObject * object)
{
	EditorTooltips *tooltips = EDITOR_TOOLTIPS (object);
	GList *current;
	EditorTooltipsData *tooltipsdata;

	g_return_if_fail (tooltips != NULL);

	DEBUG_PRINT ("destroying tooltips...");
	if (tooltips->timer_tag) {
		g_source_remove (tooltips->timer_tag);
		tooltips->timer_tag = 0;
	}

	if (tooltips->tips_data_list != NULL) {
		current = g_list_first (tooltips->tips_data_list);
		while (current != NULL) {
			tooltipsdata = (EditorTooltipsData *) current->data;
			current = current->next;
			editor_tooltips_widget_remove (tooltipsdata->widget,
						      tooltipsdata);
		}
	}

	editor_tooltips_unset_tip_window (tooltips);
}

static void
editor_tooltips_update_screen (EditorTooltips * tooltips,
			      gboolean new_window)
{
	gboolean screen_changed = FALSE;

	if (tooltips->active_tips_data &&
	    tooltips->active_tips_data->widget) {
		GdkScreen *screen =
		    gtk_widget_get_screen (tooltips->active_tips_data->
					   widget);

		screen_changed =
		    (screen !=
		     gtk_widget_get_screen (tooltips->tip_window));

		if (screen_changed) {
			if (!new_window)
				disconnect_tip_window_display_closed
				    (tooltips);

			gtk_window_set_screen (GTK_WINDOW
					       (tooltips->tip_window),
					       screen);
		}
	}

	if (screen_changed || new_window)
		g_signal_connect (gtk_widget_get_display
				  (tooltips->tip_window), "closed",
				  G_CALLBACK (tip_window_display_closed),
				  tooltips);

}

void
editor_tooltips_force_window (EditorTooltips * tooltips)
{
	g_return_if_fail (EDITOR_IS_TOOLTIPS (tooltips));

	if (!tooltips->tip_window) {
		tooltips->tip_window = gtk_window_new (GTK_WINDOW_POPUP);
		editor_tooltips_update_screen (tooltips, TRUE);
		gtk_widget_set_app_paintable (tooltips->tip_window, TRUE);
		gtk_window_set_resizable (GTK_WINDOW
					  (tooltips->tip_window), FALSE);
		gtk_widget_set_name (tooltips->tip_window, "gtk-tooltips");
		gtk_container_set_border_width (GTK_CONTAINER
						(tooltips->tip_window), 4);

		g_signal_connect_swapped (tooltips->tip_window,
					  "expose_event",
					  G_CALLBACK
					  (editor_tooltips_paint_window),
					  tooltips);

		tooltips->tip_label = gtk_label_new (NULL);
		gtk_label_set_line_wrap (GTK_LABEL (tooltips->tip_label),
					 TRUE);
		gtk_misc_set_alignment (GTK_MISC (tooltips->tip_label),
					0.5, 0.5);
		gtk_widget_show (tooltips->tip_label);

		gtk_container_add (GTK_CONTAINER (tooltips->tip_window),
				   tooltips->tip_label);

		g_signal_connect (tooltips->tip_window,
				  "destroy",
				  G_CALLBACK (gtk_widget_destroyed),
				  &tooltips->tip_window);
	}
}

void
editor_tooltips_enable (EditorTooltips * tooltips)
{
	g_return_if_fail (tooltips != NULL);

	tooltips->enabled = TRUE;
}

void
editor_tooltips_disable (EditorTooltips * tooltips)
{
	g_return_if_fail (tooltips != NULL);

	editor_tooltips_set_active_widget (tooltips, NULL);

	tooltips->enabled = FALSE;
}

EditorTooltipsData *
editor_tooltips_data_get (GtkWidget * widget)
{
	g_return_val_if_fail (widget != NULL, NULL);

	return g_object_get_data (G_OBJECT (widget), tooltips_data_key);
}

void
editor_tooltips_set_tip (EditorTooltips * tooltips,
			GtkWidget * widget,
			const gchar * tip_text, const gchar * tip_private)
{
	EditorTooltipsData *tooltipsdata;

	g_return_if_fail (EDITOR_IS_TOOLTIPS (tooltips));
	g_return_if_fail (widget != NULL);

	tooltipsdata = editor_tooltips_data_get (widget);

	if (!tip_text) {
		if (tooltipsdata)
			editor_tooltips_widget_remove (tooltipsdata->widget,
						      tooltipsdata);
		return;
	}

	if (tooltips->active_tips_data
	    && tooltips->active_tips_data->widget == widget
	    && GTK_WIDGET_DRAWABLE (tooltips->active_tips_data->widget)) {

		g_free (tooltipsdata->tip_text);
		g_free (tooltipsdata->tip_private);

		tooltipsdata->tip_text = g_strdup (tip_text);
		tooltipsdata->tip_private = g_strdup (tip_private);

		editor_tooltips_draw_tips (tooltips);
	} else {
		DEBUG_PRINT ("tooltip data aws not present. Setting now" );
		g_object_ref (widget);

		if (tooltipsdata)
			editor_tooltips_widget_remove (tooltipsdata->widget,
						      tooltipsdata);

		tooltipsdata = g_new0 (EditorTooltipsData, 1);

		tooltipsdata->tooltips = tooltips;
		tooltipsdata->widget = widget;

		tooltipsdata->tip_text = g_strdup (tip_text);
		tooltipsdata->tip_private = g_strdup (tip_private);

		tooltips->tips_data_list =
		    g_list_append (tooltips->tips_data_list, tooltipsdata);
		g_signal_connect_after (widget, "event-after",
					G_CALLBACK
					(editor_tooltips_event_handler),
					tooltipsdata);

		g_object_set_data (G_OBJECT (widget), tooltips_data_key,
				   tooltipsdata);

		g_signal_connect (widget, "unmap",
				  G_CALLBACK (editor_tooltips_widget_unmap),
				  tooltipsdata);

		g_signal_connect (widget, "unrealize",
				  G_CALLBACK (editor_tooltips_widget_unmap),
				  tooltipsdata);

		g_signal_connect (widget, "destroy",
				  G_CALLBACK
				  (editor_tooltips_widget_remove),
				  tooltipsdata);
	}
}

static gint
editor_tooltips_paint_window (EditorTooltips * tooltips)
{
	gtk_paint_flat_box (tooltips->tip_window->style,
			    tooltips->tip_window->window, GTK_STATE_NORMAL,
			    GTK_SHADOW_OUT, NULL,
			    GTK_WIDGET (tooltips->tip_window), "tooltip",
			    0, 0, -1, -1);

	return FALSE;
}

static void
editor_tooltips_draw_tips (EditorTooltips * tooltips)
{
	GtkRequisition requisition;
	GtkWidget *widget;
	GtkStyle *style;
	gint x, y, w, h, scr_w, scr_h;
	EditorTooltipsData *data;
	gboolean keyboard_mode;
	GdkScreen *screen;

	if (!tooltips->tip_window)
		editor_tooltips_force_window (tooltips);
	else if (GTK_WIDGET_VISIBLE (tooltips->tip_window))
		g_get_current_time (&tooltips->last_popdown);

	gtk_widget_ensure_style (tooltips->tip_window);
	style = tooltips->tip_window->style;

	widget = tooltips->active_tips_data->widget;

	keyboard_mode = get_keyboard_mode (widget);

	editor_tooltips_update_screen (tooltips, FALSE);

	screen = gtk_widget_get_screen (widget);
	scr_w = gdk_screen_get_width (screen);
	scr_h = gdk_screen_get_height (screen);

	data = tooltips->active_tips_data;

	gtk_label_set_markup (GTK_LABEL (tooltips->tip_label),
			      data->tip_text);

	gtk_widget_size_request (tooltips->tip_window, &requisition);
	w = requisition.width;
	h = requisition.height;

	gdk_window_get_origin (widget->window, &x, &y);
	if (GTK_WIDGET_NO_WINDOW (widget)) {
		x += widget->allocation.x;
		y += widget->allocation.y;
	}

	x += widget->allocation.width / 2;

	if (!keyboard_mode)
		gdk_window_get_pointer (gdk_screen_get_root_window
					(screen), &x, NULL, NULL);

	x -= (w / 2 + 4);

	if ((x + w) > scr_w)
		x -= (x + w) - scr_w;
	else if (x < 0)
		x = 0;

	if ((y + h + widget->allocation.height + 4) > scr_h)
		y = y - h - 4;
	else
		y = y + widget->allocation.height + 4;

	gtk_window_move (GTK_WINDOW (tooltips->tip_window), x, y);
	gtk_widget_show (tooltips->tip_window);
}

static gint
editor_tooltips_timeout (gpointer data)
{
	EditorTooltips *tooltips = (EditorTooltips *) data;

	GDK_THREADS_ENTER ();

	if (tooltips->active_tips_data != NULL &&
	    GTK_WIDGET_DRAWABLE (tooltips->active_tips_data->widget))
		editor_tooltips_draw_tips (tooltips);

	GDK_THREADS_LEAVE ();

	return FALSE;
}

static void
editor_tooltips_set_active_widget (EditorTooltips * tooltips,
				  GtkWidget * widget)
{
	if (tooltips->tip_window) {
		if (GTK_WIDGET_VISIBLE (tooltips->tip_window))
			g_get_current_time (&tooltips->last_popdown);
		gtk_widget_hide (tooltips->tip_window);
	}
	if (tooltips->timer_tag) {
		g_source_remove (tooltips->timer_tag);
		tooltips->timer_tag = 0;
	}

	tooltips->active_tips_data = NULL;

	if (widget) {
		GList *list;

		for (list = tooltips->tips_data_list; list;
		     list = list->next) {
			EditorTooltipsData *tooltipsdata;

			tooltipsdata = list->data;

			if (tooltipsdata->widget == widget &&
			    GTK_WIDGET_DRAWABLE (widget)) {
				tooltips->active_tips_data = tooltipsdata;
				break;
			}
		}
	} else {
		tooltips->use_sticky_delay = FALSE;
	}
}

static void
editor_tooltips_show_tip (GtkWidget * widget)
{
	EditorTooltipsData *tooltipsdata;

	tooltipsdata = editor_tooltips_data_get (widget);

	if (tooltipsdata &&
	    (!tooltipsdata->tooltips->active_tips_data ||
	     tooltipsdata->tooltips->active_tips_data->widget != widget)) {
		editor_tooltips_set_active_widget (tooltipsdata->tooltips,
						  widget);
		editor_tooltips_draw_tips (tooltipsdata->tooltips);
	}
}

static void
editor_tooltips_hide_tip (GtkWidget * widget)
{
	EditorTooltipsData *tooltipsdata;

	tooltipsdata = editor_tooltips_data_get (widget);

	if (tooltipsdata &&
	    (tooltipsdata->tooltips->active_tips_data &&
	     tooltipsdata->tooltips->active_tips_data->widget == widget))
		editor_tooltips_set_active_widget (tooltipsdata->tooltips,
						  NULL);
}

static gboolean
editor_tooltips_recently_shown (EditorTooltips * tooltips)
{
	GTimeVal now;
	glong msec;

	g_get_current_time (&now);
	msec = (now.tv_sec - tooltips->last_popdown.tv_sec) * 1000 +
	    (now.tv_usec - tooltips->last_popdown.tv_usec) / 1000;
	return (msec < STICKY_REVERT_DELAY);
}

static gboolean
get_keyboard_mode (GtkWidget * widget)
{
	GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
	if (GTK_IS_WINDOW (toplevel))
		return
		    GPOINTER_TO_UINT (g_object_get_data
				      (G_OBJECT (toplevel),
				       "gtk-tooltips-keyboard-mode"));
	else 
		return FALSE;
}

static void
start_keyboard_mode (GtkWidget * widget)
{
	GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
	if (GTK_IS_WINDOW (toplevel)) {
		GtkWidget *focus = GTK_WINDOW (toplevel)->focus_widget;

		g_object_set_data (G_OBJECT (toplevel),
				   "gtk-tooltips-keyboard-mode",
				   GUINT_TO_POINTER (TRUE));

		if (focus)
			editor_tooltips_show_tip (focus);
	}
}

static void
stop_keyboard_mode (GtkWidget * widget)
{
	GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
	if (GTK_IS_WINDOW (toplevel)) {
		GtkWidget *focus = GTK_WINDOW (toplevel)->focus_widget;
		if (focus)
			editor_tooltips_hide_tip (focus);

		g_object_set_data (G_OBJECT (toplevel),
				   "gtk-tooltips-keyboard-mode",
				   GUINT_TO_POINTER (FALSE));
	}
}

static void
editor_tooltips_start_delay (EditorTooltips * tooltips, GtkWidget * widget)
{
	EditorTooltipsData *old_tips_data;

	old_tips_data = tooltips->active_tips_data;
	if (tooltips->enabled &&
	    (!old_tips_data || old_tips_data->widget != widget)) {
		guint delay;

		editor_tooltips_set_active_widget (tooltips, widget);

		if (tooltips->use_sticky_delay &&
		    editor_tooltips_recently_shown (tooltips))
			delay = STICKY_DELAY;
		else
			delay = tooltips->delay;
		tooltips->timer_tag = g_timeout_add (delay,
						     editor_tooltips_timeout,
						     (gpointer)
						     tooltips);
	}
}

static void
editor_tooltips_event_handler (GtkWidget * widget, GdkEvent * event)
{
	EditorTooltips *tooltips;
	EditorTooltipsData *old_tips_data;
	GtkWidget *event_widget;
	gboolean keyboard_mode = get_keyboard_mode (widget);

	if ((event->type == GDK_LEAVE_NOTIFY
	     || event->type == GDK_ENTER_NOTIFY)
	    && event->crossing.detail == GDK_NOTIFY_INFERIOR)
		return;

	old_tips_data = editor_tooltips_data_get (widget);
	tooltips = old_tips_data->tooltips;

	if (keyboard_mode) {
		switch (event->type) {
		case GDK_FOCUS_CHANGE:
			if (event->focus_change.in)
				editor_tooltips_show_tip (widget);
			else
				editor_tooltips_hide_tip (widget);
			break;
		default:
			break;
		}
	} else {
		if (event->type != GDK_KEY_PRESS
		    && event->type != GDK_KEY_RELEASE) {
			event_widget = gtk_get_event_widget (event);
			if (event_widget != widget)
				return;
		}

		switch (event->type) {
		case GDK_EXPOSE:
			/* do nothing */
			break;
		case GDK_ENTER_NOTIFY:
			if (!
			    (GTK_IS_MENU_ITEM (widget)
			     && GTK_MENU_ITEM (widget)->submenu))
				editor_tooltips_start_delay (tooltips,
							    widget);
			break;

		case GDK_LEAVE_NOTIFY:
			{
				gboolean use_sticky_delay;

				use_sticky_delay = tooltips->tip_window &&
				    GTK_WIDGET_VISIBLE (tooltips->
							tip_window);
				editor_tooltips_set_active_widget (tooltips,
								  NULL);
				tooltips->use_sticky_delay =
				    use_sticky_delay;
			}
			break;
#if 0
		case GDK_MOTION_NOTIFY:

			/* Handle menu items specially ... pend popup for each motion
			 * on other widgets, we ignore motion.
			 */
			if (GTK_IS_MENU_ITEM (widget)
			    && !GTK_MENU_ITEM (widget)->submenu) {
				/* Completely evil hack to make sure we get the LEAVE_NOTIFY
				 */
				GTK_PRIVATE_SET_FLAG (widget,
						      GTK_LEAVE_PENDING);
				editor_tooltips_set_active_widget (tooltips,
								  NULL);
				editor_tooltips_start_delay (tooltips,
							    widget);
				break;
			}
			break;	/* ignore */
#endif
		case GDK_BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
		case GDK_KEY_PRESS:
		case GDK_KEY_RELEASE:
		case GDK_PROXIMITY_IN:
		case GDK_SCROLL:
			editor_tooltips_set_active_widget (tooltips, NULL);
			break;
		default:
			break;
		}
	}
}

static void
editor_tooltips_widget_unmap (GtkWidget * widget, gpointer data)
{
	EditorTooltipsData *tooltipsdata = (EditorTooltipsData *) data;
	EditorTooltips *tooltips = tooltipsdata->tooltips;

	if (tooltips->active_tips_data &&
	    (tooltips->active_tips_data->widget == widget))
		editor_tooltips_set_active_widget (tooltips, NULL);
}

static void
editor_tooltips_widget_remove (GtkWidget * widget, gpointer data)
{
	EditorTooltipsData *tooltipsdata = (EditorTooltipsData *) data;
	EditorTooltips *tooltips = tooltipsdata->tooltips;

	editor_tooltips_widget_unmap (widget, data);
	tooltips->tips_data_list = g_list_remove (tooltips->tips_data_list,
						  tooltipsdata);
	editor_tooltips_destroy_data (tooltipsdata);
}

void
_editor_tooltips_toggle_keyboard_mode (GtkWidget * widget)
{
	if (get_keyboard_mode (widget))
		stop_keyboard_mode (widget);
	else
		start_keyboard_mode (widget);
}