/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>
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

#include "midori-addons.h"
#include "midori-app.h"
#include "midori-browser.h"
#include "midori-console.h"
#include "midori-extension.h"
#include "midori-panel.h"
#include "midori-stock.h"
#include "midori-view.h"
#include "midori-websettings.h"


#include "sokoke.h"
#include "gjs.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef HAVE_LIBXML
    #include <libxml/parser.h>
    #include <libxml/tree.h>
#endif

#ifdef HAVE_SQLITE
    #include <sqlite3.h>
#endif

#if ENABLE_NLS
    #include <libintl.h>
#endif

#define MIDORI_HISTORY_ERROR g_quark_from_string("MIDORI_HISTORY_ERROR")

typedef enum
{
    MIDORI_HISTORY_ERROR_DB_OPEN,    /* Error opening the database file */
    MIDORI_HISTORY_ERROR_EXEC_SQL,   /* Error executing SQL statement */

} MidoriHistoryError;

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
    GtkIconSource* icon_source;
    GtkIconSet* icon_set;
    GtkIconFactory* factory = gtk_icon_factory_new ();
    gsize i;

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
    };

    for (i = 0; i < G_N_ELEMENTS (items); i++)
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
            g_warning (_("Invalid configuration value '%s'"), property);
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
            g_warning (_("Invalid configuration value '%s'"), property);
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
            if (!G_IS_PARAM_SPEC_STRING (pspecs[j]))
                continue;
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
        katze_array_remove_item (trash, obsolete_item);
    }
}

#ifdef HAVE_LIBXML
static KatzeItem*
katze_item_from_xmlNodePtr (xmlNodePtr cur)
{
    KatzeItem* item;
    xmlChar* key;

    item = katze_item_new ();
    key = xmlGetProp (cur, (xmlChar*)"href");
    katze_item_set_uri (item, (gchar*)key);
    g_free (key);

    cur = cur->xmlChildrenNode;
    while (cur)
    {
        if (!xmlStrcmp (cur->name, (const xmlChar*)"title"))
        {
            key = xmlNodeGetContent (cur);
            katze_item_set_name (item, g_strstrip ((gchar*)key));
            g_free (key);
        }
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"desc"))
        {
            key = xmlNodeGetContent (cur);
            katze_item_set_text (item, g_strstrip ((gchar*)key));
            g_free (key);
        }
        cur = cur->next;
    }
    return item;
}

/* Create an array from an xmlNodePtr */
static KatzeArray*
katze_array_from_xmlNodePtr (xmlNodePtr cur)
{
    KatzeArray* array;
    xmlChar* key;
    KatzeItem* item;

    array = katze_array_new (KATZE_TYPE_ARRAY);

    key = xmlGetProp (cur, (xmlChar*)"folded");
    if (key)
    {
        /* if (!g_ascii_strncasecmp ((gchar*)key, "yes", 3))
            folder->folded = TRUE;
        else if (!g_ascii_strncasecmp ((gchar*)key, "no", 2))
            folder->folded = FALSE;
        else
            g_warning ("XBEL: Unknown value for folded."); */
        xmlFree (key);
    }

    cur = cur->xmlChildrenNode;
    while (cur)
    {
        if (!xmlStrcmp (cur->name, (const xmlChar*)"title"))
        {
            key = xmlNodeGetContent (cur);
            katze_item_set_name (KATZE_ITEM (array), g_strstrip ((gchar*)key));
        }
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"desc"))
        {
            key = xmlNodeGetContent (cur);
            katze_item_set_text (KATZE_ITEM (array), g_strstrip ((gchar*)key));
        }
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"folder"))
        {
            item = (KatzeItem*)katze_array_from_xmlNodePtr (cur);
            katze_array_add_item (array, item);
        }
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"bookmark"))
        {
            item = katze_item_from_xmlNodePtr (cur);
            katze_array_add_item (array, item);
        }
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"separator"))
        {
            item = katze_item_new ();
            katze_array_add_item (array, item);
        }
        cur = cur->next;
    }
    return array;
}

