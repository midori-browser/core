/*
 Copyright (C) 2007 Henrik Hedberg <hhedberg@innologies.fi>
 Copyright (C) 2009 Nadav Wiener <nadavwr@yahoo.com>
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "katze-scrolled.h"
#include "katze-utils.h"

#define DEFAULT_INTERVAL 50
#define DEFAULT_DECELERATION 0.7
#define DEFAULT_DRAGGING_STOPPED_DELAY 100

/**
 * SECTION:katze-scrolled
 * @short_description: Implements drag scrolling and kinetic scrolling
 * @see_also: #GtkScrolledWindow
 *
 * A scrolled window derived from #GtkScrolledWindow that implements
 * drag scrolling and kinetic scrolling. Can be used as a drop-in replacement
 * for the existing #GtkScrolledWindow.
 *
 * If a direct child of the #KatzeScrolled has its own window
 * (InputOnly is enough for events), it is automatically activated when added
 * as a child. All motion events in that area will be used to scroll.
 *
 * If some descendant widgets capture button press, button release and/ or
 * motion nofity events, the user can not scroll the area by pressing those
 * widgets (unless the widget is activated). #GtkButton is a typical example
 * of that. Usually that is the desired behaviour.
 *
 * Any widget can be registered to provide pointer events for the
 * #KatzeScrolled by using the
 * #katze_scrolled_activate_scrolling function.
 *
 **/

G_DEFINE_TYPE (KatzeScrolled, katze_scrolled, GTK_TYPE_SCROLLED_WINDOW);

enum
{
    PROP_0,

    PROP_DRAG_SCROLLING,
    PROP_KINETIC_SCROLLING
};

static void
katze_scrolled_set_property (GObject* object,
                             guint prop_id,
                             const GValue* value,
                             GParamSpec* pspec);

static void
katze_scrolled_get_property (GObject* object,
                             guint prop_id,
                             GValue* value,
                             GParamSpec* pspec);

static void
katze_scrolled_dispose (GObject* object);

static void
katze_scrolled_activate_scrolling (KatzeScrolled* scrolled,
                                   GtkWidget*     widget);

static void
katze_scrolled_set_drag_scrolling (KatzeScrolled* scrolled,
                                   gboolean       drag_scrolling);

struct _KatzeScrolledPrivate
{
    /* Settings */
    guint interval;
    gdouble deceleration;
    gboolean drag_scrolling;
    gboolean kinetic_scrolling;
    guint32 dragging_stopped_delay;
    gboolean scrolling_hints;

    /* Temporary variables */
    gboolean dragged;
    gboolean press_received;
    GdkWindow* synthetic_crossing_event_window;

    /* Disabling twice happening scrolling adjustment */
    GtkAdjustment* hadjustment;
    GtkWidget* viewport;

    /* Motion scrolling */
    gint start_x;
    gint start_y;
    gint previous_x;
    gint previous_y;
    gint farest_x;
    gint farest_y;
    guint32 start_time;
    guint32 previous_time;
    guint32 farest_time;
    gboolean going_right;
    gboolean going_down;

    /* Kinetic scrolling */
    guint scrolling_timeout_id;
    gdouble horizontal_speed;
    gdouble vertical_speed;
    gdouble horizontal_deceleration;
    gdouble vertical_deceleration;
};

typedef struct _KatzeScrolledState KatzeScrolledState;
typedef gboolean (*KatzeScrolledEventHandler)(GdkEvent*           event,
                                              KatzeScrolledState* state,
                                              gpointer            user_data);

typedef struct
{
    KatzeScrolledEventHandler event_handler;
    gpointer user_data;
} EventHandlerData;

struct _KatzeScrolledState
{
    GList* current_event_handler;
};

static GList* event_handlers = NULL;

static void
katze_scrolled_event_handler_func (GdkEvent* event,
                                   gpointer  data);

static void
katze_scrolled_event_handler_append (KatzeScrolledEventHandler event_handler,
                                     gpointer                  user_data)
{
    EventHandlerData* data;

    data = g_new0 (EventHandlerData, 1);
    data->event_handler = event_handler;
    data->user_data = user_data;
    event_handlers = g_list_append (event_handlers, data);

    gdk_event_handler_set ((GdkEventFunc)katze_scrolled_event_handler_func, NULL, NULL);
}

