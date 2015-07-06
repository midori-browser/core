/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

/* When cross-compiling assume at least WinXP */
#ifdef _WIN32
    #define _WIN32_WINNT 0x0501
    #include <unistd.h>
    #include <windows.h>
#endif

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "midori-app.h"
#include "midori-platform.h"
#include "midori-core.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#if ENABLE_NLS
    #include <libintl.h>
    #include <locale.h>
#endif

#ifdef HAVE_LIBNOTIFY
    #include <libnotify/notify.h>
    #ifndef NOTIFY_CHECK_VERSION
        #define NOTIFY_CHECK_VERSION(x,y,z) 0
    #endif
#endif

#ifdef HAVE_SIGNAL_H
    #include <signal.h>
#endif

struct _MidoriApp
{
    GApplication parent_instance;

    MidoriWebSettings* settings;
    KatzeArray* bookmarks;
    KatzeArray* trash;
    KatzeArray* search_engines;
    KatzeArray* history;
    GKeyFile* speeddial;
    KatzeArray* extensions;
    KatzeArray* browsers;

    MidoriBrowser* browser;
};

static gchar* app_name = NULL;

struct _MidoriAppClass
{
    GApplicationClass parent_class;

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

G_DEFINE_TYPE (MidoriApp, midori_app, G_TYPE_APPLICATION);

enum
{
    PROP_0,

