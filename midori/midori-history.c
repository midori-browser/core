/*
 Copyright (C) 2010-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori/midori-history.h"

#include <glib/gi18n-lib.h>
#include <midori/midori-core.h>

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

KatzeArray*
midori_history_new (char** errmsg)
{
    MidoriHistoryDatabase* database;
    GError* error = NULL;
    sqlite3* db;
    gboolean has_day = FALSE;
    sqlite3_stmt* stmt;
    gint result;
    gchar* sql;
    gchar* bookmarks_filename;
    KatzeArray* array;

    g_return_val_if_fail (errmsg != NULL, NULL);

    database = midori_history_database_new (NULL, &error);
    if (error != NULL)
    {
        *errmsg = g_strdup (error->message);
        g_error_free (error);
        return NULL;
    }

    db = midori_database_get_db (MIDORI_DATABASE (database));
    g_return_val_if_fail (db != NULL, NULL);

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

    array = katze_array_new (KATZE_TYPE_ARRAY);
    g_object_set_data (G_OBJECT (array), "db", db);
    g_signal_connect (array, "clear",
                      G_CALLBACK (midori_history_clear_cb), db);
    return array;
}

void
midori_history_on_quit (KatzeArray*        array,
                        MidoriWebSettings* settings)
{
    gint max_history_age = katze_object_get_int (settings, "maximum-history-age");
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