static void
katze_scrolled_event_handler_next (GdkEvent*           event,
                                   KatzeScrolledState* state)
{
    EventHandlerData* data;
    gboolean stop_propagating;

    state->current_event_handler = g_list_next (state->current_event_handler);
    if (state->current_event_handler)
    {
        data = (EventHandlerData*)state->current_event_handler->data;
        stop_propagating = data->event_handler (event, state, data->user_data);
        if (!stop_propagating && state->current_event_handler)
            g_critical ("%s: handler returned FALSE without calling %s first",
                        G_STRFUNC, G_STRFUNC);
    }
    else
        gtk_main_do_event (event);
}

static void
katze_scrolled_event_handler_func (GdkEvent* event,
                                   gpointer  user_data)
{
    KatzeScrolledState* state;
    EventHandlerData* data;
    gboolean stop_propagating;

    state = g_slice_new (KatzeScrolledState);
    state->current_event_handler = g_list_first (event_handlers);
    if (state->current_event_handler)
    {
        data = (EventHandlerData*)state->current_event_handler->data;
        stop_propagating = data->event_handler (event, state, data->user_data);
        if (!stop_propagating && state->current_event_handler)
            g_critical ("%s: handler returned FALSE without calling %s first",
                        G_STRFUNC, "katze_scrolled_event_handler_next");
    }
    else
        gtk_main_do_event (event);

    g_slice_free (KatzeScrolledState, state);
}

static GdkWindow* current_gdk_window;
static KatzeScrolled* current_scrolled_window;
static GtkWidget* current_widget;
static gboolean synthetized_crossing_event;

static GTree* activated_widgets;

static gint
compare_pointers (gconstpointer a,
                  gconstpointer b)
{
    return a - b;
}

static void
disable_hadjustment (KatzeScrolled* scrolled)
{
    KatzeScrolledPrivate* priv = scrolled->priv;
    GtkAdjustment* hadjustment;
    GtkWidget* viewport;

    if ((hadjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scrolled)))
        && priv->hadjustment != hadjustment)
    {
        priv->hadjustment = hadjustment;
        priv->viewport = NULL;
        viewport = GTK_WIDGET (scrolled);
        while (GTK_IS_BIN (viewport))
        {
            viewport = gtk_bin_get_child (GTK_BIN (viewport));
            if (GTK_IS_VIEWPORT (viewport))
            {
                priv->viewport = viewport;
                break;
            }
        }
    }
    g_signal_handlers_block_matched (priv->hadjustment, G_SIGNAL_MATCH_DATA,
                                     0, 0, 0, 0, priv->viewport);
}

static void
enable_hadjustment (KatzeScrolled* scrolled)
{
    KatzeScrolledPrivate* priv = scrolled->priv;

    g_signal_handlers_unblock_matched (priv->hadjustment, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, priv->viewport);
}

static gdouble
calculate_timeout_scroll_values (gdouble  old_value,
                                 gdouble  upper_limit,
                                 gdouble* scrolling_speed_pointer,
                                 gdouble  deceleration,
                                 gdouble* other_deceleration,
                                 gdouble normal_deceleration)
{
    gdouble new_value = old_value;

    if (*scrolling_speed_pointer > deceleration ||
        *scrolling_speed_pointer < -deceleration)
    {
        if (old_value + *scrolling_speed_pointer <= 0.0)
        {
            new_value = -1.0;
            *scrolling_speed_pointer = 0.0;
            *other_deceleration = normal_deceleration;
        }
        else if (old_value + *scrolling_speed_pointer >= upper_limit)
        {
            new_value = upper_limit;
            *scrolling_speed_pointer = 0.0;
            *other_deceleration = normal_deceleration;
        }
        else
            new_value = old_value + *scrolling_speed_pointer;
        if (*scrolling_speed_pointer > deceleration)
            *scrolling_speed_pointer -= deceleration;
        else if (*scrolling_speed_pointer < -deceleration)
            *scrolling_speed_pointer += deceleration;
    }

    return new_value;
}

