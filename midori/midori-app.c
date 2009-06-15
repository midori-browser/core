/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "midori-app.h"
#include "sokoke.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#if HAVE_UNIQUE
    typedef gpointer MidoriAppInstance;
    #define MidoriAppInstanceNull NULL
    #include <unique/unique.h>
#else
    typedef gint MidoriAppInstance;
    #define MidoriAppInstanceNull -1
    #include "socket.h"
#endif

typedef struct _NotifyNotification NotifyNotification;

typedef struct
{
    gboolean            (*init)               (const gchar* app_name);
    void                (*uninit)             (void);
    NotifyNotification* (*notification_new)   (const gchar* summary,
                                               const gchar* body,
                                               const gchar* icon,
                                               GtkWidget*   attach);
    gboolean            (*notification_show)  (NotifyNotification* notification,
                                               GError**            error);
} LibNotifyFuncs;

struct _MidoriApp
{
    GObject parent_instance;

    MidoriBrowser* browser;
    GtkAccelGroup* accel_group;

    gchar* name;
    MidoriWebSettings* settings;
    KatzeArray* bookmarks;
    KatzeArray* trash;
    KatzeArray* search_engines;
    KatzeArray* history;
    KatzeArray* extensions;
    KatzeArray* browsers;

    MidoriAppInstance instance;

    /* libnotify handling */
    gchar*         program_notify_send;
    GModule*       libnotify_module;
    LibNotifyFuncs libnotify_funcs;
};

struct _MidoriAppClass
{
    GObjectClass parent_class;

    /* Signals */
    void
    (*add_browser)            (MidoriApp*     app,
                               MidoriBrowser* browser);
    void
    (*remove_browser)         (MidoriApp*     app,
                               MidoriBrowser* browser);
    void
    (*quit)                   (MidoriApp*     app);
};

G_DEFINE_TYPE (MidoriApp, midori_app, G_TYPE_OBJECT)

enum
{
    PROP_0,

    PROP_NAME,
    PROP_SETTINGS,
    PROP_BOOKMARKS,
    PROP_TRASH,
    PROP_SEARCH_ENGINES,
    PROP_HISTORY,
    PROP_EXTENSIONS,
    PROP_BROWSERS,
    PROP_BROWSER,
    PROP_BROWSER_COUNT
};

