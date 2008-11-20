/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

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

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#if HAVE_UNIQUE
    #include <unique/unique.h>
#endif

struct _MidoriApp
{
    GObject parent_instance;

    GList* browsers;
    MidoriBrowser* browser;
    GtkAccelGroup* accel_group;

    MidoriWebSettings* settings;
    KatzeArray* bookmarks;
    KatzeArray* trash;
    KatzeArray* search_engines;
    KatzeArray* history;
    KatzeArray* extensions;

    gpointer instance;
};

G_DEFINE_TYPE (MidoriApp, midori_app, G_TYPE_OBJECT)

static MidoriApp* _midori_app_singleton = NULL;

enum
{
    PROP_0,

    PROP_SETTINGS,
    PROP_BOOKMARKS,
    PROP_TRASH,
    PROP_SEARCH_ENGINES,
    PROP_HISTORY,
    PROP_EXTENSIONS,
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
                              const gchar*   uri,
                              MidoriApp*     app)
{
    MidoriBrowser* new_browser = g_object_new (MIDORI_TYPE_BROWSER,
                                               "settings", app->settings,
                                               "bookmarks", app->bookmarks,
                                               "trash", app->trash,
                                               "search-engines", app->search_engines,
                                               "history", app->history,
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
    app->browsers = g_list_remove (app->browsers, browser);
    if (g_list_nth (app->browsers, 0))
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

void
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

    app->browsers = g_list_prepend (app->browsers, browser);

    #if HAVE_UNIQUE
    if (app->instance)
        unique_app_watch_window (app->instance, GTK_WINDOW (browser));
    #endif
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
    gobject_class->constructor = midori_app_constructor;
    gobject_class->finalize = midori_app_finalize;
    gobject_class->set_property = midori_app_set_property;
    gobject_class->get_property = midori_app_get_property;

    class->add_browser = _midori_app_add_browser;
    class->quit = midori_app_quit;

    g_object_class_install_property (gobject_class,
                                     PROP_SETTINGS,
                                     g_param_spec_object (
                                     "settings",
                                     "Settings",
                                     "The associated settings",
                                     MIDORI_TYPE_WEB_SETTINGS,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_BOOKMARKS,
                                     g_param_spec_object (
                                     "bookmarks",
                                     "Bookmarks",
                                     "The bookmarks folder, containing all bookmarks",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_TRASH,
                                     g_param_spec_object (
                                     "trash",
                                     "Trash",
                                     "The trash, collecting recently closed tabs and windows",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SEARCH_ENGINES,
                                     g_param_spec_object (
                                     "search-engines",
                                     "Search Engines",
                                     "The list of search engines",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_HISTORY,
                                     g_param_spec_object (
                                     "history",
                                     "History",
                                     "The list of history items",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_EXTENSIONS,
                                     g_param_spec_object (
                                     "extensions",
                                     "Extensions",
                                     "The list of extensions",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_BROWSER,
                                     g_param_spec_object (
                                     "browser",
                                     "Browser",
                                     "The current browser",
                                     MIDORI_TYPE_BROWSER,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_BROWSER_COUNT,
                                     g_param_spec_uint (
                                     "browser-count",
                                     "Browser Count",
                                     "The current number of browsers",
                                     0, G_MAXUINT, 0,
                                     G_PARAM_READABLE));
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

#if HAVE_UNIQUE
static UniqueResponse
midori_browser_message_received_cb (UniqueApp*         instance,
                                    UniqueCommand      command,
                                    UniqueMessageData* message,
                                    guint              time,
                                    MidoriApp*         app)
{
  UniqueResponse response;
  MidoriBrowser* browser;
  gchar** uris;
  MidoriNewPage open_external_pages_in;
  gboolean first;

  switch (command)
  {
  case UNIQUE_ACTIVATE:
      gtk_window_set_screen (GTK_WINDOW (app->browser),
                             unique_message_data_get_screen (message));
      gtk_window_present (GTK_WINDOW (app->browser));
      response = UNIQUE_RESPONSE_OK;
      break;
  case UNIQUE_NEW:
      browser = g_object_new (MIDORI_TYPE_BROWSER,
                              "settings", app->settings,
                              "bookmarks", app->bookmarks,
                              "trash", app->trash,
                              "search-engines", app->search_engines,
                              "history", app->history,
                              NULL);
      /* FIXME: Should open the homepage according to settings */
      midori_browser_add_uri (browser, "");
      midori_browser_activate_action (browser, "Location");
      midori_app_add_browser (app, browser);
      gtk_window_set_screen (GTK_WINDOW (app->browser),
                             unique_message_data_get_screen (message));
      gtk_widget_show (GTK_WIDGET (browser));
      response = UNIQUE_RESPONSE_OK;
      break;
  case UNIQUE_OPEN:
      gtk_window_set_screen (GTK_WINDOW (app->browser),
                             unique_message_data_get_screen (message));
      gtk_window_present (GTK_WINDOW (app->browser));
      uris = unique_message_data_get_uris (message);
      if (!uris)
          response = UNIQUE_RESPONSE_FAIL;
      else
      {
          g_object_get (app->settings, "open-external-pages-in",
              &open_external_pages_in, NULL);
          if (open_external_pages_in == MIDORI_NEW_PAGE_WINDOW)
          {
              browser = g_object_new (MIDORI_TYPE_BROWSER,
                  "settings", app->settings,
                  "bookmarks", app->bookmarks,
                  "trash", app->trash,
                  "search-engines", app->search_engines,
                  "history", app->history,
                  NULL);
              midori_app_add_browser (app, browser);
              gtk_window_set_screen (GTK_WINDOW (app->browser),
                                     unique_message_data_get_screen (message));
              gtk_widget_show (GTK_WIDGET (browser));
          }
          else
              browser = app->browser;
          first = (open_external_pages_in == MIDORI_NEW_PAGE_CURRENT);
          while (*uris)
          {
              if (first)
              {
                  midori_browser_set_current_uri (browser, *uris);
                  first = FALSE;
              }
              else
                  midori_browser_set_current_page (browser,
                      midori_browser_add_uri (browser, *uris));
              uris++;
          }
          /* g_strfreev (uris); */
          response = UNIQUE_RESPONSE_OK;
      }
      break;
  default:
      response = UNIQUE_RESPONSE_FAIL;
      break;
  }

  return response;
}
#endif

static void
midori_app_init (MidoriApp* app)
{
    #if HAVE_UNIQUE
    gchar* display_name;
    gchar* instance_name;
    guint i, n;
    #endif

    g_assert (!_midori_app_singleton);

    _midori_app_singleton = app;

    app->accel_group = gtk_accel_group_new ();

    app->settings = midori_web_settings_new ();
    app->bookmarks = NULL;
    app->trash = NULL;
    app->search_engines = NULL;
    app->history = NULL;
    app->extensions = NULL;

    #if HAVE_UNIQUE
    display_name = g_strdup (gdk_display_get_name (gdk_display_get_default ()));
    n = strlen (display_name);
    for (i = 0; i < n; i++)
        if (display_name[i] == ':' || display_name[i] == '.')
            display_name[i] = '_';
    instance_name = g_strdup_printf ("de.twotoasts.midori_%s", display_name);
    app->instance = unique_app_new (instance_name, NULL);
    g_free (instance_name);
    g_free (display_name);
    g_signal_connect (app->instance, "message-received",
                      G_CALLBACK (midori_browser_message_received_cb), app);
    #else
    app->instance = NULL;
    #endif
}

static void
midori_app_finalize (GObject* object)
{
    MidoriApp* app = MIDORI_APP (object);

    g_list_free (app->browsers);
    g_object_unref (app->accel_group);

    katze_object_assign (app->settings, NULL);
    katze_object_assign (app->bookmarks, NULL);
    katze_object_assign (app->trash, NULL);
    katze_object_assign (app->search_engines, NULL);
    katze_object_assign (app->history, NULL);
    katze_object_assign (app->extensions, NULL);

    katze_object_assign (app->instance, NULL);

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
    case PROP_BROWSER:
        g_value_set_object (value, app->browser);
        break;
    case PROP_BROWSER_COUNT:
        g_value_set_uint (value, g_list_length (app->browsers));
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
 * If Midori was built without single instance support
 * this function will always return %FALSE.
 *
 * Return value: %TRUE if an instance is already running
 **/
gboolean
midori_app_instance_is_running (MidoriApp* app)
{
    g_return_val_if_fail (MIDORI_IS_APP (app), FALSE);

    #if HAVE_UNIQUE
    return unique_app_is_running (app->instance);
    #else
    return FALSE;
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
    #if HAVE_UNIQUE
    UniqueResponse response;
    #endif

    g_return_val_if_fail (MIDORI_IS_APP (app), FALSE);
    g_return_val_if_fail (midori_app_instance_is_running (app), FALSE);

    #if HAVE_UNIQUE
    response = unique_app_send_message (app->instance, UNIQUE_ACTIVATE, NULL);
    if (response == UNIQUE_RESPONSE_OK)
        return TRUE;
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

    g_return_val_if_fail (MIDORI_IS_APP (app), FALSE);
    g_return_val_if_fail (midori_app_instance_is_running (app), FALSE);

    #if HAVE_UNIQUE
    response = unique_app_send_message (app->instance, UNIQUE_NEW, NULL);
    if (response == UNIQUE_RESPONSE_OK)
        return TRUE;
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

    g_return_val_if_fail (MIDORI_IS_APP (app), FALSE);
    g_return_val_if_fail (midori_app_instance_is_running (app), FALSE);
    g_return_val_if_fail (uris != NULL, FALSE);

    #if HAVE_UNIQUE
    message = unique_message_data_new ();
    unique_message_data_set_uris (message, uris);
    response = unique_app_send_message (app->instance, UNIQUE_OPEN, message);
    unique_message_data_free (message);
    if (response == UNIQUE_RESPONSE_OK)
        return TRUE;
    #endif
    return FALSE;
}

/**
 * midori_app_add_browser:
 * @app: a #MidoriApp
 * @browser: a #MidoriBrowser
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
    g_return_if_fail (MIDORI_IS_APP (app));
    g_return_if_fail (MIDORI_IS_BROWSER (browser));

    g_signal_emit (app, signals[ADD_BROWSER], 0, browser);
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
