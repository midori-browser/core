/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-array.h"
#include "midori-bookmarks-db.h"
#include "midori-history.h"
#include "midori-preferences.h"
#include "midori-privatedata.h"
#include "midori-session.h"
#include "midori-searchaction.h"
#include "midori-panel.h"
#include "panels/midori-bookmarks.h"
#include "panels/midori-history.h"
#include "sokoke.h"
#include <glib/gi18n-lib.h>

static MidoriBrowser*
midori_frontend_browser_new_window_cb (MidoriBrowser* browser,
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

static void
midori_browser_privacy_preferences_cb (MidoriBrowser*    browser,
                                       KatzePreferences* preferences,
                                       gpointer          user_data)
{
    MidoriWebSettings* settings = midori_browser_get_settings (browser);
    midori_preferences_add_privacy_category (preferences, settings);
}

MidoriBrowser*
midori_web_app_new (const gchar* webapp,
                    gchar**      open_uris,
                    gchar**      execute_commands,
                    gint         inactivity_reset,
                    const gchar* block_uris)
{
    guint i;
    g_return_val_if_fail (webapp != NULL, NULL);

    midori_paths_init (MIDORI_RUNTIME_MODE_APP, webapp);
    /*
       Set sanitized URI as class name which .desktop files use as StartupWMClass
       So dock type launchers can distinguish different apps with the same executable
     */
    gchar* wm_class = g_strdelimit (g_strdup (webapp), ":.\\/", '_');
    gdk_set_program_class (wm_class);
    g_free (wm_class);

    MidoriBrowser* browser = midori_browser_new ();
    g_signal_connect (browser, "new-window",
        G_CALLBACK (midori_frontend_browser_new_window_cb), NULL);
    g_signal_connect (browser, "show-preferences",
        G_CALLBACK (midori_browser_privacy_preferences_cb), NULL);

    midori_browser_set_action_visible (browser, "Menubar", FALSE);
    midori_browser_set_action_visible (browser, "CompactMenu", FALSE);
    midori_browser_set_action_visible (browser, "AddSpeedDial", FALSE);
    midori_browser_set_action_visible (browser, "Navigationbar", FALSE);
    GtkActionGroup* action_group = midori_browser_get_action_group (browser);
    GtkAction* action = gtk_action_group_get_action (action_group, "Location");
    gtk_action_set_sensitive (action, FALSE);

    MidoriWebSettings* settings = midori_settings_new_full (NULL);
    g_object_set (settings,
                  "show-menubar", FALSE,
                  "toolbar-items", "Back,Forward,ReloadStop,Location,Homepage,Preferences",
                  "show-statusbar", FALSE,
                  "show-panel", FALSE,
                  "last-window-state", MIDORI_WINDOW_NORMAL,
                  "inactivity-reset", inactivity_reset,
                  "block-uris", block_uris,
                  NULL);
    midori_load_soup_session_full (settings);

    KatzeArray* search_engines = midori_search_engines_new_from_folder (NULL);
    g_object_set (browser,
                  "show-tabs", open_uris != NULL,
                  "settings", settings,
                  NULL);
    midori_browser_set_action_visible (browser, "Panel", FALSE);
    g_object_unref (search_engines);

    if (webapp != NULL)
    {
        gchar* tmp_uri = sokoke_magic_uri (webapp, FALSE, TRUE);
        g_object_set (settings, "homepage", tmp_uri, NULL);
        midori_browser_add_uri (browser, tmp_uri);
        g_free (tmp_uri);
    }

    for (i = 0; open_uris && open_uris[i]; i++)
    {
        gchar* new_uri = sokoke_magic_uri (open_uris[i], FALSE, TRUE);
        midori_browser_add_uri (browser, new_uri);
        g_free (new_uri);
    }
    if (midori_browser_get_n_pages (browser) == 0)
        midori_browser_add_uri (browser, "about:blank");
    gtk_widget_show (GTK_WIDGET (browser));

    for (i = 0; execute_commands && execute_commands[i]; i++)
    {
        midori_browser_assert_action (browser, execute_commands[i]);
        midori_browser_activate_action (browser, execute_commands[i]);
    }
    midori_session_persistent_settings (settings, NULL);
    /* FIXME need proper stock extension mechanism */
    midori_browser_activate_action (browser, "libtransfers." G_MODULE_SUFFIX "=true");
    midori_browser_activate_action (browser, "libabout." G_MODULE_SUFFIX "=true");
    midori_browser_activate_action (browser, "libopen-with." G_MODULE_SUFFIX "=true");
    g_assert (g_module_error () == NULL);
    return browser;
}

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
    if (!midori_array_to_file (trash, config_file, "xbel-tiny", &error))
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

MidoriBrowser*
midori_private_app_new (const gchar* config,
                        const gchar* webapp,
                        gchar**      open_uris,
                        gchar**      execute_commands,
                        gint         inactivity_reset,
                        const gchar* block_uris)
{
    guint i;

    midori_paths_init (MIDORI_RUNTIME_MODE_PRIVATE, config);
#ifndef HAVE_WEBKIT2
    g_object_set_data (G_OBJECT (webkit_get_default_session ()), "pass-through-console", (void*)1);
#endif

    /* Mask the timezone, which can be read by Javascript */
    g_setenv ("TZ", "UTC", TRUE);

    MidoriBrowser* browser = midori_browser_new ();
    g_signal_connect (browser, "new-window",
        G_CALLBACK (midori_frontend_browser_new_window_cb), NULL);

    MidoriWebSettings* settings = midori_settings_new_full (NULL);
    g_object_set (settings,
                  "preferred-languages", "en",
                  "enable-private-browsing", TRUE,
                  "first-party-cookies-only", TRUE,
                  "enable-html5-database", FALSE,
                  "enable-html5-local-storage", FALSE,
                  "enable-offline-web-application-cache", FALSE,
    /* Arguably DNS prefetching is or isn't a privacy concern. For the
     * lack of more fine-grained control we'll go the safe route. */
                  "enable-dns-prefetching", FALSE,
                  "strip-referer", TRUE,
                  "show-panel", FALSE,
                  "last-window-state", MIDORI_WINDOW_NORMAL,
                  "inactivity-reset", inactivity_reset,
                  "block-uris", block_uris,
                  NULL);
    midori_load_soup_session (settings);

    /* In-memory trash for re-opening closed tabs */
    KatzeArray* trash = katze_array_new (KATZE_TYPE_ITEM);
    g_signal_connect_after (trash, "add-item",
      G_CALLBACK (midori_trash_add_item_no_save_cb), NULL);

    KatzeArray* search_engines = midori_search_engines_new_from_folder (NULL);
    g_object_set (browser,
                  "settings", settings,
                  "trash", trash,
                  "search-engines", search_engines,
                  NULL);
    g_object_unref (settings);
    g_object_unref (trash);
    g_object_unref (search_engines);

    midori_browser_set_action_visible (browser, "Tools", FALSE);
    midori_browser_set_action_visible (browser, "ClearPrivateData", FALSE);
    midori_browser_set_action_visible (browser, "AddSpeedDial", FALSE);
    #if GTK_CHECK_VERSION (3, 0, 0)
    g_object_set (gtk_widget_get_settings (GTK_WIDGET (browser)),
                  "gtk-application-prefer-dark-theme", TRUE,
                  NULL);
    #endif

    if (webapp != NULL)
    {
        gchar* tmp_uri = sokoke_magic_uri (webapp, FALSE, TRUE);
        g_object_set (settings, "homepage", tmp_uri, NULL);
        midori_browser_add_uri (browser, tmp_uri);
        g_free (tmp_uri);
    }

    for (i = 0; open_uris && open_uris[i]; i++)
    {
        gchar* new_uri = sokoke_magic_uri (open_uris[i], FALSE, TRUE);
        midori_browser_add_uri (browser, new_uri);
        g_free (new_uri);
    }
    if (midori_browser_get_n_pages (browser) == 0)
        midori_browser_add_uri (browser, "about:private");
    gtk_widget_show (GTK_WIDGET (browser));

    for (i = 0; execute_commands && execute_commands[i]; i++)
    {
        midori_browser_assert_action (browser, execute_commands[i]);
        midori_browser_activate_action (browser, execute_commands[i]);
    }

    /* FIXME need proper stock extension mechanism */
    midori_browser_activate_action (browser, "libtransfers." G_MODULE_SUFFIX "=true");
    midori_browser_activate_action (browser, "libabout." G_MODULE_SUFFIX "=true");
    midori_browser_activate_action (browser, "libopen-with." G_MODULE_SUFFIX "=true");
    g_assert (g_module_error () == NULL);

    return browser;
}

static void
midori_browser_show_preferences_cb (MidoriBrowser*    browser,
                                    KatzePreferences* preferences,
                                    MidoriApp*        app)
{
    midori_preferences_add_extension_category (preferences, app);
}

static void
midori_app_add_browser_cb (MidoriApp*     app,
                           MidoriBrowser* browser,
                           gpointer       user_data)
{
    GtkWidget* panel;
    GtkWidget* addon;

    panel = katze_object_get_object (browser, "panel");

    addon = g_object_new (MIDORI_TYPE_BOOKMARKS, "app", app, "visible", TRUE, NULL);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));

    addon = g_object_new (MIDORI_TYPE_HISTORY, "app", app, "visible", TRUE, NULL);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));

    /* Extensions */
    g_signal_connect (browser, "show-preferences",
        G_CALLBACK (midori_browser_privacy_preferences_cb), NULL);
    g_signal_connect (browser, "show-preferences",
        G_CALLBACK (midori_browser_show_preferences_cb), app);

    g_object_unref (panel);
}

