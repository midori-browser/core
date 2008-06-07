/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "main.h"

#include "sokoke.h"
#include "search.h"

#include "midori-app.h"
#include "midori-websettings.h"
#include "midori-trash.h"
#include "midori-browser.h"
#include "gjs.h"

#include <katze/katze.h>
#include <string.h>
#include <gtk/gtk.h>

#include "config.h"

#ifdef ENABLE_NLS
    #include <libintl.h>
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
        { STOCK_BOOKMARK_ADD,   N_("_Add Bookmark"), 0, 0, NULL },
        { STOCK_FORM_FILL,      N_("_Form Fill"), 0, 0, NULL },
        { STOCK_HOMEPAGE,       N_("_Homepage"), 0, 0, NULL },
        { STOCK_TAB_NEW,        N_("New _Tab"), 0, 0, NULL },
        { STOCK_WINDOW_NEW,     N_("New _Window"), 0, 0, NULL },
        #if !GTK_CHECK_VERSION(2, 10, 0)
        { GTK_STOCK_SELECT_ALL, N_("Select _All"), 0, 0, NULL },
        #endif
        #if !GTK_CHECK_VERSION(2, 8, 0)
        { GTK_STOCK_FULLSCREEN, N_("_Fullscreen"), 0, 0, NULL },
        { GTK_STOCK_LEAVE_FULLSCREEN, N_("_Leave Fullscreen"), 0, 0, NULL },
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

static void
locale_init (void)
{
#ifdef ENABLE_NLS
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
            gint integer = sokoke_key_file_get_integer_default (key_file,
                "settings", property,
                G_PARAM_SPEC_INT (pspec)->default_value, NULL);
            g_object_set (settings, property, integer, NULL);
        }
        else if (type == G_TYPE_PARAM_FLOAT)
        {
            gdouble number = sokoke_key_file_get_double_default (key_file,
                "settings", property,
                G_PARAM_SPEC_FLOAT (pspec)->default_value, NULL);
            g_object_set (settings, property, number, NULL);
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
            if (enum_value)
                 g_object_set (settings, property, enum_value->value, NULL);
             else
                 g_warning (_("Value '%s' is invalid for %s"),
                            string, property);

            g_free (string);
            g_type_class_unref (enum_class);
        }
        else
            g_warning (_("Unhandled settings value '%s'"), property);
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
        else if (type == G_TYPE_PARAM_FLOAT)
        {
            gdouble number;
            g_object_get (settings, property, &number, NULL);
            g_key_file_set_double (key_file, "settings", property, number);
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
            g_warning (_("Unhandled settings value '%s'"), property);
    }
    gboolean saved = sokoke_key_file_save_to_file (key_file, filename, error);
    g_key_file_free (key_file);
    return saved;
}