    PROP_NAME,
    PROP_SETTINGS,
    PROP_BOOKMARKS,
    PROP_TRASH,
    PROP_SEARCH_ENGINES,
    PROP_HISTORY,
    PROP_SPEED_DIAL,
    PROP_EXTENSIONS,
    PROP_BROWSERS,
    PROP_BROWSER
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

static MidoriBrowser*
midori_browser_new_window_cb (MidoriBrowser* browser,
                              MidoriBrowser* new_browser,
                              MidoriApp*     app)
{
    if (new_browser)
        g_object_set (new_browser,
                      "settings", app->settings,
                      "bookmarks", app->bookmarks,
                      "trash", app->trash,
                      "search-engines", app->search_engines,
                      "history", app->history,
                      "speed-dial", app->speeddial,
                      NULL);
    else
        new_browser = midori_app_create_browser (app);

    midori_app_add_browser (app, new_browser);
    gtk_widget_show (GTK_WIDGET (new_browser));

    return new_browser;
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
    {
        app->browser = katze_array_get_nth_item (app->browsers, 0);
        return FALSE;
    }
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

    #if GTK_CHECK_VERSION (3, 0, 0)
    if (app->browser == NULL)
    {
        gchar* filename;
        if ((filename = midori_paths_get_res_filename ("gtk3.css")))
        {
            GtkCssProvider* css_provider = gtk_css_provider_new ();
            GError* error = NULL;
            gtk_css_provider_load_from_path (css_provider, filename, &error);
            if (error == NULL)
            {
                gtk_style_context_add_provider_for_screen (
                    gtk_widget_get_screen (GTK_WIDGET (browser)),
                    GTK_STYLE_PROVIDER (css_provider),
                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            }
            else
            {
                g_warning ("Failed to load \"%s\": %s", filename, error->message);
                g_error_free (error);
            }
            g_free (filename);
        }
    }
    #endif

    app->browser = browser;
}

#ifdef HAVE_SIGNAL_H
static MidoriApp* app_singleton;
static void
midori_app_signal_handler (int signal_id)
{
    signal (signal_id, 0);
    if (!midori_paths_is_readonly ())
        midori_app_quit (app_singleton);
    if (kill (getpid (), signal_id))
      exit (1);
}
#endif

static void
_midori_app_quit (MidoriApp* app)
{
    if (!midori_paths_is_readonly ())
    {
        gchar* config_file = midori_paths_get_config_filename_for_writing ("running");
        g_unlink (config_file);
        g_free (config_file);
    }
}

static void
midori_app_class_init (MidoriAppClass* class)
{
    GObjectClass* gobject_class;

    /**
     * MidoriApp::add-browser:
     * @app: the object on which the signal is emitted
     * @browser: a #MidoriBrowser
     *
     * A new browser is being added to the app,
     * see midori_app_add_browser().
     */
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
     * A browser is being removed from the app because it
     * was destroyed.
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

    /**
     * MidoriApp::quit:
     * @app: the object on which the signal is emitted
     * @browser: a #MidoriBrowser
     *
     * The app is being quit, see midori_app_quit().
     */
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
                                     NULL,
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
    * MidoriApp:speed-dial:
    *
    * The speed dial configuration file.
    *
    * Since: 0.3.4
    */
    g_object_class_install_property (gobject_class,
                                     PROP_SPEED_DIAL,
                                     g_param_spec_pointer (
                                     "speed-dial",
                                     "Speeddial",
                                     "Pointer to key-value object with speed dial items",
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
}

static void
midori_app_raise_window (GtkWindow* window,
                         GdkScreen* screen)
{
    if (screen)
        gtk_window_set_screen (window, screen);
    gtk_window_present (window);
    gtk_window_deiconify (window);
}

static void
midori_app_debug_open (MidoriApp*   app,
                       GFile**      files,
                       gint         n_files,
                       const gchar* hint)
{
    if (midori_debug ("app"))
    {
        g_print ("app(%s) open: %d files [",
                 g_application_get_is_remote (G_APPLICATION (app)) ? "send" : "receive",
                 n_files);
        gint i;
        for (i = 0; i < n_files; i++)
        {
            gchar* uri = g_file_get_uri (files[i]);
            g_print ("%s ", uri);
            g_free (uri);
        }
        g_print ("] hint '%s'\n", hint);
    }
}

static void
midori_app_activate_cb (MidoriApp* app,
                        gpointer   user_data)
{
    if (midori_debug ("app"))
        g_print ("app(receive) activate\n");
    if (app->browser)
        midori_app_raise_window (GTK_WINDOW (app->browser), NULL);
}

static void
midori_app_open_cb (MidoriApp* app,
                    GFile**    files,
                    gint       n_files,
                    gchar*     hint,
                    gpointer   user_data)
{
    midori_app_debug_open (app, files, n_files, hint);

    if (!strcmp (hint, "window"))
    {
        MidoriBrowser* browser = midori_app_create_browser (app);
        midori_app_add_browser (app, browser);
        midori_browser_add_uri (browser, "about:home");
        midori_browser_activate_action (browser, "Location");
        gtk_widget_show (GTK_WIDGET (browser));
        midori_app_raise_window (GTK_WINDOW (browser), NULL);
        return;
    }

    if (n_files == 0 && strcmp (hint, ""))
    {
        midori_browser_activate_action (app->browser, hint);
        return;
    }

    MidoriBrowser* browser;
    MidoriNewPage open_external_pages_in;
    gboolean first;

    g_object_get (app->settings, "open-new-pages-in", &open_external_pages_in, NULL);
    if (open_external_pages_in == MIDORI_NEW_PAGE_WINDOW)
    {
        browser = midori_app_create_browser (app);
        midori_app_add_browser (app, browser);
        gtk_widget_show (GTK_WIDGET (browser));
    }
    else
        browser = app->browser;
    midori_app_raise_window (GTK_WINDOW (browser), NULL);

    first = (open_external_pages_in == MIDORI_NEW_PAGE_CURRENT);

    gint i;
    for (i = 0; i < n_files; i++)
    {
        gchar* uri = g_file_get_uri (files[i]);
        if (midori_uri_recursive_fork_protection (uri, FALSE))
        {
            if (first)
            {
                midori_browser_set_current_uri (browser, uri);
                first = FALSE;
            }
            else
            {
                /* Switch to already open tab if possible */
                KatzeArray* items = midori_browser_get_proxy_array (browser);
                KatzeItem* found = katze_array_find_uri (items, uri);
                if (found != NULL)
                    midori_browser_set_current_item (browser, found);
                else
                    midori_browser_set_current_tab (browser,
                        midori_browser_add_uri (browser, uri));
            }
        }
        g_free (uri);
    }
}

static void
midori_app_startup_cb (GApplication* app,
                       gpointer      user_data)
{
    g_signal_connect (app, "activate",
                      G_CALLBACK (midori_app_activate_cb), NULL);
    g_signal_connect (app, "open",
                      G_CALLBACK (midori_app_open_cb), NULL);
}

static void
midori_app_network_changed (GNetworkMonitor* monitor,
                            gboolean         available,
                            MidoriApp*       app)
{
    if (available) 
    {
        MidoriBrowser *browser;
        KATZE_ARRAY_FOREACH_ITEM (browser, app->browsers) {
            GList* tabs = midori_browser_get_tabs (browser);
            for (; tabs != NULL; tabs = g_list_next (tabs))
                if (midori_tab_get_load_error (MIDORI_TAB (tabs->data)) == MIDORI_LOAD_ERROR_NETWORK)
                    midori_view_reload (tabs->data, FALSE);
            g_list_free (tabs);
        }
    }
}

static void
midori_app_create_instance (MidoriApp* app)
{
    if (g_application_get_is_registered (G_APPLICATION (app)))
        return;

    const gchar* config = midori_paths_get_config_dir_for_reading ();
    gchar* config_hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, config, -1);
    gchar* name_hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, app_name, -1);
    katze_assign (app_name, g_strconcat (PACKAGE_NAME,
        "_", config_hash, "_", name_hash, NULL));
    g_free (config_hash);
    g_free (name_hash);
    g_object_notify (G_OBJECT (app), "name");

    GdkDisplay* display = gdk_display_get_default ();
    #ifdef GDK_WINDOWING_X11
    /* On X11: :0 or :0.0 which is equivalent */
    gchar* display_name = g_strndup (gdk_display_get_name (display), 2);
    #else
    gchar* display_name = g_strdup (gdk_display_get_name (display));
    #endif
    g_strdelimit (display_name, ":.\\/", '_');
    gchar* instance_name = g_strdup_printf ("de.twotoasts.%s_%s", app_name, display_name);
    g_free (display_name);
    katze_assign (app_name, instance_name);

    if (midori_debug ("app"))
        g_print ("app registering %s\n", app_name);
    g_object_set (app,
                  "application-id", app_name,
                  "flags", G_APPLICATION_HANDLES_OPEN,
                  NULL);
    g_signal_connect (app, "startup", G_CALLBACK (midori_app_startup_cb), NULL);

    g_signal_connect (g_network_monitor_get_default (), "network-changed",
                       G_CALLBACK (midori_app_network_changed), app);

    GError* error = NULL;
    if (!g_application_register (G_APPLICATION (app), NULL, &error))
        midori_error (error->message);
}

const gchar*
midori_app_get_name (MidoriApp* app)
{
    return app_name;
}

gboolean
midori_app_get_crashed (MidoriApp* app)
{
    static gint cache = -1;

    if (cache != -1)
        return (gboolean) cache;

    if (!midori_paths_is_readonly ())
    {
        /* We test for the presence of a dummy file which is created once
           and deleted during normal runtime, but persists in case of a crash. */
        gchar* config_file = midori_paths_get_config_filename_for_writing ("running");
        gboolean crashed = (g_access (config_file, F_OK) == 0);
        if (!crashed)
            g_file_set_contents (config_file, "RUNNING", -1, NULL);
        g_free (config_file);
        if (crashed) {
            cache = 1;
            return TRUE;
        }
    }

    cache = 0;

    return FALSE;
}

static void
midori_app_init (MidoriApp* app)
{
    #ifdef HAVE_SIGNAL_H
    app_singleton = app;
    #ifdef SIGHUP
    signal (SIGHUP, &midori_app_signal_handler);
    #endif
    #ifdef SIGINT
    signal (SIGINT, &midori_app_signal_handler);
    #endif
    #ifdef SIGTERM
    signal (SIGTERM, &midori_app_signal_handler);
    #endif
    #ifdef SIGQUIT
    signal (SIGQUIT, &midori_app_signal_handler);
    #endif
    #endif

    app->settings = NULL;
    app->bookmarks = NULL;
    app->trash = NULL;
    app->search_engines = NULL;
    app->history = NULL;
    app->speeddial = NULL;
    app->extensions = katze_array_new (KATZE_TYPE_ARRAY);
    app->browsers = katze_array_new (MIDORI_TYPE_BROWSER);

    #ifdef HAVE_LIBNOTIFY
    notify_init (PACKAGE_NAME);
    #endif
}

static void
midori_app_finalize (GObject* object)
{
    MidoriApp* app = MIDORI_APP (object);

    katze_assign (app_name, NULL);
    katze_object_assign (app->settings, NULL);
    katze_object_assign (app->bookmarks, NULL);
    katze_object_assign (app->trash, NULL);
    katze_object_assign (app->search_engines, NULL);
    katze_object_assign (app->history, NULL);
    app->speeddial = NULL;
    katze_object_assign (app->extensions, NULL);
    katze_object_assign (app->browsers, NULL);

    #ifdef HAVE_LIBNOTIFY
    if (notify_is_initted ())
        notify_uninit ();
    #endif

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
        katze_assign (app_name, g_value_dup_string (value));
        break;
    case PROP_SETTINGS:
        katze_object_assign (app->settings, g_value_dup_object (value));
        break;
    case PROP_BOOKMARKS:
        katze_object_assign (app->bookmarks, g_value_dup_object (value));
        break;
    case PROP_TRASH:
        katze_object_assign (app->trash, g_value_dup_object (value));
        break;
    case PROP_SEARCH_ENGINES:
        katze_object_assign (app->search_engines, g_value_dup_object (value));
        break;
    case PROP_HISTORY:
        katze_object_assign (app->history, g_value_dup_object (value));
        break;
    case PROP_SPEED_DIAL:
        app->speeddial = g_value_get_pointer (value);
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
        g_value_set_string (value, app_name);
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
    case PROP_SPEED_DIAL:
        g_value_set_pointer (value, app->speeddial);
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
 * Return value: a new #MidoriApp
 **/
MidoriApp*
midori_app_new (const gchar* name)
{
    return g_object_new (MIDORI_TYPE_APP, "name", name, NULL);
}

/**
 * midori_app_new_proxy:
 * @app: a #MidoriApp, or %NULL
 *
 * Instantiates a proxy #MidoriApp that can be passed to untrusted code
 * or for sensitive use cases. Properties can be freely changed.
 *
 * Return value: a new #MidoriApp
 *
 * Since: 0.5.0
 **/
MidoriApp*
midori_app_new_proxy (MidoriApp* app)
{
    g_return_val_if_fail (MIDORI_IS_APP (app) || !app, NULL);

    return midori_app_new (NULL);
}

static gboolean instance_is_not_running = FALSE;
static gboolean instance_is_running = FALSE;

void
midori_app_set_instance_is_running (gboolean is_running)
{
    instance_is_not_running = !is_running;
    instance_is_running = is_running;
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

    if (instance_is_not_running)
        return FALSE;
    else if (instance_is_running)
        return TRUE;

    midori_app_create_instance (app);
    return g_application_get_is_remote (G_APPLICATION (app));
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
    g_return_val_if_fail (MIDORI_IS_APP (app), FALSE);
    g_return_val_if_fail (midori_app_instance_is_running (app), FALSE);

    if (midori_debug ("app"))
        g_print ("app(send) activate\n");
    g_application_activate (G_APPLICATION (app));
    return TRUE;
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
    g_return_val_if_fail (MIDORI_IS_APP (app), FALSE);
    g_return_val_if_fail (midori_app_instance_is_running (app), FALSE);

    midori_app_debug_open (app, NULL, -1, "window");
    g_application_open (G_APPLICATION (app), NULL, -1, "window");
    return TRUE;
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
    g_return_val_if_fail (MIDORI_IS_APP (app), FALSE);
    g_return_val_if_fail (midori_app_instance_is_running (app), FALSE);
    g_return_val_if_fail (uris != NULL, FALSE);

    gint n_files = g_strv_length (uris);
    GFile** files = g_new (GFile*, n_files);
    /* Encode URLs to avoid GFile treating them wrongly */
    int i;
    for (i = 0; i < n_files; i++)
    {
        gchar* new_uri = sokoke_magic_uri (uris[i], TRUE, TRUE);
        files[i] = g_file_new_for_uri (new_uri);
        g_free (new_uri);
    }
    midori_app_debug_open (app, files, n_files, "");
    g_application_open (G_APPLICATION (app), files, n_files, "");
    return TRUE;
}

/**
 * midori_app_send_command:
 * @app: a #MidoriApp
 * @command: a string vector of a command to execute
 *
 * Sends a command to an instance of Midori, which
 * is either the current process or an already running
 * instance with the same name on the default display.
 *
 * Names of GtkAction objects of MidoriBrowser are recognized as commands.
 *
 * Return value: %TRUE if the message was sent successfully
 *
 * Since: 0.1.8
 **/
gboolean
midori_app_send_command (MidoriApp* app,
                         gchar**    command)
{
    g_return_val_if_fail (MIDORI_IS_APP (app), FALSE);
    g_return_val_if_fail (command != NULL, FALSE);

    if (!midori_app_instance_is_running (app))
    {
        MidoriBrowser* browser = midori_browser_new ();
        int i;
        for (i = 0; command && command[i]; i++)
            midori_browser_assert_action (browser, command[i]);
        gtk_widget_destroy (GTK_WIDGET (browser));
    }

    gint n_files = g_strv_length (command);
    int i;
    for (i = 0; i < n_files; i++)
    {
        midori_app_debug_open (app, NULL, 0, command[i]);
        g_application_open (G_APPLICATION (app), NULL, 0, command[i]);
    }
    return TRUE;
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

void
midori_app_set_browsers (MidoriApp*     app,
                         KatzeArray*    browsers,
                         MidoriBrowser* browser)
{
    g_return_if_fail (MIDORI_IS_APP (app));
    g_return_if_fail (KATZE_IS_ARRAY (browsers));
    katze_object_assign (app->browsers, g_object_ref (browsers));
    app->browser = browser;
}

/**
 * midori_app_create_browser:
 * @app: a #MidoriApp
 *
 * Creates a #MidoriBrowser which inherits its settings,
 * bookmarks, trash, search engines and history from  @app.
 *
 * Note that creating a browser this way can be a lot
 * faster than setting it up manually.
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
                         "speed-dial", app->speeddial,
                         NULL);
}

/**
 * midori_app_get_browsers:
 * @app: a #MidoriApp
 *
 * Retrieves the browsers as a list.
 *
 * Return value: (transfer container) (element-type Midori.Browser): a newly allocated #Glist of #MidoriBrowser
 *
 * Since: 0.2.5
 **/
GList*
midori_app_get_browsers (MidoriApp* app)
{
    g_return_val_if_fail (MIDORI_IS_APP (app), NULL);

    return katze_array_get_items (app->browsers);
}

/**
 * midori_app_get_browser:
 * @app: a #MidoriApp
 *
 * Determines the current browser, which is the one that was
 * last focussed.
 *
 * Return value: the current #MidoriBrowser
 *
 * Since: 0.2.5
 **/
MidoriBrowser*
midori_app_get_browser (MidoriApp* app)
{
    g_return_val_if_fail (MIDORI_IS_APP (app), NULL);

    return app->browser;
}

/**
 * midori_app_quit:
 * @app: a #MidoriApp
 *
 * Quits the #MidoriApp.
 *
 * Since 0.1.2 the "quit" signal is always emitted before quitting.
 **/
void
midori_app_quit (MidoriApp* app)
{
    g_return_if_fail (MIDORI_IS_APP (app));

    g_signal_emit (app, signals[QUIT], 0);
}

/**
 * midori_app_send_notification:
 * @app: a #MidoriApp
 * @title: title of the notification
 * @message: text of the notification, or NULL
 *
 * Send #message to a notification service to display it.
 *
 * There is no guarantee that the message has been sent and displayed, as
 * there might not be any notification service available.
 *
 * Since 0.1.7
 **/
void
midori_app_send_notification (MidoriApp*   app,
                              const gchar* title,
                              const gchar* message)
{
    g_return_if_fail (MIDORI_IS_APP (app));
    g_return_if_fail (title);

    #ifdef HAVE_LIBNOTIFY
    if (notify_is_initted ())
    {
        #if NOTIFY_CHECK_VERSION (0, 7, 0)
        NotifyNotification* note = notify_notification_new (title, message, "midori");
        #else
        NotifyNotification* note = notify_notification_new (title, message, "midori", NULL);
        #endif
        notify_notification_show (note, NULL);
        g_object_unref (note);
    }
    #elif !defined(G_OS_WIN32)
    GNotification* notification = g_notification_new (title);
    g_notification_set_body (notification, message);
    GIcon* icon = g_themed_icon_new ("midori");
    g_notification_set_icon (notification, icon);
    g_object_unref (icon);
    g_application_send_notification (G_APPLICATION (app), NULL, notification);
    g_object_unref (notification);
    #endif
}

/**
 * midori_app_setup:
 *
 * Saves the argument vector, initializes threading and registers
 * several custom stock items and prepares localization.
 *
 * Since: 0.4.2
 **/
void
midori_app_setup (gint               *argc,
                  gchar**            *argument_vector,
                  const GOptionEntry *entries)
{

    GtkIconSource* icon_source;
    GtkIconSet* icon_set;
    GtkIconFactory* factory;
    gsize i;
    GError* error = NULL;
    gboolean success;

    static GtkStockItem items[] =
    {
        { "network-error" },
        { "network-idle" },
        { STOCK_IMAGE },
        { MIDORI_STOCK_WEB_BROWSER },
        { STOCK_NEWS_FEED },
        { STOCK_STYLE },

        { STOCK_BOOKMARKS,    N_("_Bookmarks"), GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KEY_B },
        { STOCK_BOOKMARK_ADD, N_("Add Boo_kmark") },
        { STOCK_EXTENSION,    N_("_Extensions") },
        { STOCK_HISTORY,      N_("_History"), GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KEY_H },
        { STOCK_SCRIPT,       N_("_Userscripts") },
        { STOCK_STYLE,        N_("User_styles") },
        { STOCK_TAB_NEW,      N_("New _Tab") },
        { MIDORI_STOCK_TRANSFER,     N_("_Transfers"), GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KEY_J },
        { MIDORI_STOCK_PLUGINS,      N_("Netscape p_lugins") },
        { STOCK_USER_TRASH,   N_("_Closed Tabs") },
        { STOCK_WINDOW_NEW,   N_("New _Window") },
        { STOCK_FOLDER_NEW,   N_("New _Folder") },
    };

    /* Print messages to stdout on Win32 console, cf. AbiWord
     * http://svn.abisource.com/abiword/trunk/src/wp/main/win/Win32Main.cpp */
    #ifdef _WIN32
    if (fileno (stdout) != -1
    && _get_osfhandle (fileno (stdout)) != -1)
    {
        /* stdout is already being redirected to a file */
    }
    else
    {
        typedef BOOL (WINAPI *AttachConsole_t) (DWORD);
        AttachConsole_t p_AttachConsole =
            (AttachConsole_t) GetProcAddress (GetModuleHandle ("kernel32.dll"), "AttachConsole");
        if (p_AttachConsole != NULL && p_AttachConsole (ATTACH_PARENT_PROCESS))
        {
            freopen ("CONOUT$", "w", stdout);
            dup2 (fileno (stdout), 1);
            freopen ("CONOUT$", "w", stderr);
            dup2 (fileno (stderr), 2);
        }
    }
    #endif

    /* Midori.Paths uses GFile */
    g_type_init ();
    /* Preserve argument vector */
    midori_paths_init_exec_path (*argument_vector, *argc);

    #ifdef G_OS_WIN32
    {
	gchar* exec_dir = g_win32_get_package_installation_directory_of_module (NULL);
	gchar* dr_mingw_dll = g_build_filename (exec_dir, "bin", "exchndl.dll", NULL);

	if (g_file_test (dr_mingw_dll, G_FILE_TEST_EXISTS)) {
	    LoadLibrary(dr_mingw_dll);
	}
	g_free (exec_dir);
	g_free (dr_mingw_dll);
    }
    #endif

    #if ENABLE_NLS
    if (g_getenv ("MIDORI_NLSPATH"))
        bindtextdomain (GETTEXT_PACKAGE, g_getenv ("MIDORI_NLSPATH"));
    else
    #ifdef G_OS_WIN32
    {
        gchar* path = midori_paths_get_data_filename ("locale", FALSE);
        bindtextdomain (GETTEXT_PACKAGE, path);
        g_free (path);
    }
    #else
        bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    #endif
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    #endif

    #if GTK_CHECK_VERSION (3, 0, 0)
    success = gtk_init_with_args (argc, argument_vector, _("[Addresses]"),
                                  entries, GETTEXT_PACKAGE, &error);
    #else
    success = gtk_init_with_args (argc, argument_vector, _("[Addresses]"),
                                  (GOptionEntry*)entries, GETTEXT_PACKAGE, &error);
    #endif

    factory = gtk_icon_factory_new ();
    for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
        icon_set = gtk_icon_set_new ();
        icon_source = gtk_icon_source_new ();
        gtk_icon_source_set_icon_name (icon_source, items[i].stock_id);
        gtk_icon_set_add_source (icon_set, icon_source);
        gtk_icon_source_free (icon_source);
        gtk_icon_factory_add (factory, items[i].stock_id, icon_set);
        gtk_icon_set_unref (icon_set);
    }
    gtk_stock_add_static ((GtkStockItem*)items, G_N_ELEMENTS (items));
    gtk_icon_factory_add_default (factory);
    g_object_unref (factory);

    if (!success)
        midori_error (error->message);
}

void
midori_error (const gchar* format,
              ...)
{
    g_printerr ("%s - ", g_get_application_name ());
    va_list args;
    va_start (args, format);
    g_vfprintf (stderr, format, args);
    va_end (args);
    g_printerr ("\n");
    exit (1);
}

gboolean
midori_debug (const gchar* token)
{
    static const gchar* debug_token = NULL;
    const gchar* debug_tokens = "wk2:no-multi-render-process adblock:match adblock:parse adblock:time adblock:element adblock:css startup headers body referer cookies paths hsts unarmed db:bookmarks db:history db:tabby mouse app database addons:match ";
    if (debug_token == NULL)
    {
        gchar* found_token;
        const gchar* debug = g_getenv ("MIDORI_DEBUG");
        const gchar* legacy_touchscreen = g_getenv ("MIDORI_TOUCHSCREEN");
        if (legacy_touchscreen && *legacy_touchscreen)
            g_warning ("MIDORI_TOUCHSCREEN is obsolete: "
                "GTK+ 3.4 enables touchscreens automatically, "
                "older GTK+ versions aren't supported as of Midori 0.4.9");
        if (debug && (found_token = strstr (debug_tokens, debug)) && *(found_token + strlen (debug)) == ' ')
            debug_token = g_intern_static_string (debug);
        else if (debug)
            g_warning ("Unrecognized value '%s' for MIDORI_DEBUG.", debug);
        else
            debug_token = "NONE";
        if (!debug_token)
        {
            debug_token = "INVALID";
            g_print ("Supported values: %s\n", debug_tokens);
        }
    }
    if (debug_token != g_intern_static_string ("NONE")
     && !strstr (debug_tokens, token))
        g_warning ("Token '%s' passed to midori_debug is not a known token.", token);
    return debug_token == g_intern_static_string (token);
}