static void
do_timeout_scroll (KatzeScrolled* scrolled)
{
    KatzeScrolledPrivate* priv = scrolled->priv;
    GtkScrolledWindow* gtk_scrolled = GTK_SCROLLED_WINDOW (scrolled);
    GtkAdjustment* hadjustment;
    GtkAdjustment* vadjustment;
    gdouble hpage_size, hupper, hvalue, new_hvalue;
    gdouble vpage_size, vupper, vvalue, new_vvalue;

    hadjustment = gtk_scrolled_window_get_hadjustment (gtk_scrolled);
    hpage_size = gtk_adjustment_get_page_size (hadjustment);
    hupper = gtk_adjustment_get_upper (hadjustment);
    hvalue = gtk_adjustment_get_value (hadjustment);
    new_hvalue = calculate_timeout_scroll_values (hvalue,
        hupper - hpage_size,
        &priv->horizontal_speed,
        priv->horizontal_deceleration,
        &priv->vertical_deceleration,
        priv->deceleration);

    vadjustment = gtk_scrolled_window_get_vadjustment (gtk_scrolled);
    vpage_size = gtk_adjustment_get_page_size (vadjustment);
    vupper = gtk_adjustment_get_upper (vadjustment);
    vvalue = gtk_adjustment_get_value (vadjustment);
    new_vvalue = calculate_timeout_scroll_values (vvalue,
        vupper - vpage_size,
        &priv->vertical_speed,
        priv->vertical_deceleration,
        &priv->horizontal_deceleration,
        priv->deceleration);

    if (new_vvalue != vvalue)
    {
        if (new_hvalue != hvalue)
        {
            disable_hadjustment (scrolled);
            gtk_adjustment_set_value (hadjustment, new_hvalue);
            enable_hadjustment (scrolled);
        }
        gtk_adjustment_set_value (vadjustment, new_vvalue);
    }
    else if (new_hvalue != hvalue)
        gtk_adjustment_set_value (hadjustment, new_hvalue);
}

static gboolean
timeout_scroll (gpointer data)
{
    gboolean ret = TRUE;
    KatzeScrolled* scrolled = KATZE_SCROLLED (data);
    KatzeScrolledPrivate* priv = scrolled->priv;

    gdk_threads_enter ();
    do_timeout_scroll (scrolled);

    if (priv->vertical_speed < priv->deceleration &&
        priv->vertical_speed > -priv->deceleration &&
        priv->horizontal_speed < priv->deceleration &&
        priv->horizontal_speed > -priv->deceleration)
    {
        priv->scrolling_timeout_id = 0;
        ret = FALSE;
    }
    gdk_threads_leave ();

    return ret;
}

static gdouble
calculate_motion_scroll_values (gdouble old_value,
                                gdouble upper_limit,
                                gint    current_coordinate,
                                gint    previous_coordinate)
{
    gdouble new_value = old_value;
    gint movement;

    movement = current_coordinate - previous_coordinate;

    if (old_value - movement < upper_limit)
        new_value = old_value - movement;
    else
        new_value = upper_limit;

    return new_value;
}

