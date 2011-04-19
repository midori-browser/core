/*
 Copyright (C) 2007-2009 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2008 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "midori.h"
#include "midori-array.h"
#include "midori-bookmarks.h"
#include "midori-extensions.h"
#include "midori-history.h"
#include "midori-transfers.h"

#include "sokoke.h"

#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#include <webkit/webkit.h>
#include <sqlite3.h>

#if ENABLE_NLS
    #include <libintl.h>
    #include <locale.h>
#endif

#ifdef HAVE_SIGNAL_H
    #include <signal.h>
#endif

#if HAVE_HILDON
    #define BOOKMARK_FILE "/home/user/.bookmarks/MyBookmarks.xml"
#else
    #define BOOKMARK_FILE "bookmarks.xbel"
#endif

#ifdef HAVE_X11_EXTENSIONS_SCRNSAVER_H
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <X11/extensions/scrnsaver.h>
    #include <gdk/gdkx.h>
#endif

static gchar*
build_config_filename (const gchar* filename)
{
    return g_build_filename (sokoke_set_config_dir (NULL), filename, NULL);
}

static MidoriWebSettings*
settings_and_accels_new (const gchar* config,
                         gchar***     extensions)
{
    MidoriWebSettings* settings = midori_web_settings_new ();
    gchar* config_file = g_build_filename (config, "config", NULL);
    GKeyFile* key_file = g_key_file_new ();
    GError* error = NULL;
    GObjectClass* class;
    guint i, n_properties;
    GParamSpec** pspecs;
    GParamSpec* pspec;
    GType type;
    const gchar* property;
    gchar* str;
    gint integer;
    gfloat number;
    gboolean boolean;

    if (!g_key_file_load_from_file (key_file, config_file,
                                    G_KEY_FILE_KEEP_COMMENTS, &error))
    {
        if (error->code == G_FILE_ERROR_NOENT)
        {
            GError* inner_error = NULL;
            katze_assign (config_file, sokoke_find_config_filename (NULL, "config"));
            g_key_file_load_from_file (key_file, config_file,
                                       G_KEY_FILE_KEEP_COMMENTS, &inner_error);
            if (inner_error != NULL)
            {
                printf (_("The configuration couldn't be loaded: %s\n"),
                        inner_error->message);
                g_error_free (inner_error);
            }
        }
        else
            printf (_("The configuration couldn't be loaded: %s\n"),
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
        if (!g_key_file_has_key (key_file, "settings", property, NULL))
            continue;

        if (type == G_TYPE_PARAM_STRING)
        {
            str = g_key_file_get_string (key_file, "settings", property, NULL);
            g_object_set (settings, property, str, NULL);
            g_free (str);
        }
        else if (type == G_TYPE_PARAM_INT)
        {
            integer = g_key_file_get_integer (key_file, "settings", property, NULL);
            g_object_set (settings, property, integer, NULL);
        }
        else if (type == G_TYPE_PARAM_FLOAT)
        {
            number = g_key_file_get_double (key_file, "settings", property, NULL);
            g_object_set (settings, property, number, NULL);
        }
        else if (type == G_TYPE_PARAM_BOOLEAN)
        {
            boolean = g_key_file_get_boolean (key_file, "settings", property, NULL);
            g_object_set (settings, property, boolean, NULL);
        }
        else if (type == G_TYPE_PARAM_ENUM)
        {
            GEnumClass* enum_class = G_ENUM_CLASS (
                g_type_class_peek (pspec->value_type));
            GEnumValue* enum_value;
            str = g_key_file_get_string (key_file, "settings", property, NULL);
            enum_value = g_enum_get_value_by_name (enum_class, str);
            if (enum_value)
                g_object_set (settings, property, enum_value->value, NULL);
            else
                g_warning (_("Value '%s' is invalid for %s"),
                           str, property);
            g_free (str);
        }
        else
            g_warning (_("Invalid configuration value '%s'"), property);
    }
    g_free (pspecs);

    *extensions = g_key_file_get_keys (key_file, "extensions", NULL, NULL);

    g_key_file_free (key_file);

    /* Load accelerators */
    katze_assign (config_file, g_build_filename (config, "accels", NULL));
    if (g_access (config_file, F_OK) != 0)
        katze_assign (config_file, sokoke_find_config_filename (NULL, "accels"));
    gtk_accel_map_load (config_file);
    g_free (config_file);

    return settings;
}

static gboolean
settings_save_to_file (MidoriWebSettings* settings,
                       MidoriApp*         app,
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
    gboolean saved;
    KatzeArray* extensions = katze_object_get_object (app, "extensions");
    MidoriExtension* extension;
    gchar** _extensions;

    key_file = g_key_file_new ();
    class = G_OBJECT_GET_CLASS (settings);
    pspecs = g_object_class_list_properties (class, &n_properties);
    for (i = 0; i < n_properties; i++)
    {
        pspec = pspecs[i];
        type = G_PARAM_SPEC_TYPE (pspec);
        property = g_param_spec_get_name (pspec);
        if (!(pspec->flags & G_PARAM_WRITABLE))
            continue;
        if (type == G_TYPE_PARAM_STRING)
        {
            gchar* string;
            const gchar* def_string = G_PARAM_SPEC_STRING (pspec)->default_value;
            g_object_get (settings, property, &string, NULL);
            if (!string)
                string = g_strdup ("");
            if (!def_string)
                def_string = "";
            if (strcmp (string, def_string))
                g_key_file_set_string (key_file, "settings", property, string);
            g_free (string);
        }
        else if (type == G_TYPE_PARAM_INT)
        {
            gint integer;
            g_object_get (settings, property, &integer, NULL);
            if (integer != G_PARAM_SPEC_INT (pspec)->default_value)
                g_key_file_set_integer (key_file, "settings", property, integer);
        }
        else if (type == G_TYPE_PARAM_FLOAT)
        {
            gfloat number;
            g_object_get (settings, property, &number, NULL);
            if (number != G_PARAM_SPEC_FLOAT (pspec)->default_value)
                g_key_file_set_double (key_file, "settings", property, number);
        }
        else if (type == G_TYPE_PARAM_BOOLEAN)
        {
            gboolean truth;
            g_object_get (settings, property, &truth, NULL);
            if (truth != G_PARAM_SPEC_BOOLEAN (pspec)->default_value)
                g_key_file_set_boolean (key_file, "settings", property, truth);
        }
        else if (type == G_TYPE_PARAM_ENUM)
        {
            GEnumClass* enum_class = G_ENUM_CLASS (
                g_type_class_peek (pspec->value_type));
            gint integer;
            GEnumValue* enum_value;
            g_object_get (settings, property, &integer, NULL);
            enum_value = g_enum_get_value (enum_class, integer);
            if (integer != G_PARAM_SPEC_ENUM (pspec)->default_value)
                g_key_file_set_string (key_file, "settings", property,
                                       enum_value->value_name);
        }
        else
            g_warning (_("Invalid configuration value '%s'"), property);
    }
    g_free (pspecs);

    if (extensions)
    {
        KATZE_ARRAY_FOREACH_ITEM (extension, extensions)
            if (midori_extension_is_active (extension))
                g_key_file_set_boolean (key_file, "extensions",
                    g_object_get_data (G_OBJECT (extension), "filename"), TRUE);
        g_object_unref (extensions);
    }
    else if ((_extensions = g_object_get_data (G_OBJECT (app), "extensions")))
    {
        i = 0;
        while (_extensions[i])
        {
            g_key_file_set_boolean (key_file, "extensions", _extensions[i], TRUE);
            i++;
        }
    }

    saved = sokoke_key_file_save_to_file (key_file, filename, error);
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
            if (!G_IS_PARAM_SPEC_STRING (pspecs[j]))
                continue;
            property = g_param_spec_get_name (pspecs[j]);
            value = g_key_file_get_string (key_file, engines[i],
	                                   property, NULL);
            g_object_set (item, property, value, NULL);
            g_free (value);
        }
        katze_array_add_item (search_engines, item);
    }
    g_free (pspecs);
    g_strfreev (engines);
    g_key_file_free (key_file);
    return search_engines;
}

static KatzeArray*
search_engines_new_from_folder (const gchar* config,
                                GString*     error_messages)
{
    gchar* config_file = g_build_filename (config, "search", NULL);
    GError* error = NULL;
    KatzeArray* search_engines;

    search_engines = search_engines_new_from_file (config_file, &error);
    /* We ignore for instance empty files */
    if (error && (error->code == G_KEY_FILE_ERROR_PARSE
        || error->code == G_FILE_ERROR_NOENT))
    {
        g_error_free (error);
        error = NULL;
    }
    if (!error && katze_array_is_empty (search_engines))
    {
        g_object_unref (search_engines);
        #ifdef G_OS_WIN32
        gchar* dir = g_win32_get_package_installation_directory_of_module (NULL);
        katze_assign (config_file,
            g_build_filename (dir, "etc", "xdg", PACKAGE_NAME, "search", NULL));
        g_free (dir);
        search_engines = search_engines_new_from_file (config_file, NULL);
        #else
        katze_assign (config_file,
            sokoke_find_config_filename (NULL, "search"));
        search_engines = search_engines_new_from_file (config_file, NULL);
        #endif
    }
    else if (error)
    {
        if (error->code != G_FILE_ERROR_NOENT && error_messages)
            g_string_append_printf (error_messages,
                _("The search engines couldn't be loaded. %s\n"),
                error->message);
        g_error_free (error);
    }
    g_free (config_file);
    return search_engines;
}

static gboolean
search_engines_save_to_file (KatzeArray*  search_engines,
                             const gchar* filename,
                             GError**     error)
{
    GKeyFile* key_file;
    guint j, n_properties;
    KatzeItem* item;
    const gchar* name;
    GParamSpec** pspecs;
    const gchar* property;
    gchar* value;
    gboolean saved;

    key_file = g_key_file_new ();
    pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (search_engines),
                                             &n_properties);
    KATZE_ARRAY_FOREACH_ITEM (item, search_engines)
    {
        name = katze_item_get_name (item);
        for (j = 0; j < n_properties; j++)
        {
            if (!G_IS_PARAM_SPEC_STRING (pspecs[j]))
                continue;
            property = g_param_spec_get_name (pspecs[j]);
            g_object_get (item, property, &value, NULL);
            if (value)
                g_key_file_set_string (key_file, name, property, value);
            g_free (value);
        }
    }
    g_free (pspecs);
    saved = sokoke_key_file_save_to_file (key_file, filename, error);
    g_key_file_free (key_file);

    return saved;
}

