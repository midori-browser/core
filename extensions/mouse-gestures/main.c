/*
 Copyright (C) 2009 Matthias Kruk <mkruk@matthiaskruk.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>
#include "mouse-gestures.h"

#define MOUSE_GESTURES_VERSION "0.1"
#define DEVIANCE 20 // the deviance to determine if a line is straight or not
#define MINLENGTH 50 // the minimal length of a line to be treated as a gesture

// #define __MOUSE_GESTURES_DEBUG__ // uncomment for debugging purposes

MouseGesture *gesture;

void mouse_gesture_clear(MouseGesture *g)
{
	g->start.x = 0;
	g->start.y = 0;
	g->middle.x = 0;
	g->middle.y = 0;
	g->end.x = 0;
	g->end.y = 0;
	g->last = MOUSE_BUTTON_UNSET;
	return;
}

MouseGesture* mouse_gesture_new(void)
{
	MouseGesture *g = g_new(MouseGesture, 1);
	mouse_gesture_clear(g);
	return(g);
}

static gboolean mouse_gestures_handle_events(GtkWidget *widget, GdkEvent *event, MidoriBrowser *browser)
{
	if(event->type == GDK_BUTTON_PRESS) // a button was pressed
	{
		if(gesture->last == MOUSE_BUTTON_UNSET) // if the gesture was previously cleaned -> new gesture -> new start coordinates
		{
			gesture->start.x = event->button.x;
			gesture->start.y = event->button.y;
			gesture->last = event->button.button;
		}
		return(TRUE);
	}
	else if(event->type == GDK_MOTION_NOTIFY) // the mouse was moved
	{
		if(gesture->last != MOUSE_BUTTON_UNSET)
		{
			guint x, y;
			x = event->motion.x;
			y = event->motion.y;
			if((gesture->start.x - x < DEVIANCE && gesture->start.x - x > -DEVIANCE) ||
			   (gesture->start.y - y < DEVIANCE && gesture->start.y - y > -DEVIANCE))
			{
				gesture->middle.x = x;
				gesture->middle.y = y;
			}
			else if((gesture->middle.x - x < DEVIANCE && gesture->middle.x - x > -DEVIANCE) ||
			   (gesture->middle.y - y < DEVIANCE && gesture->middle.y - y > -DEVIANCE))
			{
				gesture->end.x = x;
				gesture->end.y = y;
			}
		}
		return(TRUE);
	}
	else if(event->type == GDK_BUTTON_RELEASE)
	{
		if(gesture->last == MOUSE_BUTTON_MIDDLE) // all mouse gestures will use the middle mouse button
		{
			// middle mouse button has been released
			if(gesture->middle.x - gesture->start.x < DEVIANCE && gesture->middle.x - gesture->start.x > -DEVIANCE)
			{
				// StartNode to MiddleNode is a straight vertical line
				if(gesture->middle.y > gesture->start.y + MINLENGTH)
				{
					// StartNode to MiddleNode is drawn downwards
					if(gesture->middle.y - gesture->end.y < DEVIANCE && gesture->middle.y - gesture->end.y > -DEVIANCE && gesture->end.x > gesture->middle.x + MINLENGTH)
					{
						// MiddleNode to EndNode is a straight horizontal line (to the right) -> close tab
						midori_browser_activate_action(browser, "TabClose");
						#ifdef __MOUSE_GESTURES_DEBUG__
						g_print("TabClose gesture\n");
						#endif
					}
					else if(gesture->middle.y - gesture->end.y < DEVIANCE && gesture->middle.y - gesture->end.y > -DEVIANCE && gesture->end.x < gesture->middle.x - MINLENGTH)
					{
						// MiddleNode to EndNode is a straight horizontal line (to the left) -> reload
						midori_browser_activate_action(browser, "Reload");
						#ifdef __MOUSE_GESTURES_DEBUG__
						g_print("Reload gesture\n");
						#endif
					}
					else if(gesture->end.y == 0 && gesture->end.x == 0)
					{
						// no EndNode, just a vertical line -> new tab
						midori_browser_activate_action(browser, "TabNew");
						#ifdef __MOUSE_GESTURES_DEBUG__
						g_print("TabNew gesture\n");
						#endif
					}
				}
				if(gesture->middle.y < gesture->start.y - MINLENGTH)
				{
					// StartNode to MiddleNode is drawn upwards
					if(gesture->end.y == 0 && gesture->end.x == 0)
					{
						// no EndNode, just a vertical line -> stop
						midori_browser_activate_action(browser, "Stop");
						#ifdef __MOUSE_GESTURES_DEBUG__
						g_print("Stop gesture\n");
						#endif
					}
				}
			}
			else if(gesture->middle.y - gesture->start.y < DEVIANCE && gesture->middle.y - gesture->start.y > -DEVIANCE)
			{
				// Start Node to MiddleNode is a straight horizontal line
				if(gesture->middle.x > gesture->start.x + MINLENGTH)
				{
					// the line was drawn from left to right
					if(gesture->end.x == 0 && gesture->end.y == 0)
					{
						// we only have one horizontal line from left to right -> forward gesture
						midori_browser_activate_action(browser, "Forward");
						#ifdef __MOUSE_GESTURES_DEBUG__
						g_print("Forward gesture\n");
						#endif
					}
				}
				else if(gesture->middle.x < gesture->start.x - MINLENGTH)
				{
					// the line was drawn from right to left
					if(gesture->end.x == 0 && gesture->end.y == 0)
					{
						// we only have one horizontal line from right to left -> backwards gesture
						midori_browser_activate_action(browser, "Back");
						#ifdef __MOUSE_GESTURES_DEBUG__
						g_print("Back gesture\n");
						#endif
					}
				}
			}
		}
		mouse_gesture_clear(gesture); // gesture finished, clear it
		return(TRUE);
	}
	else
		return(FALSE); // this event is supposed to be handled again (by someone else's code) since it was irrelevant to us
}

static void mouse_gestures_tab_cb(MidoriBrowser* browser, GtkWidget *view) // this is performed when a new tab is created
{
	g_signal_connect(view, "event", G_CALLBACK(mouse_gestures_handle_events), browser); // perform the above callback when an event from the view is received
	return;
}

static void mouse_gestures_browser_cb(MidoriApp *app, MidoriBrowser *browser) // this is performed when ever a new window is created
{
	g_signal_connect(browser, "add-tab", G_CALLBACK(mouse_gestures_tab_cb), NULL); // connect the above callback to the "add-tab" signal of browser
	return;
}

/* this is performed when the extension is deactivated.
   disconnect all signal handlers, so that mouse gestures are no longer handled */
