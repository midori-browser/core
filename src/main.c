/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "main.h"

#include "global.h"
#include "helpers.h"
#include "sokoke.h"
#include "search.h"

#include "midori-websettings.h"
#include "midori-trash.h"
#include "midori-browser.h"
#include <katze/katze.h>

#include <string.h>
#include <gtk/gtk.h>

#include "config.h"

#ifdef ENABLE_NLS
# include <libintl.h>
# if HAVE_LOCALE_H
#  include <locale.h>
# endif
#endif

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

        { STOCK_BOOKMARK,       N_("Bookmark"), 0, 0, NULL },
        { STOCK_BOOKMARK_NEW,   N_("New Bookmark"), 0, 0, NULL },
        { STOCK_FORM_FILL,      N_("_Form Fill"), 0, 0, NULL },
        { STOCK_HOMEPAGE,       N_("Homepage"), 0, 0, NULL },
        { STOCK_TAB_NEW,        N_("New _Tab"), 0, 0, NULL },
        { STOCK_WINDOW_NEW,     N_("New _Window"), 0, 0, NULL },
        #if !GTK_CHECK_VERSION(2, 10, 0)
        { GTK_STOCK_SELECT_ALL, N_("Select _All", 0, 0, NULL },
        #endif
        #if !GTK_CHECK_VERSION(2, 8, 0)
        { GTK_STOCK_FULLSCREEN, N_("_Fullscreen"), 0, 0, NULL },
        { GTK_STOCK_FULLSCREEN, N_("_Leave Fullscreen"), 0, 0, NULL },
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

static gboolean
midori_browser_delete_event_cb (MidoriBrowser* browser,
                                GdkEvent*      event,
                                GList*         browsers)
{
    browsers = g_list_remove (browsers, browser);
    if (g_list_nth (browsers, 0))
        return FALSE;
    gtk_main_quit ();
    return TRUE;
}

static void
midori_browser_quit_cb (MidoriBrowser* browser,
                        GdkEvent*      event,
                        GList*         browsers)
{
    gtk_main_quit ();
}

static void
midori_browser_new_window_cb (MidoriBrowser* browser,
                              const gchar*   uri,
                              GList*         browsers)
{
    MidoriBrowser* new_browser = g_object_new (MIDORI_TYPE_BROWSER,
                                               // "settings", settings,
                                               // "trash", trash,
                                               NULL);
    // gtk_window_add_accel_group (GTK_WINDOW (browser), accel_group);
    g_object_connect (new_browser,
        "signal::new-window", midori_browser_new_window_cb, browsers,
        "signal::delete-event", midori_browser_delete_event_cb, browsers,
        "signal::quit", midori_browser_quit_cb, browsers,
        NULL);
    browsers = g_list_prepend(browsers, new_browser);
    gtk_widget_show (GTK_WIDGET (new_browser));

    midori_browser_append_uri (new_browser, uri);
}

static void
locale_init (void)
{
#ifdef ENABLE_NLS

#if HAVE_LOCALE_H
    setlocale (LC_ALL, "");
#endif

    bindtextdomain (GETTEXT_PACKAGE, MIDORI_LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif
}

int main(int argc, char** argv)
{
    locale_init();
    g_set_application_name(_("midori"));

    // Parse cli options
    gint repeats = 2;
    gboolean version = FALSE;
    GOptionEntry entries[] =
    {
     { "version", 'v', 0, G_OPTION_ARG_NONE, &version, N_("Display program version"), NULL }
    };

    GError* error = NULL;
    if(!gtk_init_with_args(&argc, &argv, _("[URL]"), entries, GETTEXT_PACKAGE, &error))
    {
        g_error_free(error);
        return 1;
    }

    if(version)
    {
        g_print(
          "%s %s - Copyright (c) 2007-2008 Christian Dywan\n\n"
          "GTK+2:  \t\t%s\n"
          "WebKit: \t\t%s\n"
          "Libsexy:\t\t%s\n"
          "libXML2:\t\t%s\n"
          "\n"
          "%s:\t\t%s\n"
          "\n"
          "%s\n"
          "\t%s\n"
          "%s\n"
          "\thttp://software.twotoasts.de\n",
          _("midori"), PACKAGE_VERSION,
          GTK_VER, WEBKIT_VER, LIBSEXY_VER, LIBXML_VER,
          _("Debugging"), SOKOKE_DEBUG_,
          _("Please report comments, suggestions and bugs to:"),
          PACKAGE_BUGREPORT,
          _("Check for new versions at:")
        );
        return 0;
    }

    // Load configuration files
    GString* errorMessages = g_string_new(NULL);
    // TODO: What about default config in a global config folder?
    gchar* configPath = g_build_filename(g_get_user_config_dir(), PACKAGE_NAME, NULL);
    g_mkdir_with_parents(configPath, 0755);
    gchar* configFile = g_build_filename(configPath, "config", NULL);
    error = NULL;
    /*CConfig* */config = config_new();
    if(!config_from_file(config, configFile, &error))
    {
        if(error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf(errorMessages
             , _("The configuration couldn't be loaded. %s\n"), error->message);
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
             , _("The search engines couldn't be loaded. %s\n"), error->message);*/
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
             , _("The bookmarks couldn't be loaded. %s\n"), error->message);
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
                 , _("The session couldn't be loaded. %s\n"), error->message);
            g_error_free(error);
        }
        g_free(configFile);
    }
    configFile = g_build_filename(configPath, "tabtrash.xbel", NULL);
    KatzeXbelItem* xbel_trash = katze_xbel_folder_new();
    error = NULL;
    if(!katze_xbel_folder_from_file(xbel_trash, configFile, &error))
    {
        if(error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf(errorMessages
             , _("The trash couldn't be loaded. %s\n"), error->message);
        g_error_free(error);
    }
    g_free(configFile);

    // In case of errors
    if(errorMessages->len)
    {
        GtkWidget* dialog = gtk_message_dialog_new(NULL
         , 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE
         , _("The following errors occured:"));
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
            katze_xbel_item_unref(xbel_trash);
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
            katze_xbel_bookmark_set_href(item, "");
        else
            katze_xbel_bookmark_set_href(item, config->homepage);
        katze_xbel_folder_prepend_item(_session, item);
    }
    g_free(configPath);

    stock_items_init();

    MidoriWebSettings* settings;
    settings = g_object_new (MIDORI_TYPE_WEB_SETTINGS,
                             "default-font-family", config->defaultFontFamily,
                             "default-font-size", config->defaultFontSize,
                             "minimum-font-size", config->minimumFontSize,
                             "default-encoding", config->defaultEncoding,
                             "auto-load-images", config->autoLoadImages,
                             "auto-shrink-images", config->autoShrinkImages,
                             "print-backgrounds", config->printBackgrounds,
                             "resizable-text-areas", config->resizableTextAreas,
                             "user-stylesheet-uri",
                             config->userStylesheet ?
                             config->userStylesheetUri : NULL,
                             "enable-scripts", config->enableScripts,
                             "enable-plugins", config->enablePlugins,
                             "tab-label-size", config->tabSize,
                             "close-button", config->tabClose,
                             "middle-click-goto", config->middleClickGoto,
                             NULL);
    webSettings = settings;

    MidoriTrash* trash = g_object_new (MIDORI_TYPE_TRASH,
                                       "limit", 10,
                                       NULL);
    guint i;
    guint n = katze_xbel_folder_get_n_items (xbel_trash);
    for (i = 0; i < n; i++)
    {
        KatzeXbelItem* item = katze_xbel_folder_get_nth_item (xbel_trash, i);
        midori_trash_prepend_xbel_item (trash, item);
    }

    GtkAccelGroup* accel_group = gtk_accel_group_new();
    GList* browsers = NULL;

    MidoriBrowser* browser = g_object_new (MIDORI_TYPE_BROWSER,
                                           "settings", settings,
                                           "trash", trash,
                                           NULL);
    gtk_window_add_accel_group (GTK_WINDOW (browser), accel_group);
    g_object_connect (browser,
        "signal::new-window", midori_browser_new_window_cb, browsers,
        "signal::delete-event", midori_browser_delete_event_cb, browsers,
        "signal::quit", midori_browser_quit_cb, browsers,
        NULL);
    browsers = g_list_prepend(browsers, browser);
    gtk_widget_show (GTK_WIDGET (browser));

    KatzeXbelItem* session = katze_xbel_folder_new ();
    n = katze_xbel_folder_get_n_items (_session);
    for (i = 0; i < n; i++)
    {
        KatzeXbelItem* item = katze_xbel_folder_get_nth_item (_session, i);
        midori_browser_append_xbel_item (browser, item);
    }
    katze_xbel_item_unref (_session);

    gtk_main ();

    g_object_unref (accel_group);

    // Save configuration files
    configPath = g_build_filename(g_get_user_config_dir(), PACKAGE_NAME, NULL);
    g_mkdir_with_parents(configPath, 0755);
    configFile = g_build_filename(configPath, "search", NULL);
    error = NULL;
    if(!search_engines_to_file(searchEngines, configFile, &error))
    {
        g_warning("The search engines couldn't be saved. %s", error->message);
        g_error_free(error);
    }
    search_engines_free(searchEngines);
    g_free(configFile);
    configFile = g_build_filename(configPath, "bookmarks.xbel", NULL);
    error = NULL;
    if(!katze_xbel_folder_to_file(bookmarks, configFile, &error))
    {
        g_warning("The bookmarks couldn't be saved. %s", error->message);
        g_error_free(error);
    }
    katze_xbel_item_unref(bookmarks);
    g_free(configFile);
    configFile = g_build_filename(configPath, "tabtrash.xbel", NULL);
    error = NULL;
    if (!katze_xbel_folder_to_file (xbel_trash, configFile, &error))
    {
        g_warning ("The trash couldn't be saved. %s", error->message);
        g_error_free (error);
    }
    katze_xbel_item_unref (xbel_trash);
    g_free (configFile);
    if(config->startup == CONFIG_STARTUP_SESSION)
    {
        configFile = g_build_filename(configPath, "session.xbel", NULL);
        error = NULL;
        if(!katze_xbel_folder_to_file(session, configFile, &error))
        {
            g_warning("The session couldn't be saved. %s", error->message);
            g_error_free(error);
        }
        g_free(configFile);
    }
    katze_xbel_item_unref(session);
    configFile = g_build_filename(configPath, "config", NULL);
    error = NULL;
    if(!config_to_file(config, configFile, &error))
    {
        g_warning("The configuration couldn't be saved. %s", error->message);
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