static void
do_motion_scroll (KatzeScrolled* scrolled,
                  GtkWidget*     widget,
                  gint           x,
                  gint           y,
                  guint32        timestamp)
{
    KatzeScrolledPrivate* priv = scrolled->priv;

    if (priv->dragged || gtk_drag_check_threshold (widget, priv->start_x, priv->start_y, x, y))
    {
        GtkAdjustment* hadjustment;
        GtkAdjustment* vadjustment;
        gdouble hpage_size, hupper, hvalue, new_hvalue;
        gdouble vpage_size, vupper, vvalue, new_vvalue;

        if (timestamp - priv->previous_time > priv->dragging_stopped_delay || !priv->dragged)
        {
            priv->dragged = TRUE;
            priv->going_right = priv->start_x < x;
            priv->going_down = priv->start_y < y;
            priv->start_x = priv->farest_x = x;
            priv->start_y = priv->farest_y = y;
            priv->start_time = priv->farest_time = timestamp;
        }
        else
        {
            if ((priv->going_right && x > priv->farest_x)
                || (!priv->going_right && x < priv->farest_x))
            {
                priv->farest_x = x;
                priv->farest_time = timestamp;
            }
            if ((priv->going_down && y > priv->farest_y)
                || (!priv->going_down && y < priv->farest_y))
            {
                priv->farest_y = y;
                priv->farest_time = timestamp;
            }
            if (gtk_drag_check_threshold (widget, priv->farest_x, priv->farest_y, x, y))
            {
                priv->start_x = priv->farest_x;
                priv->farest_x = x;
                priv->start_y = priv->farest_y;
                priv->farest_y = y;
                priv->start_time = priv->farest_time;
                priv->farest_time = timestamp;
                priv->going_right = priv->start_x < x;
                priv->going_down = priv->start_y < y;
            }
        }

        hadjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scrolled));
        hpage_size = gtk_adjustment_get_page_size (hadjustment);
        hupper = gtk_adjustment_get_upper (hadjustment);
        hvalue = gtk_adjustment_get_value (hadjustment);
        new_hvalue = calculate_motion_scroll_values (hvalue,
            hupper - hpage_size, x, priv->previous_x);

        vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled));
        vpage_size = gtk_adjustment_get_page_size (vadjustment);
        vupper = gtk_adjustment_get_upper (vadjustment);
        vvalue = gtk_adjustment_get_value (vadjustment);
        new_vvalue = calculate_motion_scroll_values (vvalue,
            vupper - vpage_size, y, priv->previous_y);
        if (new_vvalue != vvalue)
        {
            if (new_hvalue != hvalue)
            {
                disable_hadjustment (scrolled);
                gtk_adjustment_set_value (hadjustment, new_hvalue);
                enable_hadjustment (scrolled);
            }
            gtk_adjustment_set_value (vadjustment, new_vvalue);
        }
        else if (new_hvalue != hvalue)
            gtk_adjustment_set_value (hadjustment, new_hvalue);
    }

    priv->previous_y = y;
    priv->previous_x = x;
    priv->previous_time = timestamp;
}

static gboolean
button_press_event (GtkWidget*      widget,
                    GdkEventButton* event,
                    KatzeScrolled*  scrolled)
{
    KatzeScrolledPrivate* priv = scrolled->priv;
    gint x;
    gint y;
    GdkModifierType mask;

    if (!priv->drag_scrolling)
        return FALSE;

    if (event->button != 1)
        return FALSE;

    priv->press_received = TRUE;

    if (event->time - priv->previous_time < priv->dragging_stopped_delay &&
        gtk_drag_check_threshold (widget, priv->previous_x, priv->previous_y, x, y))
    {
        if (priv->scrolling_timeout_id)
        {
            g_source_remove (priv->scrolling_timeout_id);
            priv->scrolling_timeout_id = 0;
        }
        gdk_window_get_pointer (gtk_widget_get_window (GTK_WIDGET (scrolled)),
                                &x, &y, &mask);
        /* do_motion_scroll (scrolled, widget, x, y, event->time); */
    }
    else
    {
        if (priv->scrolling_timeout_id)
        {
            g_source_remove (priv->scrolling_timeout_id);
            priv->scrolling_timeout_id = 0;
            priv->previous_time = 0;
        }
        else
        {
            priv->dragged = FALSE;
            priv->previous_time = event->time;
        }
        gdk_window_get_pointer (gtk_widget_get_window (GTK_WIDGET (scrolled)),
                                &x, &y, &mask);
        priv->start_x = priv->previous_x = priv->farest_x = x;
        priv->start_y = priv->previous_y = priv->farest_y = y;
        priv->start_time  = event->time;
    }

    return FALSE;
}

static gboolean
button_release_event (GtkWidget*      widget,
                      GdkEventButton* event,
                      KatzeScrolled*  scrolled)
{
    KatzeScrolledPrivate* priv = scrolled->priv;
    gint x;
    gint y;
    GdkModifierType mask;

    gdk_window_get_pointer (gtk_widget_get_window (GTK_WIDGET (scrolled)),
                            &x, &y, &mask);
    if (priv->press_received &&
        gtk_drag_check_threshold (widget, priv->start_x, priv->start_y, x, y)) {
        priv->dragged = TRUE;
    }

    if (priv->press_received && priv->kinetic_scrolling &&
        event->time - priv->previous_time < priv->dragging_stopped_delay) {
        priv->vertical_speed = (gdouble)(priv->start_y - y) / (event->time - priv->start_time) * priv->interval;
        priv->horizontal_speed = (gdouble)(priv->start_x - x) / (event->time - priv->start_time) * priv->interval;
        if (ABS (priv->vertical_speed) > ABS (priv->horizontal_speed)) {
            priv->vertical_deceleration = priv->deceleration;
            priv->horizontal_deceleration = priv->deceleration * ABS (priv->horizontal_speed / priv->vertical_speed);
        } else {
            priv->horizontal_deceleration = priv->deceleration;
            priv->vertical_deceleration = priv->deceleration * ABS (priv->vertical_speed / priv->horizontal_speed);
        }
        priv->scrolling_timeout_id = g_timeout_add (priv->interval, timeout_scroll, scrolled);

        do_timeout_scroll (scrolled);
    }
    priv->previous_x = x;
    priv->previous_y = y;
    priv->previous_time = event->time;

    priv->press_received = FALSE;

    return FALSE;
}