enum {
    ADD_BROWSER,
    REMOVE_BROWSER,
    QUIT,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
midori_app_finalize (GObject* object);

static void
midori_app_init_libnotify (MidoriApp* app);

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

static gboolean
midori_browser_focus_in_event_cb (MidoriBrowser* browser,
                                  GdkEventFocus* event,
                                  MidoriApp*     app)
{
    app->browser = browser;
    g_object_notify (G_OBJECT (app), "browser");
    return FALSE;
}

static void
midori_browser_new_window_cb (MidoriBrowser* browser,
                              MidoriBrowser* new_browser,
                              MidoriApp*     app)
{
    g_object_set (new_browser,
                  "settings", app->settings,
                  "bookmarks", app->bookmarks,
                  "trash", app->trash,
                  "search-engines", app->search_engines,
                  "history", app->history,
                  NULL);

    midori_app_add_browser (app, new_browser);
    gtk_widget_show (GTK_WIDGET (new_browser));
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
    g_signal_emit (app, signals[REMOVE_BROWSER], 0, browser);
    katze_array_remove_item (app->browsers, browser);
    if (!katze_array_is_empty (app->browsers))
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

static void
_midori_app_add_browser (MidoriApp*     app,
                         MidoriBrowser* browser)
{
    g_return_if_fail (MIDORI_IS_APP (app));
    g_return_if_fail (MIDORI_IS_BROWSER (browser));

    gtk_window_add_accel_group (GTK_WINDOW (browser), app->accel_group);
    g_object_connect (browser,
        "signal::focus-in-event", midori_browser_focus_in_event_cb, app,
        "signal::new-window", midori_browser_new_window_cb, app,
        "signal::delete-event", midori_browser_delete_event_cb, app,
        "signal::destroy", midori_browser_destroy_cb, app,
        "signal::quit", midori_browser_quit_cb, app,
        NULL);
    g_signal_connect_swapped (browser, "send-notification",
        G_CALLBACK (midori_app_send_notification), app);

    katze_array_add_item (app->browsers, browser);

    #if HAVE_UNIQUE
    if (app->instance)
        unique_app_watch_window (app->instance, GTK_WINDOW (browser));
    #endif
}

static void
_midori_app_quit (MidoriApp* app)
{
    gtk_main_quit ();
}

static void
midori_app_class_init (MidoriAppClass* class)
{
    GObjectClass* gobject_class;

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

    /**
     * MidoriApp::remove-browser:
     * @app: the object on which the signal is emitted
     * @browser: a #MidoriBrowser
     *
     * A new browser is being added to the app.
     *
     * Since: 0.1.7
     */
    signals[REMOVE_BROWSER] = g_signal_new (
        "remove-browser",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
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

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_app_finalize;
    gobject_class->set_property = midori_app_set_property;
    gobject_class->get_property = midori_app_get_property;

    class->add_browser = _midori_app_add_browser;
    class->quit = _midori_app_quit;

    /**
     * MidoriApp:name:
     *
     * The name of the instance.
     *
     * Since: 0.1.6
     */
    g_object_class_install_property (gobject_class,
                                     PROP_NAME,
                                     g_param_spec_string (
                                     "name",
                                     "Name",
                                     "The name of the instance",
                                     "midori",
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_SETTINGS,
                                     g_param_spec_object (
                                     "settings",
                                     "Settings",
                                     "The associated settings",
                                     MIDORI_TYPE_WEB_SETTINGS,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_BOOKMARKS,
                                     g_param_spec_object (
                                     "bookmarks",
                                     "Bookmarks",
                                     "The bookmarks folder, containing all bookmarks",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_TRASH,
                                     g_param_spec_object (
                                     "trash",
                                     "Trash",
                                     "The trash, collecting recently closed tabs and windows",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_SEARCH_ENGINES,
                                     g_param_spec_object (
                                     "search-engines",
                                     "Search Engines",
                                     "The list of search engines",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_HISTORY,
                                     g_param_spec_object (
                                     "history",
                                     "History",
                                     "The list of history items",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_EXTENSIONS,
                                     g_param_spec_object (
                                     "extensions",
                                     "Extensions",
                                     "The list of extensions",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
    * MidoriApp:browsers:
    *
    * The list of browsers.
    *
    * Since: 0.1.3
    */
    g_object_class_install_property (gobject_class,
                                     PROP_BROWSERS,
                                     g_param_spec_object (
                                     "browsers",
                                     "Browsers",
                                     "The list of browsers",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    /**
    * MidoriApp:browser:
    *
    * The current browser, that is the one that was last used.
    *
    * Since: 0.1.3
    */
    g_object_class_install_property (gobject_class,
                                     PROP_BROWSER,
                                     g_param_spec_object (
                                     "browser",
                                     "Browser",
                                     "The current browser",
                                     MIDORI_TYPE_BROWSER,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    /**
    * MidoriApp:browser-count:
    *
    * The number of browsers.
    *
    * Deprecated: 0.1.3 Use MidoriApp:browsers instead.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_BROWSER_COUNT,
                                     g_param_spec_uint (
                                     "browser-count",
                                     "Browser Count",
                                     "The current number of browsers",
                                     0, G_MAXUINT, 0,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static gboolean
midori_app_command_received (MidoriApp*   app,
                             const gchar* command,
                             gchar**      uris,
                             GdkScreen*   screen)
{
    if (g_str_equal (command, "activate"))
    {
        gtk_window_set_screen (GTK_WINDOW (app->browser), screen);
        gtk_window_present (GTK_WINDOW (app->browser));
        return TRUE;
    }
    else if (g_str_equal (command, "new"))
    {
        MidoriBrowser* browser = midori_app_create_browser (app);
        midori_app_add_browser (app, browser);
        /* FIXME: Should open the homepage according to settings */
        midori_browser_add_uri (browser, "");
        midori_browser_activate_action (browser, "Location");
        gtk_window_set_screen (GTK_WINDOW (app->browser), screen);
        gtk_widget_show (GTK_WIDGET (browser));
        return TRUE;
    }
    else if (g_str_equal (command, "open"))
    {
        gtk_window_set_screen (GTK_WINDOW (app->browser), screen);
        gtk_window_present (GTK_WINDOW (app->browser));
        if (!uris)
            return FALSE;
        else
        {
            MidoriBrowser* browser;
            MidoriNewPage open_external_pages_in;
            gboolean first;

            g_object_get (app->settings, "open-external-pages-in",
                          &open_external_pages_in, NULL);
            if (open_external_pages_in == MIDORI_NEW_PAGE_WINDOW)
            {
                browser = midori_app_create_browser (app);
                midori_app_add_browser (app, browser);
                gtk_window_set_screen (GTK_WINDOW (app->browser), screen);
                gtk_widget_show (GTK_WIDGET (browser));
            }
            else
                browser = app->browser;
            first = (open_external_pages_in == MIDORI_NEW_PAGE_CURRENT);
            while (*uris)
            {
                gchar* fixed_uri = sokoke_magic_uri (*uris, NULL);
                if (first)
                {
                    midori_browser_set_current_uri (browser, fixed_uri);
                    first = FALSE;
                }
                else
                    midori_browser_set_current_page (browser,
                        midori_browser_add_uri (browser, fixed_uri));
                g_free (fixed_uri);
                uris++;
            }
            return TRUE;
        }
    }

    return FALSE;
}

#if HAVE_UNIQUE
static UniqueResponse
midori_browser_message_received_cb (UniqueApp*         instance,
                                    UniqueCommand      command,
                                    UniqueMessageData* message,
                                    guint              timestamp,
                                    MidoriApp*         app)
{
  gboolean success;
  GdkScreen* screen = unique_message_data_get_screen (message);

  switch (command)
  {
  case UNIQUE_ACTIVATE:
      success = midori_app_command_received (app, "activate", NULL, screen);
      break;
  case UNIQUE_NEW:
      success = midori_app_command_received (app, "new", NULL, screen);
      break;
  case UNIQUE_OPEN:
  {
      gchar** uris = unique_message_data_get_uris (message);
      success = midori_app_command_received (app, "open", uris, screen);
      /* g_strfreev (uris); */
      break;
  }
  default:
      success = FALSE;
      break;
  }

  return success ? UNIQUE_RESPONSE_OK : UNIQUE_RESPONSE_FAIL;
}
#else
static gboolean
midori_app_io_channel_watch_cb (GIOChannel*  channel,
                                GIOCondition condition,
                                MidoriApp*   app)
{
    GdkScreen* screen = gtk_widget_get_screen (GTK_WIDGET (app->browser));
    gint fd, sock;
    gchar buf[4096];
    struct sockaddr_in caddr;
    guint caddr_len = sizeof(caddr);

    fd = app->instance;
    sock = accept (fd, (struct sockaddr *)&caddr, &caddr_len);

    while (fd_gets (sock, buf, sizeof (buf)) != -1)
    {
        if (strncmp (buf, "activate", 8) == 0)
        {
            midori_app_command_received (app, "open", NULL, screen);
        }
        else if (strncmp (buf, "new", 3) == 0)
        {
            midori_app_command_received (app, "new", NULL, screen);
        }
        else if (strncmp (buf, "open", 4) == 0)
        {
            while (fd_gets (sock, buf, sizeof (buf)) != -1 && *buf != '.')
            {
                gchar** uris = g_strsplit (g_strstrip (buf), "\n", 2);
                midori_app_command_received (app, "open", uris, screen);
                g_strfreev (uris);
            }
        }
    }

    gtk_window_present (GTK_WINDOW (app->browser));

    fd_close (sock);

    return TRUE;
}
#endif

static MidoriAppInstance
midori_app_create_instance (MidoriApp*   app,
                            const gchar* name)
{
    MidoriAppInstance instance;
    GdkDisplay* display;
    gchar* display_name;
    gchar* instance_name;
    guint i, n;
    #if !HAVE_UNIQUE
    gboolean exists;
    GIOChannel* channel;
    #endif

    if (!name)
        name = "midori";

    if (!(display = gdk_display_get_default ()))
        return MidoriAppInstanceNull;

    display_name = g_strdup (gdk_display_get_name (display));
    n = strlen (display_name);
    for (i = 0; i < n; i++)
        if (display_name[i] == ':' || display_name[i] == '.')
            display_name[i] = '_';
    instance_name = g_strdup_printf ("de.twotoasts.%s_%s", name, display_name);

    #if HAVE_UNIQUE
    instance = unique_app_new (instance_name, NULL);
    g_signal_connect (instance, "message-received",
                      G_CALLBACK (midori_browser_message_received_cb), app);
    #else
    instance = socket_init (instance_name, sokoke_set_config_dir (NULL), &exists);
    g_object_set_data (G_OBJECT (app), "sock-exists",
        exists ? (gpointer)0xdeadbeef : NULL);
    channel = g_io_channel_unix_new (instance);
    g_io_add_watch (channel, G_IO_IN | G_IO_PRI | G_IO_ERR,
        (GIOFunc)midori_app_io_channel_watch_cb, app);
    #endif

    g_free (instance_name);
    g_free (display_name);

    return instance;
}

static void
midori_app_init (MidoriApp* app)
{
    app->accel_group = gtk_accel_group_new ();

    app->settings = NULL;
    app->bookmarks = NULL;
    app->trash = NULL;
    app->search_engines = NULL;
    app->history = NULL;
    app->extensions = NULL;
    app->browsers = katze_array_new (MIDORI_TYPE_BROWSER);

    app->instance = MidoriAppInstanceNull;

    midori_app_init_libnotify (app);
}

static void
midori_app_finalize (GObject* object)
{
    MidoriApp* app = MIDORI_APP (object);

    g_object_unref (app->accel_group);

    katze_assign (app->name, NULL);
    katze_object_assign (app->settings, NULL);
    katze_object_assign (app->bookmarks, NULL);
    katze_object_assign (app->trash, NULL);
    katze_object_assign (app->search_engines, NULL);
    katze_object_assign (app->history, NULL);
    katze_object_assign (app->extensions, NULL);
    katze_object_assign (app->browsers, NULL);

    #if HAVE_UNIQUE
    katze_object_assign (app->instance, NULL);
    #else
    sock_cleanup ();
    #endif

    if (app->libnotify_module)
    {
        app->libnotify_funcs.uninit ();
        g_module_close (app->libnotify_module);
    }
    katze_assign (app->program_notify_send, NULL);

    G_OBJECT_CLASS (midori_app_parent_class)->finalize (object);
}

static void
midori_app_set_property (GObject*      object,
                         guint         prop_id,
                         const GValue* value,
                         GParamSpec*   pspec)
{
    MidoriApp* app = MIDORI_APP (object);

    switch (prop_id)
    {
    case PROP_NAME:
        katze_assign (app->name, g_value_dup_string (value));
        break;
    case PROP_SETTINGS:
        katze_object_assign (app->settings, g_value_dup_object (value));
        /* FIXME: Propagate settings to all browsers */
        break;
    case PROP_BOOKMARKS:
        katze_object_assign (app->bookmarks, g_value_dup_object (value));
        /* FIXME: Propagate bookmarks to all browsers */
        break;
    case PROP_TRASH:
        katze_object_assign (app->trash, g_value_dup_object (value));
        /* FIXME: Propagate trash to all browsers */
        break;
    case PROP_SEARCH_ENGINES:
        katze_object_assign (app->search_engines, g_value_dup_object (value));
        /* FIXME: Propagate search engines to all browsers */
        break;
    case PROP_HISTORY:
        katze_object_assign (app->history, g_value_dup_object (value));
        /* FIXME: Propagate history to all browsers */
        break;
    case PROP_EXTENSIONS:
        katze_object_assign (app->extensions, g_value_dup_object (value));
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

    switch (prop_id)
    {
    case PROP_NAME:
        g_value_set_string (value, app->name);
        break;
    case PROP_SETTINGS:
        g_value_set_object (value, app->settings);
        break;
    case PROP_BOOKMARKS:
        g_value_set_object (value, app->bookmarks);
        break;
    case PROP_TRASH:
        g_value_set_object (value, app->trash);
        break;
    case PROP_SEARCH_ENGINES:
        g_value_set_object (value, app->search_engines);
        break;
    case PROP_HISTORY:
        g_value_set_object (value, app->history);
        break;
    case PROP_EXTENSIONS:
        g_value_set_object (value, app->extensions);
        break;
    case PROP_BROWSERS:
        g_value_set_object (value, app->browsers);
        break;
    case PROP_BROWSER:
        g_value_set_object (value, app->browser);
        break;
    case PROP_BROWSER_COUNT:
        g_value_set_uint (value, katze_array_get_length (app->browsers));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
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
 * midori_app_instance_is_running:
 * @app: a #MidoriApp
 *
 * Determines whether an instance of Midori is
 * already running on the default display.
 *
 * Use the "name" property if you want to run more
 * than one instance.
 *
 * Return value: %TRUE if an instance is already running
 **/
gboolean
midori_app_instance_is_running (MidoriApp* app)
{
    g_return_val_if_fail (MIDORI_IS_APP (app), FALSE);

    if (app->instance == MidoriAppInstanceNull)
        app->instance = midori_app_create_instance (app, app->name);
    #if HAVE_UNIQUE
    if (app->instance)
        return unique_app_is_running (app->instance);
    #else
        return g_object_get_data (G_OBJECT (app), "sock-exists") != NULL;
    #endif
    return FALSE;
}

/**
 * midori_app_instance_send_activate:
 * @app: a #MidoriApp
 *
 * Sends a message to an instance of Midori already
 * running on the default display, asking to activate it.
 *
 * Practically the current browser will be focussed.
 *
 * Return value: %TRUE if the message was sent successfully
 **/
gboolean
midori_app_instance_send_activate (MidoriApp* app)
{
    #if HAVE_UNIQUE
    UniqueResponse response;
    #endif

    /* g_return_val_if_fail (MIDORI_IS_APP (app), FALSE); */
    g_return_val_if_fail (midori_app_instance_is_running (app), FALSE);

    #if HAVE_UNIQUE
    if (app->instance)
    {
        response = unique_app_send_message (app->instance, UNIQUE_ACTIVATE, NULL);
        if (response == UNIQUE_RESPONSE_OK)
            return TRUE;
    }
    #else
    if (app->instance > -1)
    {
        send_open_command (app->instance, "activate", NULL);
        return TRUE;
    }
    #endif
    return FALSE;
}

/**
 * midori_app_instance_send_new_browser:
 * @app: a #MidoriApp
 *
 * Sends a message to an instance of Midori already
 * running on the default display, asking to open a new browser.
 *
 * Return value: %TRUE if the message was sent successfully
 **/
gboolean
midori_app_instance_send_new_browser (MidoriApp* app)
{
    #if HAVE_UNIQUE
    UniqueResponse response;
    #endif

    /* g_return_val_if_fail (MIDORI_IS_APP (app), FALSE); */
    g_return_val_if_fail (midori_app_instance_is_running (app), FALSE);

    #if HAVE_UNIQUE
    if (app->instance)
    {
        response = unique_app_send_message (app->instance, UNIQUE_NEW, NULL);
        if (response == UNIQUE_RESPONSE_OK)
            return TRUE;
    }
    #else
    if (app->instance > -1)
    {
        send_open_command (app->instance, "new", NULL);
        return TRUE;
    }
    #endif
    return FALSE;
}

/**
 * midori_app_instance_send_uris:
 * @app: a #MidoriApp
 * @uris: a string vector of URIs
 *
 * Sends a message to an instance of Midori already
 * running on the default display, asking to open @uris.
 *
 * The strings in @uris will each be opened in a new tab.
 *
 * Return value: %TRUE if the message was sent successfully
 **/
gboolean
midori_app_instance_send_uris (MidoriApp* app,
                               gchar**    uris)
{
    #if HAVE_UNIQUE
    UniqueMessageData* message;
    UniqueResponse response;
    #endif

    /* g_return_val_if_fail (MIDORI_IS_APP (app), FALSE); */
    g_return_val_if_fail (midori_app_instance_is_running (app), FALSE);
    g_return_val_if_fail (uris != NULL, FALSE);

    #if HAVE_UNIQUE
    if (app->instance)
    {
        message = unique_message_data_new ();
        unique_message_data_set_uris (message, uris);
        response = unique_app_send_message (app->instance, UNIQUE_OPEN, message);
        unique_message_data_free (message);
        if (response == UNIQUE_RESPONSE_OK)
            return TRUE;
    }
    #else
    if (app->instance > -1)
    {
        send_open_command (app->instance, "open", uris);
        return TRUE;
    }
    #endif
    return FALSE;
}

/**
 * midori_app_add_browser:
 * @app: a #MidoriApp
 * @browser: a #MidoriBrowser
 *
 * Adds a #MidoriBrowser to the #MidoriApp.
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
    g_return_if_fail (MIDORI_IS_APP (app));
    g_return_if_fail (MIDORI_IS_BROWSER (browser));

    g_signal_emit (app, signals[ADD_BROWSER], 0, browser);
}

/**
 * midori_app_create_browser:
 * @app: a #MidoriApp
 *
 * Creates a #MidoriBrowser which inherits its settings,
 * bookmarks, trash, search engines and history from  @app.
 *
 * Return value: a new #MidoriBrowser
 *
 * Since: 0.1.2
 **/
MidoriBrowser*
midori_app_create_browser (MidoriApp* app)
{
    g_return_val_if_fail (MIDORI_IS_APP (app), NULL);

    return g_object_new (MIDORI_TYPE_BROWSER,
                         "settings", app->settings,
                         "bookmarks", app->bookmarks,
                         "trash", app->trash,
                         "search-engines", app->search_engines,
                         "history", app->history,
                         NULL);
}

/**
 * midori_app_quit:
 * @app: a #MidoriApp
 *
 * Quits the #MidoriApp singleton.
 *
 * Since 0.1.2 the "quit" signal is always emitted before quitting.
 **/
void
midori_app_quit (MidoriApp* app)
{
    g_return_if_fail (MIDORI_IS_APP (app));

    g_signal_emit (app, signals[QUIT], 0);
}

static void
midori_app_init_libnotify (MidoriApp* app)
{
    gint i;
    const gchar* sonames[] = { "libnotify.so", "libnotify.so.1", NULL };

    for (i = 0; sonames[i] != NULL && app->libnotify_module == NULL; i++ )
    {
        app->libnotify_module = g_module_open (sonames[i], G_MODULE_BIND_LOCAL);
    }

    if (app->libnotify_module != NULL)
    {
        g_module_symbol (app->libnotify_module, "notify_init",
            (void*) &(app->libnotify_funcs.init));
        g_module_symbol (app->libnotify_module, "notify_uninit",
            (void*) &(app->libnotify_funcs.uninit));
        g_module_symbol (app->libnotify_module, "notify_notification_new",
            (void*) &(app->libnotify_funcs.notification_new));
        g_module_symbol (app->libnotify_module, "notify_notification_show",
            (void*) &(app->libnotify_funcs.notification_show));

        /* init libnotify */
        if (!app->libnotify_funcs.init || !app->libnotify_funcs.init ("midori"))
        {
             g_module_close (app->libnotify_module);
             app->libnotify_module = NULL;
        }
    }

    app->program_notify_send = g_find_program_in_path ("notify-send");
}

/**
 * midori_app_send_notification:
 * @app: a #MidoriApp
 * @title: title of the notification
 * @message: text of the notification, or NULL
 *
 * Send #message to the notification daemon to display it.
 * This is done by using libnotify if available or by using the program
 * "notify-send" as a fallback.
 *
 * There is no guarantee that the message have been sent and displayed, as
 * neither libnotify nor "notify-send" might be available or the
 * notification daemon might not be running.
 *
 * Since 0.1.7
 **/
void
midori_app_send_notification (MidoriApp*   app,
                              const gchar* title,
                              const gchar* message)
{
    gboolean sent = FALSE;

    g_return_if_fail (MIDORI_IS_APP (app));
    g_return_if_fail (title);

    if (app->libnotify_module)
    {
        NotifyNotification* n;

        n = app->libnotify_funcs.notification_new (title, message, "midori", NULL);
        sent = app->libnotify_funcs.notification_show (n, NULL);
        g_object_unref (n);
    }
    /* Fall back to the command line program "notify-send" */
    if (!sent && app->program_notify_send)
    {
        gchar* msgq = g_shell_quote (message);
        gchar* titleq = g_shell_quote (title);
        gchar* command = g_strdup_printf ("%s -i midori %s %s",
            app->program_notify_send, titleq, msgq);

        g_spawn_command_line_async (command, NULL);

        g_free (titleq);
        g_free (msgq);
        g_free (command);
    }
}
