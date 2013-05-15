/*
 Copyright (C) 2009 Matthias Kruk <mkruk@matthiaskruk.de>
 Copyright (C) 2009-2010 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>
#include <math.h>

typedef struct _MouseGesture MouseGesture;
typedef enum _MouseButton MouseButton;

enum _MouseButton
{
    MOUSE_BUTTON_LEFT = 1,
    MOUSE_BUTTON_RIGHT = 3,
    MOUSE_BUTTON_MIDDLE = 2,
    MOUSE_BUTTON_UNSET = 0
};

/* equivalent to the angle measured anticlockwise from east, divided by 45 or pi/4 */
typedef enum
{
    STROKE_EAST = 0,
    STROKE_NORTHEAST,
    STROKE_NORTH,
    STROKE_NORTHWEST,
    STROKE_WEST,
    STROKE_SOUTHWEST,
    STROKE_SOUTH,
    STROKE_SOUTHEAST,
    STROKE_NONE,
} MouseGestureDirection;

static const gchar* direction_names[]=
{
    "E",
    "NE",
    "N",
    "NW",
    "W",
    "SW",
    "S",
    "SE",
    "NONE",
};

#define N_DIRECTIONS 8

#define DEVIANCE (15 * M_PI / 180)
#define MINLENGTH 30

char** config_actions = NULL;
MouseGestureDirection** config_gestures = NULL;

const char* default_actions[]=
{
    "TabClose",
    "Reload",
    "TabNew",
    "Stop",
    "Forward",
    "Back",
    NULL
};

const MouseGestureDirection default_gesture_strokes[] =
{
    STROKE_SOUTH, STROKE_EAST, STROKE_NONE,
    STROKE_SOUTH, STROKE_WEST, STROKE_NONE,
    STROKE_SOUTH, STROKE_NONE,
    STROKE_NORTH, STROKE_NONE,
    STROKE_EAST, STROKE_NONE,
    STROKE_WEST, STROKE_NONE,
    STROKE_NONE,
};

const MouseGestureDirection* default_gestures[] =
{
    &default_gesture_strokes[0],
    &default_gesture_strokes[3],
    &default_gesture_strokes[6],
    &default_gesture_strokes[8],
    &default_gesture_strokes[10],
    &default_gesture_strokes[12],
    &default_gesture_strokes[14],
};