static void mouse_gestures_deactivate(MidoriExtension *extension, MidoriApp *app)
{
	gulong signal_id = g_signal_handler_find(app, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, mouse_gestures_browser_cb, NULL); // get the signal handler id
	if(signal_id != 0) // if that id is valid
		g_signal_handler_disconnect(app, signal_id); // disconnect the signal
	KatzeArray *browsers = katze_object_get_object(app, "browsers"); // get the array that contains all browsers
	guint i; // our counter variable :)
	for(i = 0; i < katze_array_get_length(browsers); i++) // from the first to the last browser
	{
		MidoriBrowser *browser = katze_array_get_nth_item(browsers, i); // get a pointer on the current browser
		signal_id = g_signal_handler_find(browser, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, mouse_gestures_tab_cb, NULL); // search forthe signal handler id
		if(signal_id != 0) // and if its not invalid..
			g_signal_handler_disconnect(browser, signal_id); // disconnect it

		GtkWidget *notebook = katze_object_get_object(browser, "notebook"); // get a pointer on the notebook
		gint j; // another counter
		for(j = 0; j < gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)); j++) // from the first to the last tab
		{
			GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), j); // get a pointer on the tab's view
			signal_id = g_signal_handler_find(page, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, mouse_gestures_handle_events, NULL); // find the signal id of the event handler
			if(signal_id != 0) // if the id is valid
				g_signal_handler_disconnect(page, signal_id); // disconnect the handler
		}
	}
	g_signal_handlers_disconnect_by_func(extension, mouse_gestures_deactivate, app);
	g_free(gesture); // free the structure that contains the gesture information
	return;
}

static void mouse_gestures_activate(MidoriExtension *extension, MidoriApp *app) // this is performed on extension-activation
{
	gesture = mouse_gesture_new();
    g_signal_connect(app, "add-browser", G_CALLBACK(mouse_gestures_browser_cb), NULL); // connect the above callback to the "add-browser" signal of app
	g_signal_connect(extension, "deactivate", G_CALLBACK(mouse_gestures_deactivate), app); // connect the deactivate callback to the extension
	return;
}

MidoriExtension* extension_init(void)
{
    MidoriExtension* extension = NULL;
	extension = g_object_new(MIDORI_TYPE_EXTENSION,
							 "name", _("Mouse Gestures"),
							 "description", _("Control Midori by moving the mouse"),
							 "version", MOUSE_GESTURES_VERSION,
							 "authors", "Matthias Kruk <mkruk@matthiaskruk.de>", NULL);
    g_signal_connect(extension, "activate", G_CALLBACK(mouse_gestures_activate), NULL); // connect the callback that's executed on activation
    return(extension);
}