static gboolean
motion_notify_event (GtkWidget*      widget,
                     GdkEventMotion* event,
                     KatzeScrolled*  scrolled)
{
    KatzeScrolledPrivate* priv = scrolled->priv;
    gint x;
    gint y;
    GdkModifierType mask;

    if (priv->press_received)
    {
        gdk_window_get_pointer (gtk_widget_get_window (GTK_WIDGET (scrolled)),
                                &x, &y, &mask);
        do_motion_scroll (scrolled, widget, x, y, event->time);
    }

    return FALSE;
}

static gboolean
katze_scrolled_event_handler (GdkEvent*           event,
                              KatzeScrolledState* state,
                              gpointer            user_data)
{
    gboolean stop_propagating;
    GdkEventCrossing crossing;

    stop_propagating = FALSE;

    if (event->type == GDK_BUTTON_PRESS)
    {
        gdk_window_get_user_data (event->button.window, (gpointer)&current_widget);

        if ((current_scrolled_window = g_tree_lookup (activated_widgets, current_widget)))
        {
            current_gdk_window = event->button.window;
            stop_propagating = button_press_event (current_widget, &event->button, current_scrolled_window);
        }
        else
            current_gdk_window = NULL;
    }
    else if (event->any.window == current_gdk_window)
    {
        if (event->type == GDK_MOTION_NOTIFY)
        {
            if (current_scrolled_window->priv->dragged)
                stop_propagating = motion_notify_event (current_widget, &event->motion, current_scrolled_window);
            else
            {
                stop_propagating = motion_notify_event (current_widget, &event->motion, current_scrolled_window);
                if (current_scrolled_window->priv->dragged)
                {
                    crossing.type = GDK_LEAVE_NOTIFY;
                    crossing.window = event->motion.window;
                    crossing.send_event = event->motion.send_event;
                    crossing.subwindow = gtk_widget_get_window (
                        GTK_WIDGET (current_scrolled_window));
                    crossing.time = event->motion.time;
                    crossing.x = event->motion.x;
                    crossing.y = event->motion.y;
                    crossing.x_root = event->motion.x_root;
                    crossing.y_root = event->motion.y_root;
                    crossing.mode = GDK_CROSSING_GRAB;
                    crossing.detail = GDK_NOTIFY_ANCESTOR;
                    crossing.focus = TRUE;
                    crossing.state = event->motion.state;

                    gtk_main_do_event ((GdkEvent*)&crossing);
                    synthetized_crossing_event = TRUE;
                }
            }
        }
        else if ((event->type == GDK_ENTER_NOTIFY || event->type == GDK_LEAVE_NOTIFY) &&
                   synthetized_crossing_event)
            stop_propagating = TRUE;
        else if (event->type == GDK_BUTTON_RELEASE)
            stop_propagating = button_release_event (current_widget, &event->button, current_scrolled_window);
    }

    if (!stop_propagating)
        katze_scrolled_event_handler_next (event, state);

    if (event->type == GDK_BUTTON_RELEASE && event->button.window == current_gdk_window)
    {
        crossing.type = GDK_ENTER_NOTIFY;
        crossing.window = event->button.window;
        crossing.send_event = event->button.send_event;
        crossing.subwindow = gtk_widget_get_window (
            GTK_WIDGET (current_scrolled_window));
        crossing.time = event->button.time;
        crossing.x = event->button.x;
        crossing.y = event->button.y;
        crossing.x_root = event->button.x_root;
        crossing.y_root = event->button.y_root;
        crossing.mode = GDK_CROSSING_UNGRAB;
        crossing.detail = GDK_NOTIFY_ANCESTOR;
        crossing.focus = TRUE;
        crossing.state = event->button.state;

        gtk_main_do_event ((GdkEvent*)&crossing);
        synthetized_crossing_event = FALSE;
    }

    return stop_propagating;
}

