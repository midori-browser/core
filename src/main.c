/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "main.h"

#include "browser.h"
#include "global.h"
#include "helpers.h"
#include "sokoke.h"
#include "search.h"
#include "webView.h"
#include "../katze/katze.h"

#include <string.h>
#include <gtk/gtk.h>
#include <webkit.h>

#include "config.h"

// -- stock icons

static void stock_items_init(void)
{
    static GtkStockItem items[] =
    {
        { STOCK_LOCK_OPEN },
        { STOCK_LOCK_SECURE },
        { STOCK_LOCK_BROKEN },
        { STOCK_SCRIPT },
        { STOCK_THEME },
        { STOCK_USER_TRASH },

        { STOCK_BOOKMARK,       "Bookmark", 0, 0, NULL },
        { STOCK_BOOKMARK_NEW,   "New Bookmark", 0, 0, NULL },
        { STOCK_BOOKMARKS,      "_Bookmarks", 0, 0, NULL },
        { STOCK_DOWNLOADS,      "_Downloads", 0, 0, NULL },
        { STOCK_CONSOLE,        "_Console", 0, 0, NULL },
        { STOCK_EXTENSIONS,     "_Extensions", 0, 0, NULL },
        { STOCK_FORM_FILL,      "_Form Fill", 0, 0, NULL },
        { STOCK_HISTORY,        "History", 0, 0, NULL },
        { STOCK_HOMEPAGE,       "Homepage", 0, 0, NULL },
        { STOCK_LOCATION,       "Location Entry", 0, 0, NULL },
        { STOCK_NEWSFEED,       "Newsfeed", 0, 0, NULL },
        { STOCK_PLUGINS,        "_Plugins", 0, 0, NULL },
        { STOCK_POPUPS_BLOCKED, "Blocked Popups", 0, 0, NULL },
        { STOCK_SOURCE_VIEW,    "View Source", 0, 0, NULL },
        { STOCK_TAB_CLOSE,      "C_lose Tab", 0, 0, NULL },
        { STOCK_TAB_NEW,        "New _Tab", 0, 0, NULL },
        { STOCK_WINDOW_CLOSE,   "_Close Window", 0, 0, NULL },
        { STOCK_WINDOW_NEW,     "New _Window", 0, 0, NULL },
        #if !GTK_CHECK_VERSION(2, 10, 0)
        { GTK_STOCK_SELECT_ALL, "Select _All", 0, 0, (gchar*)"gtk20" },
        #endif
    };
    GtkIconFactory* factory = gtk_icon_factory_new();
    guint i;
    for(i = 0; i < (guint)G_N_ELEMENTS(items); i++)
    {
        GtkIconSource* iconSource = gtk_icon_source_new();
        gtk_icon_source_set_icon_name(iconSource, items[i].stock_id);
        GtkIconSet* iconSet = gtk_icon_set_new();
        gtk_icon_set_add_source(iconSet, iconSource);
        gtk_icon_source_free(iconSource);
        gtk_icon_factory_add(factory, items[i].stock_id, iconSet);
        gtk_icon_set_unref(iconSet);
    }
    gtk_stock_add_static(items, G_N_ELEMENTS(items));
    gtk_icon_factory_add_default(factory);
    g_object_unref(factory);
}

// -- main function

