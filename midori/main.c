/*
 Copyright (C) 2007-2009 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2008 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-app.h"
#include "midori-array.h"
#include "midori-bookmarks.h"
#include "panels/midori-bookmarks.h"
#include "midori-history.h"
#include "panels/midori-history.h"
#include "midori-transfers.h"
#include "midori-panel.h"
#include "midori-platform.h"
#include "midori-preferences.h"
#include "midori-privatedata.h"
#include "midori-searchaction.h"
#include "midori/midori-session.h"
#include <midori/midori-core.h>

#include <config.h>
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <webkit/webkit.h>
#include <sqlite3.h>

static void
midori_trash_add_item_no_save_cb (KatzeArray* trash,
                                  GObject*    item)
{
    if (katze_array_get_nth_item (trash, 10))
    {
        KatzeItem* obsolete_item = katze_array_get_nth_item (trash, 0);
        katze_array_remove_item (trash, obsolete_item);
    }
}

static void
midori_trash_remove_item_cb (KatzeArray* trash,
                             GObject*    item)
{
    gchar* config_file = midori_paths_get_config_filename_for_writing ("tabtrash.xbel");
    GError* error = NULL;
    midori_trash_add_item_no_save_cb (trash, item);
    if (!midori_array_to_file (trash, config_file, "xbel", &error))
    {
        /* i18n: Trash, or wastebin, containing closed tabs */
        g_warning (_("The trash couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);
}

static void
midori_trash_add_item_cb (KatzeArray* trash,
                          GObject*    item)
{
    midori_trash_remove_item_cb (trash, item);
}

static void
midori_browser_show_preferences_cb (MidoriBrowser*    browser,
                                    KatzePreferences* preferences,
                                    MidoriApp*        app)
{
    midori_preferences_add_extension_category (preferences, app);
}

static void
midori_browser_privacy_preferences_cb (MidoriBrowser*    browser,
                                       KatzePreferences* preferences,
                                       MidoriApp*        app)
{
    MidoriWebSettings* settings = midori_browser_get_settings (browser);
    midori_preferences_add_privacy_category (preferences, settings);
}

static void
midori_app_add_browser_cb (MidoriApp*     app,
                           MidoriBrowser* browser,
                           KatzeNet*      net)
{
    GtkWidget* panel;
    GtkWidget* addon;

    panel = katze_object_get_object (browser, "panel");

    addon = g_object_new (MIDORI_TYPE_BOOKMARKS, "app", app, "visible", TRUE, NULL);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));

    addon = g_object_new (MIDORI_TYPE_HISTORY, "app", app, "visible", TRUE, NULL);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));

    addon = g_object_new (MIDORI_TYPE_TRANSFERS, "app", app, "visible", TRUE, NULL);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));

    /* Extensions */
    g_signal_connect (browser, "show-preferences",
        G_CALLBACK (midori_browser_privacy_preferences_cb), app);
    g_signal_connect (browser, "show-preferences",
        G_CALLBACK (midori_browser_show_preferences_cb), app);

    g_object_unref (panel);
}

static void
button_modify_preferences_clicked_cb (GtkWidget*         button,
                                      MidoriWebSettings* settings)
{
    GtkWidget* dialog = midori_preferences_new (
        GTK_WINDOW (gtk_widget_get_toplevel (button)), settings);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_DELETE_EVENT)
        gtk_widget_destroy (dialog);
}

static void
button_disable_extensions_clicked_cb (GtkWidget* button,
                                      MidoriApp* app)
{
    /* Reset frozen list of active extensions */
    g_object_set_data (G_OBJECT (app), "extensions", NULL);
    gtk_widget_set_sensitive (button, FALSE);
}

