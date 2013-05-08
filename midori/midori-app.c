/*
 Copyright (C) 2008-2012 Christian Dywan <christian@twotoasts.de>

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

#if HAVE_UNIQUE
    typedef gpointer MidoriAppInstance;
    #define MidoriAppInstanceNull NULL
    #if defined(G_DISABLE_DEPRECATED) && !defined(G_CONST_RETURN)
        #define G_CONST_RETURN
    #endif
    #include <unique/unique.h>
    #ifdef G_DISABLE_DEPRECATED
        #undef G_CONST_RETUTN
    #endif
    #define MIDORI_UNIQUE_COMMAND 1
#else
    typedef gint MidoriAppInstance;
    #define MidoriAppInstanceNull -1
    #include "socket.h"
#endif

#if HAVE_LIBNOTIFY
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
    GObject parent_instance;

    MidoriWebSettings* settings;
    KatzeArray* bookmarks;
    KatzeArray* trash;
    KatzeArray* search_engines;
    KatzeArray* history;
    GKeyFile* speeddial;
    KatzeArray* extensions;
    KatzeArray* browsers;

    MidoriBrowser* browser;
    MidoriAppInstance instance;

    #if !HAVE_LIBNOTIFY
    gchar* program_notify_send;
    #endif
};

static gchar* app_name = NULL;

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
    #if HAVE_UNIQUE
    /* We *do not* let unique watch windows because that includes
        bringing windows in the foreground, even from other workspaces.
    if (app->instance)
        unique_app_watch_window (app->instance, GTK_WINDOW (browser)); */
    #endif
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
    gtk_window_set_screen (window, screen);
    gtk_window_present (window);
    gtk_window_deiconify (window);
}

static gboolean
midori_app_command_received (MidoriApp*   app,
                             const gchar* command,
                             gchar**      uris,
                             GdkScreen*   screen)
{
    if (!screen)
    {
        if (app->browser && gtk_widget_has_screen (GTK_WIDGET (app->browser)))
            screen = gtk_widget_get_screen (GTK_WIDGET (app->browser));
        else
            screen = gdk_screen_get_default ();
    }

    if (g_str_equal (command, "activate"))
    {
        if (!app->browser)
            return FALSE;

        midori_app_raise_window (GTK_WINDOW (app->browser), screen);
        return TRUE;
    }
    else if (g_str_equal (command, "new"))
    {
        MidoriBrowser* browser = midori_app_create_browser (app);
        midori_app_add_browser (app, browser);
        midori_browser_add_uri (browser, "about:home");
        midori_browser_activate_action (browser, "Location");
        gtk_widget_show (GTK_WIDGET (browser));
        midori_app_raise_window (GTK_WINDOW (browser), screen);
        return TRUE;
    }
    else if (g_str_equal (command, "open"))
    {
        if (!app->browser)
            return FALSE;

        if (!uris)
            return FALSE;
        else
        {
            MidoriBrowser* browser;
            MidoriNewPage open_external_pages_in;
            gboolean first;

            g_object_get (app->settings, "open-new-pages-in",
                          &open_external_pages_in, NULL);
            if (open_external_pages_in == MIDORI_NEW_PAGE_WINDOW)
            {
                browser = midori_app_create_browser (app);
                midori_app_add_browser (app, browser);
                gtk_widget_show (GTK_WIDGET (browser));
            }
            else
                browser = app->browser;

            midori_app_raise_window (GTK_WINDOW (browser), screen);

            first = (open_external_pages_in == MIDORI_NEW_PAGE_CURRENT);
            while (*uris)
            {
                gchar* fixed_uri = g_uri_unescape_string (*uris, NULL);
                if (sokoke_recursive_fork_protection (fixed_uri, FALSE))
                {
                    if (first)
                    {
                        midori_browser_set_current_uri (browser, fixed_uri);
                        first = FALSE;
                    }
                    else
                    {
                        /* Switch to already open tab if possible */
                        KatzeArray* items = midori_browser_get_proxy_array (browser);
                        KatzeItem* found = katze_array_find_uri (items, fixed_uri);
                        if (found != NULL)
                            midori_browser_set_current_item (browser, found);
                        else
                            midori_browser_set_current_tab (browser,
                                midori_browser_add_uri (browser, fixed_uri));
                    }
                }
                g_free (fixed_uri);
                uris++;
            }
            return TRUE;
        }
    }
    else if (g_str_equal (command, "command"))
    {
        if (!uris || !app->browser)
            return FALSE;
        gint i;
        for (i = 0; uris && uris[i]; i++)
            midori_browser_activate_action (app->browser, uris[i]);
        return TRUE;
    }

    return FALSE;
}

