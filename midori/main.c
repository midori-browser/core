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
#include "midori-addons.h"
#include "midori-array.h"
#include "midori-bookmarks.h"
#include "midori-console.h"
#include "midori-extensions.h"
#include "midori-history.h"
#include "midori-plugins.h"
#include "midori-transfers.h"

#include "sokoke.h"
#include "compat.h"

#if HAVE_UNISTD_H
    #include <unistd.h>
    #define is_writable(_cfg_filename) \
        !g_access (_cfg_filename, W_OK) || \
        !g_file_test (_cfg_filename, G_FILE_TEST_EXISTS)
#else
    #define is_writable(_cfg_filename) 1
#endif
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#include <webkit/webkit.h>

#if HAVE_SQLITE
    #include <sqlite3.h>
#endif

#if ENABLE_NLS
    #include <libintl.h>
    #include <locale.h>
#endif

#if HAVE_HILDON
    #include <libosso.h>
#endif

#define MIDORI_HISTORY_ERROR g_quark_from_string("MIDORI_HISTORY_ERROR")

typedef enum
{
    MIDORI_HISTORY_ERROR_DB_OPEN,    /* Error opening the database file */
    MIDORI_HISTORY_ERROR_EXEC_SQL,   /* Error executing SQL statement */

} MidoriHistoryError;

static gchar*
build_config_filename (const gchar* filename)
{
    const gchar* path = sokoke_set_config_dir (NULL);
    g_mkdir_with_parents (path, 0700);
    return g_build_filename (path, filename, NULL);
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
    gchar* str;
    gint integer;
    gfloat number;
    gboolean boolean;

    if (!g_key_file_load_from_file (key_file, filename,
                                   G_KEY_FILE_KEEP_COMMENTS, &error))
    {
        if (error->code != G_FILE_ERROR_NOENT)
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
        if (type == G_TYPE_PARAM_STRING)
        {
            str = sokoke_key_file_get_string_default (key_file,
                "settings", property,
                G_PARAM_SPEC_STRING (pspec)->default_value, NULL);
            g_object_set (settings, property, str, NULL);
            g_free (str);
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
            str = sokoke_key_file_get_string_default (key_file,
                "settings", property,
                enum_value->value_name, NULL);
            enum_value = g_enum_get_value_by_name (enum_class, str);
            if (enum_value)
                g_object_set (settings, property, enum_value->value, NULL);
            else
                g_warning (_("Value '%s' is invalid for %s"),
                           str, property);

            g_free (str);
            g_type_class_unref (enum_class);
        }
        else
            g_warning (_("Invalid configuration value '%s'"), property);
    }
    g_free (pspecs);
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
    gboolean saved;

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
            gchar* prop_comment = g_strdup_printf ("# %s", property);
            g_key_file_set_string (key_file, "settings", prop_comment, "");
            g_free (prop_comment);
            continue;
        }
        if (type == G_TYPE_PARAM_STRING)
        {
            gchar* string;
            g_object_get (settings, property, &string, NULL);
            g_key_file_set_string (key_file, "settings", property,
                                   string ? string : "");
            g_free (string);
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
            g_warning (_("Invalid configuration value '%s'"), property);
    }
    g_free (pspecs);
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

static gboolean
search_engines_save_to_file (KatzeArray*  search_engines,
                             const gchar* filename,
                             GError**     error)
{
    GKeyFile* key_file;
    guint i, j, n_properties;
    KatzeItem* item;
    const gchar* name;
    GParamSpec** pspecs;
    const gchar* property;
    gchar* value;
    gboolean saved;

    key_file = g_key_file_new ();
    pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (search_engines),
                                             &n_properties);
    i = 0;
    while ((item = katze_array_get_nth_item (search_engines, i++)))
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

#if HAVE_SQLITE
/* Open database 'dbname' */
static sqlite3*
db_open (const char* dbname,
         GError**    error)
{
    sqlite3* db;

    if (sqlite3_open (dbname, &db))
    {
        if (error)
        {
            *error = g_error_new (MIDORI_HISTORY_ERROR,
                                  MIDORI_HISTORY_ERROR_DB_OPEN,
                                  _("Failed to open database: %s\n"),
                                  sqlite3_errmsg (db));
        }
        sqlite3_close (db);
        return NULL;
    }
    return (db);
}

/* Close database 'db' */
static void
db_close (sqlite3* db)
{
    sqlite3_close (db);
}

/* Execute an SQL statement and run 'callback' on the result data */
static gboolean
db_exec_callback (sqlite3*    db,
                  const char* sqlcmd,
                  int         (*callback)(void*, int, char**, char**),
                  void*       cbarg,
                  GError**    error)
{
    char* errmsg;

    if (sqlite3_exec (db, sqlcmd, callback, cbarg, &errmsg) != SQLITE_OK)
    {
        if (error)
        {
            *error = g_error_new (MIDORI_HISTORY_ERROR,
                                  MIDORI_HISTORY_ERROR_EXEC_SQL,
                                  _("Failed to execute database statement: %s\n"),
                                  errmsg);
        }
        sqlite3_free (errmsg);
        return FALSE;
    }
    return TRUE;
}