/* Loads the contents from an xmlNodePtr into an array. */
static gboolean
katze_array_from_xmlDocPtr (KatzeArray* array,
                            xmlDocPtr   doc)
{
    xmlNodePtr cur;
    xmlChar* version;
    gchar* value;
    KatzeItem* item;

    cur = xmlDocGetRootElement (doc);
    version = xmlGetProp (cur, (xmlChar*)"version");
    if (xmlStrcmp (version, (xmlChar*)"1.0"))
        g_warning ("XBEL version is not 1.0.");
    xmlFree (version);

    value = (gchar*)xmlGetProp (cur, (xmlChar*)"title");
    katze_item_set_name (KATZE_ITEM (array), value);
    g_free (value);

    value = (gchar*)xmlGetProp (cur, (xmlChar*)"desc");
    katze_item_set_text (KATZE_ITEM (array), value);
    g_free (value);

    if ((cur = xmlDocGetRootElement (doc)) == NULL)
    {
        /* Empty document */
        return FALSE;
    }
    if (xmlStrcmp (cur->name, (const xmlChar*)"xbel"))
    {
        /* Wrong document kind */
        return FALSE;
    }
    cur = cur->xmlChildrenNode;
    while (cur)
    {
        item = NULL;
        if (!xmlStrcmp (cur->name, (const xmlChar*)"folder"))
            item = (KatzeItem*)katze_array_from_xmlNodePtr (cur);
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"bookmark"))
            item = katze_item_from_xmlNodePtr (cur);
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"separator"))
            item = katze_item_new ();
        /*else if (!xmlStrcmp (cur->name, (const xmlChar*)"info"))
            item = katze_xbel_parse_info (xbel, cur);*/
        if (item)
            katze_array_add_item (array, item);
        cur = cur->next;
    }
    return TRUE;
}

static gboolean
katze_array_from_file (KatzeArray*  array,
                       const gchar* filename,
                       GError**     error)
{
    xmlDocPtr doc;

    g_return_val_if_fail (katze_array_is_a (array, KATZE_TYPE_ITEM), FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);

    if (!g_file_test (filename, G_FILE_TEST_EXISTS))
    {
        /* File doesn't exist */
        *error = g_error_new_literal (G_FILE_ERROR, G_FILE_ERROR_NOENT,
                                      _("File not found."));
        return FALSE;
    }

    if ((doc = xmlParseFile (filename)) == NULL)
    {
        /* No valid xml or broken encoding */
        *error = g_error_new_literal (G_FILE_ERROR, G_FILE_ERROR_FAILED,
                                      _("Malformed document."));
        return FALSE;
    }

    if (!katze_array_from_xmlDocPtr (array, doc))
    {
        /* Parsing failed */
        xmlFreeDoc (doc);
        *error = g_error_new_literal (G_FILE_ERROR, G_FILE_ERROR_FAILED,
                                      _("Malformed document."));
        return FALSE;
    }
    xmlFreeDoc (doc);
    return TRUE;
}
#endif

#ifdef HAVE_SQLITE
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
                                  _("Failed to open database: %s\n"),
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

    sqlcmd = g_strdup_printf (
        "DELETE FROM history WHERE uri = '%s' AND"
        " title = '%s' AND date = %" G_GINT64_FORMAT,
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
    g_free (sqlcmd);
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
    sqlcmd = g_strdup_printf ("INSERT INTO history VALUES"
                              "('%s', '%s', %" G_GUINT64_FORMAT ", -1)",
                              katze_item_get_uri (item),
                              katze_item_get_name (item),
                              katze_item_get_added (item));
    success = db_exec (db, sqlcmd, &error);
    g_free (sqlcmd);
    if (!success)
    {
        g_printerr (_("Failed to add history item: %s\n"), error->message);
        g_error_free (error);
        return ;
    }
}