#if HAVE_UNIQUE
static UniqueResponse
midori_browser_message_received_cb (UniqueApp*         instance,
                                    gint               command,
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
  case MIDORI_UNIQUE_COMMAND:
  {
      gchar** uris = unique_message_data_get_uris (message);
      success = midori_app_command_received (app, "command", uris, screen);
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
        else if (strncmp (buf, "command", 7) == 0)
        {
            guint i = 0;
            gchar** uris = g_new (gchar*, 100);
            while (fd_gets (sock, buf, sizeof (buf)) != -1 && *buf != '.')
            {
                uris[i++] = g_strdup (g_strstrip (buf));
                if (i == 99)
                    break;
            }
            uris[i] = NULL;
            midori_app_command_received (app, "command", uris, screen);
            g_strfreev (uris);
        }
    }

    fd_close (sock);

    return TRUE;
}
#endif

static MidoriAppInstance
midori_app_create_instance (MidoriApp* app)
{
    MidoriAppInstance instance;
    GdkDisplay* display;
    gchar* display_name;
    gchar* instance_name;
    #if !HAVE_UNIQUE
    gboolean exists;
    GIOChannel* channel;
    #endif

    if (!(display = gdk_display_get_default ()))
        return MidoriAppInstanceNull;

    {
        #if HAVE_UNIQUE
        const gchar* config = midori_paths_get_config_dir_for_reading ();
        gchar* config_hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, config, -1);
        gchar* name_hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, app_name, -1);
        katze_assign (app_name, g_strconcat (PACKAGE_NAME,
            "_", config_hash, "_", name_hash, NULL));
        g_free (config_hash);
        g_free (name_hash);
        #else
        katze_assign (app_name, g_strdup (PACKAGE_NAME));
        #endif
        g_object_notify (G_OBJECT (app), "name");
    }

    #ifdef GDK_WINDOWING_X11
    /* On X11: :0 or :0.0 which is equivalent */
    display_name = g_strndup (gdk_display_get_name (display), 2);
    #else
    display_name = g_strdup (gdk_display_get_name (display));
    #endif
    g_strdelimit (display_name, ":.\\/", '_');
    instance_name = g_strdup_printf ("de.twotoasts.%s_%s", app_name, display_name);
    g_free (display_name);
    katze_assign (app_name, instance_name);

    #if HAVE_UNIQUE
    instance = unique_app_new (instance_name, NULL);
    unique_app_add_command (instance, "midori-command", MIDORI_UNIQUE_COMMAND);
    g_signal_connect (instance, "message-received",
                      G_CALLBACK (midori_browser_message_received_cb), app);
    #else
    instance = socket_init (instance_name, midori_paths_get_config_dir_for_writing (), &exists);
    g_object_set_data (G_OBJECT (app), "sock-exists",
        exists ? (gpointer)0xdeadbeef : NULL);
    if (instance != MidoriAppInstanceNull)
    {
        channel = g_io_channel_unix_new (instance);
        g_io_add_watch (channel, G_IO_IN | G_IO_PRI | G_IO_ERR,
            (GIOFunc)midori_app_io_channel_watch_cb, app);
    }
    #endif
    return instance;
}

const gchar*
midori_app_get_name (MidoriApp* app)
{
    return app_name;
}

