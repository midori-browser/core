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

static MidoriWebSettings*
settings_new_from_file (const gchar* filename)
{
    MidoriWebSettings* settings = midori_web_settings_new ();
    GKeyFile* key_file = g_key_file_new ();
    GError* error = NULL;
    if (!g_key_file_load_from_file (key_file, filename,
                                   G_KEY_FILE_KEEP_COMMENTS, &error))
    {
        if (error->code != G_FILE_ERROR_NOENT)
            printf (_("The configuration couldn't be loaded. %s\n"),
                    error->message);
        g_error_free (error);
    }
    GObjectClass* class = G_OBJECT_GET_CLASS (settings);
    guint i, n_properties;
    GParamSpec** pspecs = g_object_class_list_properties (class, &n_properties);
    for (i = 0; i < n_properties; i++)
    {
        GParamSpec* pspec = pspecs[i];
        if (!(pspec->flags & G_PARAM_WRITABLE))
            continue;
        GType type = G_PARAM_SPEC_TYPE (pspec);
        const gchar* property = g_param_spec_get_name (pspec);
        if (type == G_TYPE_PARAM_STRING)
        {
            gchar* string = sokoke_key_file_get_string_default (key_file,
                "settings", property,
                G_PARAM_SPEC_STRING (pspec)->default_value, NULL);
            g_object_set (settings, property, string, NULL);
            g_free (string);
        }
        else if (type == G_TYPE_PARAM_INT)
        {
            guint integer = sokoke_key_file_get_integer_default (key_file,
                "settings", property,
                G_PARAM_SPEC_INT (pspec)->default_value, NULL);
            g_object_set (settings, property, integer, NULL);
        }
        else if (type == G_TYPE_PARAM_BOOLEAN)
        {
            gboolean boolean = sokoke_key_file_get_boolean_default (key_file,
                "settings", property,
                G_PARAM_SPEC_BOOLEAN (pspec)->default_value, NULL);
            g_object_set (settings, property, boolean, NULL);
        }
        else if (type == G_TYPE_PARAM_ENUM)
        {
            GEnumClass* enum_class = G_ENUM_CLASS (
                g_type_class_ref (pspec->value_type));
            GEnumValue* enum_value = g_enum_get_value (enum_class,
                G_PARAM_SPEC_ENUM (pspec)->default_value);
            gchar* string = sokoke_key_file_get_string_default (key_file,
                "settings", property,
                enum_value->value_name, NULL);
            enum_value = g_enum_get_value_by_name (enum_class, string);
            g_object_set (settings, property, enum_value->value, NULL);
            g_free (string);
            g_type_class_unref (enum_class);
        }
        else
            g_warning ("Unhandled settings property '%s'", property);
    }
    return settings;
}

static gboolean
settings_save_to_file (MidoriWebSettings* settings,
                       const gchar*       filename,
                       GError**           error)
{
    GKeyFile* key_file = g_key_file_new ();
    GObjectClass* class = G_OBJECT_GET_CLASS (settings);
    guint i, n_properties;
    GParamSpec** pspecs = g_object_class_list_properties (class, &n_properties);
    for (i = 0; i < n_properties; i++)
    {
        GParamSpec* pspec = pspecs[i];
        GType type = G_PARAM_SPEC_TYPE (pspec);
        const gchar* property = g_param_spec_get_name (pspec);
        if (!(pspec->flags & G_PARAM_WRITABLE))
        {
            gchar* comment = g_strdup_printf ("# %s", property);
            g_key_file_set_string (key_file, "settings", comment, "");
            g_free (comment);
            continue;
        }
        if (type == G_TYPE_PARAM_STRING)
        {
            const gchar* string;
            g_object_get (settings, property, &string, NULL);
            g_key_file_set_string (key_file, "settings", property,
                                   string ? string : "");
        }
        else if (type == G_TYPE_PARAM_INT)
        {
            gint integer;
            g_object_get (settings, property, &integer, NULL);
            g_key_file_set_integer (key_file, "settings", property, integer);
        }
        else if (type == G_TYPE_PARAM_BOOLEAN)
        {
            gboolean boolean;
            g_object_get (settings, property, &boolean, NULL);
            g_key_file_set_boolean (key_file, "settings", property, boolean);
        }
        else if (type == G_TYPE_PARAM_ENUM)
        {
            GEnumClass* enum_class = G_ENUM_CLASS (
                g_type_class_ref (pspec->value_type));
            gint integer;
            g_object_get (settings, property, &integer, NULL);
            GEnumValue* enum_value = g_enum_get_value (enum_class, integer);
            g_key_file_set_string (key_file, "settings", property,
                                   enum_value->value_name);
        }
        else
            g_warning ("Unhandled settings property '%s'", property);
    }
    gboolean saved = sokoke_key_file_save_to_file (key_file, filename, error);
    g_key_file_free (key_file);
    return saved;
}