/* Execute a SQL statement */
static gboolean
db_exec (sqlite3*    db,
         const char* sqlcmd,
         GError**    error)
{
    return (db_exec_callback (db, sqlcmd, NULL, NULL, error));
}

/* sqlite method for retrieving the date/ time */
static int
gettimestr (void*  data,
            int    argc,
            char** argv,
            char** colname)
{
    KatzeItem* item = KATZE_ITEM (data);
    (void) colname;

    g_return_val_if_fail (argc == 1, 1);

    katze_item_set_added (item, g_ascii_strtoull (argv[0], NULL, 10));
    return 0;
}

static void
midori_history_remove_item_cb (KatzeArray* history,
                               KatzeItem*  item,
                               sqlite3*    db)
{
    gchar* sqlcmd;
    gboolean success = TRUE;
    GError* error = NULL;

    g_return_if_fail (KATZE_IS_ITEM (item));

    sqlcmd = sqlite3_mprintf (
        "DELETE FROM history WHERE uri = '%q' AND"
        " title = '%q' AND date = %" G_GINT64_FORMAT,
        katze_item_get_uri (item),
        katze_item_get_name (item),
        katze_item_get_added (item));
    success = db_exec (db, sqlcmd, &error);
    if (!success)
    {
        g_printerr (_("Failed to remove history item: %s\n"), error->message);
        g_error_free (error);
        return ;
    }
    sqlite3_free (sqlcmd);
}

static void
midori_history_clear_before_cb (KatzeArray* item,
                                sqlite3*    db)
{
    g_signal_handlers_block_by_func (item, midori_history_remove_item_cb, db);
}

static void
midori_history_clear_cb (KatzeArray* history,
                         sqlite3*    db)
{
    GError* error = NULL;

    g_return_if_fail (KATZE_IS_ARRAY (history));

    if (!db_exec (db, "DELETE FROM history", &error))
    {
        g_printerr (_("Failed to clear history: %s\n"), error->message);
        g_error_free (error);
    }
}

static void
midori_history_notify_item_cb (KatzeItem*  item,
                               GParamSpec* pspec,
                               sqlite3*    db)
{
    gchar* sqlcmd;
    gboolean success = TRUE;
    GError* error = NULL;

    sqlcmd = sqlite3_mprintf ("UPDATE history SET title='%q' WHERE "
                              "uri='%q' AND date=%" G_GUINT64_FORMAT,
                              katze_item_get_name (item),
                              katze_item_get_uri (item),
                              katze_item_get_added (item));
    success = db_exec (db, sqlcmd, &error);
    sqlite3_free (sqlcmd);
    if (!success)
    {
        g_printerr (_("Failed to add history item: %s\n"), error->message);
        g_error_free (error);
        return ;
    }
}

static void
midori_history_add_item_cb (KatzeArray* array,
                            KatzeItem*  item,
                            sqlite3*    db)
{
    gchar* sqlcmd;
    gboolean success = TRUE;
    GError* error = NULL;

    g_return_if_fail (KATZE_IS_ITEM (item));

    if (KATZE_IS_ARRAY (item))
    {
        g_signal_connect_after (item, "add-item",
                G_CALLBACK (midori_history_add_item_cb), db);
        g_signal_connect (item, "remove-item",
                G_CALLBACK (midori_history_remove_item_cb), db);
        g_signal_connect (item, "clear",
            G_CALLBACK (midori_history_clear_before_cb), db);
        return;
    }

    /* New item, set added to the current date/ time */
    if (!katze_item_get_added (item))
    {
        if (!db_exec_callback (db, "SELECT date('now')",
                               gettimestr, item, &error))
        {
            g_printerr (_("Failed to add history item: %s\n"), error->message);
            g_error_free (error);
            return;
        }
    }
    sqlcmd = sqlite3_mprintf ("INSERT INTO history VALUES"
                              "('%q', '%q', %" G_GUINT64_FORMAT ","
                              " %" G_GUINT64_FORMAT ")",
                              katze_item_get_uri (item),
                              katze_item_get_name (item),
                              katze_item_get_added (item),
                              katze_item_get_added (KATZE_ITEM (array)));
    success = db_exec (db, sqlcmd, &error);
    sqlite3_free (sqlcmd);
    if (!success)
    {
        g_printerr (_("Failed to add history item: %s\n"), error->message);
        g_error_free (error);
        return ;
    }

    /* The title is set after the item is added */
    g_signal_connect_after (item, "notify::name",
                            G_CALLBACK (midori_history_notify_item_cb), db);
}