int main(int argc, char** argv)
{
    g_set_application_name(PACKAGE_NAME);

    // Parse cli options
    gint repeats = 2;
    gboolean version = FALSE;
    GOptionEntry entries[] =
    {
     { "repeats", 'r', 0, G_OPTION_ARG_INT, &repeats, "An unused value", "N" },
     { "version", 'v', 0, G_OPTION_ARG_NONE, &version, "Display program version", NULL }
    };

    GError* error = NULL;
    if(!gtk_init_with_args(&argc, &argv, "[URI]", entries, NULL/*GETTEXT_PACKAGE*/, &error))
    {
        g_error_free(error);
        return 1;
    }

    if(version)
    {
        g_print(PACKAGE_STRING " - Copyright (c) 2007 Christian Dywan\n\n"
         "GTK+2:      " GTK_VER "\n"
         "WebKit:     " WEBKIT_VER "\n"
         "Libsexy:    " LIBSEXY_VER "\n"
         "libXML2:    " LIBXML_VER "\n"
         "GetText:    N/A\n"
         "\n"
         "Debugging:  " SOKOKE_DEBUG_ "\n"
         "\n"
         "Please report comments, suggestions and bugs to:\n"
         "\t" PACKAGE_BUGREPORT "\n"
         "Check for new versions at:\n"
         "\thttp://software.twotoasts.de\n");
        return 0;
    }

    // Load configuration files
    GString* errorMessages = g_string_new(NULL);
    // TODO: What about default config in a global config folder?
    gchar* configPath = g_build_filename(g_get_user_config_dir(), PACKAGE_NAME, NULL);
    g_mkdir_with_parents(configPath, 0755);
    gchar* configFile = g_build_filename(configPath, "config", NULL);
    error = NULL;
    config = config_new();
    if(!config_from_file(config, configFile, &error))
    {
        if(error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf(errorMessages
             , "Configuration was not loaded. %s\n", error->message);
        g_error_free(error);
    }
    g_free(configFile);
    configFile = g_build_filename(configPath, "accels", NULL);
    gtk_accel_map_load(configFile);
    g_free(configFile);
    configFile = g_build_filename(configPath, "search", NULL);
    error = NULL;
    searchEngines = search_engines_new();
    if(!search_engines_from_file(&searchEngines, configFile, &error))
    {
        // FIXME: We may have a "file empty" error, how do we recognize that?
        /*if(error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf(errorMessages
             , "Notice: No search engines loaded. %s\n", error->message);*/
        g_error_free(error);
    }
    g_free(configFile);
    configFile = g_build_filename(configPath, "bookmarks.xbel", NULL);
    bookmarks = katze_xbel_folder_new();
    error = NULL;
    if(!katze_xbel_folder_from_file(bookmarks, configFile, &error))
    {
        if(error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf(errorMessages
             , "Bookmarks couldn't be loaded. %s\n", error->message);
        g_error_free(error);
    }
    g_free(configFile);
    KatzeXbelItem* _session = katze_xbel_folder_new();
    if(config->startup == CONFIG_STARTUP_SESSION)
    {
        configFile = g_build_filename(configPath, "session.xbel", NULL);
        error = NULL;
        if(!katze_xbel_folder_from_file(_session, configFile, &error))
        {
            if(error->code != G_FILE_ERROR_NOENT)
                g_string_append_printf(errorMessages
                 , "Session couldn't be loaded. %s\n", error->message);
            g_error_free(error);
        }
        g_free(configFile);
    }
    configFile = g_build_filename(configPath, "tabtrash.xbel", NULL);
    tabtrash = katze_xbel_folder_new();
    error = NULL;
    if(!katze_xbel_folder_from_file(tabtrash, configFile, &error))
    {
        if(error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf(errorMessages
             , "Tabtrash couldn't be loaded. %s\n", error->message);
        g_error_free(error);
    }
    g_free(configFile);

    // In case of errors
    if(errorMessages->len)
    {
        GtkWidget* dialog = gtk_message_dialog_new(NULL
         , 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE
         , "The following errors occured.");
        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), FALSE);
        gtk_window_set_title(GTK_WINDOW(dialog), g_get_application_name());
        // FIXME: Use custom program icon
        gtk_window_set_icon_name(GTK_WINDOW(dialog), "web-browser");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog)
         , "%s", errorMessages->str);
        gtk_dialog_add_buttons(GTK_DIALOG(dialog)
         , GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL
         , "_Ignore", GTK_RESPONSE_ACCEPT
         , NULL);
        if(gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT)
        {
            config_free(config);
            search_engines_free(searchEngines);
            katze_xbel_item_unref(bookmarks);
            katze_xbel_item_unref(_session);
            g_string_free(errorMessages, TRUE);
            return 0;
        }
        gtk_widget_destroy(dialog);
        /* FIXME: Since we will overwrite files that could not be loaded
                  , would we want to make backups? */
    }
    g_string_free(errorMessages, TRUE);

    // TODO: Handle any number of separate uris from argv 
    // Open as many tabs as we have uris, seperated by pipes
    gchar* uri = argc > 1 ? strtok(g_strdup(argv[1]), "|") : NULL;
    while(uri != NULL)
    {
        KatzeXbelItem* item = katze_xbel_bookmark_new();
        gchar* uriReady = magic_uri(uri, FALSE);
        katze_xbel_bookmark_set_href(item, uriReady);
        g_free(uriReady);
        katze_xbel_folder_append_item(_session, item);
        uri = strtok(NULL, "|");
    }
    g_free(uri);

    if(katze_xbel_folder_is_empty(_session))
    {
        KatzeXbelItem* item = katze_xbel_bookmark_new();
        if(config->startup == CONFIG_STARTUP_BLANK)
            katze_xbel_bookmark_set_href(item, "about:blank");
        else
            katze_xbel_bookmark_set_href(item, config->homepage);
        katze_xbel_folder_prepend_item(_session, item);
    }
    g_free(configPath);

    accel_group = gtk_accel_group_new();
    stock_items_init();
    browsers = NULL;

    webSettings = g_object_new(WEBKIT_TYPE_WEB_SETTINGS
     , "default-font-family" , config->defaultFontFamily
     , "default-font-size"   , config->defaultFontSize
     , "minimum-font-size"   , config->minimumFontSize
     , "default-encoding"    , config->defaultEncoding
     , "auto-load-images"    , config->autoLoadImages
     , "auto-shrink-images"  , config->autoShrinkImages
     , "print-backgrounds"   , config->printBackgrounds
     , "resizable-text-areas", config->resizableTextAreas
     , "user-stylesheet-uri" , config->userStylesheet ? config->userStylesheetUri : NULL
     , "enable-scripts"      , config->enableScripts
     , "enable-plugins"      , config->enablePlugins
     , NULL);

    session = katze_xbel_folder_new();
    CBrowser* browser = NULL;
    guint n = katze_xbel_folder_get_n_items(_session);
    guint i;
    for(i = 0; i < n; i++)
    {
        KatzeXbelItem* item = katze_xbel_folder_get_nth_item(_session, i);
        browser = browser_new(browser);
        webView_open(browser->webView, katze_xbel_bookmark_get_href(item));
    }
    katze_xbel_item_unref(_session);

    gtk_main();

    // Save configuration files
    configPath = g_build_filename(g_get_user_config_dir(), PACKAGE_NAME, NULL);
    g_mkdir_with_parents(configPath, 0755);
    configFile = g_build_filename(configPath, "search", NULL);
    error = NULL;
    if(!search_engines_to_file(searchEngines, configFile, &error))
    {
        g_warning("Search engines couldn't be saved. %s", error->message);
        g_error_free(error);
    }
    search_engines_free(searchEngines);
    g_free(configFile);
    configFile = g_build_filename(configPath, "bookmarks.xbel", NULL);
    error = NULL;
    if(!katze_xbel_folder_to_file(bookmarks, configFile, &error))
    {
        g_warning("Bookmarks couldn't be saved. %s", error->message);
        g_error_free(error);
    }
    katze_xbel_item_unref(bookmarks);
    g_free(configFile);
    configFile = g_build_filename(configPath, "tabtrash.xbel", NULL);
    error = NULL;
    if(!katze_xbel_folder_to_file(tabtrash, configFile, &error))
    {
        g_warning("Tabtrash couldn't be saved. %s", error->message);
        g_error_free(error);
    }
    katze_xbel_item_unref(tabtrash);
    g_free(configFile);
    if(config->startup == CONFIG_STARTUP_SESSION)
    {
        configFile = g_build_filename(configPath, "session.xbel", NULL);
        error = NULL;
        if(!katze_xbel_folder_to_file(session, configFile, &error))
        {
            g_warning("Session couldn't be saved. %s", error->message);
            g_error_free(error);
        }
        g_free(configFile);
    }
    katze_xbel_item_unref(session);
    configFile = g_build_filename(configPath, "config", NULL);
    error = NULL;
    if(!config_to_file(config, configFile, &error))
    {
        g_warning("Configuration couldn't be saved. %s", error->message);
        g_error_free(error);
    }
    config_free(config);
    g_free(configFile);
    configFile = g_build_filename(configPath, "accels", NULL);
    gtk_accel_map_save(configFile);
    g_free(configFile);
    g_free(configPath);
    return 0;
}