static void
katze_scrolled_add (GtkContainer* container,
                    GtkWidget*    widget)
{
    katze_scrolled_activate_scrolling (KATZE_SCROLLED (container), widget);

    (*GTK_CONTAINER_CLASS (katze_scrolled_parent_class)->add) (container, widget);
}

static void
katze_scrolled_realize (GtkWidget* widget)
{
    KatzeScrolled* scrolled = KATZE_SCROLLED (widget);
    gboolean drag_scrolling;
    GtkPolicyType policy;
    GdkWindow* window;
    GdkWindowAttr attr;

    (*GTK_WIDGET_CLASS (katze_scrolled_parent_class)->realize) (widget);

    drag_scrolling = katze_widget_has_touchscreen_mode (widget);
    policy = drag_scrolling ? GTK_POLICY_NEVER : GTK_POLICY_AUTOMATIC;
    g_object_set (scrolled, "drag-scrolling", drag_scrolling,
        "hscrollbar-policy", policy, "vscrollbar-policy", policy, NULL);

    window = g_object_ref (gtk_widget_get_parent_window (widget));
    gtk_widget_set_window (widget, window);

    attr.height = attr.width = 10;
    attr.event_mask = GDK_EXPOSURE_MASK;
    attr.wclass = GDK_INPUT_OUTPUT;
    attr.window_type = GDK_WINDOW_CHILD;
    attr.override_redirect = TRUE;

    gtk_widget_set_realized (widget, TRUE);
}

static void
katze_scrolled_dispose (GObject* object)
{
    KatzeScrolled* scrolled = KATZE_SCROLLED (object);
    KatzeScrolledPrivate* priv = scrolled->priv;

    if (priv->scrolling_timeout_id)
    {
        g_source_remove (priv->scrolling_timeout_id);
        priv->scrolling_timeout_id = 0;
    }

    (*G_OBJECT_CLASS (katze_scrolled_parent_class)->dispose) (object);
}

static void
katze_scrolled_class_init (KatzeScrolledClass* class)
{
    GObjectClass* gobject_class;
    GtkWidgetClass* widget_class;
    GtkContainerClass* container_class;
    GParamFlags flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    gobject_class = G_OBJECT_CLASS (class);
    widget_class = GTK_WIDGET_CLASS (class);
    container_class = GTK_CONTAINER_CLASS (class);

    gobject_class->set_property = katze_scrolled_set_property;
    gobject_class->get_property = katze_scrolled_get_property;
    gobject_class->dispose = katze_scrolled_dispose;

    widget_class->realize = katze_scrolled_realize;

    container_class->add = katze_scrolled_add;

    /**
     * KatzeScrolled:drag-scrolling:
     *
     * Whether the widget can be scrolled by dragging its contents.
     *
     * If "gtk-touchscreen-mode" is enabled, drag scrolling is
     * automatically enabled.
     *
     * Since: 0.2.0
     */
    g_object_class_install_property (gobject_class,
                                     PROP_DRAG_SCROLLING,
                                     g_param_spec_boolean (
                                     "drag-scrolling",
                                     "Drag Scrolling",
                                     "Whether the widget can be scrolled by dragging its contents",
                                     FALSE,
                                     flags));

    /**
     * KatzeScrolled:kinetic-scrolling:
     *
     * Whether drag scrolling is kinetic, that is releasing the
     * pointer keeps the contents scrolling further relative to
     * the speed with which they were dragged.
     *
     * Since: 0.2.0
     */
    g_object_class_install_property (gobject_class,
                                     PROP_KINETIC_SCROLLING,
                                     g_param_spec_boolean (
                                     "kinetic-scrolling",
                                     "Kinetic Scrolling",
                                     "Whether drag scrolling is kinetic",
                                     TRUE,
                                     flags));

    activated_widgets = g_tree_new ((GCompareFunc)compare_pointers);
    current_gdk_window = NULL;

    /* Usually touchscreen mode is either always set or it isn't, so it
      should be a safe optimization to not setup events if not needed. */
    if (katze_widget_has_touchscreen_mode (NULL))
        katze_scrolled_event_handler_append (katze_scrolled_event_handler, NULL);

    g_type_class_add_private (class, sizeof (KatzeScrolledPrivate));
}