static void
midori_history_clear_cb (KatzeArray* array,
                         sqlite3*    db)
{
    char* errmsg = NULL;
    if (sqlite3_exec (db, "DELETE FROM history; DELETE FROM search",
                      NULL, NULL, &errmsg) != SQLITE_OK)
    {
        g_printerr (_("Failed to clear history: %s\n"), errmsg);
        sqlite3_free (errmsg);
    }
}

static gboolean
midori_history_initialize (KatzeArray*  array,
                           const gchar* filename,
                           const gchar* bookmarks_filename,
                           char**       errmsg)
{
    sqlite3* db;
    gboolean has_day;
    sqlite3_stmt* stmt;
    gint result;
    gchar* sql;

    has_day = FALSE;

    if (sqlite3_open (filename, &db) != SQLITE_OK)
    {
        if (errmsg)
            *errmsg = g_strdup_printf (_("Failed to open database: %s\n"),
                                       sqlite3_errmsg (db));
        sqlite3_close (db);
        return FALSE;
    }

    sqlite3_exec (db, "PRAGMA journal_mode = TRUNCATE;", NULL, NULL, errmsg);
    if (*errmsg)
    {
        g_warning ("Failed to set journal mode: %s", *errmsg);
        sqlite3_free (*errmsg);
    }
    if (sqlite3_exec (db,
                      "CREATE TABLE IF NOT EXISTS "
                      "history (uri text, title text, date integer, day integer);"
                      "CREATE TABLE IF NOT EXISTS "
                      "search (keywords text, uri text, day integer);",
                      NULL, NULL, errmsg) != SQLITE_OK)
        return FALSE;

    sqlite3_prepare_v2 (db, "SELECT day FROM history LIMIT 1", -1, &stmt, NULL);
    result = sqlite3_step (stmt);
    if (result == SQLITE_ROW)
        has_day = TRUE;
    sqlite3_finalize (stmt);

    if (!has_day)
        sqlite3_exec (db,
                      "BEGIN TRANSACTION;"
                      "CREATE TEMPORARY TABLE backup (uri text, title text, date integer);"
                      "INSERT INTO backup SELECT uri,title,date FROM history;"
                      "DROP TABLE history;"
                      "CREATE TABLE history (uri text, title text, date integer, day integer);"
                      "INSERT INTO history SELECT uri,title,date,"
                      "julianday(date(date,'unixepoch','start of day','+1 day'))"
                      " - julianday('0001-01-01','start of day')"
                      "FROM backup;"
                      "DROP TABLE backup;"
                      "COMMIT;",
                      NULL, NULL, errmsg);

    sql = g_strdup_printf ("ATTACH DATABASE '%s' AS bookmarks", bookmarks_filename);
    sqlite3_exec (db, sql, NULL, NULL, errmsg);
    g_free (sql);
    g_object_set_data (G_OBJECT (array), "db", db);
    g_signal_connect (array, "clear",
                      G_CALLBACK (midori_history_clear_cb), db);

    return TRUE;
}

static void
midori_history_terminate (KatzeArray* array,
                          gint        max_history_age)
{
    sqlite3* db = g_object_get_data (G_OBJECT (array), "db");
    char* errmsg = NULL;
    gchar* sqlcmd = g_strdup_printf (
        "DELETE FROM history WHERE "
        "(julianday(date('now')) - julianday(date(date,'unixepoch')))"
        " >= %d", max_history_age);
    if (sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        /* i18n: Couldn't remove items that are older than n days */
        g_printerr (_("Failed to remove old history items: %s\n"), errmsg);
        sqlite3_free (errmsg);
    }
    g_free (sqlcmd);
    sqlite3_close (db);
}

static void
midori_bookmarks_add_item_cb (KatzeArray* array,
                              KatzeItem*  item,
                              sqlite3*    db)
{
    midori_bookmarks_insert_item_db (db, item,
        katze_item_get_meta_string (item, "folder"));
}

static void
midori_bookmarks_remove_item_cb (KatzeArray* array,
                                 KatzeItem*  item,
                                 sqlite3*    db)
{
    gchar* sqlcmd;
    char* errmsg = NULL;

    if (KATZE_ITEM_IS_BOOKMARK (item))
        sqlcmd = sqlite3_mprintf (
            "DELETE FROM bookmarks WHERE uri = '%q' "
            " AND folder = '%q'",
            katze_item_get_uri (item),
            katze_item_get_meta_string (item, "folder"));

    else
       sqlcmd = sqlite3_mprintf (
            "DELETE FROM bookmarks WHERE title = '%q'"
            " AND folder = '%q'",
            katze_item_get_name (item),
            katze_item_get_meta_string (item, "folder"));

    if (sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        g_printerr (_("Failed to remove history item: %s\n"), errmsg);
        sqlite3_free (errmsg);
    }

    sqlite3_free (sqlcmd);
}

static sqlite3*
midori_bookmarks_initialize (KatzeArray*  array,
                             const gchar* filename,
                             char**       errmsg)
{
    sqlite3* db;

    if (sqlite3_open (filename, &db) != SQLITE_OK)
    {
        if (errmsg)
            *errmsg = g_strdup_printf (_("Failed to open database: %s\n"),
                                       sqlite3_errmsg (db));
        sqlite3_close (db);
        return NULL;
    }

    if (sqlite3_exec (db,
                      "CREATE TABLE IF NOT EXISTS "
                      "bookmarks (uri text, title text, folder text, "
                      "desc text, app integer, toolbar integer);",
                      NULL, NULL, errmsg) != SQLITE_OK)
        return NULL;
    g_signal_connect (array, "add-item",
                      G_CALLBACK (midori_bookmarks_add_item_cb), db);
    g_signal_connect (array, "remove-item",
                      G_CALLBACK (midori_bookmarks_remove_item_cb), db);
    return db;
}

static void
midori_bookmarks_import (const gchar* filename,
                         sqlite3*     db)
{
    KatzeArray* bookmarks;
    GError* error = NULL;

    bookmarks = katze_array_new (KATZE_TYPE_ARRAY);

    if (!midori_array_from_file (bookmarks, filename, "xbel", &error))
    {
        g_warning (_("The bookmarks couldn't be saved. %s"), error->message);
        g_error_free (error);
        return;
    }
    midori_bookmarks_import_array_db (db, bookmarks, "");
}

static void
midori_session_add_delay (KatzeArray* session)
{
    KatzeItem* item;
    KATZE_ARRAY_FOREACH_ITEM (item, session)
        katze_item_set_meta_integer (item, "delay", 1);
}