static MidoriStartup
midori_show_diagnostic_dialog (MidoriWebSettings* settings,
                               KatzeArray*        session)
{
    GtkWidget* dialog;
    GtkWidget* content_area;
    GtkWidget* align;
    GtkWidget* box;
    GtkWidget* button;
    MidoriApp* app = katze_item_get_parent (KATZE_ITEM (session));
    MidoriStartup load_on_startup = katze_object_get_enum (settings, "load-on-startup");
    gint response;

    dialog = gtk_message_dialog_new (
        NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
        _("Midori seems to have crashed the last time it was opened. "
          "If this happened repeatedly, try one of the following options "
          "to solve the problem."));
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
    gtk_window_set_title (GTK_WINDOW (dialog), g_get_application_name ());
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    align = gtk_alignment_new (0.5, 0.5, 0.5, 0.5);
    gtk_container_add (GTK_CONTAINER (content_area), align);
    box = gtk_hbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (align), box);
    button = gtk_button_new_with_mnemonic (_("Modify _preferences"));
    g_signal_connect (button, "clicked",
        G_CALLBACK (button_modify_preferences_clicked_cb), settings);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 4);
    button = gtk_button_new_with_mnemonic (_("Disable all _extensions"));
    if (g_object_get_data (G_OBJECT (app), "extensions"))
        g_signal_connect (button, "clicked",
            G_CALLBACK (button_disable_extensions_clicked_cb), app);
    else
        gtk_widget_set_sensitive (button, FALSE);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 4);
    gtk_widget_show_all (align);
    button = katze_property_proxy (settings, "show-crash-dialog", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Show a dialog after Midori crashed"));
    gtk_widget_show (button);
    gtk_container_add (GTK_CONTAINER (content_area), button);
    gtk_container_set_focus_child (GTK_CONTAINER (dialog), gtk_dialog_get_action_area (GTK_DIALOG (dialog)));
    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
        _("Discard old tabs"), MIDORI_STARTUP_BLANK_PAGE,
        _("Show last tabs without loading"), MIDORI_STARTUP_DELAYED_PAGES,
        _("Show last open tabs"), MIDORI_STARTUP_LAST_OPEN_PAGES,
        NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog),
        load_on_startup == MIDORI_STARTUP_HOMEPAGE
        ? MIDORI_STARTUP_BLANK_PAGE : load_on_startup);
    if (1)
    {
        /* GtkLabel can't wrap the text properly. Until some day
           this works, we implement this hack to do it ourselves. */
        GtkWidget* hbox;
        GtkWidget* vbox;
        GtkWidget* label;
        GList* ch;
        GtkRequisition req;

        ch = gtk_container_get_children (GTK_CONTAINER (content_area));
        hbox = (GtkWidget*)g_list_nth_data (ch, 0);
        g_list_free (ch);
        ch = gtk_container_get_children (GTK_CONTAINER (hbox));
        vbox = (GtkWidget*)g_list_nth_data (ch, 1);
        g_list_free (ch);
        ch = gtk_container_get_children (GTK_CONTAINER (vbox));
        label = (GtkWidget*)g_list_nth_data (ch, 0);
        g_list_free (ch);
        gtk_widget_size_request (content_area, &req);
        gtk_widget_set_size_request (label, req.width * 0.9, -1);
    }

    response = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    if (response == GTK_RESPONSE_DELETE_EVENT)
        response = G_MAXINT;
    else if (response == MIDORI_STARTUP_BLANK_PAGE)
        katze_array_clear (session);
    return response;
}

#define HAVE_OFFSCREEN GTK_CHECK_VERSION (2, 20, 0)

static void
snapshot_load_finished_cb (GtkWidget*      web_view,
                           WebKitWebFrame* web_frame,
                           gchar*          filename)
{
    #if HAVE_OFFSCREEN
    GdkPixbuf* pixbuf = gtk_offscreen_window_get_pixbuf (GTK_OFFSCREEN_WINDOW (
        gtk_widget_get_parent (web_view)));
    gdk_pixbuf_save (pixbuf, filename, "png", NULL, "compression", "7", NULL);
    g_object_unref (pixbuf);
    #else
    GError* error;
    GtkPrintOperation* operation = gtk_print_operation_new ();

    gtk_print_operation_set_export_filename (operation, filename);
    error = NULL;
    webkit_web_frame_print_full (web_frame, operation,
        GTK_PRINT_OPERATION_ACTION_EXPORT, &error);

    if (error != NULL)
    {
        g_error ("%s", error->message);
        gtk_main_quit ();
    }

    g_object_unref (operation);
    #endif
    g_print (_("Snapshot saved to: %s\n"), filename);
    gtk_main_quit ();
}