static void
button_disable_extensions_clicked_cb (GtkWidget* button,
                                      MidoriApp* app)
{
    /* Reset frozen list of active extensions */
    g_object_set_data (G_OBJECT (app), "extensions", NULL);
    gtk_widget_set_sensitive (button, FALSE);
}

static void
button_modify_preferences_clicked_cb (GtkWidget*         button,
                                      MidoriWebSettings* settings)
{
    GtkWidget* dialog = midori_preferences_new (
        GTK_WINDOW (gtk_widget_get_toplevel (button)), settings);
    if (midori_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_DELETE_EVENT)
        gtk_widget_destroy (dialog);
}

static void
midori_frontend_crash_log_cb (GtkWidget* button,
                              gchar*     crash_log)
{
    GError* error = NULL;
    if (!gtk_show_uri (gtk_widget_get_screen (button), crash_log, 0, &error))
    {
        sokoke_message_dialog (GTK_MESSAGE_ERROR,
                               _("Could not run external program."),
                               error->message, FALSE);
        g_error_free (error);
    }
}

static void
midori_frontend_debugger_cb (GtkWidget* button,
                             GtkDialog* dialog)
{
    gtk_dialog_response (dialog, GTK_RESPONSE_HELP);
}

static gint
midori_frontend_diagnostic_dialog (MidoriApp*         app,
                                   MidoriWebSettings* settings,
                                   KatzeArray*        session)
{
    GtkWidget* dialog;
    GtkWidget* content_area;
    GtkWidget* align;
    GtkWidget* box;
    GtkWidget* button;
    MidoriStartup load_on_startup = katze_object_get_enum (settings, "load-on-startup");
    gint response;

    dialog = gtk_message_dialog_new (
        NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
        _("Midori crashed the last time it was opened. You can report the problem at %s."),
        PACKAGE_BUGREPORT);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
    gtk_window_set_title (GTK_WINDOW (dialog), g_get_application_name ());
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    align = gtk_alignment_new (0.5, 0.5, 0.5, 0.5);
    gtk_box_pack_start (GTK_BOX (content_area), align, FALSE, TRUE, 0);
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
    gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, TRUE, 0);
    gtk_container_set_focus_child (GTK_CONTAINER (dialog), gtk_dialog_get_action_area (GTK_DIALOG (dialog)));
    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
        _("Discard old tabs"), MIDORI_STARTUP_BLANK_PAGE,
        _("Show last tabs without loading"), MIDORI_STARTUP_DELAYED_PAGES,
        _("Show last open tabs"), MIDORI_STARTUP_LAST_OPEN_PAGES,
        NULL);

    gchar* crash_log = g_build_filename (midori_paths_get_runtime_dir (), "gdb.bt", NULL);
    if (g_access (crash_log, F_OK) == 0)
    {
        GtkWidget* log_button = gtk_button_new_with_mnemonic (_("Show last crash _log"));
        g_signal_connect_data (log_button, "clicked",
            G_CALLBACK (midori_frontend_crash_log_cb), crash_log,
            (GClosureNotify)g_free, 0);
        gtk_widget_show (log_button);
        gtk_box_pack_start (GTK_BOX (box), log_button, FALSE, FALSE, 4);
    }
    else
        g_free (crash_log);

    gchar* gdb = g_find_program_in_path ("gdb");
    if (gdb != NULL)
    {
        GtkWidget* gdb_button = gtk_button_new_with_mnemonic (_("Run in _debugger"));
        g_signal_connect (button, "clicked",
            G_CALLBACK (midori_frontend_debugger_cb), dialog);
        gtk_widget_show (gdb_button);
        gtk_box_pack_start (GTK_BOX (box), gdb_button, FALSE, FALSE, 4);
    }
    gtk_dialog_set_default_response (GTK_DIALOG (dialog),
        load_on_startup == MIDORI_STARTUP_HOMEPAGE
        ? MIDORI_STARTUP_BLANK_PAGE : load_on_startup);

    /* GtkLabel can't wrap the text properly. Until some day
       this works, we implement this hack to do it ourselves. */
    GList* ch = gtk_container_get_children (GTK_CONTAINER (content_area));
    GtkWidget* hbox = (GtkWidget*)g_list_nth_data (ch, 0);
    g_list_free (ch);
    ch = gtk_container_get_children (GTK_CONTAINER (hbox));
    GtkWidget* vbox = (GtkWidget*)g_list_nth_data (ch, 1);
    g_list_free (ch);
    ch = gtk_container_get_children (GTK_CONTAINER (vbox));
    GtkWidget* label = (GtkWidget*)g_list_nth_data (ch, 0);
    g_list_free (ch);
    GtkRequisition req;
    gtk_widget_size_request (content_area, &req);
    gtk_widget_set_size_request (label, req.width * 0.9, -1);

    response = midori_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    if (response == GTK_RESPONSE_DELETE_EVENT)
        response = G_MAXINT;
    else if (response == GTK_RESPONSE_HELP)
    {
        sokoke_spawn_gdb (gdb, FALSE);
        response = G_MAXINT;
    }
    else if (response == MIDORI_STARTUP_BLANK_PAGE)
        katze_array_clear (session);
    return response;
}