static int
midori_history_add_items (void*  data,
                          int    argc,
                          char** argv,
                          char** colname)
{
    KatzeItem* item;
    KatzeArray* parent;
    KatzeArray* array;
    gint64 date;
    gint64 day;
    gint i;
    gint j;
    gint n;
    gint ncols = 4;
    gchar token[50];

    array = KATZE_ARRAY (data);
    g_return_val_if_fail (KATZE_IS_ARRAY (array), 1);

    /* Test whether have the right number of columns */
    g_return_val_if_fail (argc % ncols == 0, 1);

    for (i = 0; i < (argc - ncols) + 1; i++)
    {
        if (argv[i])
        {
            if (colname[i] && !g_ascii_strcasecmp (colname[i], "uri") &&
                colname[i + 1] && !g_ascii_strcasecmp (colname[i + 1], "title") &&
                colname[i + 2] && !g_ascii_strcasecmp (colname[i + 2], "date") &&
                colname[i + 3] && !g_ascii_strcasecmp (colname[i + 3], "day"))
            {
                item = katze_item_new ();
                katze_item_set_uri (item, argv[i]);
                katze_item_set_name (item, argv[i + 1]);
                date = g_ascii_strtoull (argv[i + 2], NULL, 10);
                day = g_ascii_strtoull (argv[i + 3], NULL, 10);
                katze_item_set_added (item, date);

                n = katze_array_get_length (array);
                for (j = n - 1; j >= 0; j--)
                {
                    parent = katze_array_get_nth_item (array, j);
                    if (day == katze_item_get_added (KATZE_ITEM (parent)))
                        break;
                }
                if (j < 0)
                {
                    parent = katze_array_new (KATZE_TYPE_ARRAY);
                    katze_item_set_added (KATZE_ITEM (parent), day);
                    strftime (token, sizeof (token), "%x",
                          localtime ((time_t *)&date));
                    katze_item_set_name (KATZE_ITEM (parent), token);
                    katze_array_add_item (array, parent);
                }
                katze_array_add_item (parent, item);
            }
        }
    }
    return 0;
}

static int
midori_history_test_day_column (void*  data,
                                int    argc,
                                char** argv,
                                char** colname)
{
    gint i;
    gboolean* has_day;

    has_day = (gboolean*)data;

    for (i = 0; i < argc; i++)
    {
        if (argv[i] &&
            !g_ascii_strcasecmp (colname[i], "name") &&
            !g_ascii_strcasecmp (argv[i], "day"))
        {
            *has_day = TRUE;
            break;
        }
    }

    return 0;
}

static sqlite3*
midori_history_initialize (KatzeArray*  array,
                           const gchar* filename,
                           GError**     error)
{
    sqlite3* db;
    KatzeItem* item;
    gint i;
    gboolean has_day;

    has_day = FALSE;

    if ((db = db_open (filename, error)) == NULL)
        return db;

    if (!db_exec (db,
                  "CREATE TABLE IF NOT EXISTS "
                  "history(uri text, title text, date integer, day integer)",
                  error))
        return NULL;

    if (!db_exec_callback (db,
                           "PRAGMA table_info(history)",
                           midori_history_test_day_column,
                           &has_day, error))
        return NULL;

    if (!has_day)
    {
        if (!db_exec (db,
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
                      error))
        return NULL;
    }

    if (!db_exec_callback (db,
                           "SELECT uri, title, date, day FROM history "
                           "ORDER BY date ASC",
                           midori_history_add_items,
                           array,
                           error))
        return NULL;

    i = 0;
    while ((item = katze_array_get_nth_item (array, i++)))
    {
        g_signal_connect_after (item, "add-item",
            G_CALLBACK (midori_history_add_item_cb), db);
        g_signal_connect (item, "remove-item",
            G_CALLBACK (midori_history_remove_item_cb), db);
        g_signal_connect (item, "clear",
            G_CALLBACK (midori_history_clear_before_cb), db);
    }
    return db;
}

static void
midori_history_terminate (sqlite3* db,
                          gint     max_history_age)
{
    gchar* sqlcmd;
    gboolean success = TRUE;
    GError* error = NULL;

    sqlcmd = g_strdup_printf (
        "DELETE FROM history WHERE "
        "(julianday(date('now')) - julianday(date(date,'unixepoch')))"
        " >= %d", max_history_age);
    db_exec (db, sqlcmd, &error);
    if (!success)
    {
        /* i18n: Couldn't remove items that are older than n days */
        g_printerr (_("Failed to remove old history items: %s\n"), error->message);
        g_error_free (error);
        return ;
    }
    g_free (sqlcmd);
    db_close (db);
}
#endif

static void
midori_app_quit_cb (MidoriApp* app)
{
    gchar* config_file = build_config_filename ("running");
    g_unlink (config_file);
    g_free (config_file);
}

