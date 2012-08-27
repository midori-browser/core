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
#include "sokoke.h"
#include "midori-core.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <config.h>
#ifdef HAVE_UNISTD_H
    #include <unistd.h>
#endif

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
        katze_item_get_meta_integer (item, "parentid"));
}

void
midori_bookmarks_remove_item_cb (KatzeArray* array,
                                 KatzeItem*  item,
                                 sqlite3*    db)
{
    gchar* sqlcmd;
    char* errmsg = NULL;


    sqlcmd = sqlite3_mprintf (
            "DELETE FROM bookmarks WHERE id = %" G_GINT64_FORMAT ";",
            katze_item_get_meta_integer (item, "id"));

    if (sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        g_printerr (_("Failed to remove history item: %s\n"), errmsg);
        sqlite3_free (errmsg);
    }

    sqlite3_free (sqlcmd);
}

#define _APPEND_TO_SQL_ERRORMSG(custom_errmsg) \
    do { \
        if (sql_errmsg) \
        { \
            g_string_append_printf (errmsg_str, "%s : %s\n", custom_errmsg, sql_errmsg); \
            sqlite3_free (sql_errmsg); \
        } \
        else \
            g_string_append (errmsg_str, custom_errmsg); \
    } while (0)

gboolean
midori_bookmarks_import_from_old_db (sqlite3*     db,
                                     const gchar* oldfile,
                                     gchar**      errmsg)
{
    gint sql_errcode;
    gboolean failure = FALSE;
    gchar* sql_errmsg = NULL;
    GString* errmsg_str = g_string_new (NULL);
    gchar* attach_stmt = sqlite3_mprintf ("ATTACH DATABASE %Q AS old_db;", oldfile);
    const gchar* convert_stmts =
        "BEGIN TRANSACTION;"
        "INSERT INTO main.bookmarks (parentid, title, uri, desc, app, toolbar) "
        "SELECT NULL AS parentid, title, uri, desc, app, toolbar "
        "FROM old_db.bookmarks;"
        "UPDATE main.bookmarks SET parentid = ("
        "SELECT id FROM main.bookmarks AS b1 WHERE b1.title = ("
        "SELECT folder FROM old_db.bookmarks WHERE title = main.bookmarks.title));"
        "COMMIT;";
    const gchar* detach_stmt = "DETACH DATABASE old_db;";

    *errmsg = NULL;
    sql_errcode = sqlite3_exec (db, attach_stmt, NULL, NULL, &sql_errmsg);
    sqlite3_free (attach_stmt);

    if (sql_errcode != SQLITE_OK)
    {
        _APPEND_TO_SQL_ERRORMSG (_("failed to ATTACH old db"));
        goto convert_failed;
    }

    if (sqlite3_exec (db, convert_stmts, NULL, NULL, &sql_errmsg) != SQLITE_OK)
    {
        failure = TRUE;
        _APPEND_TO_SQL_ERRORMSG (_("failed to import from old db"));

        /* try to get back to previous state */
        if (sqlite3_exec (db, "ROLLBACK TRANSACTION;", NULL, NULL, &sql_errmsg) != SQLITE_OK)
            _APPEND_TO_SQL_ERRORMSG (_("failed to rollback the transaction"));
    }

    if (sqlite3_exec (db, detach_stmt, NULL, NULL, &sql_errmsg) != SQLITE_OK)
        _APPEND_TO_SQL_ERRORMSG (_("failed to DETACH "));

    if (failure)
    {
    convert_failed:
        *errmsg = g_string_free (errmsg_str, FALSE);
        g_print ("ERRORR: %s\n", errmsg_str->str);
        return FALSE;
    }

    return TRUE;
}
#undef _APPEND_TO_SQL_ERRORMSG