int main(int argc, char** argv)
{
    locale_init();
    g_set_application_name(_("midori"));

    // Parse cli options
    gboolean version = FALSE;
    GOptionEntry entries[] =
    {
     { "version", 'v', 0, G_OPTION_ARG_NONE, &version,
       N_("Display program version"), NULL }
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
    GString* error_messages = g_string_new (NULL);
    gchar* config_path = g_build_filename (g_get_user_config_dir (),
                                           PACKAGE_NAME, NULL);
    g_mkdir_with_parents (config_path, 0755);
    gchar* config_file = g_build_filename (config_path, "config", NULL);
    error = NULL;
    MidoriWebSettings* settings = settings_new_from_file (config_file);
    webSettings = settings;
    katze_assign (config_file, g_build_filename (config_path, "accels", NULL));
    gtk_accel_map_load (config_file);
    katze_assign (config_file, g_build_filename (config_path, "search", NULL));
    error = NULL;
    searchEngines = search_engines_new ();
    if (!search_engines_from_file (&searchEngines, config_file, &error))
    {
        // FIXME: We may have a "file empty" error, how do we recognize that?
        /*if (error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf (error_messages,
                _("The search engines couldn't be loaded. %s\n"),
                error->message);*/
        g_error_free (error);
    }
    katze_assign (config_file, g_build_filename (config_path, "bookmarks.xbel",
                                                 NULL));
    bookmarks = katze_xbel_folder_new();
    error = NULL;
    if (!katze_xbel_folder_from_file (bookmarks, config_file, &error))
    {
        if (error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf (error_messages,
                _("The bookmarks couldn't be loaded. %s\n"), error->message);
        g_error_free (error);
    }
    g_free (config_file);
    KatzeXbelItem* _session = katze_xbel_folder_new();
    config = config_new ();
    if(config->startup == CONFIG_STARTUP_SESSION)
    {
        config_file = g_build_filename (config_path, "session.xbel", NULL);
        error = NULL;
        if (!katze_xbel_folder_from_file (_session, config_file, &error))
        {
            if (error->code != G_FILE_ERROR_NOENT)
                g_string_append_printf (error_messages,
                    _("The session couldn't be loaded. %s\n"), error->message);
            g_error_free (error);
        }
        g_free (config_file);
    }
    config_file = g_build_filename (config_path, "tabtrash.xbel", NULL);
    KatzeXbelItem* xbel_trash = katze_xbel_folder_new ();
    error = NULL;
    if (!katze_xbel_folder_from_file (xbel_trash, config_file, &error))
    {
        if (error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf(error_messages,
                _("The trash couldn't be loaded. %s\n"), error->message);
        g_error_free (error);
    }
    g_free (config_file);

    // In case of errors
    if (error_messages->len)
    {
        GtkWidget* dialog = gtk_message_dialog_new(NULL
         , 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE
         , _("The following errors occured:"));
        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), FALSE);
        gtk_window_set_title(GTK_WINDOW(dialog), g_get_application_name());
        // FIXME: Use custom program icon
        gtk_window_set_icon_name(GTK_WINDOW(dialog), "web-browser");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog)
         , "%s", error_messages->str);
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
            g_string_free(error_messages, TRUE);
            return 0;
        }
        gtk_widget_destroy(dialog);
        /* FIXME: Since we will overwrite files that could not be loaded
                  , would we want to make backups? */
    }
    g_string_free (error_messages, TRUE);

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
    g_free (config_path);

    stock_items_init();

    MidoriTrash* trash = g_object_new (MIDORI_TYPE_TRASH,
                                       "limit", 10,
                                       NULL);
    guint n = katze_xbel_folder_get_n_items (xbel_trash);
    guint i;
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
    config_path = g_build_filename (g_get_user_config_dir(), PACKAGE_NAME,
                                    NULL);
    g_mkdir_with_parents (config_path, 0755);
    config_file = g_build_filename (config_path, "search", NULL);
    error = NULL;
    if (!search_engines_to_file (searchEngines, config_file, &error))
    {
        g_warning("The search engines couldn't be saved. %s", error->message);
        g_error_free(error);
    }
    search_engines_free(searchEngines);
    g_free (config_file);
    config_file = g_build_filename (config_path, "bookmarks.xbel", NULL);
    error = NULL;
    if (!katze_xbel_folder_to_file (bookmarks, config_file, &error))
    {
        g_warning("The bookmarks couldn't be saved. %s", error->message);
        g_error_free(error);
    }
    katze_xbel_item_unref(bookmarks);
    g_free (config_file);
    config_file = g_build_filename (config_path, "tabtrash.xbel", NULL);
    error = NULL;
    if (!katze_xbel_folder_to_file (xbel_trash, config_file, &error))
    {
        g_warning ("The trash couldn't be saved. %s", error->message);
        g_error_free (error);
    }
    katze_xbel_item_unref (xbel_trash);
    if(config->startup == CONFIG_STARTUP_SESSION)
    {
        katze_assign (config_file, g_build_filename (config_path,
                                                     "session.xbel", NULL));
        error = NULL;
        if (!katze_xbel_folder_to_file (session, config_file, &error))
        {
            g_warning ("The session couldn't be saved. %s", error->message);
            g_error_free (error);
        }
    }
    katze_xbel_item_unref (session);
    katze_assign (config_file, g_build_filename (config_path, "config", NULL));
    error = NULL;
    if (!settings_save_to_file (settings, config_file, &error))
    {
        g_warning ("The configuration couldn't be saved. %s", error->message);
        g_error_free (error);
    }
    config_free (config);
    katze_assign (config_file, g_build_filename (config_path, "accels", NULL));
    gtk_accel_map_save (config_file);
    g_free (config_file);
    g_free (config_path);
    return 0;
}