static MidoriBrowser*
midori_web_app_browser_new_window_cb (MidoriBrowser* browser,
                                      MidoriBrowser* new_browser,
                                      gpointer       user_data)
{
    if (new_browser == NULL)
        new_browser = midori_browser_new ();
    g_object_set (new_browser,
        "settings", midori_browser_get_settings (browser),
        NULL);
    gtk_widget_show (GTK_WIDGET (new_browser));
    return new_browser;
}

int
main (int    argc,
      char** argv)
{
    gchar* webapp;
    gchar* config;
    gboolean private;
    gboolean portable;
    gboolean plain;
    gboolean diagnostic_dialog;
    gboolean back_from_crash;
    gboolean run;
    gchar* snapshot;
    gboolean execute;
    gboolean help_execute;
    gboolean version;
    gchar** uris;
    gchar* block_uris;
    gint inactivity_reset;
    MidoriApp* app;
    gboolean result;
    GError* error;
    GOptionEntry entries[] =
    {
       { "app", 'a', 0, G_OPTION_ARG_STRING, &webapp,
       N_("Run ADDRESS as a web application"), N_("ADDRESS") },
       { "config", 'c', 0, G_OPTION_ARG_FILENAME, &config,
       N_("Use FOLDER as configuration folder"), N_("FOLDER") },
       { "private", 'p', 0, G_OPTION_ARG_NONE, &private,
       N_("Private browsing, no changes are saved"), NULL },
       #ifdef G_OS_WIN32
       { "portable", 'P', 0, G_OPTION_ARG_NONE, &portable,
       N_("Portable mode, all runtime files are stored in one place"), NULL },
       #endif
       { "plain", '\0', 0, G_OPTION_ARG_NONE, &plain,
       N_("Plain GTK+ window with WebKit, akin to GtkLauncher"), NULL },
       { "diagnostic-dialog", 'd', 0, G_OPTION_ARG_NONE, &diagnostic_dialog,
       N_("Show a diagnostic dialog"), NULL },
       { "run", 'r', 0, G_OPTION_ARG_NONE, &run,
       N_("Run the specified filename as javascript"), NULL },
       { "snapshot", 's', 0, G_OPTION_ARG_STRING, &snapshot,
       N_("Take a snapshot of the specified URI"), NULL },
       { "execute", 'e', 0, G_OPTION_ARG_NONE, &execute,
       N_("Execute the specified command"), NULL },
       { "help-execute", 0, 0, G_OPTION_ARG_NONE, &help_execute,
       N_("List available commands to execute with -e/ --execute"), NULL },
       { "version", 'V', 0, G_OPTION_ARG_NONE, &version,
       N_("Display program version"), NULL },
       { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &uris,
       N_("Addresses"), NULL },
       { "block-uris", 'b', 0, G_OPTION_ARG_STRING, &block_uris,
       N_("Block URIs according to regular expression PATTERN"), _("PATTERN") },
       #ifdef HAVE_X11_EXTENSIONS_SCRNSAVER_H
       { "inactivity-reset", 'i', 0, G_OPTION_ARG_INT, &inactivity_reset,
       /* i18n: CLI: Close tabs, clear private data, open starting page */
       N_("Reset Midori after SECONDS seconds of inactivity"), N_("SECONDS") },
       #endif
     { NULL }
    };
    GString* error_messages;
    gchar** extensions;
    MidoriWebSettings* settings;
    gchar* config_file;
    MidoriSpeedDial* dial;
    MidoriStartup load_on_startup;
    KatzeArray* search_engines;
    KatzeArray* bookmarks;
    KatzeArray* history;
    KatzeArray* session;
    KatzeArray* trash;
    guint i;
    gchar* uri;
    KatzeItem* item;
    gchar* uri_ready;
    gchar* errmsg;
    #ifdef G_ENABLE_DEBUG
        gboolean startup_timer = midori_debug ("startup");
        #define midori_startup_timer(tmrmsg) if (startup_timer) \
            g_debug (tmrmsg, (g_test_timer_last () - g_test_timer_elapsed ()) * -1)
    #else
        #define midori_startup_timer(tmrmsg)
    #endif

    /* Parse cli options */
    webapp = NULL;
    config = NULL;
    private = FALSE;
    portable = FALSE;
    plain = FALSE;
    back_from_crash = FALSE;
    diagnostic_dialog = FALSE;
    run = FALSE;
    snapshot = NULL;
    execute = FALSE;
    help_execute = FALSE;
    version = FALSE;
    uris = NULL;
    block_uris = NULL;
    inactivity_reset = 0;
    error = NULL;
    midori_app_setup (&argc, &argv, entries);

    /* Relative config path */
    if (config && !g_path_is_absolute (config))
    {
        gchar* old_config = config;
        gchar* current_dir = g_get_current_dir ();
        config = g_build_filename (current_dir, old_config, NULL);
        g_free (current_dir);
        g_free (old_config);
    }

    g_set_application_name (_("Midori"));
    /* Versioned prgname to override menuproxy blacklist */
    g_set_prgname (PACKAGE_NAME "4");

    if (version)
    {
        g_print (
          "%s %s\n\n"
          "Copyright (c) 2007-2012 Christian Dywan\n\n"
          "%s\n"
          "\t%s\n\n"
          "%s\n"
          "\thttp://www.midori-browser.org\n",
          _("Midori"), PACKAGE_VERSION,
          _("Please report comments, suggestions and bugs to:"),
          PACKAGE_BUGREPORT,
          _("Check for new versions at:")
        );
        return 0;
    }

    if (help_execute)
    {
        MidoriBrowser* browser = midori_browser_new ();
        GtkActionGroup* action_group = midori_browser_get_action_group (browser);
        GList* actions = gtk_action_group_list_actions (action_group);
        GList* temp = actions;
        guint length = 1;
        gchar* space;

        for (; temp; temp = g_list_next (temp))
        {
            GtkAction* action = temp->data;
            length = MAX (length, 1 + strlen (gtk_action_get_name (action)));
        }

        space = g_strnfill (length, ' ');
        for (; actions; actions = g_list_next (actions))
        {
            GtkAction* action = actions->data;
            const gchar* name = gtk_action_get_name (action);
            gchar* padding = g_strndup (space, strlen (space) - strlen (name));
            gchar* label = katze_object_get_string (action, "label");
            gchar* stripped = katze_strip_mnemonics (label);
            gchar* tooltip = katze_object_get_string (action, "tooltip");
            g_print ("%s%s%s%s%s\n", name, padding, stripped,
                     tooltip ? ": " : "", tooltip ? tooltip : "");
            g_free (tooltip);
            g_free (padding);
            g_free (label);
            g_free (stripped);
        }
        g_free (space);
        g_list_free (actions);
        gtk_widget_destroy (GTK_WIDGET (browser));
        return 0;
    }

    if (snapshot)
    {
        gchar* filename;
        gint fd;
        GtkWidget* web_view;
        gchar* uri;
        #if HAVE_OFFSCREEN
        GtkWidget* offscreen;
        GdkScreen* screen;

        fd = g_file_open_tmp ("snapshot-XXXXXX.png", &filename, &error);
        #else
        fd = g_file_open_tmp ("snapshot-XXXXXX.pdf", &filename, &error);
        #endif
        close (fd);

        error = NULL;
        if (error)
        {
            g_error ("%s", error->message);
            return 1;
        }

        if (g_unlink (filename) == -1)
        {
            g_error ("%s", g_strerror (errno));
            return 1;
        }

        web_view = webkit_web_view_new ();
        #if HAVE_OFFSCREEN
        offscreen = gtk_offscreen_window_new ();
        gtk_container_add (GTK_CONTAINER (offscreen), web_view);
        if ((screen = gdk_screen_get_default ()))
            gtk_widget_set_size_request (web_view,
                gdk_screen_get_width (screen), gdk_screen_get_height (screen));
        else
            gtk_widget_set_size_request (web_view, 800, 600);
        gtk_widget_show_all (offscreen);
        #endif
        g_signal_connect (web_view, "load-finished",
            G_CALLBACK (snapshot_load_finished_cb), filename);
        uri = sokoke_prepare_uri (snapshot);
        webkit_web_view_load_uri (WEBKIT_WEB_VIEW (web_view), uri);
        g_free (uri);
        gtk_main ();
        g_free (filename);
        return 0;
    }

    if (plain)
    {
        GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        GtkWidget* scrolled = gtk_scrolled_window_new (NULL, NULL);
        GtkWidget* web_view = webkit_web_view_new ();
        gchar* uri = sokoke_prepare_uri (
            (uris != NULL && uris[0]) ? uris[0] : "http://www.example.com");

        gint width, height;
        GdkRectangle monitor;
        GdkScreen* screen = gtk_window_get_screen (GTK_WINDOW (window));
        gdk_screen_get_monitor_geometry (screen, 0, &monitor);
        width = monitor.width / 1.7; height = monitor.height / 1.7;
        gtk_window_set_default_size (GTK_WINDOW (window), width, height);

        gtk_container_add (GTK_CONTAINER (window), scrolled);
        gtk_container_add (GTK_CONTAINER (scrolled), web_view);
        g_signal_connect (window, "delete-event",
            G_CALLBACK (gtk_main_quit), window);
        gtk_widget_show_all (window);
        webkit_web_view_load_uri (WEBKIT_WEB_VIEW (web_view), uri);
        g_free (uri);
        gtk_main ();
        return 0;
    }

    midori_private_data_register_built_ins ();

    if (private)
        midori_paths_init (MIDORI_RUNTIME_MODE_PRIVATE, config);
    else if (webapp)
        midori_paths_init (MIDORI_RUNTIME_MODE_APP, config);
    else if (portable)
        midori_paths_init (MIDORI_RUNTIME_MODE_PORTABLE, config);
    else
        midori_paths_init (MIDORI_RUNTIME_MODE_NORMAL, config);

    /* Web Application or Private Browsing support */
    if (webapp || private || run)
    {
        MidoriBrowser* browser = midori_browser_new ();
        g_signal_connect (browser, "new-window",
            G_CALLBACK (midori_web_app_browser_new_window_cb), NULL);
        g_object_set_data (G_OBJECT (webkit_get_default_session ()),
                           "pass-through-console", (void*)1);
        midori_startup_timer ("Browser: \t%f");

        if (private || !webapp)
        {
            settings = midori_settings_new_full (NULL);
            search_engines = midori_search_engines_new_from_folder (NULL);
            g_object_set (browser,
                          "search-engines", search_engines,
                          "settings", settings,
                          NULL);
            g_object_unref (search_engines);
        }
        else
            settings = g_object_ref (midori_browser_get_settings (browser));

        if (private)
        {
            /* Mask the timezone, which can be read by Javascript */
            g_setenv ("TZ", "UTC", TRUE);
            /* In-memory trash for re-opening closed tabs */
            trash = katze_array_new (KATZE_TYPE_ITEM);
            g_signal_connect_after (trash, "add-item",
              G_CALLBACK (midori_trash_add_item_no_save_cb), NULL);
            g_object_set (browser, "trash", trash, NULL);

            g_object_set (settings,
                          "preferred-languages", "en",
                          "enable-private-browsing", TRUE,
            #ifdef HAVE_LIBSOUP_2_29_91
                          "first-party-cookies-only", TRUE,
            #endif
                          "enable-html5-database", FALSE,
                          "enable-html5-local-storage", FALSE,
                          "enable-offline-web-application-cache", FALSE,
            /* Arguably DNS prefetching is or isn't a privacy concern. For the
             * lack of more fine-grained control we'll go the safe route. */
            #if WEBKIT_CHECK_VERSION (1, 3, 11)
                          "enable-dns-prefetching", FALSE,
            #endif
                          "strip-referer", TRUE, NULL);
            midori_browser_set_action_visible (browser, "Tools", FALSE);
            midori_browser_set_action_visible (browser, "ClearPrivateData", FALSE);
            #if GTK_CHECK_VERSION (3, 0, 0)
            g_object_set (gtk_widget_get_settings (GTK_WIDGET (browser)),
                          "gtk-application-prefer-dark-theme", TRUE,
                          NULL);
            #endif

            /* Informative text unless we have a URI */
            if (webapp == NULL && uris == NULL)
                midori_browser_add_uri (browser, "about:private");
        }

        midori_load_soup_session (settings);

        if (run)
        {
            gchar* script = NULL;
            error = NULL;

            if (g_file_get_contents (uris ? *uris : NULL, &script, NULL, &error))
            {
                #if 0 /* HAVE_OFFSCREEN */
                GtkWidget* offscreen = gtk_offscreen_window_new ();
                #endif
                gchar* msg = NULL;
                GtkWidget* view = midori_view_new_with_item (NULL, settings);
                g_object_set (settings, "open-new-pages-in", MIDORI_NEW_PAGE_WINDOW, NULL);
                midori_browser_add_tab (browser, view);
                #if 0 /* HAVE_OFFSCREEN */
                gtk_container_add (GTK_CONTAINER (offscreen), GTK_WIDGET (browser));
                gtk_widget_show_all (offscreen);
                #else
                gtk_widget_show_all (GTK_WIDGET (browser));
                gtk_widget_hide (GTK_WIDGET (browser));
                #endif
                midori_view_execute_script (MIDORI_VIEW (view), script, &msg);
                if (msg != NULL)
                {
                    g_error ("%s\n", msg);
                    g_free (msg);
                }
            }
            else if (error != NULL)
            {
                g_error ("%s\n", error->message);
                g_error_free (error);
            }
            else
                g_error ("%s\n", _("An unknown error occured"));
            g_free (script);
        }

        if (webapp)
        {
            gchar* tmp_uri = sokoke_prepare_uri (webapp);
            midori_browser_set_action_visible (browser, "Menubar", FALSE);
            midori_browser_set_action_visible (browser, "CompactMenu", FALSE);
            midori_browser_add_uri (browser, tmp_uri);
            g_object_set (settings,
                          "show-menubar", FALSE,
                          "show-navigationbar", FALSE,
                          "toolbar-items", "Back,Forward,ReloadStop,Location,Homepage",
                          "show-statusbar", FALSE,
                          "enable-developer-extras", FALSE,
                          "homepage", tmp_uri,
                          NULL);
            g_object_set (browser, "show-tabs", FALSE, NULL);
            g_free (tmp_uri);
        }

        g_object_set (settings,
                      "show-panel", FALSE,
                      "last-window-state", MIDORI_WINDOW_NORMAL,
                      "inactivity-reset", inactivity_reset,
                      "block-uris", block_uris,
                      NULL);
        midori_browser_set_action_visible (browser, "Panel", FALSE);
        midori_startup_timer ("Setup config: \t%f");
        g_signal_connect (browser, "quit",
            G_CALLBACK (gtk_main_quit), NULL);
        g_signal_connect (browser, "destroy",
            G_CALLBACK (gtk_main_quit), NULL);
        if (!run)
        {
            gtk_widget_show (GTK_WIDGET (browser));
            midori_browser_activate_action (browser, "Location");
        }
        if (execute)
        {
            for (i = 0; uris[i] != NULL; i++)
                midori_browser_activate_action (browser, uris[i]);
        }
        else if (uris != NULL)
        {
            for (i = 0; uris[i] != NULL; i++)
            {
                gchar* new_uri = sokoke_prepare_uri (uris[i]);
                midori_browser_add_uri (browser, new_uri);
                g_free (new_uri);
            }
        }

        if (midori_browser_get_current_uri (browser) == NULL)
            midori_browser_add_uri (browser, "about:blank");

        midori_startup_timer ("App created: \t%f");
        gtk_main ();
        return 0;
    }

    app = midori_app_new ();
    midori_startup_timer ("App created: \t%f");

    /* FIXME: The app might be 'running' but actually showing a dialog
              after a crash, so running a new window isn't a good idea. */
    if (midori_app_instance_is_running (app))
    {
        GtkWidget* dialog;

        if (execute)
            result = midori_app_send_command (app, uris);
        else if (uris)
            result = midori_app_instance_send_uris (app, uris);
        else
            result = midori_app_instance_send_new_browser (app);

        if (result)
            return 0;

        sokoke_message_dialog (GTK_MESSAGE_INFO,
            _("An instance of Midori is already running but not responding.\n"),
            "", TRUE);
        /* FIXME: Allow killing the existing instance */
        return 1;
    }

    katze_assign (config, g_strdup (midori_paths_get_config_dir_for_writing ()));
    /* Load configuration file */
    error_messages = g_string_new (NULL);
    error = NULL;
    settings = midori_settings_new_full (&extensions);
    g_object_set (settings,
                  "enable-developer-extras", TRUE,
                  "enable-html5-database", TRUE,
                  "block-uris", block_uris,
                  NULL);
    if (inactivity_reset > 0)
        g_object_set (settings, "inactivity-reset", inactivity_reset, NULL);
    midori_startup_timer ("Config and accels read: \t%f");

    /* Load search engines */
    search_engines = midori_search_engines_new_from_folder (error_messages);
    /* Pick first search engine as default if not set */
    g_object_get (settings, "location-entry-search", &uri, NULL);
    if (!(uri && *uri) && !katze_array_is_empty (search_engines))
    {
        item = katze_array_get_nth_item (search_engines, 0);
        g_object_set (settings, "location-entry-search",
                      katze_item_get_uri (item), NULL);
    }
    g_free (uri);
    midori_startup_timer ("Search read: \t%f");

    errmsg = NULL;
    if (!(bookmarks = midori_bookmarks_new (&errmsg)))
    {
        g_string_append_printf (error_messages,
            _("Bookmarks couldn't be loaded: %s\n"), errmsg);
        katze_assign (errmsg, NULL);
    }
    midori_startup_timer ("Bookmarks read: \t%f");

    config_file = NULL;
    session = katze_array_new (KATZE_TYPE_ITEM);
    load_on_startup = katze_object_get_enum (settings, "load-on-startup");
    if (load_on_startup >= MIDORI_STARTUP_LAST_OPEN_PAGES)
    {
        katze_assign (config_file, midori_paths_get_config_filename_for_reading ("session.xbel"));
        error = NULL;
        if (!midori_array_from_file (session, config_file, "xbel", &error))
        {
            if (error->code != G_FILE_ERROR_NOENT)
                g_string_append_printf (error_messages,
                    _("The session couldn't be loaded: %s\n"), error->message);
            g_error_free (error);
        }
    }
    midori_startup_timer ("Session read: \t%f");

    trash = katze_array_new (KATZE_TYPE_ITEM);
    katze_assign (config_file, g_build_filename (config, "tabtrash.xbel", NULL));
    error = NULL;
    if (!midori_array_from_file (trash, config_file, "xbel", &error))
    {
        if (error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf (error_messages,
                _("The trash couldn't be loaded: %s\n"), error->message);
        g_error_free (error);
    }
    midori_startup_timer ("Trash read: \t%f");

    if (!(history = midori_history_new (&errmsg)))
    {
        g_string_append_printf (error_messages,
            _("The history couldn't be loaded: %s\n"), errmsg);
        katze_assign (errmsg, NULL);
    }
    midori_startup_timer ("History read: \t%f");

    katze_assign (config_file, g_build_filename (config, "speeddial", NULL));
    dial = midori_speed_dial_new (config_file, NULL);

    /* In case of errors */
    if (error_messages->len)
    {
        GtkWidget* dialog = gtk_message_dialog_new (
            NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE,
            _("The following errors occured:"));
        gtk_message_dialog_format_secondary_text (
            GTK_MESSAGE_DIALOG (dialog), "%s", error_messages->str);
        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                _("_Ignore"), GTK_RESPONSE_ACCEPT,
                                NULL);
        if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
            return 0;
        gtk_widget_destroy (dialog);
        /* FIXME: Since we will overwrite files that could not be loaded
                  , would we want to make backups? */
    }
    g_string_free (error_messages, TRUE);

    /* If -e or --execute was specified, "uris" refers to the command. */
    if (!execute)
    {
        i = 0;
        while (uris && uris[i])
        {
            item = katze_item_new ();
            item->uri = sokoke_prepare_uri (uris[i]);
            /* Never delay command line arguments */
            katze_item_set_meta_integer (item, "delay", 0);
            katze_array_add_item (session, item);
        }
        i++;
    }

    katze_assign (config_file, g_build_filename (config, "search", NULL));
    midori_search_engines_set_filename (search_engines, config_file);

    g_signal_connect_after (trash, "add-item",
        G_CALLBACK (midori_trash_add_item_cb), NULL);
    g_signal_connect_after (trash, "remove-item",
        G_CALLBACK (midori_trash_remove_item_cb), NULL);

    katze_item_set_parent (KATZE_ITEM (session), app);
    g_object_set_data_full (G_OBJECT (app), "extensions", extensions, (GDestroyNotify)g_strfreev);

    if (midori_app_get_crashed (app)
     && katze_object_get_boolean (settings, "show-crash-dialog")
     && !katze_array_is_empty (session))
        diagnostic_dialog = TRUE;

    if (diagnostic_dialog)
    {
        load_on_startup = midori_show_diagnostic_dialog (settings, session);
        if (load_on_startup == G_MAXINT)
            return 0;
    }
    g_object_set_data (G_OBJECT (settings), "load-on-startup", GINT_TO_POINTER (load_on_startup));
    midori_startup_timer ("Signal setup: \t%f");

    g_object_set (app, "settings", settings,
                       "bookmarks", bookmarks,
                       "trash", trash,
                       "search-engines", search_engines,
                       "history", history,
                       "speed-dial", dial,
                       NULL);
    g_object_unref (history);
    g_object_unref (search_engines);
    g_object_unref (bookmarks);
    g_object_unref (trash);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (midori_app_add_browser_cb), NULL);
    midori_startup_timer ("App prepared: \t%f");

    g_idle_add (midori_load_soup_session_full, settings);
    g_idle_add (midori_load_extensions, app);
    g_idle_add (midori_load_session, session);

    if (execute)
        g_object_set_data (G_OBJECT (app), "execute-command", uris);

    gtk_main ();

    g_object_notify (G_OBJECT (settings), "load-on-startup");
    midori_bookmarks_on_quit (bookmarks);
    midori_history_on_quit (history, settings);
    midori_private_data_on_quit (settings);
    /* Removing KatzeHttpCookies makes it save outstanding changes */
    soup_session_remove_feature_by_type (webkit_get_default_session (),
                                         KATZE_TYPE_HTTP_COOKIES);

    load_on_startup = katze_object_get_int (settings, "load-on-startup");
    if (load_on_startup < MIDORI_STARTUP_LAST_OPEN_PAGES)
    {
        katze_assign (config_file, midori_paths_get_config_filename_for_writing ("session.xbel"));
        g_unlink (config_file);
    }

    return 0;
}
