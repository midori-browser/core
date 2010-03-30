/*
 Copyright (C) 2009 Matthias Kruk <mkruk@matthiaskruk.de>
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

typedef struct _MouseGesture MouseGesture;
typedef enum _MouseButton MouseButton;

enum _MouseButton {
    MOUSE_BUTTON_LEFT = 1,
    MOUSE_BUTTON_RIGHT = 3,
    MOUSE_BUTTON_MIDDLE = 2,
    MOUSE_BUTTON_UNSET = 0
};

struct MouseGestureNode {
    double x;
    double y;
} MouseGestureNode_t;

struct _MouseGesture {
    struct MouseGestureNode start;
    struct MouseGestureNode middle;
    struct MouseGestureNode end;
    MouseButton last;
};

#define DEVIANCE 20
#define MINLENGTH 50

#define MOUSE_GESTURES_BUTTON MOUSE_BUTTON_MIDDLE

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
    MouseGesture* g = g_new (MouseGesture, 1);
    mouse_gesture_clear (g);

    return g;
}

static gboolean
mouse_gestures_button_press_event_cb (GtkWidget*     web_view,
                                      GdkEvent*      event,
                                      MidoriBrowser* browser)
{
    if (event->button.button == MOUSE_GESTURES_BUTTON)
    {
        /* If the gesture was previously cleaned,
           start a new gesture and coordinates. */
        if (gesture->last == MOUSE_BUTTON_UNSET)
        {
            gesture->start.x = event->button.x;
            gesture->start.y = event->button.y;
            gesture->last = event->button.button;
        }
        return TRUE;
    }

    return FALSE;
}

static gboolean
mouse_gestures_motion_notify_event_cb (GtkWidget*     web_view,
                                       GdkEvent*      event,
                                       MidoriBrowser* browser)
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

        return TRUE;
    }

    return FALSE;
}

static gboolean
mouse_gestures_button_release_event_cb (GtkWidget*     web_view,
                                        GdkEvent*      event,
                                        MidoriBrowser* browser)
{
    /* All mouse gestures will use this mouse button */
    if (gesture->last == MOUSE_GESTURES_BUTTON)
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
                     /* We moved down then right: close the tab */
                     midori_browser_activate_action (browser, "TabClose");
                /* Then we the final vertical move is between the bounds and
                we moved left more than MINLENGTH pixels */
                else if ((gesture->middle.y - gesture->end.y < DEVIANCE) &&
                         (gesture->middle.y - gesture->end.y > -DEVIANCE) &&
                         (gesture->end.x + MINLENGTH < gesture->middle.x))
                     /* We moved down then left: reload */
                     midori_browser_activate_action (browser, "Reload");
                /* The end node was never updated, we only did a vertical move */
                else if(gesture->end.y == 0 && gesture->end.x == 0)
                    /* We moved down then: create a new tab */
                    midori_browser_activate_action (browser, "TabNew");
            }
            /* We initially moved up more than MINLENGTH pixels */
            else if (gesture->middle.y + MINLENGTH < gesture->start.y)
            {
                /* The end node was never updated, we only did a vertical move */
                if (gesture->end.y == 0 && gesture->end.x == 0)
                    /* We moved up: stop */
                    midori_browser_activate_action (browser, "Stop");
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
                    /* We moved right: forward */
                    midori_browser_activate_action (browser, "Forward");
            }
            /* We initially moved left more than MINLENGTH pixels */
            else if (gesture->middle.x + MINLENGTH < gesture->start.x)
            {
                /* The end node was never updated, we only did an horizontal move */
                if (gesture->end.x == 0 && gesture->end.y == 0)
                    /* We moved left: back */
                    midori_browser_activate_action (browser, "Back");
            }
        }

        mouse_gesture_clear (gesture);

        return TRUE;
    }

    return FALSE;
}

static void
mouse_gestures_add_tab_cb (MidoriBrowser*   browser,
                           MidoriView*      view,
                           MidoriExtension* extension)
{
    GtkWidget* web_view = midori_view_get_web_view (view);

    g_object_connect (web_view,
        "signal::button-press-event",
        mouse_gestures_button_press_event_cb, browser,
        "signal::motion-notify-event",
        mouse_gestures_motion_notify_event_cb, browser,
        "signal::button-release-event",
        mouse_gestures_button_release_event_cb, browser,
        NULL);
}

static void
mouse_gestures_deactivate_cb (MidoriExtension* extension,
                              MidoriBrowser*   browser);

static void
mouse_gestures_add_tab_foreach_cb (MidoriView*      view,
                                   MidoriBrowser*   browser,
                                   MidoriExtension* extension)
{
    mouse_gestures_add_tab_cb (browser, view, extension);
}

static void
mouse_gestures_app_add_browser_cb (MidoriApp*       app,
                                   MidoriBrowser*   browser,
                                   MidoriExtension* extension)
{
    midori_browser_foreach (browser,
          (GtkCallback)mouse_gestures_add_tab_foreach_cb, extension);
    g_signal_connect (browser, "add-tab",
        G_CALLBACK (mouse_gestures_add_tab_cb), extension);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (mouse_gestures_deactivate_cb), browser);
}

static void
mouse_gestures_deactivate_tabs (MidoriView*    view,
                                MidoriBrowser* browser)
{
    GtkWidget* web_view = midori_view_get_web_view (view);

    g_object_disconnect (web_view,
        "any_signal::button-press-event",
        mouse_gestures_button_press_event_cb, browser,
        "any_signal::motion-notify-event",
        mouse_gestures_motion_notify_event_cb, browser,
        "any_signal::button-release-event",
        mouse_gestures_button_release_event_cb, browser,
        NULL);
}

static void
mouse_gestures_deactivate_cb (MidoriExtension* extension,
                              MidoriBrowser*   browser)
{
    MidoriApp* app = midori_extension_get_app (extension);

    g_signal_handlers_disconnect_by_func (
        extension, mouse_gestures_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, mouse_gestures_app_add_browser_cb, extension);
    g_signal_handlers_disconnect_by_func (
       browser, mouse_gestures_add_tab_cb, extension);
    midori_browser_foreach (browser,
        (GtkCallback)mouse_gestures_deactivate_tabs, browser);

    g_free (gesture);
}

static void
mouse_gestures_activate_cb (MidoriExtension* extension,
                            MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;
    guint i;

    gesture = mouse_gesture_new ();

    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++)))
        mouse_gestures_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (mouse_gestures_app_add_browser_cb), extension);

    g_object_unref (browsers);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Mouse Gestures"),
        "description", _("Control Midori by moving the mouse"),
        "version", "0.1",
        "authors", "Matthias Kruk <mkruk@matthiaskruk.de>", NULL);
    midori_extension_install_integer (extension, "button", MOUSE_GESTURES_BUTTON);

    g_signal_connect (extension, "activate",
        G_CALLBACK (mouse_gestures_activate_cb), NULL);

    return extension;
}