gboolean
midori_app_get_crashed (MidoriApp* app)
{
    if (!midori_paths_is_readonly ())
    {
        /* We test for the presence of a dummy file which is created once
           and deleted during normal runtime, but persists in case of a crash. */
        gchar* config_file = midori_paths_get_config_filename_for_writing ("running");
        gboolean crashed = (g_access (config_file, F_OK) == 0);
        if (!crashed)
            g_file_set_contents (config_file, "RUNNING", -1, NULL);
        g_free (config_file);
        if (crashed)
            return TRUE;
    }

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

    app->instance = MidoriAppInstanceNull;

    #if HAVE_LIBNOTIFY
    notify_init (PACKAGE_NAME);
    #else
    app->program_notify_send = g_find_program_in_path ("notify-send");
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

    #if HAVE_UNIQUE
    katze_object_assign (app->instance, NULL);
    #else
    sock_cleanup ();
    #endif

    #if HAVE_LIBNOTIFY
    if (notify_is_initted ())
        notify_uninit ();
    #else
        katze_assign (app->program_notify_send, NULL);
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

    if (app->instance == MidoriAppInstanceNull)
        app->instance = midori_app_create_instance (app);

    #if HAVE_UNIQUE
    return app->instance && unique_app_is_running (app->instance);
    #else
    return g_object_get_data (G_OBJECT (app), "sock-exists") != NULL;
    #endif
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

    #if HAVE_UNIQUE
    if (app->instance)
    {
        UniqueResponse response = unique_app_send_message (app->instance, UNIQUE_ACTIVATE, NULL);
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
    g_return_val_if_fail (MIDORI_IS_APP (app), FALSE);
    g_return_val_if_fail (midori_app_instance_is_running (app), FALSE);

    #if HAVE_UNIQUE
    if (app->instance)
    {
        UniqueResponse response = unique_app_send_message (app->instance, UNIQUE_NEW, NULL);
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
    g_return_val_if_fail (MIDORI_IS_APP (app), FALSE);
    g_return_val_if_fail (midori_app_instance_is_running (app), FALSE);
    g_return_val_if_fail (uris != NULL, FALSE);

    #if HAVE_UNIQUE
    if (app->instance)
    {
        UniqueMessageData* message;
        UniqueResponse response;
        /* Encode any IDN addresses because libUnique doesn't like them */
        int i = 0;
        while (uris[i] != NULL)
        {
            gchar* new_uri = sokoke_magic_uri (uris[i], TRUE, TRUE);
            gchar* escaped_uri = g_uri_escape_string (new_uri, NULL, FALSE);
            g_free (new_uri);
            katze_assign (uris[i], escaped_uri);
            i++;
        }

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
        for (i=0; command && command[i]; i++)
            midori_browser_assert_action (browser, command[i]);
        gtk_widget_destroy (GTK_WIDGET (browser));
        return midori_app_command_received (app, "command", command, NULL);
    }

    #if HAVE_UNIQUE
    if (app->instance)
    {
        UniqueResponse response;
        UniqueMessageData* message = unique_message_data_new ();
        unique_message_data_set_uris (message, command);
        response = unique_app_send_message (app->instance,
            MIDORI_UNIQUE_COMMAND, message);
        unique_message_data_free (message);
        if (response == UNIQUE_RESPONSE_OK)
            return TRUE;
    }
    #else
    if (app->instance > -1)
    {
        send_open_command (app->instance, "command", command);
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
 * Return value: a newly allocated #Glist of #MidoriBrowser
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

    #if HAVE_LIBNOTIFY
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
    #else
    /* Fall back to the command line program "notify-send" */
    if (app->program_notify_send)
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

    /* libSoup uses threads, therefore if WebKit is built with libSoup
     * or Midori is using it, we need to initialize threads. */
    #if !GLIB_CHECK_VERSION (2, 32, 0)
    if (!g_thread_supported ()) g_thread_init (NULL);
    #endif

    /* Midori.Paths uses GFile */
    g_type_init ();
    /* Preserve argument vector */
    midori_paths_init_exec_path (*argument_vector, *argc);

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

    #ifdef HAVE_GRANITE_CLUTTER
    success = gtk_clutter_init_with_args (argc, argument_vector, _("[Addresses]"),
                                          (GOptionEntry*)entries, GETTEXT_PACKAGE, &error);
    #elif GTK_CHECK_VERSION (3, 0, 0)
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
    const gchar* debug_tokens = "headers body referer cookies paths hsts unarmed bookmarks ";
    const gchar* full_debug_tokens = "adblock:match adblock:time startup ";
    if (debug_token == NULL)
    {
        gchar* found_token;
        const gchar* debug = g_getenv ("MIDORI_DEBUG");
        const gchar* legacy_touchscreen = g_getenv ("MIDORI_TOUCHSCREEN");
        if (legacy_touchscreen && *legacy_touchscreen)
            g_warning ("MIDORI_TOUCHSCREEN is obsolete: "
                "GTK+ 3.4 enables touchscreens automatically, "
                "older GTK+ versions aren't supported as of Midori 0.4.9");
        if (debug && (found_token = strstr (full_debug_tokens, debug)) && *(found_token + strlen (debug)) == ' ')
        {
            #ifdef G_ENABLE_DEBUG
            debug_token = g_intern_static_string (debug);
            #else
            g_warning ("Value '%s' for MIDORI_DEBUG requires a full debugging build.", debug);
            #endif
        }
        else if (debug && (found_token = strstr (debug_tokens, debug)) && *(found_token + strlen (debug)) == ' ')
            debug_token = g_intern_static_string (debug);
        else if (debug)
            g_warning ("Unrecognized value '%s' for MIDORI_DEBUG.", debug);
        else
            debug_token = "NONE";
        if (!debug_token)
        {
            debug_token = "INVALID";
            g_print ("Supported values: %s\nWith full debugging: %s\n",
                     debug_tokens, full_debug_tokens);
        }
    }
    if (debug_token != g_intern_static_string ("NONE")
     && !strstr (debug_tokens, token) && !strstr (full_debug_tokens, token))
        g_warning ("Token '%s' passed to midori_debug is not a known token.", token);
    return debug_token == g_intern_static_string (token);
}