static int
midori_history_add_items (void*  data,
                          int    argc,
                          char** argv,
                          char** colname)
{
    KatzeItem* item;
    KatzeArray* parent = NULL;
    KatzeArray* array = KATZE_ARRAY (data);
    gint64 date;
    time_t newdate;
    gint i, j, n;
    gint ncols = 3;

    g_return_val_if_fail (KATZE_IS_ARRAY (array), 1);

    /* Test whether have the right number of columns */
    g_return_val_if_fail (argc % ncols == 0, 1);

    for (i = 0; i <= (argc - ncols); i++)
    {
        if (argv[i])
        {
            if (colname[i] && !g_ascii_strcasecmp (colname[i], "uri") &&
                colname[i + 1] && !g_ascii_strcasecmp (colname[i + 1], "title") &&
                colname[i + 2] && !g_ascii_strcasecmp (colname[i + 2], "date"))
            {
                item = katze_item_new ();
                katze_item_set_uri (item, argv[i]);
                katze_item_set_name (item, argv[i + 1]);
                date = g_ascii_strtoull (argv[i + 2], NULL, 10);
                katze_item_set_added (item, date);

                n = katze_array_get_length (array);
                for (j = 0; j < n; j++)
                {
                    parent = katze_array_get_nth_item (array, j);
                    newdate = katze_item_get_added (KATZE_ITEM (parent));
                    if (sokoke_same_day ((time_t *)&date, (time_t *)&newdate))
                        break;
                }
                if (j == n)
                {
                    parent = katze_array_new (KATZE_TYPE_ARRAY);
                    katze_item_set_added (KATZE_ITEM (parent), date);
                    katze_array_add_item (array, parent);
                }
                katze_array_add_item (parent, item);
            }
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
    gint i, n;

    if ((db = db_open (filename, error)) == NULL)
        return db;

    if (!db_exec (db,
                  "CREATE TABLE IF NOT EXISTS "
                  "history(uri text, title text, date integer, visits integer)",
                  error))
        return NULL;

    if (!db_exec_callback (db,
                           "SELECT uri, title, date FROM history "
                           "ORDER BY date ASC",
                           midori_history_add_items,
                           array,
                           error))
        return NULL;

    n = katze_array_get_length (array);
    for (i = 0; i < n; i++)
    {
        item = katze_array_get_nth_item (array, i);
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
katze_item_to_data (KatzeItem* item)
{
    gchar* markup;

    g_return_val_if_fail (KATZE_IS_ITEM (item), NULL);

    markup = NULL;
    if (KATZE_IS_ARRAY (item))
    {
        GString* _markup = g_string_new (NULL);
        guint n = katze_array_get_length (KATZE_ARRAY (item));
        guint i;
        for (i = 0; i < n; i++)
        {
            KatzeItem* _item = katze_array_get_nth_item (KATZE_ARRAY (item), i);
            gchar* item_markup = katze_item_to_data (_item);
            g_string_append (_markup, item_markup);
            g_free (item_markup);
        }
        /* gchar* folded = item->folded ? NULL : g_strdup_printf (" folded=\"no\""); */
        gchar* title = _simple_xml_element ("title", katze_item_get_name (item));
        gchar* desc = _simple_xml_element ("desc", katze_item_get_text (item));
        markup = g_strdup_printf ("<folder%s>\n%s%s%s</folder>\n",
                                  "" /* folded ? folded : "" */,
                                  title, desc,
                                  g_string_free (_markup, FALSE));
        /* g_free (folded); */
        g_free (title);
        g_free (desc);
    }
    else if (katze_item_get_uri (item))
    {
        gchar* href_escaped = g_markup_escape_text (katze_item_get_uri (item), -1);
        gchar* href = g_strdup_printf (" href=\"%s\"", href_escaped);
        g_free (href_escaped);
        gchar* title = _simple_xml_element ("title", katze_item_get_name (item));
        gchar* desc = _simple_xml_element ("desc", katze_item_get_text (item));
        markup = g_strdup_printf ("<bookmark%s>\n%s%s%s</bookmark>\n",
                                  href,
                                  title, desc,
                                  "");
        g_free (href);
        g_free (title);
        g_free (desc);
    }
    else
        markup = g_strdup ("<separator/>\n");
    return markup;
}

static gchar*
katze_array_to_xml (KatzeArray* array,
                    GError**    error)
{
    GString* inner_markup;
    guint i, n;
    KatzeItem* item;
    gchar* item_xml;
    gchar* title;
    gchar* desc;
    gchar* outer_markup;

    g_return_val_if_fail (katze_array_is_a (array, KATZE_TYPE_ITEM), NULL);

    inner_markup = g_string_new (NULL);
    n = katze_array_get_length (array);
    for (i = 0; i < n; i++)
    {
        item = katze_array_get_nth_item (array, i);
        item_xml = katze_item_to_data (item);
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

    g_return_val_if_fail (katze_array_is_a (array, KATZE_TYPE_ITEM), FALSE);
    g_return_val_if_fail (filename, FALSE);

    if (!(data = katze_array_to_xml (array, error)))
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
midori_view_console_message_cb (GtkWidget*     view,
                                const gchar*   message,
                                gint           line,
                                const gchar*   source_id,
                                MidoriConsole* console)
{
    midori_console_add (console, message, line, source_id);
}

static void
midori_browser_add_tab_cb (MidoriBrowser* browser,
                           MidoriView*    view,
                           MidoriConsole* console)
{
    g_signal_connect (view, "console-message",
        G_CALLBACK (midori_view_console_message_cb), console);
}

static void
midori_app_add_browser_cb (MidoriApp*     app,
                           MidoriBrowser* browser,
                           KatzeNet*      net)
{
    GtkWidget* panel;
    GtkWidget* addon;
    GtkWidget* toolbar;

    panel = katze_object_get_object (browser, "panel");

    /* Transfers */
    #if 0
    addon = midori_view_new (net);
    gtk_widget_show (addon);
    midori_panel_append_widget (MIDORI_PANEL (panel), addon,
                                STOCK_TRANSFERS, _("Transfers"), NULL);
    #endif

    /* Console */
    addon = midori_console_new ();
    gtk_widget_show (addon);
    toolbar = midori_console_get_toolbar (MIDORI_CONSOLE (addon));
    gtk_widget_show (toolbar);
    midori_panel_append_widget (MIDORI_PANEL (panel), addon,
                                STOCK_CONSOLE, _("Console"), toolbar);
    g_signal_connect (browser, "add-tab",
        G_CALLBACK (midori_browser_add_tab_cb), addon);

    /* Userscripts */
    addon = midori_addons_new (MIDORI_ADDON_USER_SCRIPTS, GTK_WIDGET (browser));
    gtk_widget_show (addon);
    toolbar = midori_addons_get_toolbar (MIDORI_ADDONS (addon));
    gtk_widget_show (toolbar);
    midori_panel_append_widget (MIDORI_PANEL (panel), addon,
                                STOCK_SCRIPTS, _("Userscripts"), toolbar);

    /* Userstyles */
    addon = midori_addons_new (MIDORI_ADDON_USER_STYLES, GTK_WIDGET (browser));
    gtk_widget_show (addon);
    toolbar = midori_addons_get_toolbar (MIDORI_ADDONS (addon));
    gtk_widget_show (toolbar);
    midori_panel_append_widget (MIDORI_PANEL (panel), addon,
                                STOCK_STYLES, _("Userstyles"), toolbar);

    /* Extensions */
    #if 0
    addon = midori_addons_new (MIDORI_ADDON_EXTENSIONS);
    gtk_widget_show (addon);
    toolbar = midori_addons_get_toolbar (MIDORI_ADDONS (addon));
    gtk_widget_show (toolbar);
    midori_panel_append_page (MIDORI_PANEL (panel), addon,
                              STOCK_EXTENSIONS, _("_Extensions"), toolbar);
    #endif
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
    g_mkdir_with_parents (config_path, 0700);
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
                               KatzeArray*    session)
{
    g_object_disconnect (browser, "any-signal",
                         G_CALLBACK (midori_browser_session_cb), session, NULL);
}

int
main (int    argc,
      char** argv)
{
    gboolean run;
    gboolean version;
    gchar** uris;
    MidoriApp* app;
    gboolean result;
    GError* error;
    GOptionEntry entries[] =
    {
       { "run", 'r', 0, G_OPTION_ARG_NONE, &run,
       N_("Run the specified filename as javascript"), NULL },
       { "version", 'V', 0, G_OPTION_ARG_NONE, &version,
       N_("Display program version"), NULL },
       { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &uris,
       N_("URIs"), NULL },
     { NULL }
    };
    JSGlobalContextRef js_context;
    gchar* exception;
    MidoriStartup load_on_startup;
    gchar* homepage;
    KatzeArray* search_engines;
    KatzeArray* bookmarks;
    KatzeArray* history;
    KatzeArray* _session;
    KatzeArray* trash;
    MidoriBrowser* browser;
    KatzeArray* session;
    guint n, i;
    gchar* uri;
    KatzeItem* item;
    gchar* uri_ready;
    #ifdef HAVE_SQLITE
    sqlite3* db;
    gint max_history_age;
    #endif

    #if ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    #endif

    /* Parse cli options */
    run = FALSE;
    version = FALSE;
    uris = NULL;
    error = NULL;
    if (!gtk_init_with_args (&argc, &argv, _("[URIs]"), entries,
                             GETTEXT_PACKAGE, &error))
    {
        g_print ("%s - %s\n", _("Midori"), error->message);
        g_error_free (error);
        return 1;
    }

    /* libSoup uses threads, therefore if WebKit is built with libSoup
       or Midori is using it, we need to initialize threads. */
    if (!g_thread_supported ()) g_thread_init (NULL);
    stock_items_init ();
    g_set_application_name (_("Midori"));

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
    if (run)
    {
        if (uris && *uris)
        {
            js_context = gjs_global_context_new ();
            exception = NULL;
            gjs_script_from_file (js_context, *uris, &exception);
            JSGlobalContextRelease (js_context);
            if (!exception)
                return 0;
            g_print ("%s - Exception: %s\n", *uris, exception);
            return 1;
        }
        else
        {
            g_print ("%s - %s\n", _("Midori"), _("No filename specified"));
            return 1;
        }
    }

    app = midori_app_new ();
    if (midori_app_instance_is_running (app))
    {
        /* TODO: Open as many tabs as we have uris, seperated by pipes */
        if (uris)
            result = midori_app_instance_send_uris (app, uris);
        else
            result = midori_app_instance_send_new_browser (app);

        if (result)
            return 0;

        g_print (_("An instance of Midori is already running but not responding.\n"));
        /* FIXME: Do we want a graphical error message? */
        return 1;
    }

    /* Load configuration files */
    GString* error_messages = g_string_new (NULL);
    gchar* config_path = g_build_filename (g_get_user_config_dir (),
                                           PACKAGE_NAME, NULL);
    g_mkdir_with_parents (config_path, 0700);
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
    bookmarks = katze_array_new (KATZE_TYPE_ARRAY);
    #ifdef HAVE_LIBXML
    katze_assign (config_file, g_build_filename (config_path, "bookmarks.xbel",
                                                 NULL));
    error = NULL;
    if (!katze_array_from_file (bookmarks, config_file, &error))
    {
        if (error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf (error_messages,
                _("The bookmarks couldn't be loaded: %s\n"), error->message);
        g_error_free (error);
    }
    g_free (config_file);
    #endif
    _session = katze_array_new (KATZE_TYPE_ITEM);
    #ifdef HAVE_LIBXML
    g_object_get (settings, "load-on-startup", &load_on_startup, NULL);
    if (load_on_startup == MIDORI_STARTUP_LAST_OPEN_PAGES)
    {
        config_file = g_build_filename (config_path, "session.xbel", NULL);
        error = NULL;
        if (!katze_array_from_file (_session, config_file, &error))
        {
            if (error->code != G_FILE_ERROR_NOENT)
                g_string_append_printf (error_messages,
                    _("The session couldn't be loaded: %s\n"), error->message);
            g_error_free (error);
        }
        g_free (config_file);
    }
    #endif
    trash = katze_array_new (KATZE_TYPE_ITEM);
    #ifdef HAVE_LIBXML
    config_file = g_build_filename (config_path, "tabtrash.xbel", NULL);
    error = NULL;
    if (!katze_array_from_file (trash, config_file, &error))
    {
        if (error->code != G_FILE_ERROR_NOENT)
            g_string_append_printf (error_messages,
                _("The trash couldn't be loaded: %s\n"), error->message);
        g_error_free (error);
    }
    g_free (config_file);
    #endif
    #ifdef HAVE_SQLITE
    config_file = g_build_filename (config_path, "history.db", NULL);
    #endif
    history = katze_array_new (KATZE_TYPE_ARRAY);
    #ifdef HAVE_SQLITE
    error = NULL;
    if ((db = midori_history_initialize (history, config_file, &error)) == NULL)
    {
        g_string_append_printf (error_messages,
            _("The history couldn't be loaded: %s\n"), error->message);
        g_error_free (error);
    }
    g_free (config_file);
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
                                "_Ignore", GTK_RESPONSE_ACCEPT,
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

    if (katze_array_is_empty (_session))
    {
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
    }
    g_free (config_path);

    g_signal_connect_after (trash, "add-item",
        G_CALLBACK (midori_web_list_add_item_cb), NULL);
    #ifdef HAVE_SQLITE
    g_signal_connect_after (history, "add-item",
        G_CALLBACK (midori_history_add_item_cb), db);
    g_signal_connect_after (history, "clear",
        G_CALLBACK (midori_history_clear_cb), db);
    #endif

    /* Load extensions */
    KatzeArray* extensions;
    gchar* extension_path;
    GDir* extension_dir;
    const gchar* filename;
    MidoriExtension* extension;

    extensions = katze_array_new (MIDORI_TYPE_EXTENSION);
    extension_path = g_build_filename (LIBDIR, PACKAGE_NAME, NULL);
    if (g_module_supported ())
        extension_dir = g_dir_open (extension_path, 0, NULL);
    else
        extension_dir = NULL;
    if (extension_dir)
    {
        while ((filename = g_dir_read_name (extension_dir)))
        {
            gchar* fullname;
            GModule* module;
            typedef MidoriExtension* (*extension_init_func)(void);
            extension_init_func extension_init;

            fullname = g_build_filename (extension_path, filename, NULL);
            module = g_module_open (fullname, G_MODULE_BIND_LOCAL);
            g_free (fullname);
            if (!module)
            {
                g_warning ("%s", g_module_error ());
                continue;
            }
            ;
            if (!g_module_symbol (module, "extension_init",
                             (gpointer) &extension_init))
            {
                g_warning ("%s", g_module_error ());
                continue;
            }
            extension = extension_init ();
            katze_array_add_item (extensions, extension);
        }
        g_dir_close (extension_dir);
    }

    g_object_set (app, "settings", settings,
                       "bookmarks", bookmarks,
                       "trash", trash,
                       "search-engines", search_engines,
                       "history", history,
                       "extensions", extensions,
                       NULL);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (midori_app_add_browser_cb), NULL);

    n = katze_array_get_length (extensions);
    for (i = 0; i < n; i++)
    {
        extension = katze_array_get_nth_item (extensions, i);
        g_signal_emit_by_name (extension, "activate", app);
    }

    browser = g_object_new (MIDORI_TYPE_BROWSER,
                            "settings", settings,
                            "bookmarks", bookmarks,
                            "trash", trash,
                            "search-engines", search_engines,
                            "history", history,
                            NULL);
    midori_app_add_browser (app, browser);
    gtk_widget_show (GTK_WIDGET (browser));

    session = midori_browser_get_proxy_array (browser);
    n = katze_array_get_length (_session);
    for (i = 0; i < n; i++)
    {
        item = katze_array_get_nth_item (_session, i);
        midori_browser_add_item (browser, item);
    }
    /* FIXME: Switch to the last active page */
    item = katze_array_get_nth_item (_session, 0);
    if (!strcmp (katze_item_get_uri (item), ""))
        midori_browser_activate_action (browser, "Location");
    g_object_unref (_session);

    g_signal_connect_after (browser, "notify::uri",
        G_CALLBACK (midori_browser_session_cb), session);
    g_signal_connect_after (browser, "add-tab",
        G_CALLBACK (midori_browser_session_cb), session);
    g_signal_connect_after (browser, "remove-tab",
        G_CALLBACK (midori_browser_session_cb), session);
    g_object_weak_ref (G_OBJECT (session),
        (GWeakNotify)(midori_browser_weak_notify_cb), browser);

    gtk_main ();

    /* Save configuration files */
    config_path = g_build_filename (g_get_user_config_dir (), PACKAGE_NAME,
                                    NULL);
    g_mkdir_with_parents (config_path, 0700);
    g_object_unref (history);
    #ifdef HAVE_SQLITE
    g_object_get (settings, "maximum-history-age", &max_history_age, NULL);
    midori_history_terminate (db, max_history_age);
    #endif
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
    if (!katze_array_to_file (bookmarks, config_file, &error))
    {
        g_warning (_("The bookmarks couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_object_unref (bookmarks);
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