int
main (int argc, char** argv)
{
    MidoriStartup load_on_startup;
    gchar* homepage;

    locale_init ();
    g_set_application_name (_("midori"));

    // Parse cli options
    gboolean version = FALSE;
    GOptionEntry entries[] =
    {
     { "version", 'v', 0, G_OPTION_ARG_NONE, &version,
       N_("Display program version"), NULL },
     { NULL }
    };

    GError* error = NULL;
    if (!gtk_init_with_args (&argc, &argv, _("[URL]"), entries,
                             GETTEXT_PACKAGE, &error))
    {
        g_error_free (error);
        return 1;
    }

    if (version)
    {
        g_print (
          "%s %s\n\n"
          "Copyright (c) 2007-2008 Christian Dywan\n\n"
          "%s\n"
          "\t%s\n\n"
          "%s\n"
          "\thttp://software.twotoasts.de\n",
          _("midori"), PACKAGE_VERSION,
          _("Please report comments, suggestions and bugs to:"),
          PACKAGE_BUGREPORT,
          _("Check for new versions at:")
        );
        return 0;
    }

    // Standalone gjs support
    if (argc > 1 && argv[1] && g_str_has_suffix (argv[1], ".js"))
    {
        JSGlobalContextRef js_context = gjs_global_context_new ();
        gchar* exception = NULL;
        gjs_script_from_file (js_context, argv[1], &exception);
        JSGlobalContextRelease (js_context);
        if (!exception)
            return 0;
        printf ("%s - Exception: %s\n", argv[1], exception);
        return 1;
    }

    // Load configuration files
    GString* error_messages = g_string_new (NULL);
    gchar* config_path = g_build_filename (g_get_user_config_dir (),
                                           PACKAGE_NAME, NULL);
    g_mkdir_with_parents (config_path, 0755);
    gchar* config_file = g_build_filename (config_path, "config", NULL);
    error = NULL;
    MidoriWebSettings* settings = settings_new_from_file (config_file);
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
    KatzeXbelItem* _session = katze_xbel_folder_new ();
    g_object_get (settings, "load-on-startup", &load_on_startup, NULL);
    if (load_on_startup == MIDORI_STARTUP_LAST_OPEN_PAGES)
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
    gchar* uri = argc > 1 ? strtok (g_strdup(argv[1]), "|") : NULL;
    while (uri != NULL)
    {
        KatzeXbelItem* item = katze_xbel_bookmark_new ();
        gchar* uri_ready = sokoke_magic_uri (uri, NULL);
        katze_xbel_bookmark_set_href (item, uri_ready);
        g_free (uri_ready);
        katze_xbel_folder_append_item (_session, item);
        uri = strtok (NULL, "|");
    }
    g_free (uri);

    if (katze_xbel_folder_is_empty (_session))
    {
        KatzeXbelItem* item = katze_xbel_bookmark_new ();
        if (load_on_startup == MIDORI_STARTUP_BLANK_PAGE)
            katze_xbel_bookmark_set_href (item, "");
        else
        {
            g_object_get (settings, "homepage", &homepage, NULL);
            katze_xbel_bookmark_set_href (item, homepage);
            g_free (homepage);
        }
        katze_xbel_folder_prepend_item (_session, item);
    }
    g_free (config_path);

    stock_items_init ();

    MidoriApp* app = midori_app_new ();
    g_object_set (app, "settings", settings, NULL);

    MidoriTrash* trash = midori_app_get_trash (app);
    guint n = katze_xbel_folder_get_n_items (xbel_trash);
    guint i;
    for (i = 0; i < n; i++)
    {
        KatzeXbelItem* item = katze_xbel_folder_get_nth_item (xbel_trash, i);
        midori_trash_prepend_xbel_item (trash, item);
    }

    MidoriBrowser* browser = g_object_new (MIDORI_TYPE_BROWSER,
                                           "settings", settings,
                                           "trash", trash,
                                           NULL);
    g_signal_emit_by_name (app, "add-browser", browser);

    gtk_widget_show (GTK_WIDGET (browser));

    KatzeXbelItem* session = katze_xbel_folder_new ();
    n = katze_xbel_folder_get_n_items (_session);
    for (i = 0; i < n; i++)
    {
        KatzeXbelItem* item = katze_xbel_folder_get_nth_item (_session, i);
        midori_browser_add_xbel_item (browser, item);
    }
    // FIXME: Switch to the last active page
    KatzeXbelItem* item = katze_xbel_folder_get_nth_item (_session, 0);
    if (!strcmp (katze_xbel_bookmark_get_href (item), ""))
        midori_browser_activate_action (browser, "Location");
    katze_xbel_item_unref (_session);

    // Load extensions
    JSGlobalContextRef js_context = gjs_global_context_new ();
    // FIXME: We want to honor system installed addons as well
    gchar* addon_path = g_build_filename (g_get_user_data_dir (), PACKAGE_NAME,
                                          "extensions", NULL);
    GDir* addon_dir = g_dir_open (addon_path, 0, NULL);
    if (addon_dir)
    {
        const gchar* filename;
        while ((filename = g_dir_read_name (addon_dir)))
        {
            gchar* fullname = g_build_filename (addon_path, filename, NULL);
            gchar* exception = NULL;
            gjs_script_from_file (js_context, fullname, &exception);
            if (exception)
            // FIXME: Do we want to print this somewhere else?
            // FIXME Convert the filename to UTF8
                printf ("%s - Exception: %s\n", filename, exception);
            g_free (fullname);
        }
        g_dir_close (addon_dir);
    }

    gtk_main ();

    JSGlobalContextRelease (js_context);

    // Save configuration files
    config_path = g_build_filename (g_get_user_config_dir(), PACKAGE_NAME,
                                    NULL);
    g_mkdir_with_parents (config_path, 0755);
    config_file = g_build_filename (config_path, "search", NULL);
    error = NULL;
    if (!search_engines_to_file (searchEngines, config_file, &error))
    {
        g_warning (_("The search engines couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    search_engines_free(searchEngines);
    g_free (config_file);
    config_file = g_build_filename (config_path, "bookmarks.xbel", NULL);
    error = NULL;
    if (!katze_xbel_folder_to_file (bookmarks, config_file, &error))
    {
        g_warning (_("The bookmarks couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    katze_xbel_item_unref(bookmarks);
    g_free (config_file);
    config_file = g_build_filename (config_path, "tabtrash.xbel", NULL);
    error = NULL;
    if (!katze_xbel_folder_to_file (xbel_trash, config_file, &error))
    {
        g_warning (_("The trash couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    katze_xbel_item_unref (xbel_trash);
    g_object_get (settings, "load-on-startup", &load_on_startup, NULL);
    if(load_on_startup == MIDORI_STARTUP_LAST_OPEN_PAGES)
    {
        katze_assign (config_file, g_build_filename (config_path,
                                                     "session.xbel", NULL));
        error = NULL;
        if (!katze_xbel_folder_to_file (session, config_file, &error))
        {
            g_warning (_("The session couldn't be saved. %s"), error->message);
            g_error_free (error);
        }
    }
    katze_xbel_item_unref (session);
    katze_assign (config_file, g_build_filename (config_path, "config", NULL));
    error = NULL;
    if (!settings_save_to_file (settings, config_file, &error))
    {
        g_warning (_("The configuration couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    katze_assign (config_file, g_build_filename (config_path, "accels", NULL));
    gtk_accel_map_save (config_file);
    g_free (config_file);
    g_free (config_path);
    return 0;
}
