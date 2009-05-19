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
#define DEVIANCE 20
#define MINLENGTH 50

/* #define __MOUSE_GESTURES_DEBUG__ */

MouseGesture *gesture;

void mouse_gesture_clear (MouseGesture *g)
{
    g->start.x = 0;
    g->start.y = 0;
    g->middle.x = 0;
    g->middle.y = 0;
    g->end.x = 0;
    g->end.y = 0;
    g->last = MOUSE_BUTTON_UNSET;
}

MouseGesture* mouse_gesture_new (void)
{
    MouseGesture *g = g_new (MouseGesture, 1);
    mouse_gesture_clear (g);

    return g;
}

static gboolean mouse_gestures_handle_events (GtkWidget     *widget,
                                              GdkEvent      *event,
                                              MidoriBrowser *browser)
{
    /* A button was pressed */
    if (event->type == GDK_BUTTON_PRESS)
    {
        /* If the gesture was previously cleaned, start a new gesture and coordinates */
        if (gesture->last == MOUSE_BUTTON_UNSET)
            {
                gesture->start.x = event->button.x;
                gesture->start.y = event->button.y;
                gesture->last = event->button.button;
            }

        return TRUE;
    }
    else if (event->type == GDK_MOTION_NOTIFY)
    {
        if (gesture->last != MOUSE_BUTTON_UNSET)
        {
            guint x, y;

            x = event->motion.x;
            y = event->motion.y;

            if ((gesture->start.x - x < DEVIANCE && gesture->start.x - x > -DEVIANCE) ||
                (gesture->start.y - y < DEVIANCE && gesture->start.y - y > -DEVIANCE))
            {
                gesture->middle.x = x;
                gesture->middle.y = y;
            }
            else if ((gesture->middle.x - x < DEVIANCE && gesture->middle.x - x > -DEVIANCE) ||
                     (gesture->middle.y - y < DEVIANCE && gesture->middle.y - y > -DEVIANCE))
            {
                gesture->end.x = x;
		gesture->end.y = y;
            }
        }

        return TRUE;
    }
    else if (event->type == GDK_BUTTON_RELEASE)
    {
        /* All mouse gestures will use the middle mouse button */
        if (gesture->last == MOUSE_BUTTON_MIDDLE)
        {
            /* The initial horizontal move is between the bounds */
            if ((gesture->middle.x - gesture->start.x < DEVIANCE) &&
                (gesture->middle.x - gesture->start.x > -DEVIANCE))
            {
                 /* We initially moved down more than MINLENGTH pixels */
		if (gesture->middle.y > gesture->start.y + MINLENGTH)
		{
		    /* Then we the final vertical move is between the bounds and
                       we moved right more than MINLENGTH pixels */
                    if ((gesture->middle.y - gesture->end.y < DEVIANCE) &&
                        (gesture->middle.y - gesture->end.y > -DEVIANCE) &&
                        (gesture->end.x > gesture->middle.x + MINLENGTH))
                    {
                         /* We moved down then right: close the tab */
                         midori_browser_activate_action (browser, "TabClose");
                    }
                    /* Then we the final vertical move is between the bounds and
                    we moved left more than MINLENGTH pixels */
                    else if ((gesture->middle.y - gesture->end.y < DEVIANCE) &&
                             (gesture->middle.y - gesture->end.y > -DEVIANCE) &&
                             (gesture->end.x + MINLENGTH < gesture->middle.x))
                    {
                         /* We moved down then left: reload */
                         midori_browser_activate_action (browser, "Reload");
                    }
                    /* The end node was never updated, we only did a vertical move */
                    else if(gesture->end.y == 0 && gesture->end.x == 0)
                    {
                        /* We moved down then: create a new tab */
                        midori_browser_activate_action (browser, "TabNew");
                    }
                }
                /* We initially moved up more than MINLENGTH pixels */
		else if (gesture->middle.y + MINLENGTH < gesture->start.y)
		{
                    /* The end node was never updated, we only did a vertical move */
                    if (gesture->end.y == 0 && gesture->end.x == 0)
                    {
                        /* We moved up: stop */
                        midori_browser_activate_action (browser, "Stop");
                    }
                }
            }
            /* The initial horizontal move is between the bounds */
            else if ((gesture->middle.y - gesture->start.y < DEVIANCE) &&
                     (gesture->middle.y - gesture->start.y > -DEVIANCE))
            {
                /* We initially moved right more than MINLENGTH pixels */
                if (gesture->middle.x > gesture->start.x + MINLENGTH)
                {
                    /* The end node was never updated, we only did an horizontal move */
                    if (gesture->end.x == 0 && gesture->end.y == 0)
                    {
                        /* We moved right: forward */
                        midori_browser_activate_action (browser, "Forward");
                    }
                }
                /* We initially moved left more than MINLENGTH pixels */
                else if (gesture->middle.x + MINLENGTH < gesture->start.x)
		{
                    /* The end node was never updated, we only did an horizontal move */
                    if (gesture->end.x == 0 && gesture->end.y == 0)
                    {
                        /* We moved left: back */
                        midori_browser_activate_action (browser, "Back");
                    }
                }
            }
        }

	mouse_gesture_clear (gesture);

        return TRUE;
    }
    else
        return FALSE;
}