static void
settings_notify_cb (MidoriWebSettings* settings,
                    GParamSpec*        pspec)
{
    gchar* config_file;
    GError* error;

    config_file = build_config_filename ("config");
    error = NULL;
    if (!settings_save_to_file (settings, config_file, &error))
    {
        g_warning (_("The configuration couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);
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
    gchar* config_file;
    GError* error;

    config_file = build_config_filename ("search");
    error = NULL;
    if (!search_engines_save_to_file (search_engines, config_file, &error))
    {
        g_warning (_("The search engines couldn't be saved. %s"),
                   error->message);
        g_error_free (error);
    }
    g_free (config_file);
}

static void
midori_bookmarks_notify_item_cb (KatzeArray* folder,
                                 GParamSpec* pspec,
                                 KatzeArray* bookmarks)
{
    gchar* config_file;
    GError* error;

    config_file = build_config_filename ("bookmarks.xbel");
    error = NULL;
    if (!midori_array_to_file (bookmarks, config_file, "xbel", &error))
    {
        g_warning (_("The bookmarks couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);
}

static void
midori_bookmarks_add_item_cb (KatzeArray* folder,
                              GObject*    item,
                              KatzeArray* bookmarks);

static void
midori_bookmarks_remove_item_cb (KatzeArray* folder,
                                 GObject*    item,
                                 KatzeArray* bookmarks);

static void
midori_bookmarks_add_item_cb (KatzeArray* folder,
                              GObject*    item,
                              KatzeArray* bookmarks)
{
    gchar* config_file;
    GError* error;

    config_file = build_config_filename ("bookmarks.xbel");
    error = NULL;
    if (!midori_array_to_file (bookmarks, config_file, "xbel", &error))
    {
        g_warning (_("The bookmarks couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);

    if (folder == bookmarks && KATZE_IS_ARRAY (item))
    {
        g_signal_connect_after (item, "add-item",
            G_CALLBACK (midori_bookmarks_add_item_cb), bookmarks);
        g_signal_connect_after (item, "remove-item",
            G_CALLBACK (midori_bookmarks_remove_item_cb), bookmarks);
    }

    g_signal_connect_after (item, "notify",
        G_CALLBACK (midori_bookmarks_notify_item_cb), bookmarks);
}

static void
midori_bookmarks_remove_item_cb (KatzeArray* folder,
                                 GObject*    item,
                                 KatzeArray* bookmarks)
{
    gchar* config_file;
    GError* error;

    config_file = build_config_filename ("bookmarks.xbel");
    error = NULL;
    if (!midori_array_to_file (bookmarks, config_file, "xbel", &error))
    {
        g_warning (_("The bookmarks couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);

    if (KATZE_IS_ARRAY (item))
        g_signal_handlers_disconnect_by_func (item,
            midori_bookmarks_add_item_cb, bookmarks);
}

static void
midori_trash_add_item_cb (KatzeArray* trash,
                          GObject*    item)
{
    gchar* config_file;
    GError* error;
    GObject* obsolete_item;

    config_file = build_config_filename ("tabtrash.xbel");
    error = NULL;
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
    gchar* config_file;
    GError* error;

    config_file = build_config_filename ("tabtrash.xbel");
    error = NULL;
    if (!midori_array_to_file (trash, config_file, "xbel", &error))
    {
        g_warning (_("The trash couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);
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

    /* Console */
    addon = g_object_new (MIDORI_TYPE_CONSOLE, "app", app, NULL);
    gtk_widget_show (addon);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));

    /* Userscripts */
    addon = midori_addons_new (MIDORI_ADDON_USER_SCRIPTS, GTK_WIDGET (browser));
    gtk_widget_show (addon);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));

    /* Userstyles */
    addon = midori_addons_new (MIDORI_ADDON_USER_STYLES, GTK_WIDGET (browser));
    gtk_widget_show (addon);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));

    /* Plugins */
    addon = g_object_new (MIDORI_TYPE_PLUGINS, "app", app, NULL);
    gtk_widget_show (addon);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));

    /* Extensions */
    addon = g_object_new (MIDORI_TYPE_EXTENSIONS, "app", app, NULL);
    gtk_widget_show (addon);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));

    g_object_unref (panel);
}

static void
midori_browser_session_cb (MidoriBrowser* browser,
                           gpointer       pspec,
                           KatzeArray*    session)
{
    gchar* config_file;
    GError* error;

    config_file = build_config_filename ("session.xbel");
    error = NULL;
    if (!midori_array_to_file (session, config_file, "xbel", &error))
    {
        g_warning (_("The session couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);
}

static void
midori_browser_weak_notify_cb (MidoriBrowser* browser,
                               KatzeArray*    session)
{
    g_object_disconnect (browser, "any-signal",
                         G_CALLBACK (midori_browser_session_cb), session, NULL);
}

static void
soup_session_settings_notify_http_proxy_cb (MidoriWebSettings* settings,
                                            GParamSpec*        pspec,
                                            SoupSession*       session)
{
    gboolean auto_detect_proxy;
    gchar* http_proxy;
    SoupURI* proxy_uri;

    auto_detect_proxy = katze_object_get_boolean (settings, "auto-detect-proxy");
    if (auto_detect_proxy)
        http_proxy = g_strdup (g_getenv ("http_proxy"));
    else
        http_proxy = katze_object_get_string (settings, "http-proxy");
    /* soup_uri_new expects a non-NULL string with a protocol */
    if (http_proxy && g_str_has_prefix (http_proxy, "http://"))
        proxy_uri = soup_uri_new (http_proxy);
    else if (http_proxy && *http_proxy)
    {
        gchar* fixed_http_proxy = g_strconcat ("http://", http_proxy, NULL);
        proxy_uri = soup_uri_new (fixed_http_proxy);
        g_free (fixed_http_proxy);
    }
    else
        proxy_uri = NULL;
    g_free (http_proxy);
    g_object_set (session, "proxy-uri", proxy_uri, NULL);
    if (proxy_uri)
        soup_uri_free (proxy_uri);
}

static void
soup_session_settings_notify_ident_string_cb (MidoriWebSettings* settings,
                                              GParamSpec*        pspec,
                                              SoupSession*       session)
{
    gchar* ident_string = katze_object_get_string (settings, "ident-string");
    g_object_set (session, "user-agent", ident_string, NULL);
    g_free (ident_string);
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

static void
midori_soup_session_prepare (SoupSession*       session,
                             SoupCookieJar*     cookie_jar,
                             MidoriWebSettings* settings)
{
    SoupSessionFeature* feature;
    gchar* config_file;

    soup_session_settings_notify_http_proxy_cb (settings, NULL, session);
    soup_session_settings_notify_ident_string_cb (settings, NULL, session);
    g_signal_connect (settings, "notify::http-proxy",
        G_CALLBACK (soup_session_settings_notify_http_proxy_cb), session);
    g_signal_connect (settings, "notify::auto-detect-proxy",
        G_CALLBACK (soup_session_settings_notify_http_proxy_cb), session);
    g_signal_connect (settings, "notify::ident-string",
        G_CALLBACK (soup_session_settings_notify_ident_string_cb), session);

    soup_session_add_feature_by_type (session, KATZE_TYPE_HTTP_AUTH);
    midori_soup_session_debug (session);

    feature = g_object_new (KATZE_TYPE_HTTP_COOKIES, NULL);
    config_file = build_config_filename ("cookies.txt");
    g_object_set_data_full (G_OBJECT (feature), "filename",
                            config_file, (GDestroyNotify)g_free);
    soup_session_add_feature (session, SOUP_SESSION_FEATURE (cookie_jar));
    soup_session_add_feature (session, feature);
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
    katze_array_clear (session);
    gtk_widget_set_sensitive (button, FALSE);
}

static GtkWidget*
midori_create_diagnostic_dialog (MidoriWebSettings* settings,
                                 KatzeArray*        _session)
{
    GtkWidget* dialog;
    GdkScreen* screen;
    GtkIconTheme* icon_theme;
    GtkWidget* box;
    GtkWidget* button;

    dialog = gtk_message_dialog_new (
        NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
        _("Midori seems to have crashed the last time it was opened. "
          "If this happened repeatedly, try one of the following options "
          "to solve the problem."));
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
    gtk_widget_set_sensitive (button, FALSE);
    /* FIXME: Disable all extensions */
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 4);
    gtk_widget_show_all (box);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), box);
    if (1)
    {
        /* GtkLabel can't wrap the text properly. Until some day
           this works, we implement this hack to do it ourselves. */
        GtkWidget* content_area;
        GtkWidget* hbox;
        GtkWidget* vbox;
        GtkWidget* label;
        GList* ch;
        GtkRequisition req;

        content_area = GTK_DIALOG (dialog)->vbox;
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
midori_load_cookie_jar (gpointer data)
{
    MidoriWebSettings* settings = MIDORI_WEB_SETTINGS (data);
    SoupSession* webkit_session;
    SoupCookieJar* jar;

    webkit_session = webkit_get_default_session ();
    jar = soup_cookie_jar_new ();
    g_object_set_data (G_OBJECT (jar), "midori-settings", settings);
    midori_soup_session_prepare (webkit_session, jar, settings);
    g_object_unref (jar);

    return FALSE;
}

static gboolean
midori_load_extensions (gpointer data)
{
    MidoriApp* app = MIDORI_APP (data);
    KatzeArray* extensions;
    const gchar* filename;
    MidoriExtension* extension;
    guint i;

    /* Load extensions */
    extensions = katze_array_new (MIDORI_TYPE_EXTENSION);
    if (g_module_supported ())
    {
        /* FIXME: Read extensions from system data dirs */
        gchar* extension_path;
        GDir* extension_dir;

        if (!(extension_path = g_strdup (g_getenv ("MIDORI_EXTENSION_PATH"))))
            extension_path = g_build_filename (LIBDIR, PACKAGE_NAME, NULL);
        extension_dir = g_dir_open (extension_path, 0, NULL);
        if (extension_dir != NULL)
        {
            while ((filename = g_dir_read_name (extension_dir)))
            {
                gchar* fullname;
                GModule* module;
                typedef MidoriExtension* (*extension_init_func)(void);
                extension_init_func extension_init;

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
                    /* FIXME: Validate the extension */
                    /* Signal that we want the extension to load and save */
                    midori_extension_get_config_dir (extension);
                }
                else
                {
                    extension = g_object_new (MIDORI_TYPE_EXTENSION,
                                              "name", filename,
                                              "description", g_module_error (),
                                              NULL);
                    g_warning ("%s", g_module_error ());
                }
                katze_array_add_item (extensions, extension);
                g_object_unref (extension);
            }
            g_dir_close (extension_dir);
        }
        g_free (extension_path);
    }

    g_object_set (app, "extensions", extensions, NULL);

    i = 0;
    while ((extension = katze_array_get_nth_item (extensions, i++)))
        g_signal_emit_by_name (extension, "activate", app);

    return FALSE;
}

static gboolean
midori_load_session (gpointer data)
{
    KatzeArray* _session = KATZE_ARRAY (data);
    MidoriBrowser* browser;
    MidoriApp* app = katze_item_get_parent (KATZE_ITEM (_session));
    gchar* config_file;
    KatzeArray* session;
    KatzeItem* item;
    guint i;

    browser = midori_app_create_browser (app);
    midori_app_add_browser (app, browser);
    gtk_widget_show (GTK_WIDGET (browser));

    config_file = build_config_filename ("accels");
    if (is_writable (config_file))
        g_signal_connect_after (gtk_accel_map_get (), "changed",
            G_CALLBACK (accel_map_changed_cb), NULL);

    if (katze_array_is_empty (_session))
    {
        MidoriWebSettings* settings = katze_object_get_object (app, "settings");
        MidoriStartup load_on_startup;
        gchar* homepage;
        item = katze_item_new ();

        g_object_get (settings, "load-on-startup", &load_on_startup, NULL);
        if (load_on_startup == MIDORI_STARTUP_BLANK_PAGE)
            katze_item_set_uri (item, "");
        else
        {
            g_object_get (settings, "homepage", &homepage, NULL);
            katze_item_set_uri (item, homepage);
            g_free (homepage);
        }
        g_object_unref (settings);
        katze_array_add_item (_session, item);
        g_object_unref (item);
    }

    session = midori_browser_get_proxy_array (browser);
    i = 0;
    while ((item = katze_array_get_nth_item (_session, i++)))
        midori_browser_add_item (browser, item);
    /* FIXME: Switch to the last active page */
    item = katze_array_get_nth_item (_session, 0);
    if (!strcmp (katze_item_get_uri (item), ""))
        midori_browser_activate_action (browser, "Location");
    g_object_unref (_session);

    katze_assign (config_file, build_config_filename ("session.xbel"));
    if (is_writable (config_file))
    {
        g_signal_connect_after (browser, "notify::uri",
            G_CALLBACK (midori_browser_session_cb), session);
        g_signal_connect_after (browser, "add-tab",
            G_CALLBACK (midori_browser_session_cb), session);
        g_signal_connect_after (browser, "remove-tab",
            G_CALLBACK (midori_browser_session_cb), session);
        g_object_weak_ref (G_OBJECT (session),
            (GWeakNotify)(midori_browser_weak_notify_cb), browser);
    }

    return FALSE;
}

static gint
midori_run_script (const gchar* filename)
{
    if (!(filename))
    {
        g_print ("%s - %s\n", _("Midori"), _("No filename specified"));
        return 1;
    }

    JSGlobalContextRef js_context;
    gchar* exception;
    gchar* script;
    GError* error = NULL;

    js_context = JSGlobalContextCreateInGroup (NULL, NULL);

    if (g_file_get_contents (filename, &script, NULL, &error))
    {
        if (sokoke_js_script_eval (js_context, script, &exception))
            exception = NULL;
        g_free (script);
    }
    else if (error)
    {
        exception = g_strdup (error->message);
        g_error_free (error);
    }
    else
        exception = g_strdup (_("An unknown error occured."));

    JSGlobalContextRelease (js_context);
    if (!exception)
        return 0;

    g_print ("%s - Exception: %s\n", filename, exception);
    return 1;
}

int
main (int    argc,
      char** argv)
{
    gchar* config;
    gboolean run;
    gboolean version;
    gchar** uris;
    MidoriApp* app;
    gboolean result;
    GError* error;
    GOptionEntry entries[] =
    {
       { "config", 'c', 0, G_OPTION_ARG_FILENAME, &config,
       N_("Use FOLDER as configuration folder"), N_("FOLDER") },
       { "run", 'r', 0, G_OPTION_ARG_NONE, &run,
       N_("Run the specified filename as javascript"), NULL },
       { "version", 'V', 0, G_OPTION_ARG_NONE, &version,
       N_("Display program version"), NULL },
       { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &uris,
       N_("Addresses"), NULL },
     { NULL }
    };
    GString* error_messages;
    MidoriWebSettings* settings;
    gchar* config_file;
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
    #if HAVE_SQLITE
    sqlite3* db;
    gint max_history_age;
    #endif
    #if HAVE_HILDON
    osso_context_t* osso_context;
    #endif

    #if ENABLE_NLS
    setlocale (LC_ALL, "");
    if (g_getenv ("NLSPATH"))
        bindtextdomain (GETTEXT_PACKAGE, g_getenv ("NLSPATH"));
    else
        bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    #endif

    /* Parse cli options */
    config = NULL;
    run = FALSE;
    version = FALSE;
    uris = NULL;
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
    g_set_application_name (_("Midori"));

    if (version)
    {
        g_print (
          "%s %s\n\n"
          "Copyright (c) 2007-2009 Christian Dywan\n\n"
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

    /* Standalone javascript support */
    if (run)
        return midori_run_script (uris ? *uris : NULL);

    #if HAVE_HILDON
    osso_context = osso_initialize (PACKAGE_NAME, PACKAGE_VERSION, FALSE, NULL);

    if (!osso_context)
    {
        g_critical ("Error initializing OSSO D-Bus context - Midori");
        return 1;
    }
    #endif

    if (config && !g_path_is_absolute (config))
    {
        g_critical (_("The specified configuration folder is invalid."));
        return 1;
    }
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
    g_free (config);

    /* FIXME: The app might be 'running' but actually showing a dialog
              after a crash, so running a new window isn't a good idea. */
    if (midori_app_instance_is_running (app))
    {
        GtkWidget* dialog;

        /* TODO: Open as many tabs as we have uris, seperated by pipes */
        if (uris)
            result = midori_app_instance_send_uris (app, uris);
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

    /* Load configuration files */
    error_messages = g_string_new (NULL);
    config_file = build_config_filename ("config");
    error = NULL;
    settings = settings_new_from_file (config_file);
    katze_assign (config_file, build_config_filename ("accels"));
    gtk_accel_map_load (config_file);
    katze_assign (config_file, build_config_filename ("search"));
    error = NULL;
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
        const gchar* const * config_dirs = g_get_system_config_dirs ();
        i = 0;
        while (config_dirs[i])
        {
            g_object_unref (search_engines);
            katze_assign (config_file,
                g_build_filename (config_dirs[i], PACKAGE_NAME, "search", NULL));
            search_engines = search_engines_new_from_file (config_file, NULL);
            if (!katze_array_is_empty (search_engines))
                break;
            i++;
        }
        if (katze_array_is_empty (search_engines))
        {
            g_object_unref (search_engines);
            katze_assign (config_file,
                g_build_filename (SYSCONFDIR, "xdg", PACKAGE_NAME, "search", NULL));
            search_engines = search_engines_new_from_file (config_file, NULL);
        }
    }
    else if (error)
    {
        if (error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf (error_messages,
                _("The search engines couldn't be loaded. %s\n"),
                error->message);
        g_error_free (error);
    }
    bookmarks = katze_array_new (KATZE_TYPE_ARRAY);
    #if HAVE_LIBXML
    katze_assign (config_file, build_config_filename ("bookmarks.xbel"));
    error = NULL;
    if (!midori_array_from_file (bookmarks, config_file, "xbel", &error))
    {
        if (error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf (error_messages,
                _("The bookmarks couldn't be loaded: %s\n"), error->message);
        g_error_free (error);
    }
    #endif
    _session = katze_array_new (KATZE_TYPE_ITEM);
    #if HAVE_LIBXML
    g_object_get (settings, "load-on-startup", &load_on_startup, NULL);
    if (load_on_startup == MIDORI_STARTUP_LAST_OPEN_PAGES)
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
    trash = katze_array_new (KATZE_TYPE_ITEM);
    #if HAVE_LIBXML
    katze_assign (config_file, build_config_filename ("tabtrash.xbel"));
    error = NULL;
    if (!midori_array_from_file (trash, config_file, "xbel", &error))
    {
        if (error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf (error_messages,
                _("The trash couldn't be loaded: %s\n"), error->message);
        g_error_free (error);
    }
    #endif
    #if HAVE_SQLITE
    katze_assign (config_file, build_config_filename ("history.db"));
    #endif
    history = katze_array_new (KATZE_TYPE_ARRAY);
    #if HAVE_SQLITE
    error = NULL;
    if ((db = midori_history_initialize (history, config_file, &error)) == NULL)
    {
        g_string_append_printf (error_messages,
            _("The history couldn't be loaded: %s\n"), error->message);
        g_error_free (error);
    }
    #endif

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

    /* Open as many tabs as we have uris, seperated by pipes */
    i = 0;
    while (uris && uris[i])
    {
        uri = strtok (g_strdup (uris[i]), "|");
        while (uri != NULL)
        {
            item = katze_item_new ();
            uri_ready = sokoke_magic_uri (uri, NULL);
            katze_item_set_uri (item, uri_ready);
            g_free (uri_ready);
            katze_array_add_item (_session, item);
            uri = strtok (NULL, "|");
        }
        g_free (uri);
        i++;
    }

    katze_assign (config_file, build_config_filename ("config"));
    if (is_writable (config_file))
        g_signal_connect_after (settings, "notify",
            G_CALLBACK (settings_notify_cb), NULL);

    katze_assign (config_file, build_config_filename ("search"));
    if (is_writable (config_file))
    {
        g_signal_connect_after (search_engines, "add-item",
            G_CALLBACK (midori_search_engines_modify_cb), search_engines);
        g_signal_connect_after (search_engines, "remove-item",
            G_CALLBACK (midori_search_engines_modify_cb), search_engines);
        if (!katze_array_is_empty (search_engines))
        {
            i = 0;
            while ((item = katze_array_get_nth_item (search_engines, i++)))
                g_signal_connect_after (item, "notify",
                    G_CALLBACK (midori_search_engines_modify_cb), search_engines);
        }
    }
    katze_assign (config_file, build_config_filename ("bookmarks.xbel"));
    if (is_writable (config_file))
    {
        g_signal_connect_after (bookmarks, "add-item",
            G_CALLBACK (midori_bookmarks_add_item_cb), bookmarks);
        g_signal_connect_after (bookmarks, "remove-item",
            G_CALLBACK (midori_bookmarks_remove_item_cb), bookmarks);
        if (!katze_array_is_empty (bookmarks))
        {
            i = 0;
            while ((item = katze_array_get_nth_item (bookmarks, i++)))
            {
                if (KATZE_IS_ARRAY (item))
                {
                    g_signal_connect_after (item, "add-item",
                        G_CALLBACK (midori_bookmarks_add_item_cb), bookmarks);
                    g_signal_connect_after (item, "remove-item",
                        G_CALLBACK (midori_bookmarks_remove_item_cb), bookmarks);
                }
                g_signal_connect_after (item, "notify",
                    G_CALLBACK (midori_bookmarks_notify_item_cb), bookmarks);
            }
        }
    }
    katze_assign (config_file, build_config_filename ("tabtrash.xbel"));
    if (is_writable (config_file))
    {
        g_signal_connect_after (trash, "add-item",
            G_CALLBACK (midori_trash_add_item_cb), NULL);
        g_signal_connect_after (trash, "remove-item",
            G_CALLBACK (midori_trash_remove_item_cb), NULL);
    }
    #if HAVE_SQLITE
    katze_assign (config_file, build_config_filename ("history.db"));
    if (is_writable (config_file))
    {
        g_signal_connect_after (history, "add-item",
            G_CALLBACK (midori_history_add_item_cb), db);
        g_signal_connect_after (history, "clear",
            G_CALLBACK (midori_history_clear_cb), db);
    }
    #endif

    /* We test for the presence of a dummy file which is created once
       and deleted during normal runtime, but persists in case of a crash. */
    katze_assign (config_file, build_config_filename ("running"));
    if (katze_object_get_boolean (settings, "show-crash-dialog")
        && g_file_test (config_file, G_FILE_TEST_EXISTS))
    {
        GtkWidget* dialog = midori_create_diagnostic_dialog (settings, _session);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
    }
    else
        g_file_set_contents (config_file, "RUNNING", -1, NULL);
    g_signal_connect (app, "quit", G_CALLBACK (midori_app_quit_cb), NULL);

    g_object_set (app, "settings", settings,
                       "bookmarks", bookmarks,
                       "trash", trash,
                       "search-engines", search_engines,
                       "history", history,
                       NULL);
    g_object_unref (history);
    g_object_unref (search_engines);
    g_object_unref (bookmarks);
    g_object_unref (trash);
    g_object_unref (settings);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (midori_app_add_browser_cb), NULL);

    g_idle_add (midori_load_cookie_jar, settings);
    g_idle_add (midori_load_extensions, app);
    katze_item_set_parent (KATZE_ITEM (_session), app);
    g_idle_add (midori_load_session, _session);

    gtk_main ();

    #if HAVE_HILDON
    osso_deinitialize (osso_context);
    #endif

    #if HAVE_SQLITE
    settings = katze_object_get_object (app, "settings");
    g_object_get (settings, "maximum-history-age", &max_history_age, NULL);
    g_object_unref (settings);
    midori_history_terminate (db, max_history_age);
    #endif
    g_object_unref (app);
    g_free (config_file);
    return 0;
}