MidoriApp*
midori_normal_app_new (const gchar* config,
                       gchar*       nickname,
                       gboolean     diagnostic_dialog,
                       gchar**      open_uris,
                       gchar**      execute_commands,
                       gint         inactivity_reset,
                       const gchar* block_uris)
{
    if (g_str_has_suffix (nickname, "portable"))
        midori_paths_init (MIDORI_RUNTIME_MODE_PORTABLE, config);
    else if (g_str_has_suffix (nickname, "normal"))
        midori_paths_init (MIDORI_RUNTIME_MODE_NORMAL, config);
    else
        g_assert_not_reached ();

    MidoriApp* app = midori_app_new (nickname);
    if (midori_app_instance_is_running (app))
    {
        /* midori_debug makes no sense on a running instance */
        if (g_getenv ("MIDORI_DEBUG"))
            g_warning ("MIDORI_DEBUG only works for a new instance");

        /* It makes no sense to show a crash dialog while running */
        if (!diagnostic_dialog)
        {
            if (execute_commands != NULL)
                midori_app_send_command (app, execute_commands);
            if (open_uris != NULL)
                midori_app_instance_send_uris (app, open_uris);
            if (!execute_commands && !open_uris)
                midori_app_instance_send_new_browser (app);

            if (g_application_get_is_registered (G_APPLICATION (app)))
                return NULL;
        }

        /* FIXME: We mustn't lose the URL here; either instance is freezing or inside a crash dialog */
        sokoke_message_dialog (GTK_MESSAGE_INFO,
            _("An instance of Midori is already running but not responding.\n"),
            open_uris ? *open_uris : "", TRUE);
        return (void*)0xdeadbeef;
    }

    GString* error_messages = g_string_new (NULL);
    GError* error = NULL;
    gchar** extensions;
    MidoriWebSettings* settings = midori_settings_new_full (&extensions);
    g_object_set (settings,
#ifdef G_OS_WIN32
                  "enable-developer-extras", FALSE,
#else
                  "enable-developer-extras", TRUE,
#endif
                  "enable-html5-database", TRUE,
                  "block-uris", block_uris,
                  NULL);
    if (inactivity_reset > 0)
        g_object_set (settings, "inactivity-reset", inactivity_reset, NULL);

    KatzeArray* search_engines = midori_search_engines_new_from_folder (error_messages);
    /* Pick first search engine as default if not set */
    gchar* uri = katze_object_get_string (settings, "location-entry-search");
    if (!(uri && *uri) && !katze_array_is_empty (search_engines))
    {
        KatzeItem* item = katze_array_get_nth_item (search_engines, 0);
        g_object_set (settings, "location-entry-search",
                      katze_item_get_uri (item), NULL);
    }
    g_free (uri);

    MidoriBookmarksDb* bookmarks;
    gchar* errmsg = NULL;
    if (!(bookmarks = midori_bookmarks_db_new (&errmsg)))
    {
        g_string_append_printf (error_messages,
            _("Bookmarks couldn't be loaded: %s\n"), errmsg);
        katze_assign (errmsg, NULL);
    }

    gchar* config_file = NULL;
    KatzeArray* session = katze_array_new (KATZE_TYPE_ITEM);
    MidoriStartup load_on_startup = katze_object_get_enum (settings, "load-on-startup");
    if (load_on_startup >= MIDORI_STARTUP_LAST_OPEN_PAGES)
    {
        katze_assign (config_file, midori_paths_get_config_filename_for_reading ("session.xbel"));
        error = NULL;
        if (!midori_array_from_file (session, config_file, "xbel-tiny", &error))
        {
            if (error->code != G_FILE_ERROR_NOENT)
                g_string_append_printf (error_messages,
                    _("The session couldn't be loaded: %s\n"), error->message);
            g_error_free (error);
        }
    }

    KatzeArray* trash = katze_array_new (KATZE_TYPE_ITEM);
    g_signal_connect_after (trash, "add-item",
        G_CALLBACK (midori_trash_add_item_cb), NULL);
    g_signal_connect_after (trash, "remove-item",
        G_CALLBACK (midori_trash_remove_item_cb), NULL);
    katze_assign (config_file, g_build_filename (config, "tabtrash.xbel", NULL));
    error = NULL;
    if (!midori_array_from_file (trash, config_file, "xbel-tiny", &error))
    {
        if (error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf (error_messages,
                _("The trash couldn't be loaded: %s\n"), error->message);
        g_error_free (error);
    }

    KatzeArray* history;
    if (!(history = midori_history_new (&errmsg)))
    {
        g_string_append_printf (error_messages,
            _("The history couldn't be loaded: %s\n"), errmsg);
        katze_assign (errmsg, NULL);
    }

    katze_assign (config_file, midori_paths_get_config_filename_for_reading ("speeddial"));
    MidoriSpeedDial* dial = midori_speed_dial_new (config_file, NULL);

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
        if (midori_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
            return (void*)0xdeadbeef;
        gtk_widget_destroy (dialog);
    }
    g_string_free (error_messages, TRUE);

    g_object_set_data (G_OBJECT (app), "execute-commands", execute_commands);
    g_object_set_data (G_OBJECT (app), "open-uris", open_uris);
    g_object_set_data_full (G_OBJECT (app), "extensions", extensions, (GDestroyNotify)g_strfreev);
    katze_item_set_parent (KATZE_ITEM (session), app);

    katze_assign (config_file, midori_paths_get_config_filename_for_reading ("search"));
    midori_search_engines_set_filename (search_engines, config_file);

    if ((midori_app_get_crashed (app)
     && katze_object_get_boolean (settings, "show-crash-dialog")
     && open_uris && !execute_commands)
     || diagnostic_dialog)
    {
        gint response = midori_frontend_diagnostic_dialog (app, settings, session);
        if (response == G_MAXINT)
            return NULL;
        load_on_startup = response;
    }
    katze_item_set_parent (KATZE_ITEM (session), NULL);
    g_object_unref (session);
    g_object_set_data (G_OBJECT (settings), "load-on-startup", GINT_TO_POINTER (load_on_startup));

    g_object_set (app, "settings", settings,
                       "bookmarks", bookmarks,
                       "trash", trash,
                       "search-engines", search_engines,
                       "history", history,
                       "speed-dial", dial,
                       NULL);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (midori_app_add_browser_cb), NULL);

    midori_session_persistent_settings (settings, app);

    g_idle_add (midori_load_soup_session_full, settings);
    g_idle_add (midori_load_extensions, app);
    return app;
}

void
midori_normal_app_on_quit (MidoriApp* app)
{
    MidoriWebSettings* settings = katze_object_get_object (app, "settings");
    MidoriBookmarksDb* bookmarks = katze_object_get_object (app, "bookmarks");
    KatzeArray* history = katze_object_get_object (app, "history");

    g_object_notify (G_OBJECT (settings), "load-on-startup");
    midori_bookmarks_db_on_quit (bookmarks);
    midori_history_on_quit (history, settings);
    midori_private_data_on_quit (settings);

    MidoriStartup load_on_startup = katze_object_get_int (settings, "load-on-startup");
    if (load_on_startup < MIDORI_STARTUP_LAST_OPEN_PAGES)
    {
        gchar* config_file = midori_paths_get_config_filename_for_writing ("session.xbel");
        g_unlink (config_file);
    }
}

