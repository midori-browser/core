/*
 Copyright (C) 2010 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2010 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-bookmarks.h"
#include "panels/midori-bookmarks.h"
#include "midori-array.h"

#include <glib/gi18n.h>

#ifdef G_ENABLE_DEBUG
void midori_bookmarks_dbtracer(void* dummy, const char* query)
{
    g_printerr ("%s\n", query);
}
#endif

void
midori_bookmarks_add_item_cb (KatzeArray* array,
                              KatzeItem*  item,
                              sqlite3*    db)
{
    midori_bookmarks_insert_item_db (db, item,
        katze_item_get_meta_string (item, "folder"));
}

void
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
            katze_str_non_null (katze_item_get_meta_string (item, "folder")));

    else
       sqlcmd = sqlite3_mprintf (
            "DELETE FROM bookmarks WHERE title = '%q'"
            " AND folder = '%q'",
            katze_item_get_name (item),
            katze_str_non_null (katze_item_get_meta_string (item, "folder")));

    if (sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        g_printerr (_("Failed to remove history item: %s\n"), errmsg);
        sqlite3_free (errmsg);
    }

    sqlite3_free (sqlcmd);
}

sqlite3*
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

#ifdef G_ENABLE_DEBUG
    if (g_getenv ("MIDORI_BOOKMARKS_DEBUG"))
        sqlite3_trace (db, midori_bookmarks_dbtracer, NULL);
#endif

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

void
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