static void mouse_gestures_tab_cb (MidoriBrowser* browser, GtkWidget *view)
{
    g_signal_connect (view, "event", G_CALLBACK (mouse_gestures_handle_events), browser);
}

static void mouse_gestures_browser_cb (MidoriApp *app, MidoriBrowser *browser)
{
    g_signal_connect (browser, "add-tab", G_CALLBACK (mouse_gestures_tab_cb), NULL);
}

static void mouse_gestures_deactivate (MidoriExtension *extension, MidoriApp *app)
{
    gulong signal_id;
    KatzeArray *browsers;
    guint i;
    gint j;
    GtkWidget *notebook;

    signal_id =
        g_signal_handler_find (app, G_SIGNAL_MATCH_FUNC,
                               0, 0, NULL,
                               mouse_gestures_browser_cb, NULL);

    if(signal_id != 0)
        g_signal_handler_disconnect (app, signal_id);

    browsers = katze_object_get_object (app, "browsers");

    for (i = 0; i < katze_array_get_length (browsers); i++)
    {
        MidoriBrowser *browser;

        browser = katze_array_get_nth_item (browsers, i);

        signal_id =
            g_signal_handler_find (browser, G_SIGNAL_MATCH_FUNC,
                                   0, 0, NULL,
                                   mouse_gestures_tab_cb, NULL);

        if (signal_id != 0)
            g_signal_handler_disconnect (browser, signal_id);

        notebook = katze_object_get_object (browser, "notebook");

        for (j = 0; j < gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)); j++)
        {
            GtkWidget *page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), j);

            signal_id =
                g_signal_handler_find (page, G_SIGNAL_MATCH_FUNC,
                                       0, 0, NULL,
                                       mouse_gestures_handle_events, NULL);

            if (signal_id != 0)
                g_signal_handler_disconnect (page, signal_id);
        }
    }

    g_signal_handlers_disconnect_by_func (extension, mouse_gestures_deactivate, app);
    g_free (gesture);
}

static void mouse_gestures_activate (MidoriExtension *extension, MidoriApp *app)
{
    gesture = mouse_gesture_new ();

    g_signal_connect (app, "add-browser",
                      G_CALLBACK (mouse_gestures_browser_cb), NULL);
    g_signal_connect (extension, "deactivate",
                      G_CALLBACK (mouse_gestures_deactivate), app);
}

MidoriExtension* extension_init (void)
{
    MidoriExtension* extension;

    extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Mouse Gestures"),
        "description", _("Control Midori by moving the mouse"),
        "version", MOUSE_GESTURES_VERSION,
        "authors", "Matthias Kruk <mkruk@matthiaskruk.de>", NULL);

    g_signal_connect (extension, "activate",
                      G_CALLBACK (mouse_gestures_activate), NULL);

    return extension;
}