static void
katze_scrolled_init (KatzeScrolled* scrolled)
{
    KatzeScrolledPrivate* priv;

    scrolled->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE ((scrolled),
        KATZE_TYPE_SCROLLED, KatzeScrolledPrivate);

    priv->interval = DEFAULT_INTERVAL;
    priv->deceleration = DEFAULT_DECELERATION;
    priv->drag_scrolling = FALSE;
    priv->kinetic_scrolling = TRUE;
    priv->dragging_stopped_delay = DEFAULT_DRAGGING_STOPPED_DELAY;
}

static void
katze_scrolled_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec)
{
    KatzeScrolled* scrolled = KATZE_SCROLLED (object);

    switch (prop_id)
    {
    case PROP_DRAG_SCROLLING:
        katze_scrolled_set_drag_scrolling (scrolled, g_value_get_boolean (value));
        break;
    case PROP_KINETIC_SCROLLING:
        scrolled->priv->kinetic_scrolling = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
katze_scrolled_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec)
{
    KatzeScrolled* scrolled = KATZE_SCROLLED (object);

    switch (prop_id)
    {
    case PROP_DRAG_SCROLLING:
        g_value_set_boolean (value, scrolled->priv->drag_scrolling);
        break;
    case PROP_KINETIC_SCROLLING:
        g_value_set_boolean (value, scrolled->priv->kinetic_scrolling);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * katze_scrolled_new:
 * @hadjustment: a horizontal #GtkAdjustment, or %NULL
 * @vadjustment: a vertical #GtkAdjustment, or %NULL
 *
 * Creates a new #KatzeScrolled.
 *
 * Since: 0.2.0
 **/

GtkWidget*
katze_scrolled_new (GtkAdjustment* hadjustment,
                            GtkAdjustment* vadjustment)
{
    if (hadjustment)
        g_return_val_if_fail (GTK_IS_ADJUSTMENT (hadjustment), NULL);
    if (vadjustment)
        g_return_val_if_fail (GTK_IS_ADJUSTMENT (vadjustment), NULL);

    return gtk_widget_new (KATZE_TYPE_SCROLLED,
                           "hadjustment", hadjustment,
                           "vadjustment", vadjustment, NULL);
}

/**
 * katze_scrolled_activate_scrolling:
 * @scrolled: a #KatzeScrolled
 * @widget: a #GtkWidget of which area is made active event source for
 * drag and kinetic scrolling.
 *
 * Activates the widget so that pointer motion events inside the widget are
 * used to scroll the #KatzeScrolled. The widget can be a child of the
 * #KatzeScrolled or even a separate widget ("touchpad" style).
 *
 * The direct child of the #KatzeScrolled (typically #GtkViewport) is
 * activated automatically when added. This function has to be used if indirect
 * descendant widgets are stopping propagation of the button press and release
 * as well as motion events (for example GtkButton is doing so) but scrolling
 * should be possible inside their area too.
 *
 * This function adds #GDK_BUTTON_PRESS_MASK, #GDK_BUTTON_RELEASE_MASK,
 * #GDK_POINTER_MOTION_MASK, and #GDK_MOTION_HINT_MAKS into the widgets event mask.
 */

static void
katze_scrolled_activate_scrolling (KatzeScrolled* scrolled,
                                   GtkWidget*     widget)
{
    gtk_widget_add_events (widget,
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
        | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
    g_tree_insert (activated_widgets, widget, scrolled);
}

static void
katze_scrolled_set_drag_scrolling (KatzeScrolled* scrolled,
                                   gboolean       drag_scrolling)
{
    KatzeScrolledPrivate* priv = scrolled->priv;

    if (priv->drag_scrolling && !drag_scrolling)
    {
        if (priv->scrolling_timeout_id)
        {
            g_source_remove (priv->scrolling_timeout_id);
            priv->scrolling_timeout_id = 0;
            priv->previous_time = 0;
        }

        priv->press_received = FALSE;
    }

    priv->drag_scrolling = drag_scrolling;
}
