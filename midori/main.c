/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

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
#include "midori-websettings.h"
#include "midori-browser.h"
#include "midori-stock.h"

#include "sokoke.h"
#include "gjs.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#if ENABLE_NLS
    #include <libintl.h>
#endif

static void
stock_items_init (void)
{
    typedef struct
    {
        gchar* stock_id;
        gchar* label;
        GdkModifierType modifier;
        guint keyval;
        gchar* fallback;
    } FatStockItem;

    static FatStockItem items[] =
    {
        { STOCK_EXTENSION, NULL, 0, 0, GTK_STOCK_CONVERT },
        { STOCK_NEWS_FEED, NULL, 0, 0, GTK_STOCK_INDEX },
        { STOCK_SCRIPT, NULL, 0, 0, GTK_STOCK_EXECUTE },
        { STOCK_STYLE, NULL, 0, 0, GTK_STOCK_SELECT_COLOR },
        { STOCK_TRANSFER, NULL, 0, 0, GTK_STOCK_SAVE },

        { STOCK_BOOKMARK,       N_("_Bookmark"), 0, 0, GTK_STOCK_FILE },
        { STOCK_BOOKMARKS,      N_("_Bookmarks"), 0, 0, GTK_STOCK_DIRECTORY },
        { STOCK_BOOKMARK_ADD,   N_("_Add Bookmark"), 0, 0, GTK_STOCK_ADD },
        { STOCK_CONSOLE,        N_("_Console"), 0, 0, GTK_STOCK_DIALOG_WARNING },
        { STOCK_DISABLE,        N_("_Disable"), 0, 0, GTK_STOCK_NO },
        { STOCK_ENABLE,         N_("_Enable"), 0, 0, GTK_STOCK_YES },
        { STOCK_EXTENSIONS,     N_("_Extensions"), 0, 0, GTK_STOCK_CONVERT },
        { STOCK_HISTORY,        N_("_History"), 0, 0, GTK_STOCK_SORT_ASCENDING },
        { STOCK_HOMEPAGE,       N_("_Homepage"), 0, 0, GTK_STOCK_HOME },
        { STOCK_PAGE_HOLDER,    N_("_Pageholder"), 0, 0, GTK_STOCK_ORIENTATION_PORTRAIT },
        { STOCK_SCRIPTS,        N_("_Userscripts"), 0, 0, GTK_STOCK_EXECUTE },
        { STOCK_STYLES,         N_("User_styles"), 0, 0, GTK_STOCK_SELECT_COLOR },
        { STOCK_TAB_NEW,        N_("New _Tab"), 0, 0, GTK_STOCK_ADD },
        { STOCK_TRANSFERS,      N_("_Transfers"), 0, 0, GTK_STOCK_SAVE },
        { STOCK_USER_TRASH,     N_("_Closed Tabs and Windows"), 0, 0, "gtk-undo-ltr" },
        { STOCK_WINDOW_NEW,     N_("New _Window"), 0, 0, GTK_STOCK_ADD },
        #if !GTK_CHECK_VERSION(2, 10, 0)
        { GTK_STOCK_SELECT_ALL, N_("Select _All"), 0, 0, NULL },
        #endif
        #if !GTK_CHECK_VERSION(2, 8, 0)
        { GTK_STOCK_FULLSCREEN, N_("_Fullscreen"), 0, 0, NULL },
        { GTK_STOCK_LEAVE_FULLSCREEN, N_("_Leave Fullscreen"), 0, 0, NULL },
        #endif
    };

    GtkIconSource* icon_source;
    GtkIconSet* icon_set;
    GtkIconFactory* factory = gtk_icon_factory_new ();
    guint i;

    for (i = 0; i < (guint)G_N_ELEMENTS (items); i++)
    {
        icon_set = gtk_icon_set_new ();
        icon_source = gtk_icon_source_new ();
        if (items[i].fallback)
        {
            gtk_icon_source_set_icon_name (icon_source, items[i].fallback);
            items[i].fallback = NULL;
            gtk_icon_set_add_source (icon_set, icon_source);
        }
        gtk_icon_source_set_icon_name (icon_source, items[i].stock_id);
        gtk_icon_set_add_source (icon_set, icon_source);
        gtk_icon_source_free (icon_source);
        gtk_icon_factory_add (factory, items[i].stock_id, icon_set);
        gtk_icon_set_unref (icon_set);
    }
    gtk_stock_add ((GtkStockItem*)items, G_N_ELEMENTS (items));
    gtk_icon_factory_add_default (factory);
    g_object_unref (factory);
}

