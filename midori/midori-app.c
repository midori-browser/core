/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-app.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

G_DEFINE_TYPE (MidoriApp, midori_app, G_TYPE_OBJECT)

static MidoriApp* _midori_app_singleton = NULL;

struct _MidoriAppPrivate
{
    GList* browsers;
    MidoriBrowser* browser;
    GtkAccelGroup* accel_group;

    MidoriWebSettings* settings;
    MidoriTrash* trash;
};

#define MIDORI_APP_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
     MIDORI_TYPE_APP, MidoriAppPrivate))

enum
{
    PROP_0,

    PROP_SETTINGS,
    PROP_TRASH,
    PROP_BROWSER,
    PROP_BROWSER_COUNT
};

enum {
    ADD_BROWSER,
    QUIT,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GObject*
midori_app_constructor (GType                  type,
                        guint                  n_construct_properties,
                        GObjectConstructParam* construct_properties);

static void
midori_app_finalize (GObject* object);

static void
midori_app_set_property (GObject*      object,
                         guint         prop_id,
                         const GValue* value,
                         GParamSpec*   pspec);

static void
midori_app_get_property (GObject*    object,
                         guint       prop_id,
                         GValue*     value,
                         GParamSpec* pspec);

static void
midori_app_class_init (MidoriAppClass* class)
{
    signals[ADD_BROWSER] = g_signal_new (
        "add-browser",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriAppClass, add_browser),
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        MIDORI_TYPE_BROWSER);

    signals[QUIT] = g_signal_new (
        "quit",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriAppClass, quit),
        0,
        NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
    gobject_class->constructor = midori_app_constructor;
    gobject_class->finalize = midori_app_finalize;
    gobject_class->set_property = midori_app_set_property;
    gobject_class->get_property = midori_app_get_property;

    MidoriAppClass* midoriapp_class = MIDORI_APP_CLASS (class);
    midoriapp_class->add_browser = midori_app_add_browser;
    midoriapp_class->quit = midori_app_quit;