sqlite3*
midori_bookmarks_initialize (KatzeArray*  array,
                             char**       errmsg)
{
    sqlite3* db;
    gchar* oldfile;
    gchar* newfile;
    gboolean newfile_did_exist, oldfile_exists;
    const gchar* create_stmt;
    gchar* sql_errmsg = NULL;
    gchar* import_errmsg = NULL;

    g_return_val_if_fail (errmsg != NULL, NULL);

    oldfile = g_build_filename (midori_paths_get_config_dir (), "bookmarks.db", NULL);
    oldfile_exists = g_access (oldfile, F_OK) == 0;
    newfile = g_build_filename (midori_paths_get_config_dir (), "bookmarks_v2.db", NULL);
    newfile_did_exist = g_access (newfile, F_OK) == 0;

    /* sqlite3_open will create the file if it did not exists already */
    if (sqlite3_open (newfile, &db) != SQLITE_OK)
    {
        if (db)
            *errmsg = g_strdup_printf (_("failed to open database: %s\n"),
                    sqlite3_errmsg (db));
        else
            *errmsg = g_strdup (_("failed to open database\n"));

        goto init_failed;
    }

#ifdef G_ENABLE_DEBUG
    if (midori_debug ("bookmarks"))
        sqlite3_trace (db, midori_bookmarks_dbtracer, NULL);
#endif

    create_stmt =     /* Table structure */
        "CREATE TABLE IF NOT EXISTS bookmarks "
        "(id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "parentid INTEGER DEFAULT NULL, "
        "title TEXT, uri TEXT, desc TEXT, app INTEGER, toolbar INTEGER, "
        "pos_panel INTEGER, pos_bar INTEGER, "
        "created DATE DEFAULT CURRENT_TIMESTAMP, "
        "last_visit DATE, visit_count INTEGER DEFAULT 0, "
        "nick TEXT, "
        "FOREIGN KEY(parentid) REFERENCES bookmarks(id) "
        "ON DELETE CASCADE); PRAGMA foreign_keys = ON;"

        /* trigger: insert panel position */
        "CREATE TRIGGER IF NOT EXISTS bookmarkInsertPosPanel "
        "AFTER INSERT ON bookmarks FOR EACH ROW "
        "BEGIN UPDATE bookmarks SET pos_panel = ("
        "SELECT ifnull(MAX(pos_panel),0)+1 FROM bookmarks WHERE "
        "(NEW.parentid IS NOT NULL AND parentid = NEW.parentid) "
        "OR (NEW.parentid IS NULL AND parentid IS NULL)) "
        "WHERE id = NEW.id; END;"

        /* trigger: insert Bookmarkbar position */
        "CREATE TRIGGER IF NOT EXISTS bookmarkInsertPosBar "
        "AFTER INSERT ON bookmarks FOR EACH ROW WHEN NEW.toolbar=1 "
        "BEGIN UPDATE bookmarks SET pos_bar = ("
        "SELECT ifnull(MAX(pos_bar),0)+1 FROM bookmarks WHERE "
        "((NEW.parentid IS NOT NULL AND parentid = NEW.parentid) "
        "OR (NEW.parentid IS NULL AND parentid IS NULL)) AND toolbar=1) "
        "WHERE id = NEW.id; END;"

        /* trigger: update panel position */
        "CREATE TRIGGER IF NOT EXISTS bookmarkUpdatePosPanel "
        "BEFORE UPDATE OF parentid ON bookmarks FOR EACH ROW "
        "WHEN ((NEW.parentid IS NULL OR OLD.parentid IS NULL) "
        "AND NEW.parentid IS NOT OLD.parentid) OR "
        "((NEW.parentid IS NOT NULL AND OLD.parentid IS NOT NULL) "
        "AND NEW.parentid!=OLD.parentid) "
        "BEGIN UPDATE bookmarks SET pos_panel = pos_panel-1 "
        "WHERE ((OLD.parentid IS NOT NULL AND parentid = OLD.parentid) "
        "OR (OLD.parentid IS NULL AND parentid IS NULL)) AND pos_panel > OLD.pos_panel; "
        "UPDATE bookmarks SET pos_panel = ("
        "SELECT ifnull(MAX(pos_panel),0)+1 FROM bookmarks "
        "WHERE (NEW.parentid IS NOT NULL AND parentid = NEW.parentid) "
        "OR (NEW.parentid IS NULL AND parentid IS NULL)) "
        "WHERE id = OLD.id; END;"

        /* trigger: update Bookmarkbar position */
        "CREATE TRIGGER IF NOT EXISTS bookmarkUpdatePosBar0 "
        "AFTER UPDATE OF parentid, toolbar ON bookmarks FOR EACH ROW "
        "WHEN ((NEW.parentid IS NULL OR OLD.parentid IS NULL) "
        "AND NEW.parentid IS NOT OLD.parentid) "
        "OR ((NEW.parentid IS NOT NULL AND OLD.parentid IS NOT NULL) "
        "AND NEW.parentid!=OLD.parentid) OR (OLD.toolbar=1 AND NEW.toolbar=0) "
        "BEGIN UPDATE bookmarks SET pos_bar = NULL WHERE id = NEW.id; "
        "UPDATE bookmarks SET pos_bar = pos_bar-1 "
        "WHERE ((OLD.parentid IS NOT NULL AND parentid = OLD.parentid) "
        "OR (OLD.parentid IS NULL AND parentid IS NULL)) AND pos_bar > OLD.pos_bar; END;"

        /* trigger: update Bookmarkbar position */
        "CREATE TRIGGER IF NOT EXISTS bookmarkUpdatePosBar1 "
        "BEFORE UPDATE OF parentid, toolbar ON bookmarks FOR EACH ROW "
        "WHEN ((NEW.parentid IS NULL OR OLD.parentid IS NULL) "
        "AND NEW.parentid IS NOT OLD.parentid) OR "
        "((NEW.parentid IS NOT NULL AND OLD.parentid IS NOT NULL) "
        "AND NEW.parentid!=OLD.parentid) OR (OLD.toolbar=0 AND NEW.toolbar=1) "
        "BEGIN UPDATE bookmarks SET pos_bar = ("
        "SELECT ifnull(MAX(pos_bar),0)+1 FROM bookmarks WHERE "
        "(NEW.parentid IS NOT NULL AND parentid = NEW.parentid) "
        "OR (NEW.parentid IS NULL AND parentid IS NULL)) "
        "WHERE id = OLD.id; END;"

        /* trigger: delete panel position */
        "CREATE TRIGGER IF NOT EXISTS bookmarkDeletePosPanel "
        "AFTER DELETE ON bookmarks FOR EACH ROW "
        "BEGIN UPDATE bookmarks SET pos_panel = pos_panel-1 "
        "WHERE ((OLD.parentid IS NOT NULL AND parentid = OLD.parentid) "
        "OR (OLD.parentid IS NULL AND parentid IS NULL)) AND pos_panel > OLD.pos_panel; END;"

        /* trigger: delete Bookmarkbar position */
        "CREATE TRIGGER IF NOT EXISTS bookmarkDeletePosBar "
        "AFTER DELETE ON bookmarks FOR EACH ROW WHEN OLD.toolbar=1 "
        "BEGIN UPDATE bookmarks SET pos_bar = pos_bar-1 "
        "WHERE ((OLD.parentid IS NOT NULL AND parentid = OLD.parentid) "
        "OR (OLD.parentid IS NULL AND parentid IS NULL)) AND pos_bar > OLD.pos_bar; END;";


    if (newfile_did_exist)
    {
        /* we are done */
        goto init_success;
    }
    else
    {
        /* initial creation */
        if (sqlite3_exec (db, create_stmt, NULL, NULL, &sql_errmsg) != SQLITE_OK)
        {

            if (errmsg)
            {
                if (sql_errmsg)
                {
                    *errmsg = g_strdup_printf (_("could not create bookmarks table: %s\n"), sql_errmsg);
                    sqlite3_free (sql_errmsg);
                }
                else
                    *errmsg = g_strdup (_("could not create bookmarks table"));
            }

            /* we can as well remove the new file */
            g_unlink (newfile);
            goto init_failed;
        }

    }

    if (oldfile_exists)
        /* import from old db */
        if (!midori_bookmarks_import_from_old_db (db, oldfile, &import_errmsg))
        {
            if (errmsg)
            {
                if (import_errmsg)
                {
                    *errmsg = g_strdup_printf (_("could not import from old database: %s\n"), import_errmsg);
                    g_free (import_errmsg);
                }
                else
                    *errmsg = g_strdup_printf (_("could not import from old database"));
            }
        }

    init_success:
        g_free (newfile);
        g_free (oldfile);
        g_signal_connect (array, "add-item",
                          G_CALLBACK (midori_bookmarks_add_item_cb), db);
        g_signal_connect (array, "remove-item",
                          G_CALLBACK (midori_bookmarks_remove_item_cb), db);

        return db;

    init_failed:
        g_free (newfile);
        g_free (oldfile);

        if (db)
            sqlite3_close (db);

        return NULL;
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
    midori_bookmarks_import_array_db (db, bookmarks, 0);
}