static void
settings_notify_cb (MidoriWebSettings* settings,
                    GParamSpec*        pspec,
                    MidoriApp*         app)
{
    GError* error = NULL;
    gchar* config_file;

    /* Skip state related properties to avoid disk IO */
    if ((pspec && g_str_has_prefix (pspec->name, "last-window-"))
     || (pspec && g_str_has_prefix (pspec->name, "last-panel-")))
        return;

    config_file = build_config_filename ("config");
    if (!settings_save_to_file (settings, app, config_file, &error))
    {
        g_warning (_("The configuration couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);
}

static void
extension_activate_cb (MidoriExtension* extension,
                       MidoriApp*       app)
{
    MidoriWebSettings* settings = katze_object_get_object (app, "settings");
    settings_notify_cb (settings, NULL, app);
    g_object_unref (settings);
}

static void
accel_map_changed_cb (GtkAccelMap*    accel_map,
                      gchar*          accel_path,
                      guint           accel_key,
                      GdkModifierType accel_mods)
{
    gchar* config_file = build_config_filename ("accels");
    gtk_accel_map_save (config_file);
    g_free (config_file);
}

static void
midori_search_engines_modify_cb (KatzeArray* array,
                                 gpointer    item,
                                 KatzeArray* search_engines)
{
    gchar* config_file = build_config_filename ("search");
    GError* error = NULL;
    if (!search_engines_save_to_file (search_engines, config_file, &error))
    {
        g_warning (_("The search engines couldn't be saved. %s"),
                   error->message);
        g_error_free (error);
    }
    g_free (config_file);
}

static void
midori_search_engines_move_item_cb (KatzeArray* array,
                                    gpointer    item,
                                    gint        position,
                                    KatzeArray* search_engines)
{
    midori_search_engines_modify_cb (array, item, search_engines);
}

static void
midori_trash_add_item_cb (KatzeArray* trash,
                          GObject*    item)
{
    gchar* config_file = build_config_filename ("tabtrash.xbel");
    GError* error = NULL;
    GObject* obsolete_item;
    if (!midori_array_to_file (trash, config_file, "xbel", &error))
    {
        /* i18n: Trash, or wastebin, containing closed tabs */
        g_warning (_("The trash couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);

    if (katze_array_get_nth_item (trash, 10))
    {
        obsolete_item = katze_array_get_nth_item (trash, 0);
        katze_array_remove_item (trash, obsolete_item);
    }
}

static void
midori_trash_remove_item_cb (KatzeArray* trash,
                             GObject*    item)
{
    gchar* config_file = build_config_filename ("tabtrash.xbel");
    GError* error = NULL;
    if (!midori_array_to_file (trash, config_file, "xbel", &error))
    {
        g_warning (_("The trash couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);
}

static void
midori_browser_show_preferences_cb (MidoriBrowser*    browser,
                                    KatzePreferences* preferences,
                                    MidoriApp*        app)
{
    GtkWidget* scrolled = katze_scrolled_new (NULL, NULL);
    GtkWidget* addon = g_object_new (MIDORI_TYPE_EXTENSIONS, NULL);
    GList* children = gtk_container_get_children (GTK_CONTAINER (addon));
    GtkWidget* page;
    gtk_widget_reparent (g_list_nth_data (children, 0), scrolled);
    g_list_free (children);
    g_object_set (addon, "app", app, NULL);
    gtk_widget_show (scrolled);
    page = katze_preferences_add_category (preferences,
                                           _("Extensions"), STOCK_EXTENSIONS);
    gtk_box_pack_start (GTK_BOX (page), scrolled, TRUE, TRUE, 4);
}

static void
midori_preferences_delete_cookies_toggled_cb (GtkToggleButton*   button,
                                              MidoriWebSettings* settings)
{
    gboolean toggled = gtk_toggle_button_get_active (button);
    g_object_set (settings, "accept-cookies",
        toggled ? MIDORI_ACCEPT_COOKIES_SESSION : MIDORI_ACCEPT_COOKIES_ALL, NULL);
}

static void
midori_preferences_delete_cookies_changed_cb (GtkComboBox*       combo,
                                              MidoriWebSettings* settings)
{
    gint active = gtk_combo_box_get_active (combo);
    gint max_age;
    switch (active)
    {
    case 0: max_age =   0; break;
    case 1: max_age =   1; break;
    case 2: max_age =   7; break;
    case 3: max_age =  30; break;
    case 4: max_age = 365; break;
    default:
        max_age = 30;
    }
    g_object_set (settings, "maximum-cookie-age", max_age, NULL);
}

static void
midori_browser_privacy_preferences_cb (MidoriBrowser*    browser,
                                       KatzePreferences* preferences,
                                       MidoriApp*        app)
{
    MidoriWebSettings* settings = midori_browser_get_settings (browser);
    GtkWidget* button;
    GtkWidget* label;
    gint max_age = katze_object_get_int (settings, "maximum-cookie-age");
    guint active;
    gchar* markup;

    katze_preferences_add_category (preferences, _("Privacy"), GTK_STOCK_INDEX);
    katze_preferences_add_group (preferences, _("Web Cookies"));
    button = gtk_check_button_new_with_mnemonic (_("Delete cookies when quitting Midori"));
    katze_preferences_add_widget (preferences, button, "indented");
    if (katze_object_get_enum (settings, "accept-cookies") == MIDORI_ACCEPT_COOKIES_SESSION)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    g_signal_connect (button, "toggled",
        G_CALLBACK (midori_preferences_delete_cookies_toggled_cb), settings);
    button = gtk_combo_box_new_text ();
    gtk_combo_box_append_text (GTK_COMBO_BOX (button), _("Delete old cookies after 1 hour"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (button), _("Delete old cookies after 1 day"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (button), _("Delete old cookies after 1 week"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (button), _("Delete old cookies after 1 month"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (button), _("Delete old cookies after 1 year"));

    switch (max_age)
    {
    case   0: active = 0; break;
    case   1: active = 1; break;
    case   7: active = 2; break;
    case  30: active = 3; break;
    case 365: active = 4; break;
    default:
        active = 3;
    }
    gtk_combo_box_set_active (GTK_COMBO_BOX (button), active);
    g_signal_connect (button, "changed",
        G_CALLBACK (midori_preferences_delete_cookies_changed_cb), settings);
    katze_preferences_add_widget (preferences, button, "spanned");

    markup = g_strdup_printf ("<span size=\"smaller\">%s</span>",
        _("Cookies store login data, saved games, "
          "or user profiles for advertisement purposes."));
    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), markup);
    g_free (markup);
    katze_preferences_add_widget (preferences, label, "filled");
    #if WEBKIT_CHECK_VERSION (1, 1, 13)
    button = katze_property_proxy (settings, "enable-offline-web-application-cache", NULL);
    katze_preferences_add_widget (preferences, button, "indented");
    #endif
    #if WEBKIT_CHECK_VERSION (1, 1, 8)
    button = katze_property_proxy (settings, "enable-html5-local-storage", NULL);
    katze_preferences_add_widget (preferences, button, "spanned");
    #if !WEBKIT_CHECK_VERSION (1, 1, 14)
    button = katze_property_proxy (settings, "enable-html5-database", NULL);
    katze_preferences_add_widget (preferences, button, "indented");
    #endif
    #endif
    #if HAVE_LIBSOUP_2_27_90
    button = katze_property_proxy (settings, "strip-referer", NULL);
    katze_preferences_add_widget (preferences, button, "indented");
    #endif
    button = katze_property_label (settings, "maximum-history-age");
    katze_preferences_add_widget (preferences, button, "indented");
    button = katze_property_proxy (settings, "maximum-history-age", NULL);
    katze_preferences_add_widget (preferences, button, "spanned");
    label = gtk_label_new (_("days"));
    katze_preferences_add_widget (preferences, label, "spanned");
}

static void
midori_app_add_browser_cb (MidoriApp*     app,
                           MidoriBrowser* browser,
                           KatzeNet*      net)
{
    GtkWidget* panel;
    GtkWidget* addon;

    panel = katze_object_get_object (browser, "panel");

    /* Bookmarks */
    addon = g_object_new (MIDORI_TYPE_BOOKMARKS, "app", app, NULL);
    gtk_widget_show (addon);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));

    /* History */
    addon = g_object_new (MIDORI_TYPE_HISTORY, "app", app, NULL);
    gtk_widget_show (addon);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));

    /* Transfers */
    #if WEBKIT_CHECK_VERSION (1, 1, 3)
    addon = g_object_new (MIDORI_TYPE_TRANSFERS, "app", app, NULL);
    gtk_widget_show (addon);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));
    #endif

    /* Extensions */
    g_signal_connect (browser, "show-preferences",
        G_CALLBACK (midori_browser_privacy_preferences_cb), app);
    g_signal_connect (browser, "show-preferences",
        G_CALLBACK (midori_browser_show_preferences_cb), app);

    g_object_unref (panel);
}

static guint save_timeout = 0;

static gboolean
midori_session_save_timeout_cb (KatzeArray* session)
{
    gchar* config_file = build_config_filename ("session.xbel");
    GError* error = NULL;
    if (!midori_array_to_file (session, config_file, "xbel", &error))
    {
        g_warning (_("The session couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);

    save_timeout = 0;
    g_object_unref (session);

    return FALSE;
}

static void
midori_browser_session_cb (MidoriBrowser* browser,
                           gpointer       pspec,
                           KatzeArray*    session)
{
    if (!save_timeout)
    {
        g_object_ref (session);
        save_timeout = g_timeout_add_seconds (5,
            (GSourceFunc)midori_session_save_timeout_cb, session);
    }
}

static void
midori_app_quit_cb (MidoriBrowser* browser,
                    KatzeArray*    session)
{
    gchar* config_file = build_config_filename ("running");
    g_unlink (config_file);
    g_free (config_file);

    if (session)
        midori_session_save_timeout_cb (session);
}

static void
midori_browser_weak_notify_cb (MidoriBrowser* browser,
                               KatzeArray*    session)
{
    g_object_disconnect (browser, "any-signal",
                         G_CALLBACK (midori_browser_session_cb), session, NULL);
}

static void
midori_soup_session_set_proxy_uri (SoupSession* session,
                                   const gchar* uri)
{
    gchar* fixed_uri;
    SoupURI* proxy_uri;

    /* soup_uri_new expects a non-NULL string with a protocol */
    if (uri && g_str_has_prefix (uri, "http://"))
        proxy_uri = soup_uri_new (uri);
    else if (uri && *uri)
    {
        fixed_uri = g_strconcat ("http://", uri, NULL);
        proxy_uri = soup_uri_new (fixed_uri);
        g_free (fixed_uri);
    }
    else
        proxy_uri = NULL;
    g_object_set (session, "proxy-uri", proxy_uri, NULL);
    if (proxy_uri)
        soup_uri_free (proxy_uri);
}

static void
soup_session_settings_notify_http_proxy_cb (MidoriWebSettings* settings,
                                            GParamSpec*        pspec,
                                            SoupSession*       session)
{
    MidoriProxy proxy_type;

    proxy_type = katze_object_get_enum (settings, "proxy-type");
    if (proxy_type == MIDORI_PROXY_AUTOMATIC)
    {
        gboolean gnome_supported = FALSE;
        GModule* module;
        GType (*get_type_function) (void);
        if (g_module_supported ())
            if ((module = g_module_open ("libsoup-gnome-2.4.so", G_MODULE_BIND_LOCAL)))
            {
                if (g_module_symbol (module, "soup_proxy_resolver_gnome_get_type",
                                     (void*) &get_type_function))
                {
                    soup_session_add_feature_by_type (session, get_type_function ());
                    gnome_supported = TRUE;
                }
            }
        if (!gnome_supported)
            midori_soup_session_set_proxy_uri (session, g_getenv ("http_proxy"));
    }
    else if (proxy_type == MIDORI_PROXY_HTTP)
    {
        gchar* http_proxy = katze_object_get_string (settings, "http-proxy");
        midori_soup_session_set_proxy_uri (session, http_proxy);
        g_free (http_proxy);
    }
    else
        midori_soup_session_set_proxy_uri (session, NULL);
}

#if !WEBKIT_CHECK_VERSION (1, 1, 11)
static void
soup_session_settings_notify_ident_string_cb (MidoriWebSettings* settings,
                                              GParamSpec*        pspec,
                                              SoupSession*       session)
{
    gchar* ident_string = katze_object_get_string (settings, "user-agent");
    g_object_set (session, "user-agent", ident_string, NULL);
    g_free (ident_string);
}
#endif

static void
midori_soup_session_settings_accept_language_cb (SoupSession*       session,
                                                 SoupMessage*       msg,
                                                 MidoriWebSettings* settings)
{
    gchar* languages = katze_object_get_string (settings, "preferred-languages");
    gchar* accpt;

    /* Empty, use the system locales */
    if (!(languages && *languages))
        accpt = sokoke_accept_languages (g_get_language_names ());
    /* No =, no ., looks like a list of language names */
    else if (!(strchr (languages, '=') && strchr (languages, '.')))
    {
        gchar ** lang_names = g_strsplit_set (languages, ",; ", -1);
        accpt = sokoke_accept_languages ((const gchar* const *)lang_names);
        g_strfreev (lang_names);
    }
    /* Presumably a well formatted list including priorities */
    else
        accpt = languages;

    if (accpt != languages)
        g_free (languages);
    soup_message_headers_append (msg->request_headers, "Accept-Language", accpt);
    g_free (accpt);

    #if HAVE_LIBSOUP_2_27_90
    if (katze_object_get_boolean (settings, "strip-referer"))
    {
        const gchar* referer
            = soup_message_headers_get_one (msg->request_headers, "Referer");
        SoupURI* destination = soup_message_get_uri (msg);
        if (referer && destination && !strstr (referer, destination->host))
        {
            SoupURI* stripped_uri = soup_uri_new (referer);
            gchar* stripped_referer;
            soup_uri_set_path (stripped_uri, NULL);
            soup_uri_set_query (stripped_uri, NULL);
            stripped_referer = soup_uri_to_string (stripped_uri, FALSE);
            soup_uri_free (stripped_uri);
            if (g_getenv ("MIDORI_SOUP_DEBUG"))
                g_message ("Referer stripped");
            soup_message_headers_replace (msg->request_headers, "Referer",
                                          stripped_referer);
            g_free (stripped_referer);
        }
    }
    #endif
}

static void
midori_soup_session_debug (SoupSession* session)
{
    const char* soup_debug = g_getenv ("MIDORI_SOUP_DEBUG");

    if (soup_debug)
    {
        gint soup_debug_level = atoi (soup_debug);
        SoupLogger* logger = soup_logger_new (soup_debug_level, -1);
        soup_logger_attach (logger, session);
        g_object_unref (logger);
    }
}

static gboolean
midori_load_soup_session (gpointer settings)
{
    SoupSession* session = webkit_get_default_session ();

    #if WEBKIT_CHECK_VERSION (1, 1, 14) && defined (HAVE_LIBSOUP_2_29_91)
    const gchar* certificate_files[] =
    {
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/ssl/certs/ca-bundle.crt",
        "/usr/local/share/certs/ca-root-nss.crt",
        NULL
    };
    guint i;

    for (i = 0; i < G_N_ELEMENTS (certificate_files); i++)
        if (g_access (certificate_files[i], F_OK) == 0)
        {
            g_object_set (session,
                "ssl-ca-file", certificate_files[i],
                "ssl-strict", FALSE,
                NULL);
            break;
        }
    if (i == G_N_ELEMENTS (certificate_files))
        g_warning (_("No root certificate file is available. "
                     "SSL certificates cannot be verified."));
    #endif

    #if !WEBKIT_CHECK_VERSION (1, 3, 5)
    /* See http://stevesouders.com/ua/index.php */
    g_object_set (session, "max-conns", 60,
                           "max-conns-per-host", 6,
                           NULL);
    #endif

    soup_session_settings_notify_http_proxy_cb (settings, NULL, session);
    g_signal_connect (settings, "notify::http-proxy",
        G_CALLBACK (soup_session_settings_notify_http_proxy_cb), session);
    g_signal_connect (settings, "notify::proxy-type",
        G_CALLBACK (soup_session_settings_notify_http_proxy_cb), session);

    #if !WEBKIT_CHECK_VERSION (1, 1, 11)
    soup_session_settings_notify_ident_string_cb (settings, NULL, session);
    g_signal_connect (settings, "notify::user-agent",
        G_CALLBACK (soup_session_settings_notify_ident_string_cb), session);
    #endif

    g_signal_connect (session, "request-queued",
        G_CALLBACK (midori_soup_session_settings_accept_language_cb), settings);

    midori_soup_session_debug (session);

    return FALSE;
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
button_reset_session_clicked_cb (GtkWidget*  button,
                                 KatzeArray* session)
{
    gchar* config_file = build_config_filename ("session.old.xbel");
    GError* error = NULL;
    if (!midori_array_to_file (session, config_file, "xbel", &error))
    {
        g_warning (_("The session couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);
    katze_array_clear (session);
    gtk_widget_set_sensitive (button, FALSE);
}

static void
button_disable_extensions_clicked_cb (GtkWidget* button,
                                      MidoriApp* app)
{
    g_object_set_data (G_OBJECT (app), "extensions", NULL);
    gtk_widget_set_sensitive (button, FALSE);
}

static GtkWidget*
midori_create_diagnostic_dialog (MidoriWebSettings* settings,
                                 KatzeArray*        _session)
{
    GtkWidget* dialog;
    GtkWidget* content_area;
    GdkScreen* screen;
    GtkIconTheme* icon_theme;
    GtkWidget* box;
    GtkWidget* button;
    MidoriApp* app = katze_item_get_parent (KATZE_ITEM (_session));

    dialog = gtk_message_dialog_new (
        NULL, 0, GTK_MESSAGE_WARNING,
        #ifdef HAVE_HILDON_2_2
        GTK_BUTTONS_NONE,
        #else
        GTK_BUTTONS_OK,
        #endif
        _("Midori seems to have crashed the last time it was opened. "
          "If this happened repeatedly, try one of the following options "
          "to solve the problem."));
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
    gtk_window_set_title (GTK_WINDOW (dialog), g_get_application_name ());
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    screen = gtk_widget_get_screen (dialog);
    if (screen)
    {
        icon_theme = gtk_icon_theme_get_for_screen (screen);
        if (gtk_icon_theme_has_icon (icon_theme, "midori"))
            gtk_window_set_icon_name (GTK_WINDOW (dialog), "midori");
        else
            gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");
    }
    box = gtk_hbox_new (FALSE, 0);
    button = gtk_button_new_with_mnemonic (_("Modify _preferences"));
    g_signal_connect (button, "clicked",
        G_CALLBACK (button_modify_preferences_clicked_cb), settings);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 4);
    button = gtk_button_new_with_mnemonic (_("Reset the last _session"));
    g_signal_connect (button, "clicked",
        G_CALLBACK (button_reset_session_clicked_cb), _session);
    gtk_widget_set_sensitive (button, !katze_array_is_empty (_session));
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 4);
    button = gtk_button_new_with_mnemonic (_("Disable all _extensions"));
    if (g_object_get_data (G_OBJECT (app), "extensions"))
        g_signal_connect (button, "clicked",
            G_CALLBACK (button_disable_extensions_clicked_cb), app);
    else
        gtk_widget_set_sensitive (button, FALSE);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 4);
    gtk_widget_show_all (box);
    gtk_container_add (GTK_CONTAINER (content_area), box);
    button = katze_property_proxy (settings, "show-crash-dialog", NULL);
    gtk_widget_show (button);
    gtk_container_add (GTK_CONTAINER (content_area), button);
    #ifdef HAVE_HILDON_2_2
    box = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), box, TRUE, FALSE, 4);
    button = hildon_gtk_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_HALFSCREEN_WIDTH);
    gtk_button_set_label (GTK_BUTTON (button), GTK_STOCK_OK);
    gtk_button_set_use_stock (GTK_BUTTON (button), TRUE);
    g_signal_connect_swapped (button, "clicked",
        G_CALLBACK (gtk_widget_destroy), dialog);
    gtk_box_pack_start (GTK_BOX (box), button, TRUE, FALSE, 4);
    gtk_widget_show_all (box);
    #endif
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
    return dialog;
}

static gboolean
midori_load_soup_session_full (gpointer settings)
{
    SoupSession* session = webkit_get_default_session ();
    SoupCookieJar* jar;
    gchar* config_file;
    SoupSessionFeature* feature;

    midori_load_soup_session (settings);

    config_file = build_config_filename ("logins");
    feature = g_object_new (KATZE_TYPE_HTTP_AUTH, "filename", config_file, NULL);
    g_free (config_file);
    soup_session_add_feature (session, feature);
    g_object_unref (feature);

    jar = soup_cookie_jar_new ();
    g_object_set_data (G_OBJECT (jar), "midori-settings", settings);
    soup_session_add_feature (session, SOUP_SESSION_FEATURE (jar));
    g_object_unref (jar);

    feature = g_object_new (KATZE_TYPE_HTTP_COOKIES, NULL);
    config_file = build_config_filename ("cookies.txt");
    g_object_set_data_full (G_OBJECT (feature), "filename",
                            config_file, (GDestroyNotify)g_free);
    soup_session_add_feature (session, feature);
    g_object_unref (feature);

    return FALSE;
}

static gboolean
midori_load_extensions (gpointer data)
{
    MidoriApp* app = MIDORI_APP (data);
    gchar** active_extensions = g_object_get_data (G_OBJECT (app), "extensions");
    KatzeArray* extensions;
    #ifdef G_ENABLE_DEBUG
    gboolean startup_timer = g_getenv ("MIDORI_STARTTIME") != NULL;
    GTimer* timer;
    if (startup_timer)
        timer = g_timer_new ();
    #endif

    /* Load extensions */
    extensions = katze_array_new (MIDORI_TYPE_EXTENSION);
    g_object_set (app, "extensions", extensions, NULL);
    if (g_module_supported ())
    {
        /* FIXME: Read extensions from system data dirs */
        gchar* extension_path;
        GDir* extension_dir;

        if (!(extension_path = g_strdup (g_getenv ("MIDORI_EXTENSION_PATH"))))
        {
            #ifdef G_OS_WIN32
            {
                gchar *path = g_win32_get_package_installation_directory_of_module (NULL);
                extension_path = g_build_filename (path, "lib", PACKAGE_NAME, NULL);
                g_free (path);
                if (g_access (extension_path, F_OK) != 0)
                {
                    g_free (extension_path);
                    extension_path = g_build_filename (LIBDIR, PACKAGE_NAME, NULL);
                }
            }
            #else
            extension_path = g_build_filename (LIBDIR, PACKAGE_NAME, NULL);
            #endif
        }
        extension_dir = g_dir_open (extension_path, 0, NULL);
        if (extension_dir != NULL)
        {
            const gchar* filename;

            while ((filename = g_dir_read_name (extension_dir)))
            {
                gchar* fullname;
                GModule* module;
                typedef MidoriExtension* (*extension_init_func)(void);
                extension_init_func extension_init;
                MidoriExtension* extension = NULL;

                /* Ignore files which don't have the correct suffix */
                if (!g_str_has_suffix (filename, G_MODULE_SUFFIX))
                    continue;

                fullname = g_build_filename (extension_path, filename, NULL);
                module = g_module_open (fullname, G_MODULE_BIND_LOCAL);
                g_free (fullname);

                if (module && g_module_symbol (module, "extension_init",
                                               (gpointer) &extension_init))
                {
                    extension = extension_init ();
                    if (extension != NULL)
                    {
                        /* Signal that we want the extension to load and save */
                        g_object_set_data_full (G_OBJECT (extension), "filename",
                                                g_strdup (filename), g_free);
                        if (midori_extension_is_prepared (extension))
                            midori_extension_get_config_dir (extension);
                    }
                }

                if (!extension)
                {
                    extension = g_object_new (MIDORI_TYPE_EXTENSION,
                                              "name", filename,
                                              "description", g_module_error (),
                                              NULL);
                    g_warning ("%s", g_module_error ());
                }
                katze_array_add_item (extensions, extension);
                if (active_extensions)
                {
                    guint i = 0;
                    gchar* name;
                    while ((name = active_extensions[i++]))
                        if (!g_strcmp0 (filename, name))
                            g_signal_emit_by_name (extension, "activate", app);
                }
                g_signal_connect_after (extension, "activate",
                    G_CALLBACK (extension_activate_cb), app);
                g_signal_connect_after (extension, "deactivate",
                    G_CALLBACK (extension_activate_cb), app);
                g_object_unref (extension);
            }
            g_dir_close (extension_dir);
        }
        g_free (extension_path);
    }
    g_strfreev (active_extensions);

    #ifdef G_ENABLE_DEBUG
    if (startup_timer)
        g_debug ("Extensions:\t%f", g_test_timer_elapsed ());
    #endif

    return FALSE;
}

static void
midori_browser_action_last_session_activate_cb (GtkAction*     action,
                                                MidoriBrowser* browser)
{
    KatzeArray* old_session = katze_array_new (KATZE_TYPE_ITEM);
    gchar* config_file = build_config_filename ("session.old.xbel");
    GError* error = NULL;
    if (midori_array_from_file (old_session, config_file, "xbel", &error))
    {
        KatzeItem* item;
        KATZE_ARRAY_FOREACH_ITEM (item, old_session)
            midori_browser_add_item (browser, item);
    }
    else
    {
        g_warning (_("The session couldn't be loaded: %s\n"), error->message);
        /* FIXME: Show a graphical dialog */
        g_error_free (error);
    }
    g_free (config_file);
    gtk_action_set_sensitive (action, FALSE);
    g_signal_handlers_disconnect_by_func (action,
        midori_browser_action_last_session_activate_cb, browser);
}

static gboolean
midori_load_session (gpointer data)
{
    KatzeArray* _session = KATZE_ARRAY (data);
    MidoriBrowser* browser;
    MidoriApp* app = katze_item_get_parent (KATZE_ITEM (_session));
    MidoriWebSettings* settings = katze_object_get_object (app, "settings");
    gchar* config_file;
    KatzeArray* session;
    KatzeItem* item;
    gint64 current;
    MidoriStartup load_on_startup;
    gchar** command = g_object_get_data (G_OBJECT (app), "execute-command");
    #ifdef G_ENABLE_DEBUG
    gboolean startup_timer = g_getenv ("MIDORI_STARTTIME") != NULL;
    GTimer* timer;
    if (startup_timer)
        timer = g_timer_new ();
    #endif

    browser = midori_app_create_browser (app);
    g_signal_connect_after (katze_object_get_object (app, "settings"), "notify",
        G_CALLBACK (settings_notify_cb), app);

    config_file = build_config_filename ("session.old.xbel");
    if (g_access (config_file, F_OK) == 0)
    {
        GtkActionGroup* action_group = midori_browser_get_action_group (browser);
        GtkAction* action = gtk_action_group_get_action (action_group, "LastSession");
        g_signal_connect (action, "activate",
            G_CALLBACK (midori_browser_action_last_session_activate_cb), browser);
        gtk_action_set_visible (action, TRUE);
    }
    midori_app_add_browser (app, browser);
    gtk_widget_show (GTK_WIDGET (browser));

    katze_assign (config_file, build_config_filename ("accels"));
    g_signal_connect_after (gtk_accel_map_get (), "changed",
        G_CALLBACK (accel_map_changed_cb), NULL);

    g_object_get (settings, "load-on-startup", &load_on_startup, NULL);

    if (katze_array_is_empty (_session))
    {
        gchar* homepage;
        item = katze_item_new ();

        if (load_on_startup == MIDORI_STARTUP_BLANK_PAGE)
            katze_item_set_uri (item, "");
        else
        {
            g_object_get (settings, "homepage", &homepage, NULL);
            katze_item_set_uri (item, homepage);
            g_free (homepage);
        }
        katze_array_add_item (_session, item);
        g_object_unref (item);
    }

    if (load_on_startup == MIDORI_STARTUP_DELAYED_PAGES)
        midori_session_add_delay (_session);

    session = midori_browser_get_proxy_array (browser);
    KATZE_ARRAY_FOREACH_ITEM (item, _session)
    {
        g_object_set_data (G_OBJECT (item), "midori-view-append", (void*)1);
        katze_item_set_meta_integer (item, "dont-write-history", 1);
        midori_browser_add_item (browser, item);
    }
    current = katze_item_get_meta_integer (KATZE_ITEM (_session), "current");
    if (!(item = katze_array_get_nth_item (_session, current)))
    {
        current = 0;
        item = katze_array_get_nth_item (_session, 0);
    }
    midori_browser_set_current_page (browser, current);
    if (!g_strcmp0 (katze_item_get_uri (item), ""))
        midori_browser_activate_action (browser, "Location");

    g_object_unref (settings);
    g_object_unref (_session);

    katze_assign (config_file, build_config_filename ("session.xbel"));
    g_signal_connect_after (browser, "add-tab",
        G_CALLBACK (midori_browser_session_cb), session);
    g_signal_connect_after (browser, "remove-tab",
        G_CALLBACK (midori_browser_session_cb), session);
    g_signal_connect (app, "quit",
        G_CALLBACK (midori_app_quit_cb), session);
    g_object_weak_ref (G_OBJECT (session),
        (GWeakNotify)(midori_browser_weak_notify_cb), browser);

    if (command)
        midori_app_send_command (app, command);

    #ifdef G_ENABLE_DEBUG
    if (startup_timer)
        g_debug ("Session setup:\t%f", g_test_timer_elapsed ());
    #endif

    return FALSE;
}

static gint
midori_run_script (const gchar* filename)
{
    gchar* exception;
    gchar* script;
    GError* error;

    if (!(filename))
    {
        g_print ("%s - %s\n", _("Midori"), _("No filename specified"));
        return 1;
    }

    error = NULL;
    if (g_file_get_contents (filename, &script, NULL, &error))
    {
        JSGlobalContextRef js_context = JSGlobalContextCreateInGroup (NULL, NULL);
        if (sokoke_js_script_eval (js_context, script, &exception))
            exception = NULL;
        g_free (script);
        JSGlobalContextRelease (js_context);
    }
    else if (error)
    {
        exception = g_strdup (error->message);
        g_error_free (error);
    }
    else
        exception = g_strdup (_("An unknown error occured."));

    if (!exception)
        return 0;

    g_print ("%s - Exception: %s\n", filename, exception);
    return 1;
}

#if WEBKIT_CHECK_VERSION (1, 1, 6)
static void
snapshot_load_finished_cb (GtkWidget*      web_view,
                           WebKitWebFrame* web_frame,
                           gchar*          filename)
{
    GError* error;
    GtkPrintOperation* operation = gtk_print_operation_new ();
    GtkPrintOperationAction action = GTK_PRINT_OPERATION_ACTION_EXPORT;
    GtkPrintOperationResult result;

    gtk_print_operation_set_export_filename (operation, filename);
    error = NULL;
    result = webkit_web_frame_print_full (web_frame, operation, action, &error);

    if (error != NULL)
    {
        g_error ("%s", error->message);
        gtk_main_quit ();
    }

    g_object_unref (operation);
    g_print (_("Snapshot saved to: %s\n"), filename);
    gtk_main_quit ();
}
#endif

static void
midori_web_app_browser_notify_load_status_cb (MidoriBrowser* browser,
                                              GParamSpec*    pspec,
                                              gpointer       data)
{
    if (katze_object_get_enum (browser, "load-status") != MIDORI_LOAD_PROVISIONAL)
    {
        GtkWidget* view = midori_browser_get_current_tab (browser);
        GdkPixbuf* icon = midori_view_get_icon (MIDORI_VIEW (view));
        if (midori_view_is_blank (MIDORI_VIEW (view)))
            icon = NULL;
        gtk_window_set_icon (GTK_WINDOW (browser), icon);
    }
}

static void
midori_remove_config_file (gint         clear_prefs,
                           gint         flag,
                           const gchar* filename)
{
    if ((clear_prefs & flag) == flag)
    {
        gchar* config_file = build_config_filename (filename);
        g_unlink (config_file);
        g_free (config_file);
    }
}

static gchar*
midori_prepare_uri (const gchar *uri)
{
    gchar* uri_ready;

    if (g_path_is_absolute (uri))
        return g_filename_to_uri (uri, NULL, NULL);
    else if (g_file_test (uri, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
    {
        gchar* current_dir = g_get_current_dir ();
        uri_ready = g_strconcat ("file://", current_dir,
                                 G_DIR_SEPARATOR_S, uri, NULL);
        g_free (current_dir);
        return uri_ready;
    }

    uri_ready = sokoke_magic_uri (uri);
    if (uri_ready)
        return sokoke_uri_to_ascii (uri_ready);

    return sokoke_uri_to_ascii (uri);
}

#ifdef HAVE_SIGNAL_H
static void
signal_handler (int signal_id)
{
    signal (signal_id, 0);
    midori_app_quit_cb (NULL, NULL);
    if (kill (getpid (), signal_id))
      exit (1);
}
#endif

static GKeyFile*
speeddial_new_from_file (const gchar* config,
                         GError**     error)
{

    GKeyFile* key_file = g_key_file_new ();
    gchar* config_file = g_build_filename (config, "speeddial", NULL);
    guint i = 0;
    guint columns = 3;
    guint slot_count = 0;
    guint rows;
    gchar* slot = NULL;
    gchar* dial_id = NULL;
    gchar* uri = NULL;
    gchar* json_content;
    gchar** parts;

    if (g_key_file_load_from_file (key_file, config_file, G_KEY_FILE_NONE, error))
    {
        g_free (config_file);
        return key_file;
    }

    katze_assign (config_file, g_build_filename (config, "speeddial.json", NULL));
    g_file_get_contents (config_file, &json_content, NULL, NULL);
    parts = g_strsplit (json_content ? json_content : "", ",", -1);
    while (parts && parts[i] != NULL)
    {
        gchar* key;
        gchar* val;
        gchar** values = g_strsplit (parts[i], "\"", -1);

        if (*values[1])
        {
            if (!g_strcmp0 (values[1], "shortcuts"))
            {
                key = g_strdup (values[3]);
                val = g_strdup (values[5]);
            }
            else if (!g_strcmp0 (values[1], "thumb"))
            {
                key = g_strdup (values[1]);
                val = g_strdup (values[2]);
            }
            else
            {
                key = g_strdup (values[1]);
                val = g_strdup (values[3]);
            }

            if (g_str_equal (key, "id"))
            {
                katze_assign (slot, g_strdup (val));
                dial_id = g_strdup_printf ("Dial %s", slot + 1);
                slot_count++;
            }
            else if (g_str_equal (key, "href"))
            {
                katze_assign (uri, g_strdup (val));
                g_key_file_set_value (key_file, dial_id, "uri", uri);
            }
            else if (g_str_equal (key, "img") && (*val && strncmp (val, "#", 1)))
            {
                gsize sz;
                gint state = 0;
                guint save = 0;
                gchar* thumb_dir;
                gchar* thumb_path;
                gsize base64_size = strlen (val);
                guchar* decoded = g_malloc0 ((base64_size * 3) / 4);

                sz = g_base64_decode_step (g_strdup (val), base64_size,
                                           decoded, &state, &save);
                thumb_dir = g_build_path (G_DIR_SEPARATOR_S, g_get_user_cache_dir (),
                                          PACKAGE_NAME, "thumbnails", NULL);
                if (!g_file_test (thumb_dir, G_FILE_TEST_EXISTS))
                    katze_mkdir_with_parents (thumb_dir, 0700);
                thumb_path = sokoke_build_thumbnail_path (uri);
                g_file_set_contents (thumb_path, (gchar*)decoded, sz, NULL);

                g_free (decoded);
                g_free (thumb_dir);
                g_free (thumb_path);
            }
            else if (g_str_equal (key, "thumb"))
            {
                guint thumb_size;
                gchar* thumb_size_type;
                gchar* size_tmp = g_strndup (val + 1, strlen (val) - 3);

                thumb_size = atoi (size_tmp);
                g_free (size_tmp);
                if (thumb_size == 80)
                    thumb_size_type = g_strdup ("SMALL");
                else if (thumb_size == 240)
                    thumb_size_type = g_strdup ("BIG");
                else /* if (thumb_size == 160) */
                    thumb_size_type = g_strdup ("MEDIUM");
                g_key_file_set_value (key_file, "settings", "size", thumb_size_type);

                g_free (thumb_size_type);
            }
            else if (g_str_equal (key, "title") && *val)
            {
                g_key_file_set_value (key_file, dial_id, key, val + 3);
            }
            else if (g_str_equal (key, "width"))
            {
                columns = atoi (val);
            }

            i++;

            g_free (key);
            g_free (val);
            g_strfreev (values);
        }
    }

    /* Default to 3 x 3 grid. Calculate rows here, columns is initialized as 3 */
    rows = slot_count / columns > 3 ? slot_count / columns : 3;
    g_key_file_set_integer (key_file, "settings", "columns", columns);
    g_key_file_set_integer (key_file, "settings", "rows", rows);

    katze_assign (config_file, g_build_filename (config, "speeddial", NULL));
    sokoke_key_file_save_to_file (key_file, config_file, NULL);

    g_strfreev (parts);
    g_free (dial_id);
    g_free (slot);
    g_free (config_file);
    g_free (json_content);
    return key_file;
}

static void
midori_soup_session_block_uris_cb (SoupSession* session,
                                   SoupMessage* msg,
                                   gchar*       blocked_uris)
{
    static GRegex* regex = NULL;
    SoupURI* soup_uri;
    gchar* uri;
    if (!regex)
        regex = g_regex_new (blocked_uris, 0, 0, NULL);
    soup_uri = soup_message_get_uri (msg);
    uri = soup_uri_to_string (soup_uri, FALSE);
    if (g_regex_match (regex, uri, 0, 0))
    {
        soup_uri = soup_uri_new ("http://.invalid");
        soup_message_set_uri (msg, soup_uri);
        soup_uri_free (soup_uri);
    }
    g_free (uri);
}

typedef struct {
     MidoriBrowser* browser;
     guint timeout;
     gchar* uri;
} MidoriInactivityTimeout;

static gboolean
midori_inactivity_timeout (gpointer data)
{
    #ifdef HAVE_X11_EXTENSIONS_SCRNSAVER_H
    MidoriInactivityTimeout* mit = data;
    static Display* xdisplay = NULL;
    static XScreenSaverInfo* mit_info = NULL;
    static int has_extension = -1;
    int event_base, error_base;

    if (has_extension == -1)
    {
        GdkDisplay* display = gtk_widget_get_display (GTK_WIDGET (mit->browser));
        xdisplay = GDK_DISPLAY_XDISPLAY (display);
        has_extension = XScreenSaverQueryExtension (xdisplay,
                                                    &event_base, &error_base);
    }

    if (has_extension)
    {
        if (!mit_info)
            mit_info = XScreenSaverAllocInfo ();

        XScreenSaverQueryInfo (xdisplay, RootWindow (xdisplay, 0), mit_info);
        if (mit_info->idle / 1000 > mit->timeout)
        {
            guint i = 0;
            GtkWidget* view;
            KatzeArray* history = katze_object_get_object (mit->browser, "history");
            KatzeArray* trash = katze_object_get_object (mit->browser, "trash");
            GList* data_items = sokoke_register_privacy_item (NULL, NULL, NULL);

            while ((view = midori_browser_get_nth_tab (mit->browser, i++)))
                gtk_widget_destroy (view);
            midori_browser_set_current_uri (mit->browser, mit->uri);
            /* Clear all private data */
            if (history != NULL)
                katze_array_clear (history);
            if (trash != NULL)
                katze_array_clear (trash);
            for (; data_items != NULL; data_items = g_list_next (data_items))
                ((SokokePrivacyItem*)(data_items->data))->clear ();
        }
    }
    #else
    /* TODO: Implement for other windowing systems */
    #endif

    return TRUE;
}

static void
midori_setup_inactivity_reset (MidoriBrowser* browser,
                               gint           inactivity_reset,
                               const gchar*   uri)
{
    if (inactivity_reset > 0)
    {
        MidoriInactivityTimeout* mit = g_new (MidoriInactivityTimeout, 1);
        mit->browser = browser;
        mit->timeout = inactivity_reset;
        mit->uri = g_strdup (uri);
        g_timeout_add_seconds (inactivity_reset, midori_inactivity_timeout,
                               mit);
    }
}

static void
midori_clear_page_icons_cb (void)
{
    gchar* cache = g_build_filename (g_get_user_cache_dir (),
                                     PACKAGE_NAME, "icons", NULL);
    sokoke_remove_path (cache, TRUE);
    g_free (cache);
    cache = g_build_filename (g_get_user_data_dir (),
                              "webkit", "icondatabase", NULL);
    sokoke_remove_path (cache, TRUE);
    g_free (cache);
}

static void
midori_clear_web_cookies_cb (void)
{
    SoupSession* session = webkit_get_default_session ();
    SoupSessionFeature* jar = soup_session_get_feature (session, SOUP_TYPE_COOKIE_JAR);
    GSList* cookies = soup_cookie_jar_all_cookies (SOUP_COOKIE_JAR (jar));
    SoupSessionFeature* feature;

    for (; cookies != NULL; cookies = g_slist_next (cookies))
    {
        SoupCookie* cookie = cookies->data;
        soup_cookie_jar_delete_cookie ((SoupCookieJar*)jar, cookie);
    }
    soup_cookies_free (cookies);
    /* Removing KatzeHttpCookies makes it save outstanding changes */
    if ((feature = soup_session_get_feature (session, KATZE_TYPE_HTTP_COOKIES)))
    {
        g_object_ref (feature);
        soup_session_remove_feature (session, feature);
        soup_session_add_feature (session, feature);
        g_object_unref (feature);
    }
}

#ifdef GDK_WINDOWING_X11
static void
midori_clear_flash_cookies_cb (void)
{
    gchar* cache = g_build_filename (g_get_home_dir (), ".macromedia",
                                     "Flash_Player", NULL);
    sokoke_remove_path (cache, TRUE);
    g_free (cache);
}
#endif

#if WEBKIT_CHECK_VERSION (1, 1, 14)
static void
midori_clear_html5_databases_cb (void)
{
    webkit_remove_all_web_databases ();
}
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 13)
static void
midori_clear_offline_appcache_cb (void)
{
    /* Changing the size implies clearing the cache */
    unsigned long long maximum = webkit_application_cache_get_maximum_size ();
    webkit_application_cache_set_maximum_size (maximum - 1);
}
#endif

int
main (int    argc,
      char** argv)
{
    gchar* webapp;
    gchar* config;
    gboolean private;
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
       #if !HAVE_HILDON
       { "config", 'c', 0, G_OPTION_ARG_FILENAME, &config,
       N_("Use FOLDER as configuration folder"), N_("FOLDER") },
       #endif
       { "private", 'p', 0, G_OPTION_ARG_NONE, &private,
       N_("Private browsing, no changes are saved"), NULL },
       { "diagnostic-dialog", 'd', 0, G_OPTION_ARG_NONE, &diagnostic_dialog,
       N_("Show a diagnostic dialog"), NULL },
       { "run", 'r', 0, G_OPTION_ARG_NONE, &run,
       N_("Run the specified filename as javascript"), NULL },
       #if WEBKIT_CHECK_VERSION (1, 1, 6)
       { "snapshot", 's', 0, G_OPTION_ARG_STRING, &snapshot,
       N_("Take a snapshot of the specified URI"), NULL },
       #endif
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
    gchar* bookmarks_file;
    GKeyFile* speeddial;
    gboolean bookmarks_exist;
    MidoriStartup load_on_startup;
    KatzeArray* search_engines;
    KatzeArray* bookmarks;
    KatzeArray* history;
    KatzeArray* _session;
    KatzeArray* trash;
    guint i;
    gchar* uri;
    KatzeItem* item;
    gchar* uri_ready;
    gchar* errmsg;
    sqlite3* db;
    gint max_history_age;
    gint clear_prefs = MIDORI_CLEAR_NONE;
    #ifdef G_ENABLE_DEBUG
        gboolean startup_timer = g_getenv ("MIDORI_STARTTIME") != NULL;
        #define midori_startup_timer(tmrmsg) if (startup_timer) \
            g_debug (tmrmsg, (g_test_timer_last () - g_test_timer_elapsed ()) * -1)
    #else
        #define midori_startup_timer(tmrmsg)
    #endif

    #if ENABLE_NLS
    setlocale (LC_ALL, "");
    if (g_getenv ("MIDORI_NLSPATH"))
        bindtextdomain (GETTEXT_PACKAGE, g_getenv ("MIDORI_NLSPATH"));
    else
    #ifdef G_OS_WIN32
    {
        gchar* path = sokoke_find_data_filename ("locale");
        bindtextdomain (GETTEXT_PACKAGE, path);
        g_free (path);
    }
    #else
        bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    #endif
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    #endif

    #ifdef HAVE_SIGNAL_H
    #ifdef SIGHUP
    signal (SIGHUP, &signal_handler);
    #endif
    #ifdef SIGINT
    signal (SIGINT, &signal_handler);
    #endif
    #ifdef SIGTERM
    signal (SIGTERM, &signal_handler);
    #endif
    #ifdef SIGQUIT
    signal (SIGQUIT, &signal_handler);
    #endif
    #endif

    /* Preserve argument vector */
    sokoke_get_argv (argv);

    /* Parse cli options */
    webapp = NULL;
    config = NULL;
    private = FALSE;
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
    if (!gtk_init_with_args (&argc, &argv, _("[Addresses]"), entries,
                             GETTEXT_PACKAGE, &error))
    {
        g_print ("%s - %s\n", _("Midori"), error->message);
        g_error_free (error);
        return 1;
    }

    /* libSoup uses threads, so we need to initialize threads. */
    if (!g_thread_supported ()) g_thread_init (NULL);
    sokoke_register_stock_items ();

    if (config && !g_path_is_absolute (config))
    {
        g_critical (_("The specified configuration folder is invalid."));
        return 1;
    }

    /* Private browsing, window title, default config folder */
    if (private)
    {
        g_set_application_name (_("Midori (Private Browsing)"));
        if (!config && !webapp)
            config = g_build_filename (g_get_user_config_dir (), PACKAGE_NAME, NULL);
        /* Mask the timezone, which can be read by Javascript */
        g_setenv ("TZ", "UTC", TRUE);
    }
    else
        g_set_application_name (_("Midori"));

    #ifdef G_ENABLE_DEBUG
    if (startup_timer)
        g_test_timer_start ();
    #endif

    if (version)
    {
        g_print (
          "%s %s\n\n"
          "Copyright (c) 2007-2011 Christian Dywan\n\n"
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

    if (help_execute)
    {
        MidoriBrowser* browser = midori_browser_new ();
        GtkActionGroup* action_group = midori_browser_get_action_group (browser);
        GList* actions = gtk_action_group_list_actions (action_group);
        for (; actions; actions = g_list_next (actions))
        {
            GtkAction* action = actions->data;
            const gchar* name = gtk_action_get_name (action);
            const gchar* space = "                       ";
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
        g_list_free (actions);
        gtk_widget_destroy (GTK_WIDGET (browser));
        return 0;
    }

    #if WEBKIT_CHECK_VERSION (1, 1, 6)
    if (snapshot)
    {
        gchar* filename;
        gint fd;
        GtkWidget* web_view;

        fd = g_file_open_tmp ("snapshot-XXXXXX.pdf", &filename, &error);
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
        g_signal_connect (web_view, "load-finished",
            G_CALLBACK (snapshot_load_finished_cb), filename);
        webkit_web_view_open (WEBKIT_WEB_VIEW (web_view), snapshot);
        gtk_main ();
        g_free (filename);
        return 0;
    }
    #endif

    sokoke_register_privacy_item ("page-icons", _("Website icons"),
        G_CALLBACK (midori_clear_page_icons_cb));
    sokoke_register_privacy_item ("web-cookies", _("Cookies"),
        G_CALLBACK (midori_clear_web_cookies_cb));
    #ifdef GDK_WINDOWING_X11
    sokoke_register_privacy_item ("flash-cookies", _("'Flash' Cookies"),
        G_CALLBACK (midori_clear_flash_cookies_cb));
    #endif
    #if WEBKIT_CHECK_VERSION (1, 1, 14)
    sokoke_register_privacy_item ("html5-databases", _("HTML5 _Databases"),
        G_CALLBACK (midori_clear_html5_databases_cb));
    #endif
    #if WEBKIT_CHECK_VERSION (1, 3, 13)
    sokoke_register_privacy_item ("offline-appcache", _("Offline Application Cache"),
        G_CALLBACK (midori_clear_offline_appcache_cb));
    #endif

    /* Web Application or Private Browsing support */
    if (webapp || private)
    {
        SoupSession* session = webkit_get_default_session ();
        MidoriBrowser* browser = midori_browser_new ();
        midori_startup_timer ("Browser: \t%f");

        if (config)
        {
            settings = settings_and_accels_new (config, &extensions);
            g_strfreev (extensions);
            search_engines = search_engines_new_from_folder (config, NULL);
            g_object_set (browser, "search-engines", search_engines, NULL);
            g_object_unref (search_engines);
            speeddial = speeddial_new_from_file (config, &error);
            g_object_set (browser, "speed-dial", speeddial, NULL);
        }
        else
            settings = g_object_ref (midori_browser_get_settings (browser));

        if (webapp)
        {
            gchar* tmp_uri = midori_prepare_uri (webapp);
            g_object_set (settings,
                          "show-menubar", FALSE,
                          "show-navigationbar", FALSE,
                          "toolbar-items", "Back,Forward,ReloadStop,Location,Homepage",
                          "homepage", tmp_uri,
                          "show-statusbar", FALSE,
                          "enable-developer-extras", FALSE,
                          NULL);
            midori_browser_set_action_visible (browser, "Menubar", FALSE);
            midori_browser_add_uri (browser, tmp_uri);
            g_free (tmp_uri);
            /* Update window icon according to page */
            g_signal_connect (browser, "notify::load-status",
                G_CALLBACK (midori_web_app_browser_notify_load_status_cb), NULL);
        }

        if (private)
        {
            g_object_set (settings,
                          "preferred-languages", "en",
            #if WEBKIT_CHECK_VERSION (1, 1, 2)
                          "enable-private-browsing", TRUE,
            #endif
            #if WEBKIT_CHECK_VERSION (1, 1, 8)
                          "enable-html5-database", FALSE,
                          "enable-html5-local-storage", FALSE,
                          "enable-offline-web-application-cache", FALSE,
            #endif
            /* Arguably DNS prefetching is or isn't a privacy concern. For the
             * lack of more fine-grained control we'll go the safe route. */
                          "enable-dns-prefetching", FALSE,
                          "strip-referer", TRUE, NULL);
            midori_browser_set_action_visible (browser, "Tools", FALSE);
            midori_browser_set_action_visible (browser, "ClearPrivateData", FALSE);
        }

        if (private || !config)
        {
            /* Disable saving by setting an unwritable folder */
            sokoke_set_config_dir ("/");
        }

        g_object_set (settings, "show-panel", FALSE,
                      "last-window-state", MIDORI_WINDOW_NORMAL,
                      NULL);
        midori_browser_set_action_visible (browser, "Panel", FALSE);
        g_object_set (browser, "settings", settings, NULL);
        midori_startup_timer ("Setup config: \t%f");
        g_object_unref (settings);
        g_signal_connect (browser, "quit",
            G_CALLBACK (gtk_main_quit), NULL);
        g_signal_connect (browser, "destroy",
            G_CALLBACK (gtk_main_quit), NULL);
        gtk_widget_show (GTK_WIDGET (browser));
        midori_browser_activate_action (browser, "Location");
        if (execute)
        {
            for (i = 0; uris[i] != NULL; i++)
                midori_browser_activate_action (browser, uris[i]);
        }
        else if (uris != NULL)
        {
            for (i = 0; uris[i] != NULL; i++)
            {
                gchar* new_uri = midori_prepare_uri (uris[i]);
                midori_browser_add_uri (browser, new_uri);
                g_free (new_uri);
            }
        }

        if (midori_browser_get_current_uri (browser) == NULL)
            midori_browser_add_uri (browser, "about:blank");

        if (block_uris)
            g_signal_connect (session, "request-queued",
                G_CALLBACK (midori_soup_session_block_uris_cb),
                g_strdup (block_uris));
        midori_setup_inactivity_reset (browser, inactivity_reset, webapp);
        midori_load_soup_session (settings);
        midori_startup_timer ("App created: \t%f");
        gtk_main ();
        return 0;
    }

    /* FIXME: Inactivity reset is only supported for app mode */
    if (inactivity_reset > 0)
        g_error ("--inactivity-reset is currently only supported with --app.");

    /* Standalone javascript support */
    if (run)
        return midori_run_script (uris ? *uris : NULL);

    sokoke_set_config_dir (config);
    if (config)
    {
        gchar* name_hash;
        gchar* app_name;
        name_hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, config, -1);
        app_name = g_strconcat ("midori", "_", name_hash, NULL);
        g_free (name_hash);
        app = g_object_new (MIDORI_TYPE_APP, "name", app_name, NULL);
        g_free (app_name);
    }
    else
        app = midori_app_new ();
    katze_assign (config, (gchar*)sokoke_set_config_dir (NULL));
    midori_startup_timer ("App created: \t%f");

    /* FIXME: The app might be 'running' but actually showing a dialog
              after a crash, so running a new window isn't a good idea. */
    if (midori_app_instance_is_running (app))
    {
        GtkWidget* dialog;

        if (execute)
            result = midori_app_send_command (app, uris);
        else if (uris)
        {
            /* Encode any IDN addresses because libUnique doesn't like them */
            i = 0;
            while (uris[i] != NULL)
            {
                gchar* new_uri = midori_prepare_uri (uris[i]);
                katze_assign (uris[i], new_uri);
                i++;
            }
            result = midori_app_instance_send_uris (app, uris);
        }
        else
            result = midori_app_instance_send_new_browser (app);

        if (result)
            return 0;

        dialog = gtk_message_dialog_new (NULL,
            0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s",
            _("An instance of Midori is already running but not responding.\n"));
        if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_DELETE_EVENT)
            gtk_widget_destroy (dialog);
        /* FIXME: Allow killing the existing instance */
        return 1;
    }

    katze_mkdir_with_parents (config, 0700);
    /* Load configuration file */
    error_messages = g_string_new (NULL);
    error = NULL;
    settings = settings_and_accels_new (config, &extensions);
    g_object_set (settings, "enable-developer-extras", TRUE, NULL);
    #if WEBKIT_CHECK_VERSION (1, 1, 14)
    g_object_set (settings, "enable-html5-database", TRUE, NULL);
    #endif
    midori_startup_timer ("Config and accels read: \t%f");

    /* Load search engines */
    search_engines = search_engines_new_from_folder (config, error_messages);
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

    bookmarks = katze_array_new (KATZE_TYPE_ARRAY);
    bookmarks_file = g_build_filename (config, "bookmarks.db", NULL);
    bookmarks_exist = g_access (bookmarks_file, F_OK) == 0;
    errmsg = NULL;
    if ((db = midori_bookmarks_initialize (bookmarks, bookmarks_file, &errmsg)) == NULL)
    {
        g_string_append_printf (error_messages,
            _("Bookmarks couldn't be loaded: %s\n"), errmsg);
        g_free (errmsg);
    }
    else if (!bookmarks_exist)
    {
        /* Initial creation, import old bookmarks */
        gchar* old_bookmarks;
        if (g_path_is_absolute (BOOKMARK_FILE))
            old_bookmarks = g_strdup (BOOKMARK_FILE);
        else
            old_bookmarks = g_build_filename (config, BOOKMARK_FILE, NULL);
        if (g_access (old_bookmarks, F_OK) == 0)
        {
            midori_bookmarks_import (old_bookmarks, db);
            /* Leave old bookmarks around */
        }
        g_free (old_bookmarks);
    }
    g_object_set_data (G_OBJECT (bookmarks), "db", db);
    midori_startup_timer ("Bookmarks read: \t%f");

    config_file = NULL;
    _session = katze_array_new (KATZE_TYPE_ITEM);
    #if HAVE_LIBXML
    g_object_get (settings, "load-on-startup", &load_on_startup, NULL);
    if (load_on_startup >= MIDORI_STARTUP_LAST_OPEN_PAGES)
    {
        katze_assign (config_file, build_config_filename ("session.xbel"));
        error = NULL;
        if (!midori_array_from_file (_session, config_file, "xbel", &error))
        {
            if (error->code != G_FILE_ERROR_NOENT)
                g_string_append_printf (error_messages,
                    _("The session couldn't be loaded: %s\n"), error->message);
            g_error_free (error);
        }
    }
    #endif
    midori_startup_timer ("Session read: \t%f");

    trash = katze_array_new (KATZE_TYPE_ITEM);
    #if HAVE_LIBXML
    katze_assign (config_file, g_build_filename (config, "tabtrash.xbel", NULL));
    error = NULL;
    if (!midori_array_from_file (trash, config_file, "xbel", &error))
    {
        if (error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf (error_messages,
                _("The trash couldn't be loaded: %s\n"), error->message);
        g_error_free (error);
    }
    #endif

    midori_startup_timer ("Trash read: \t%f");
    history = katze_array_new (KATZE_TYPE_ARRAY);
    katze_assign (config_file, g_build_filename (config, "history.db", NULL));

    errmsg = NULL;
    if (!midori_history_initialize (history, config_file, bookmarks_file, &errmsg))
    {
        g_string_append_printf (error_messages,
            _("The history couldn't be loaded: %s\n"), errmsg);
        g_free (errmsg);
    }
    g_free (bookmarks_file);
    midori_startup_timer ("History read: \t%f");

    speeddial = speeddial_new_from_file (config, &error);

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
                                _("_Ignore"), GTK_RESPONSE_ACCEPT,
                                NULL);
        if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
        {
            g_object_unref (settings);
            g_object_unref (search_engines);
            g_object_unref (bookmarks);
            g_object_unref (_session);
            g_object_unref (trash);
            g_object_unref (history);
            g_string_free (error_messages, TRUE);
            return 0;
        }
        gtk_widget_destroy (dialog);
        /* FIXME: Since we will overwrite files that could not be loaded
                  , would we want to make backups? */
    }
    g_string_free (error_messages, TRUE);

    /* If -e or --execute was specified, "uris" refers to the command. */
    if (!execute)
    {
    /* Open as many tabs as we have uris, seperated by pipes */
    i = 0;
    while (uris && uris[i])
    {
        uri = strtok (g_strdup (uris[i]), "|");
        while (uri != NULL)
        {
            item = katze_item_new ();
            uri_ready = midori_prepare_uri (uri);
            katze_item_set_uri (item, uri_ready);
            g_free (uri_ready);
            katze_array_add_item (_session, item);
            uri = strtok (NULL, "|");
        }
        g_free (uri);
        i++;
    }
    }

    if (1)
    {
        g_signal_connect_after (search_engines, "add-item",
            G_CALLBACK (midori_search_engines_modify_cb), search_engines);
        g_signal_connect_after (search_engines, "remove-item",
            G_CALLBACK (midori_search_engines_modify_cb), search_engines);
        if (!katze_array_is_empty (search_engines))
        {
            KATZE_ARRAY_FOREACH_ITEM (item, search_engines)
                g_signal_connect_after (item, "notify",
                    G_CALLBACK (midori_search_engines_modify_cb), search_engines);
            g_signal_connect_after (search_engines, "move-item",
                G_CALLBACK (midori_search_engines_move_item_cb), search_engines);
        }
    }
    g_signal_connect_after (trash, "add-item",
        G_CALLBACK (midori_trash_add_item_cb), NULL);
    g_signal_connect_after (trash, "remove-item",
        G_CALLBACK (midori_trash_remove_item_cb), NULL);

    katze_item_set_parent (KATZE_ITEM (_session), app);
    g_object_set_data (G_OBJECT (app), "extensions", extensions);
    /* We test for the presence of a dummy file which is created once
       and deleted during normal runtime, but persists in case of a crash. */
    katze_assign (config_file, g_build_filename (config, "running", NULL));
    if (g_access (config_file, F_OK) == 0)
        back_from_crash = TRUE;
    else
        g_file_set_contents (config_file, "RUNNING", -1, NULL);

    if (back_from_crash)
    {
        if (katze_object_get_int (settings, "load-on-startup")
            >= MIDORI_STARTUP_LAST_OPEN_PAGES)
            midori_session_add_delay (_session);

        if (katze_object_get_boolean (settings, "show-crash-dialog"))
            diagnostic_dialog = TRUE;
    }

    if (diagnostic_dialog)
    {
        GtkWidget* dialog = midori_create_diagnostic_dialog (settings, _session);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
    }
    midori_startup_timer ("Signal setup: \t%f");

    g_object_set (app, "settings", settings,
                       "bookmarks", bookmarks,
                       "trash", trash,
                       "search-engines", search_engines,
                       "history", history,
                       "speed-dial", speeddial,
                       NULL);
    g_object_unref (history);
    g_object_unref (search_engines);
    g_object_unref (bookmarks);
    g_object_unref (trash);
    g_object_unref (settings);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (midori_app_add_browser_cb), NULL);
    midori_startup_timer ("App prepared: \t%f");

    g_idle_add (midori_load_soup_session_full, settings);
    g_idle_add (midori_load_extensions, app);
    g_idle_add (midori_load_session, _session);

    if (execute)
        g_object_set_data (G_OBJECT (app), "execute-command", uris);
    if (block_uris)
            g_signal_connect (webkit_get_default_session (), "request-queued",
                G_CALLBACK (midori_soup_session_block_uris_cb),
                g_strdup (block_uris));


    gtk_main ();

    settings = katze_object_get_object (app, "settings");
    settings_notify_cb (settings, NULL, app);

    g_object_get (settings, "maximum-history-age", &max_history_age, NULL);
    midori_history_terminate (history, max_history_age);
    /* Removing KatzeHttpCookies makes it save outstanding changes */
    soup_session_remove_feature_by_type (webkit_get_default_session (),
                                         KATZE_TYPE_HTTP_COOKIES);

    /* Clear data on quit, according to the Clear private data dialog */
    g_object_get (settings, "clear-private-data", &clear_prefs, NULL);
    if (clear_prefs & MIDORI_CLEAR_ON_QUIT)
    {
        GList* data_items = sokoke_register_privacy_item (NULL, NULL, NULL);
        gchar* clear_data = katze_object_get_string (settings, "clear-data");

        midori_remove_config_file (clear_prefs, MIDORI_CLEAR_HISTORY, "history.db");
        midori_remove_config_file (clear_prefs, MIDORI_CLEAR_TRASH, "tabtrash.xbel");

        for (; data_items != NULL; data_items = g_list_next (data_items))
        {
            SokokePrivacyItem* privacy = data_items->data;
            if (clear_data && strstr (clear_data, privacy->name))
                privacy->clear ();
        }
        g_free (clear_data);
    }

    if (katze_object_get_int (settings, "load-on-startup")
        < MIDORI_STARTUP_LAST_OPEN_PAGES)
    {
        katze_assign (config_file, g_build_filename (config, "session.xbel", NULL));
        g_unlink (config_file);
    }

    g_object_unref (settings);
    g_key_file_free (speeddial);
    g_object_unref (app);
    g_free (config_file);
    return 0;
}