    g_object_class_install_property (gobject_class,
                                     PROP_SETTINGS,
                                     g_param_spec_object (
                                     "settings",
                                     _("Settings"),
                                     _("The associated settings"),
                                     MIDORI_TYPE_WEB_SETTINGS,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_TRASH,
                                     g_param_spec_object (
                                     "trash",
                                     _("Trash"),
                                     _("The trash, collecting recently closed tabs and windows"),
                                     MIDORI_TYPE_TRASH,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_BROWSER,
                                     g_param_spec_object (
                                     "browser",
                                     _("Browser"),
                                     _("The current browser"),
                                     MIDORI_TYPE_BROWSER,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_BROWSER_COUNT,
                                     g_param_spec_uint (
                                     "browser-count",
                                     _("Browser Count"),
                                     _("The current number of browsers"),
                                     0, G_MAXUINT, 0,
                                     G_PARAM_READABLE));

    g_type_class_add_private (class, sizeof (MidoriAppPrivate));
}

static GObject*
midori_app_constructor (GType                  type,
                        guint                  n_construct_properties,
                        GObjectConstructParam* construct_properties)
{
    if (_midori_app_singleton)
        return g_object_ref (_midori_app_singleton);
    else
        return G_OBJECT_CLASS (midori_app_parent_class)->constructor (
            type, n_construct_properties, construct_properties);
}

static void
midori_app_init (MidoriApp* app)
{
    g_assert (!_midori_app_singleton);

    _midori_app_singleton = app;

    app->priv = MIDORI_APP_GET_PRIVATE (app);

    MidoriAppPrivate* priv = app->priv;

    priv->accel_group = gtk_accel_group_new ();

    priv->settings = midori_web_settings_new ();
    priv->trash = midori_trash_new (10);
}

static void
midori_app_finalize (GObject* object)
{
    MidoriApp* app = MIDORI_APP (object);
    MidoriAppPrivate* priv = app->priv;

    g_list_free (priv->browsers);
    g_object_unref (priv->accel_group);

    g_object_unref (priv->settings);
    g_object_unref (priv->trash);

    G_OBJECT_CLASS (midori_app_parent_class)->finalize (object);
}

static void
midori_app_set_property (GObject*      object,
                         guint         prop_id,
                         const GValue* value,
                         GParamSpec*   pspec)
{
    MidoriApp* app = MIDORI_APP (object);
    MidoriAppPrivate* priv = app->priv;

    switch (prop_id)
    {
    case PROP_SETTINGS:
        katze_object_assign (priv->settings, g_value_get_object (value));
        g_object_ref (priv->settings);
        // FIXME: Propagate settings to all browsers
        break;
    case PROP_TRASH:
        katze_object_assign (priv->trash, g_value_get_object (value));
        g_object_ref (priv->trash);
        // FIXME: Propagate trash to all browsers
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_app_get_property (GObject*    object,
                         guint       prop_id,
                         GValue*     value,
                         GParamSpec* pspec)
{
    MidoriApp* app = MIDORI_APP (object);
    MidoriAppPrivate* priv = app->priv;

    switch (prop_id)
    {
    case PROP_SETTINGS:
        g_value_set_object (value, priv->settings);
        break;
    case PROP_TRASH:
        g_value_set_object (value, priv->trash);
        break;
    case PROP_BROWSER:
        g_value_set_object (value, priv->browser);
        break;
    case PROP_BROWSER_COUNT:
        g_value_set_uint (value, g_list_length (priv->browsers));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static gboolean
midori_browser_focus_in_event_cb (MidoriBrowser* browser,
                                  GdkEventFocus* event,
                                  MidoriApp*     app)
{
    MidoriAppPrivate* priv = app->priv;

    priv->browser = browser;
    return FALSE;
}

static void
midori_browser_new_window_cb (MidoriBrowser* browser,
                              const gchar*   uri,
                              MidoriApp*     app)
{
    MidoriAppPrivate* priv = app->priv;

    MidoriBrowser* new_browser = g_object_new (MIDORI_TYPE_BROWSER,
                                               "settings", priv->settings,
                                               "trash", priv->trash,
                                               NULL);
    midori_browser_add_uri (new_browser, uri);
    gtk_widget_show (GTK_WIDGET (new_browser));

    g_signal_emit (app, signals[ADD_BROWSER], 0, new_browser);
}

static gboolean
midori_browser_delete_event_cb (MidoriBrowser* browser,
                                GdkEvent*      event,
                                MidoriApp*     app)
{
    return FALSE;
}

static gboolean
midori_browser_destroy_cb (MidoriBrowser* browser,
                           MidoriApp*     app)
{
    MidoriAppPrivate* priv = app->priv;

    priv->browsers = g_list_remove (priv->browsers, browser);
    if (g_list_nth (priv->browsers, 0))
        return FALSE;
    midori_app_quit (app);
    return TRUE;
}

static void
midori_browser_quit_cb (MidoriBrowser* browser,
                        MidoriApp*     app)
{
    midori_app_quit (app);
}

/**
 * midori_app_new:
 *
 * Instantiates a new #MidoriApp singleton.
 *
 * Subsequent calls will ref the initial instance.
 *
 * Return value: a new #MidoriApp
 **/
MidoriApp*
midori_app_new (void)
{
    MidoriApp* app = g_object_new (MIDORI_TYPE_APP,
                                   NULL);

    return app;
}

/**
 * midori_app_add_browser:
 *
 * Adds a #MidoriBrowser to the #MidoriApp singleton.
 *
 * The app will take care of the browser's new-window and quit signals, as well
 * as watch window closing so that the last closed window quits the app.
 * Also the app watches focus changes to indicate the 'current' browser.
 *
 * Return value: a new #MidoriApp
 **/
void
midori_app_add_browser (MidoriApp*     app,
                        MidoriBrowser* browser)
{
    MidoriAppPrivate* priv = app->priv;

    gtk_window_add_accel_group (GTK_WINDOW (browser), priv->accel_group);
    g_object_connect (browser,
        "signal::focus-in-event", midori_browser_focus_in_event_cb, app,
        "signal::new-window", midori_browser_new_window_cb, app,
        "signal::delete-event", midori_browser_delete_event_cb, app,
        "signal::destroy", midori_browser_destroy_cb, app,
        "signal::quit", midori_browser_quit_cb, app,
        NULL);

    priv->browsers = g_list_prepend (priv->browsers, browser);
}

/**
 * midori_app_get_settings:
 * @app: a #MidoriApp
 *
 * Retrieves the #MidoriWebSettings of the app.
 *
 * Return value: the assigned #MidoriWebSettings
 **/
MidoriWebSettings*
midori_app_get_settings (MidoriApp* app)
{
    g_return_val_if_fail (MIDORI_IS_APP (app), NULL);

    MidoriAppPrivate* priv = app->priv;

    return priv->settings;
}

/**
 * midori_app_set_settings:
 * @app: a #MidoriApp
 *
 * Assigns the #MidoriWebSettings to the app.
 *
 * Return value: the assigned #MidoriWebSettings
 **/
void
midori_app_set_settings (MidoriApp*         app,
                         MidoriWebSettings* settings)
{
    g_return_if_fail (MIDORI_IS_APP (app));

    g_object_set (app, "settings", settings, NULL);
}

/**
 * midori_app_get_trash:
 * @app: a #MidoriApp
 *
 * Retrieves the #MidoriTrash of the app.
 *
 * Return value: the assigned #MidoriTrash
 **/
MidoriTrash*
midori_app_get_trash (MidoriApp* app)
{
    g_return_val_if_fail (MIDORI_IS_APP (app), NULL);

    MidoriAppPrivate* priv = app->priv;

    return priv->trash;
}

/**
 * midori_app_quit:
 * @app: a #MidoriApp
 *
 * Quits the #MidoriApp singleton.
 **/
void
midori_app_quit (MidoriApp* app)
{
    g_return_if_fail (MIDORI_IS_APP (app));

    gtk_main_quit ();
}