static gboolean
parse_direction (const char* str, MouseGestureDirection* dir)
{
    int i;
    for (i = 0; i < N_DIRECTIONS; i++)
    {
        if(!strcmp(str, direction_names[i]))
        {
            *dir = i;
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean
strokes_equal (const MouseGestureDirection* a, const MouseGestureDirection* b)
{
    int i;
    for (i = 0; a[i] != STROKE_NONE && b[i] != STROKE_NONE; i++)
    {
        if(a[i] != b[i])
            return FALSE;
    }
    return a[i] == b[i];
}

struct MouseGestureNode
{
    double x;
    double y;
};

static guint
dist_sqr (guint x1, guint y1, guint x2, guint y2)
{
    guint xdiff = abs(x1 - x2);
    guint ydiff = abs(y1 - y2);
    return xdiff * xdiff + ydiff * ydiff;
}

static float
get_angle_for_direction (MouseGestureDirection direction)
{
    return direction * 2 * M_PI / N_DIRECTIONS;
}

static MouseGestureDirection
nearest_direction_for_angle (float angle)
{
    /* move halfway to the next direction so we can floor to round */
    angle += M_PI / N_DIRECTIONS;

    /* ensure we stay within [0, 2pi) */
    if (angle >= 2 * M_PI)
        angle -= 2 * M_PI;

    return (MouseGestureDirection)((angle * N_DIRECTIONS) / (2* M_PI));
}

static gboolean
vector_follows_direction (float angle, float distance, MouseGestureDirection direction)
{
    if (direction == STROKE_NONE)
        return distance < MINLENGTH / 2;

    float dir_angle = get_angle_for_direction (direction);
    if (fabsf(angle - dir_angle) < DEVIANCE || fabsf(angle - dir_angle + 2 * M_PI) < DEVIANCE)
        return TRUE;

    if(distance < MINLENGTH / 2)
        return TRUE;

    return FALSE;
}

/* returns the angle in the range [0, 2pi) (anticlockwise from east) from point 1 to 2 */
static float
get_angle_between_points (guint x1, guint y1, guint x2, guint y2)
{
    float distance = sqrtf (dist_sqr (x1, y1, x2, y2));

    /* compute the angle of the vector from a to b */
    float cval=((signed int)x2 - (signed int)x1) / distance;
    float angle = acosf (cval);
    if(y2 > y1)
        angle = 2 * M_PI - angle;

    return angle;
}

#define N_NODES 8

struct _MouseGesture
{
    MouseButton button;
    MouseGestureDirection strokes[N_NODES + 1];
    struct MouseGestureNode locations[N_NODES];
    struct MouseGestureNode last_pos;
    float last_distance;
    /* the index of the location to be filled next */
    guint count;
    MouseButton last;
};

MouseGesture *gesture = NULL;

static void
mouse_gesture_clear (MouseGesture *g)
{
    memset(g->locations, 0, sizeof(g->locations));
    g->strokes[0] = STROKE_NONE;
    g->count = 0;
    g->last_distance = 0;
    g->last = MOUSE_BUTTON_UNSET;
}

MouseGesture* mouse_gesture_new (void)
{
    MouseGesture* g = g_slice_new (MouseGesture);
    mouse_gesture_clear (g);

    return g;
}

static gboolean
mouse_gestures_button_press_event_cb (GtkWidget*     web_view,
                                      GdkEvent*      event,
                                      MidoriBrowser* browser)
{
    if (event->button.button == gesture->button)
    {
        /* If the gesture was previously cleaned,
           start a new gesture and coordinates. */
        if (gesture->count == MOUSE_BUTTON_UNSET)
        {
            gesture->locations[gesture->count].x = event->button.x;
            gesture->locations[gesture->count].y = event->button.y;
            gesture->last_pos = gesture->locations[gesture->count];
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
    /* wait until a button has been pressed */
    if (gesture->last != MOUSE_BUTTON_UNSET)
    {
        guint x, y, oldx, oldy;
        float angle, distance;
        MouseGestureDirection old_direction, new_direction;

        x = event->motion.x;
        y = event->motion.y;
        oldx = gesture->locations[gesture->count].x;
        oldy = gesture->locations[gesture->count].y;

        old_direction = gesture->strokes[gesture->count];

        angle = get_angle_between_points (oldx, oldy, x, y);
        distance = sqrtf (dist_sqr (oldx, oldy, x, y));

        /* wait until minimum distance has been reached to set an initial direction. */
        if (old_direction == STROKE_NONE)
        {
            if (distance >= MINLENGTH)
            {
                gesture->strokes[gesture->count] = nearest_direction_for_angle (angle);
                if(midori_debug ("adblock:match"))
                    g_debug ("detected %s\n", direction_names[gesture->strokes[gesture->count]]);
            }
        }
        else if (!vector_follows_direction (angle, distance, old_direction)
                 || distance < gesture->last_distance)
        {
            /* if path curves or we've reversed our movement, try to detect a new direction */
            angle = get_angle_between_points (gesture->last_pos.x, gesture->last_pos.y, x, y);
            new_direction = nearest_direction_for_angle (angle);

            if (new_direction != old_direction && gesture->count + 1 < N_NODES)
            {
                /* record this node and return to an indeterminate direction */
                gesture->count++;
                gesture->strokes[gesture->count] = STROKE_NONE;
                gesture->locations[gesture->count].x = x;
                gesture->locations[gesture->count].y = y;
                gesture->last_distance = 0;
            }
        }
        else if(distance > gesture->last_distance)
        {
            /* if following the same direction, store the progress along it for later divergence checks */
            gesture->last_pos.x = x;
            gesture->last_pos.y = y;
            gesture->last_distance = distance;
        }
        return TRUE;
    }

    return FALSE;
}

static gboolean
mouse_gestures_activate_action (MidoriView*  view,
                                const gchar* name)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (view));
    midori_browser_activate_action (browser, name);
    return TRUE;
}

static gboolean
mouse_gestures_button_release_event_cb (GtkWidget*      web_view,
                                        GdkEventButton* event,
                                        MidoriView*     view)
{
    int i;

    if (gesture->strokes[gesture->count] != STROKE_NONE)
    {
        gesture->count++;
        gesture->strokes[gesture->count] = STROKE_NONE;
    }

    const MouseGestureDirection** gestures = config_gestures ?
                                             (const MouseGestureDirection**)config_gestures :
                                             default_gestures;
    const gchar** actions = config_actions ? (const char**)config_actions : default_actions;

    for(i = 0; gestures[i][0] != STROKE_NONE; i++)
    {
        if(strokes_equal (gesture->strokes, gestures[i]))
        {
            mouse_gesture_clear (gesture);
            return mouse_gestures_activate_action (view, actions[i]);
        }
    }

    mouse_gesture_clear (gesture);

    if (MIDORI_EVENT_CONTEXT_MENU (event))
    {
        GtkWidget* menu = gtk_menu_new ();
        midori_view_populate_popup (view, menu, TRUE);
        katze_widget_popup (GTK_WIDGET (web_view), GTK_MENU (menu),
                            event, KATZE_MENU_POSITION_CURSOR);
        return TRUE;
    }

    return FALSE;
}

static void
mouse_gestures_load_config (MidoriExtension* extension)
{
    int i;
    gchar* config_file;
    gsize n_keys;
    gchar** keys;
    GKeyFile* keyfile;

    config_file = g_build_filename (midori_extension_get_config_dir (extension),
                                    "gestures", NULL);
    keyfile = g_key_file_new ();
    g_key_file_load_from_file (keyfile, config_file, G_KEY_FILE_NONE, NULL);
    g_free (config_file);

    if (!keyfile)
        return;

    keys = g_key_file_get_keys (keyfile, "gestures", &n_keys, NULL);
    if (!keys)
        return;

    if(config_gestures)
    {
        g_strfreev ((gchar**)config_gestures);
        g_strfreev (config_actions);
    }
    config_gestures = g_malloc ((n_keys + 1) * sizeof (MouseGestureDirection*));
    config_actions = g_malloc (n_keys * sizeof (gchar*));

    for(i = 0; keys[i]; i++)
    {
        gsize n_strokes;
        int j;
        gchar** stroke_strings = g_key_file_get_string_list (keyfile, "gestures", keys[i], &n_strokes,
                                                             NULL);

        config_gestures[i] = g_malloc ((n_strokes + 1) * sizeof (MouseGestureDirection));

        for (j = 0; j < n_strokes; j++)
        {
            if (!parse_direction (stroke_strings[j], &config_gestures[i][j]))
                g_warning ("mouse-gestures: failed to parse direction \"%s\"\n", stroke_strings[j]);
        }
        config_gestures[i][j] = STROKE_NONE;

        config_actions[i] = keys[i];
        g_strfreev (stroke_strings);
    }
    config_gestures[i] = g_malloc (sizeof (MouseGestureDirection));
    config_gestures[i][0] = STROKE_NONE;

    g_free (keys);
    g_key_file_free (keyfile);
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
        mouse_gestures_button_release_event_cb, view,
        NULL);
}

static void
mouse_gestures_deactivate_cb (MidoriExtension* extension,
                              MidoriBrowser*   browser);

static void
mouse_gestures_app_add_browser_cb (MidoriApp*       app,
                                   MidoriBrowser*   browser,
                                   MidoriExtension* extension)
{
    GList* tabs = midori_browser_get_tabs (browser);
    for (; tabs; tabs = g_list_next (tabs))
        mouse_gestures_add_tab_cb (browser, tabs->data, extension);
    g_list_free (tabs);
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
        mouse_gestures_button_release_event_cb, view,
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

    GList* tabs = midori_browser_get_tabs (browser);
    for (; tabs; tabs = g_list_next (tabs))
        mouse_gestures_deactivate_tabs (tabs->data, browser);
    g_list_free (tabs);
    g_slice_free (MouseGesture, gesture);
    if(config_gestures)
    {
        g_strfreev ((gchar**)config_gestures);
        config_gestures = NULL;
        g_strfreev (config_actions);
        config_actions = NULL;
    }
}

static void
mouse_gestures_activate_cb (MidoriExtension* extension,
                            MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;

    gesture = mouse_gesture_new ();
    gesture->button = midori_extension_get_integer (extension, "button");
    mouse_gestures_load_config (extension);

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
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
        "version", "0.2" MIDORI_VERSION_SUFFIX,
        "authors", "Matthias Kruk <mkruk@matthiaskruk.de>", NULL);
    midori_extension_install_integer (extension, "button", MOUSE_BUTTON_RIGHT);
    midori_extension_install_integer (extension, "actions", MOUSE_BUTTON_RIGHT);

    g_signal_connect (extension, "activate",
        G_CALLBACK (mouse_gestures_activate_cb), NULL);

    return extension;
}