static MidoriWebSettings*
settings_new_from_file (const gchar* filename)
{
    MidoriWebSettings* settings = midori_web_settings_new ();
    GKeyFile* key_file = g_key_file_new ();
    GError* error = NULL;
    GObjectClass* class;
    guint i, n_properties;
    GParamSpec** pspecs;
    GParamSpec* pspec;
    GType type;
    const gchar* property;
    gchar* string;
    gint integer;
    gfloat number;
    gboolean boolean;

    if (!g_key_file_load_from_file (key_file, filename,
                                   G_KEY_FILE_KEEP_COMMENTS, &error))
    {
        if (error->code != G_FILE_ERROR_NOENT)
            printf (_("The configuration couldn't be loaded. %s\n"),
                    error->message);
        g_error_free (error);
    }
    class = G_OBJECT_GET_CLASS (settings);
    pspecs = g_object_class_list_properties (class, &n_properties);
    for (i = 0; i < n_properties; i++)
    {
        pspec = pspecs[i];
        if (!(pspec->flags & G_PARAM_WRITABLE))
            continue;
        type = G_PARAM_SPEC_TYPE (pspec);
        property = g_param_spec_get_name (pspec);
        if (type == G_TYPE_PARAM_STRING)
        {
            string = sokoke_key_file_get_string_default (key_file,
                "settings", property,
                G_PARAM_SPEC_STRING (pspec)->default_value, NULL);
            g_object_set (settings, property, string, NULL);
            g_free (string);
        }
        else if (type == G_TYPE_PARAM_INT)
        {
            integer = sokoke_key_file_get_integer_default (key_file,
                "settings", property,
                G_PARAM_SPEC_INT (pspec)->default_value, NULL);
            g_object_set (settings, property, integer, NULL);
        }
        else if (type == G_TYPE_PARAM_FLOAT)
        {
            number = sokoke_key_file_get_double_default (key_file,
                "settings", property,
                G_PARAM_SPEC_FLOAT (pspec)->default_value, NULL);
            g_object_set (settings, property, number, NULL);
        }
        else if (type == G_TYPE_PARAM_BOOLEAN)
        {
            boolean = sokoke_key_file_get_boolean_default (key_file,
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
    GKeyFile* key_file;
    GObjectClass* class;
    guint i, n_properties;
    GParamSpec** pspecs;
    GParamSpec* pspec;
    GType type;
    const gchar* property;

    key_file = g_key_file_new ();
    class = G_OBJECT_GET_CLASS (settings);
    pspecs = g_object_class_list_properties (class, &n_properties);
    for (i = 0; i < n_properties; i++)
    {
        pspec = pspecs[i];
        type = G_PARAM_SPEC_TYPE (pspec);
        property = g_param_spec_get_name (pspec);
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
            gfloat number;
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

static KatzeArray*
search_engines_new_from_file (const gchar* filename,
                              GError**     error)
{
    KatzeArray* search_engines;
    GKeyFile* key_file;
    gchar** engines;
    guint i, j, n_properties;
    KatzeItem* item;
    GParamSpec** pspecs;
    const gchar* property;
    gchar* value;

    search_engines = katze_array_new (KATZE_TYPE_ITEM);
    key_file = g_key_file_new ();
    g_key_file_load_from_file (key_file, filename,
                               G_KEY_FILE_KEEP_COMMENTS, error);
    /*g_key_file_load_from_data_dirs(keyFile, sFilename, NULL
     , G_KEY_FILE_KEEP_COMMENTS, error);*/
    engines = g_key_file_get_groups (key_file, NULL);
    pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (search_engines),
	                                     &n_properties);
    for (i = 0; engines[i] != NULL; i++)
    {
        item = katze_item_new ();
        for (j = 0; j < n_properties; j++)
        {
            property = g_param_spec_get_name (pspecs[j]);
            value = g_key_file_get_string (key_file, engines[i],
	                                   property, NULL);
            g_object_set (item, property, value, NULL);
            g_free (value);
        }
        katze_array_add_item (search_engines, item);
    }
    g_strfreev (engines);
    g_key_file_free (key_file);
    return search_engines;
}

static gboolean
search_engines_save_to_file (KatzeArray*  search_engines,
                             const gchar* filename,
                             GError**     error)
{
    GKeyFile* key_file;
    guint n, i, j, n_properties;
    KatzeItem* item;
    const gchar* name;
    GParamSpec** pspecs;
    const gchar* property;
    gchar* value;
    gboolean saved;

    key_file = g_key_file_new ();
    n = katze_array_get_length (search_engines);
    pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (search_engines),
                                             &n_properties);
    for (i = 0; i < n; i++)
    {
        item = katze_array_get_nth_item (search_engines, i);
        name = katze_item_get_name (item);
        for (j = 0; j < n_properties; j++)
        {
            property = g_param_spec_get_name (pspecs[j]);
            g_object_get (item, property, &value, NULL);
            if (value)
                g_key_file_set_string (key_file, name, property, value);
            g_free (value);
        }
    }
    saved = sokoke_key_file_save_to_file (key_file, filename, error);
    g_key_file_free (key_file);

    return saved;
}

static void
midori_web_list_add_item_cb (KatzeArray* trash,
                             GObject*    item)
{
    guint n;
    GObject* obsolete_item;

    n = katze_array_get_length (trash);
    if (n > 10)
    {
        obsolete_item = katze_array_get_nth_item (trash, 0);
        g_object_unref (obsolete_item);
    }
}

static gchar*
_simple_xml_element (const gchar* name,
                     const gchar* value)
{
    gchar* value_escaped;
    gchar* markup;

    if (!value)
        return g_strdup ("");
    value_escaped = g_markup_escape_text (value, -1);
    markup = g_strdup_printf ("<%s>%s</%s>\n", name, value_escaped, name);
    g_free (value_escaped);
    return markup;
}

static gchar*
katze_xbel_array_to_xml (KatzeArray* array,
                         GError**    error)
{
    GString* inner_markup;
    guint i, n;
    KatzeXbelItem* item;
    gchar* item_xml;
    gchar* title;
    gchar* desc;
    gchar* outer_markup;

    g_return_val_if_fail (katze_array_is_a (array, KATZE_TYPE_XBEL_ITEM), NULL);

    inner_markup = g_string_new (NULL);
    n = katze_array_get_length (array);
    for (i = 0; i < n; i++)
    {
        item = katze_array_get_nth_item (array, i);
        item_xml = katze_xbel_item_to_data (item);
        g_string_append (inner_markup, item_xml);
        g_free (item_xml);
    }

    title = _simple_xml_element ("title", katze_item_get_name (KATZE_ITEM (array)));
    desc = _simple_xml_element ("desc", katze_item_get_text (KATZE_ITEM (array)));
    outer_markup = g_strdup_printf (
                   "%s%s<xbel version=\"1.0\">\n%s%s%s</xbel>\n",
                   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n",
                   "<!DOCTYPE xbel PUBLIC \"+//IDN python.org//DTD "
                   "XML Bookmark Exchange Language 1.0//EN//XML\" "
                   "\"http://www.python.org/topics/xml/dtds/xbel-1.0.dtd\">\n",
                   title,
                   desc,
                   g_string_free (inner_markup, FALSE));
    g_free (title);
    g_free (desc);

    return outer_markup;
}

static gboolean
katze_array_to_file (KatzeArray*  array,
                     const gchar* filename,
                     GError**     error)
{
    gchar* data;
    FILE* fp;

    g_return_val_if_fail (katze_array_is_a (array, KATZE_TYPE_XBEL_ITEM), FALSE);
    g_return_val_if_fail (filename, FALSE);

    if (!(data = katze_xbel_array_to_xml (array, error)))
        return FALSE;
    if (!(fp = fopen (filename, "w")))
    {
        *error = g_error_new_literal (G_FILE_ERROR, G_FILE_ERROR_ACCES,
                                      _("Writing failed."));
        return FALSE;
    }
    fputs (data, fp);
    fclose (fp);
    g_free (data);
    return TRUE;
}

static void
midori_browser_session_cb (MidoriBrowser* browser,
                           gpointer       arg1,
                           KatzeArray*    session)
{
    gchar* config_path;
    gchar* config_file;
    GError* error;

    config_path = g_build_filename (g_get_user_config_dir (),
                                    PACKAGE_NAME, NULL);
    g_mkdir_with_parents (config_path, 0755);
    config_file = g_build_filename (config_path, "session.xbel", NULL);
    error = NULL;
    if (!katze_array_to_file (session, config_file, &error))
    {
        g_warning (_("The session couldn't be saved. %s"), error->message);
        g_error_free (error);
    }

    g_free (config_file);
    g_free (config_path);
}

static void
midori_browser_weak_notify_cb (MidoriBrowser* browser,
                               KatzeXbelItem* session)
{
    g_object_disconnect (browser, "any-signal",
                         G_CALLBACK (midori_browser_session_cb), session, NULL);
}

int
main (int    argc,
      char** argv)
{
    gboolean version;
    gchar** uris;
    MidoriApp* app;
    gboolean result;
    GError* error;
    GOptionEntry entries[] =
    {
     { "version", 'v', 0, G_OPTION_ARG_NONE, &version,
       N_("Display program version"), NULL },
       { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &uris,
       N_("URIs"), NULL },
     { NULL }
    };
    MidoriStartup load_on_startup;
    gchar* homepage;
    KatzeArray* search_engines;
    KatzeXbelItem* bookmarks;
    guint i;
    gchar* uri;
    KatzeXbelItem* item;
    gchar* uri_ready;

    #if ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, MIDORI_LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    #endif
    g_set_application_name (_("Midori"));

    /* Parse cli options */
    version = FALSE;
    uris = NULL;
    error = NULL;
    if (!gtk_init_with_args (&argc, &argv, _("[URIs]"), entries,
                             GETTEXT_PACKAGE, &error))
    {
        if (error->code == G_OPTION_ERROR_UNKNOWN_OPTION)
            g_print ("%s - %s\n", _("Midori"), _("Unknown argument."));
        else
            g_print ("%s - %s", _("Midori"), _("Failed to setup interface."));
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
          "\thttp://www.twotoasts.de\n",
          _("Midori"), PACKAGE_VERSION,
          _("Please report comments, suggestions and bugs to:"),
          PACKAGE_BUGREPORT,
          _("Check for new versions at:")
        );
        return 0;
    }

    /* Standalone gjs support */
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

    app = midori_app_new ();
    if (midori_app_instance_is_running (app))
    {
        /* TODO: Open as many tabs as we have uris, seperated by pipes */
        if (argc > 1)
            result = midori_app_instance_send_uris (app, argv+1);
        else
            result = midori_app_instance_send_activate (app);

        if (result)
            return 0;

        /* FIXME: Do we want a graphical error message? */
        g_print (_("An instance of Midori is already running but not responding.\n"));
        return 1;
    }

    /* Load configuration files */
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
    search_engines = search_engines_new_from_file (config_file, &error);
    if (error)
    {
        /* FIXME: We may have a "file empty" error, how do we recognize that?
        if (error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf (error_messages,
                _("The search engines couldn't be loaded. %s\n"),
                error->message); */
        g_error_free (error);
    }
    katze_assign (config_file, g_build_filename (config_path, "bookmarks.xbel",
                                                 NULL));
    bookmarks = katze_xbel_folder_new ();
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
            g_string_append_printf (error_messages,
                _("The trash couldn't be loaded. %s\n"), error->message);
        g_error_free (error);
    }
    g_free (config_file);

    /* In case of errors */
    if (error_messages->len)
    {
        GdkScreen* screen;
        GtkIconTheme* icon_theme;
        GtkWidget* dialog = gtk_message_dialog_new (
            NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE,
            _("The following errors occured:"));
        gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
        gtk_window_set_title (GTK_WINDOW (dialog), g_get_application_name ());
        screen = gtk_widget_get_screen (dialog);
        if (screen)
        {
            icon_theme = gtk_icon_theme_get_for_screen (screen);
            if (gtk_icon_theme_has_icon (icon_theme, "midori"))
                gtk_window_set_icon_name (GTK_WINDOW (dialog), "midori");
            else
                gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");
        }
        gtk_message_dialog_format_secondary_text (
            GTK_MESSAGE_DIALOG (dialog), "%s", error_messages->str);
        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                "_Ignore", GTK_RESPONSE_ACCEPT,
                                NULL);
        if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
        {
            g_object_unref (settings);
            g_object_unref (search_engines);
            katze_xbel_item_unref (bookmarks);
            katze_xbel_item_unref (_session);
            katze_xbel_item_unref (xbel_trash);
            g_string_free (error_messages, TRUE);
            return 0;
        }
        gtk_widget_destroy (dialog);
        /* FIXME: Since we will overwrite files that could not be loaded
                  , would we want to make backups? */
    }
    g_string_free (error_messages, TRUE);

    /* Open as many tabs as we have uris, seperated by pipes */
    i = 0;
    while (uris && uris[i])
    {
        uri = strtok (g_strdup (uris[i]), "|");
        while (uri != NULL)
        {
            item = katze_xbel_bookmark_new ();
            uri_ready = sokoke_magic_uri (uri, NULL);
            katze_xbel_bookmark_set_href (item, uri_ready);
            g_free (uri_ready);
            katze_xbel_folder_append_item (_session, item);
            uri = strtok (NULL, "|");
        }
        g_free (uri);
        i++;
    }

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

    KatzeArray* trash = katze_array_new (KATZE_TYPE_XBEL_ITEM);
    guint n = katze_xbel_folder_get_n_items (xbel_trash);
    for (i = 0; i < n; i++)
    {
        KatzeXbelItem* item = katze_xbel_folder_get_nth_item (xbel_trash, i);
        katze_array_add_item (trash, item);
    }
    katze_xbel_item_unref (xbel_trash);
    g_signal_connect_after (trash, "add-item",
        G_CALLBACK (midori_web_list_add_item_cb), NULL);

    g_object_set (app, "settings", settings,
                       "bookmarks", bookmarks,
                       "trash", trash,
                       "search-engines", search_engines,
                       NULL);

    MidoriBrowser* browser = g_object_new (MIDORI_TYPE_BROWSER,
                                           "settings", settings,
                                           "bookmarks", bookmarks,
                                           "trash", trash,
                                           "search-engines", search_engines,
                                           NULL);
    midori_app_add_browser (app, browser);
    gtk_widget_show (GTK_WIDGET (browser));

    KatzeArray* session = midori_browser_get_proxy_xbel_array (browser);
    n = katze_xbel_folder_get_n_items (_session);
    for (i = 0; i < n; i++)
    {
        KatzeXbelItem* item = katze_xbel_folder_get_nth_item (_session, i);
        midori_browser_add_xbel_item (browser, item);
    }
    /* FIXME: Switch to the last active page */
    item = katze_xbel_folder_get_nth_item (_session, 0);
    if (!strcmp (katze_xbel_bookmark_get_href (item), ""))
        midori_browser_activate_action (browser, "Location");
    katze_xbel_item_unref (_session);

    g_signal_connect_after (browser, "notify::uri",
        G_CALLBACK (midori_browser_session_cb), session);
    g_signal_connect_after (browser, "add-tab",
        G_CALLBACK (midori_browser_session_cb), session);
    g_signal_connect_after (browser, "remove-tab",
        G_CALLBACK (midori_browser_session_cb), session);
    g_object_weak_ref (G_OBJECT (session),
        (GWeakNotify)(midori_browser_weak_notify_cb), browser);

    /* Load extensions */
    JSGlobalContextRef js_context = gjs_global_context_new ();
    /* FIXME: We want to honor system installed addons as well */
    gchar* addon_path = g_build_filename (g_get_user_data_dir (), PACKAGE_NAME,
                                          "extensions", NULL);
    GDir* addon_dir = g_dir_open (addon_path, 0, NULL);
    if (addon_dir)
    {
        const gchar* filename;
        while ((filename = g_dir_read_name (addon_dir)))
        {
            if (!g_str_has_prefix (filename, ".midori.js"))
                continue;

            gchar* fullname = g_build_filename (addon_path, filename, NULL);
            gchar* exception = NULL;
            gjs_script_from_file (js_context, fullname, &exception);
            if (exception)
            /* FIXME: Do we want to print this somewhere else? */
            /* FIXME Convert the filename to UTF8 */
                printf ("%s - Exception: %s\n", filename, exception);
            g_free (fullname);
        }
        g_dir_close (addon_dir);
    }

    gtk_main ();

    JSGlobalContextRelease (js_context);

    /* Save configuration files */
    config_path = g_build_filename (g_get_user_config_dir (), PACKAGE_NAME,
                                    NULL);
    g_mkdir_with_parents (config_path, 0755);
    config_file = g_build_filename (config_path, "search", NULL);
    error = NULL;
    if (!search_engines_save_to_file (search_engines, config_file, &error))
    {
        g_warning (_("The search engines couldn't be saved. %s"),
                   error->message);
        g_error_free (error);
    }
    g_object_unref (search_engines);
    g_free (config_file);
    config_file = g_build_filename (config_path, "bookmarks.xbel", NULL);
    error = NULL;
    if (!katze_xbel_folder_to_file (bookmarks, config_file, &error))
    {
        g_warning (_("The bookmarks couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    katze_xbel_item_unref (bookmarks);
    g_free (config_file);
    config_file = g_build_filename (config_path, "tabtrash.xbel", NULL);
    error = NULL;
    if (!katze_array_to_file (trash, config_file, &error))
    {
        g_warning (_("The trash couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_object_unref (trash);
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
