/*
 Copyright (C) 2010 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2010 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-bookmarks-db.h"

#include "midori-app.h"
#include "midori-array.h"
#include "sokoke.h"
#include "midori-core.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <config.h>
#ifdef HAVE_UNISTD_H
    #include <unistd.h>
#endif

static gint64
midori_bookmarks_insert_item_db (sqlite3*   db,
				 KatzeItem* item,
				 gint64     parentid);

static gboolean
midori_bookmarks_update_item_db (sqlite3*   db,
				 KatzeItem* item);

gint64
midori_bookmarks_insert_item_db (sqlite3*   db,
                                 KatzeItem* item,
                                 gint64     parentid)
{
    gchar* sqlcmd;
    char* errmsg = NULL;
    KatzeItem* old_parent;
    gchar* new_parentid;
    gchar* id = NULL;
    const gchar* uri = NULL;
    const gchar* desc = NULL;
    gint64 seq = 0;

    /* Bookmarks must have a name, import may produce invalid items */
    g_return_val_if_fail (katze_item_get_name (item), seq);

    if (!db)
        return seq;

    if (katze_item_get_meta_integer (item, "id") > 0)
        id = g_strdup_printf ("%" G_GINT64_FORMAT, katze_item_get_meta_integer(item, "id"));
    else
        id = g_strdup_printf ("NULL");

    if (KATZE_ITEM_IS_BOOKMARK (item))
        uri = katze_item_get_uri (item);

    if (katze_item_get_text (item))
        desc = katze_item_get_text (item);

    /* Use folder, otherwise fallback to parent folder */
    old_parent = katze_item_get_parent (item);
    if (parentid > 0)
        new_parentid = g_strdup_printf ("%" G_GINT64_FORMAT, parentid);
    else if (old_parent && katze_item_get_meta_integer (old_parent, "id") > 0)
        new_parentid = g_strdup_printf ("%" G_GINT64_FORMAT, katze_item_get_meta_integer (old_parent, "id"));
    else
        new_parentid = g_strdup_printf ("NULL");

    sqlcmd = sqlite3_mprintf (
            "INSERT INTO bookmarks (id, parentid, title, uri, desc, toolbar, app) "
            "VALUES (%q, %q, '%q', '%q', '%q', %d, %d)",
            id,
            new_parentid,
            katze_item_get_name (item),
            katze_str_non_null (uri),
            katze_str_non_null (desc),
            katze_item_get_meta_boolean (item, "toolbar"),
            katze_item_get_meta_boolean (item, "app"));

    if (sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg) == SQLITE_OK)
    {
        /* Get insert id */
        if (g_str_equal (id, "NULL"))
        {
            KatzeArray* seq_array;

            sqlite3_free (sqlcmd);
            sqlcmd = sqlite3_mprintf (
                    "SELECT seq FROM sqlite_sequence WHERE name = 'bookmarks'");

            seq_array = katze_array_from_sqlite (db, sqlcmd);
            if (katze_array_get_nth_item (seq_array, 0))
            {
                KatzeItem* seq_item = katze_array_get_nth_item (seq_array, 0);

                seq = katze_item_get_meta_integer (seq_item, "seq");
                katze_item_set_meta_integer (item, "id", seq);
            }
            g_object_unref (seq_array);
        }
    }
    else
    {
        g_printerr (_("Failed to add bookmark item: %s\n"), errmsg);
        sqlite3_free (errmsg);
    }

    sqlite3_free (sqlcmd);
    g_free (new_parentid);
    g_free (id);

    return seq;
}

gboolean
midori_bookmarks_update_item_db (sqlite3*   db,
                                 KatzeItem* item)
{
    gchar* sqlcmd;
    char* errmsg = NULL;
    gchar* parentid;
    gboolean updated;
    gchar* id;

    id = g_strdup_printf ("%" G_GINT64_FORMAT,
            katze_item_get_meta_integer (item, "id"));

    if (katze_item_get_meta_integer (item, "parentid") > 0)
        parentid = g_strdup_printf ("%" G_GINT64_FORMAT,
                                    katze_item_get_meta_integer (item, "parentid"));
    else
        parentid = g_strdup_printf ("NULL");

    sqlcmd = sqlite3_mprintf (
            "UPDATE bookmarks SET "
            "parentid=%q, title='%q', uri='%q', desc='%q', toolbar=%d, app=%d "
            "WHERE id = %q ;",
            parentid,
            katze_item_get_name (item),
            katze_str_non_null (katze_item_get_uri (item)),
            katze_str_non_null (katze_item_get_meta_string (item, "desc")),
            katze_item_get_meta_boolean (item, "toolbar"),
            katze_item_get_meta_boolean (item, "app"),
            id);

    updated = TRUE;
    if (sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        updated = FALSE;
        g_printerr (_("Failed to update bookmark: %s\n"), errmsg);
        sqlite3_free (errmsg);
    }

    sqlite3_free (sqlcmd);
    g_free (parentid);
    g_free (id);

    return updated;
}

/**
 * midori_bookmarks_db_update_item:
 * @bookmarks: the main bookmark array
 * @item: #KatzeItem the item to update
 *
 * Updates the @item in the bookmark data base.
 *
 * Since: 0.5.5
 **/
void
midori_array_update_item (KatzeArray* bookmarks,
			  KatzeItem* item)
{
    g_return_if_fail (KATZE_IS_ARRAY (bookmarks));
    g_return_if_fail (KATZE_IS_ITEM (item));
    g_return_if_fail (katze_item_get_meta_string (item, "id"));
    g_return_if_fail (0 != katze_item_get_meta_integer (item, "id"));

    sqlite3* db = g_object_get_data (G_OBJECT (bookmarks), "db");

    g_return_if_fail (db);

    midori_bookmarks_update_item_db (db, item);
}

void
midori_bookmarks_dbtracer (void*       dummy,
                           const char* query)
{
    g_printerr ("%s\n", query);
}

static void
midori_bookmarks_add_item_cb (KatzeArray* array,
                              KatzeItem*  item,
                              sqlite3*    db)
{
    midori_bookmarks_insert_item_db (db, item,
        katze_item_get_meta_integer (item, "parentid"));
}

static void
midori_bookmarks_remove_item_cb (KatzeArray* array,
                                 KatzeItem*  item,
                                 sqlite3*    db)
{
    gchar* sqlcmd;
    char* errmsg = NULL;
    gchar* id;

    id = g_strdup_printf ("%" G_GINT64_FORMAT,
            katze_item_get_meta_integer (item, "id"));

    sqlcmd = sqlite3_mprintf ("DELETE FROM bookmarks WHERE id = %q", id);

    if (sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        g_printerr (_("Failed to remove bookmark item: %s\n"), errmsg);
        sqlite3_free (errmsg);
    }

    sqlite3_free (sqlcmd);
    g_free (id);
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

KatzeArray*
midori_bookmarks_new (char** errmsg)
{
    sqlite3* db;
    gchar* oldfile;
    gchar* newfile;
    gboolean newfile_did_exist, oldfile_exists;
    const gchar* create_stmt;
    gchar* sql_errmsg = NULL;
    gchar* import_errmsg = NULL;
    KatzeArray* array;

    g_return_val_if_fail (errmsg != NULL, NULL);

    oldfile = midori_paths_get_config_filename_for_writing ("bookmarks.db");
    oldfile_exists = g_access (oldfile, F_OK) == 0;
    newfile = midori_paths_get_config_filename_for_writing ("bookmarks_v2.db");
    newfile_did_exist = g_access (newfile, F_OK) == 0;

    /* sqlite3_open will create the file if it did not exists already */
    if (sqlite3_open (newfile, &db) != SQLITE_OK)
    {
        *errmsg = g_strdup_printf (_("Failed to open database: %s\n"),
            db ? sqlite3_errmsg (db) : "(db = NULL)");
        goto init_failed;
    }

    if (midori_debug ("bookmarks"))
        sqlite3_trace (db, midori_bookmarks_dbtracer, NULL);

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
        const gchar* setup_stmt = "PRAGMA foreign_keys = ON;";
        /* initial setup */
        if (sqlite3_exec (db, setup_stmt, NULL, NULL, &sql_errmsg) != SQLITE_OK)
        {
            *errmsg = g_strdup_printf (_("Couldn't setup bookmarks: %s\n"),
                sql_errmsg ? sql_errmsg : "(err = NULL)");
            sqlite3_free (sql_errmsg);
            goto init_failed;
        }

        /* we are done */
        goto init_success;
    }
    else
    {
        /* initial creation */
        if (sqlite3_exec (db, create_stmt, NULL, NULL, &sql_errmsg) != SQLITE_OK)
        {
            *errmsg = g_strdup_printf (_("Couldn't create bookmarks table: %s\n"),
                sql_errmsg ? sql_errmsg : "(err = NULL)");
            sqlite3_free (sql_errmsg);

            /* we can as well remove the new file */
            g_unlink (newfile);
            goto init_failed;
        }

    }

    if (oldfile_exists)
        /* import from old db */
        if (!midori_bookmarks_import_from_old_db (db, oldfile, &import_errmsg))
        {
            *errmsg = g_strdup_printf (_("Couldn't import from old database: %s\n"),
                import_errmsg ? import_errmsg : "(err = NULL)");
            g_free (import_errmsg);
        }

    init_success:
        g_free (newfile);
        g_free (oldfile);
        array = katze_array_new (KATZE_TYPE_ARRAY);
        g_signal_connect (array, "add-item",
                          G_CALLBACK (midori_bookmarks_add_item_cb), db);
        g_signal_connect (array, "remove-item",
                          G_CALLBACK (midori_bookmarks_remove_item_cb), db);
        g_object_set_data (G_OBJECT (array), "db", db);
        return array;

    init_failed:
        g_free (newfile);
        g_free (oldfile);

        if (db)
            sqlite3_close (db);

        return NULL;
}

void
midori_bookmarks_on_quit (KatzeArray* array)
{
    g_return_if_fail (KATZE_IS_ARRAY (array));

    sqlite3* db = g_object_get_data (G_OBJECT (array), "db");
    g_return_if_fail (db != NULL);
    sqlite3_close (db);
}

void
midori_bookmarks_import_array (KatzeArray* bookmarks,
                               KatzeArray* array,
                               gint64      parentid)
{
    GList* list;
    KatzeItem* item;

    if (!bookmarks)
        return;

    KATZE_ARRAY_FOREACH_ITEM_L (item, array, list)
    {
        katze_item_set_meta_integer (item, "parentid", parentid);
        katze_array_add_item (bookmarks, item);
        if (KATZE_IS_ARRAY (item))
          midori_bookmarks_import_array (bookmarks, KATZE_ARRAY (item),
                                         katze_item_get_meta_integer(item, "id"));
    }
    g_list_free (list);
}

/**
 * midori_array_query_recursive:
 * @array: the main bookmark array
 * @fields: comma separated list of fields
 * @condition: condition, like "folder = '%q'"
 * @value: a value to be inserted if @condition contains %q
 * @recursive: if %TRUE include children
 *
 * Stores the result in a #KatzeArray.
 *
 * Return value: a #KatzeArray on success, %NULL otherwise
 *
 * Since: 0.4.4
 **/
KatzeArray*
midori_array_query_recursive (KatzeArray*  bookmarks,
                              const gchar* fields,
                              const gchar* condition,
                              const gchar* value,
                              gboolean     recursive)
{
    sqlite3* db;
    gchar* sqlcmd;
    char* sqlcmd_value;
    KatzeArray* array;
    KatzeItem* item;
    GList* list;

    g_return_val_if_fail (KATZE_IS_ARRAY (bookmarks), NULL);
    g_return_val_if_fail (fields, NULL);
    g_return_val_if_fail (condition, NULL);
    db = g_object_get_data (G_OBJECT (bookmarks), "db");
    g_return_val_if_fail (db != NULL, NULL);

    sqlcmd = g_strdup_printf ("SELECT %s FROM bookmarks WHERE %s "
                              "ORDER BY (uri='') ASC, title DESC", fields, condition);
    if (strstr (condition, "%q"))
    {
        sqlcmd_value = sqlite3_mprintf (sqlcmd, value ? value : "");
        array = katze_array_from_sqlite (db, sqlcmd_value);
        sqlite3_free (sqlcmd_value);
    }
    else
        array = katze_array_from_sqlite (db, sqlcmd);
    g_free (sqlcmd);

    if (!recursive)
        return array;

    KATZE_ARRAY_FOREACH_ITEM_L (item, array, list)
    {
        if (KATZE_ITEM_IS_FOLDER (item))
        {
            gchar* parentid = g_strdup_printf ("%" G_GINT64_FORMAT,
					       katze_item_get_meta_integer (item, "id"));
            KatzeArray* subarray = midori_array_query_recursive (bookmarks,
								 fields, "parentid=%q", parentid, TRUE);
            KatzeItem* subitem;
            GList* sublist;

            KATZE_ARRAY_FOREACH_ITEM_L (subitem, subarray, sublist)
            {
                katze_array_add_item (KATZE_ARRAY (item), subitem);
            }

            g_object_unref (subarray);
            g_free (parentid);
        }
    }
    g_list_free (list);
    return array;
}

static gint64
count_from_sqlite (sqlite3*     db,
		   const gchar* sqlcmd)
{
    gint64 count = -1;
    sqlite3_stmt* stmt;
    gint result;
    
    result = sqlite3_prepare_v2 (db, sqlcmd, -1, &stmt, NULL);
    if (result != SQLITE_OK)
        return -1;

    g_assert (sqlite3_column_count (stmt) == 1);
    
    if ((result = sqlite3_step (stmt)) == SQLITE_ROW)
	count = sqlite3_column_int64(stmt, 0);

    sqlite3_clear_bindings (stmt);
    sqlite3_reset (stmt);

    return count;
}

static gint64
midori_array_count_recursive_by_id (KatzeArray*  bookmarks,
				    const gchar* condition,
				    const gchar* value,
				    gint64       id,
				    gboolean     recursive)
{
    gint64 count = -1;
    sqlite3* db;
    gchar* sqlcmd;
    char* sqlcmd_value;
    sqlite3_stmt* stmt;
    gint result;
    GList* ids;
    GList* iter_ids;

    g_return_val_if_fail (condition, -1);
    g_return_val_if_fail (KATZE_IS_ARRAY (bookmarks), -1);
    db = g_object_get_data (G_OBJECT (bookmarks), "db");
    g_return_val_if_fail (db != NULL, -1);

    g_assert(!strstr("parentid", condition));

    if (id > 0)
	sqlcmd = g_strdup_printf ("SELECT COUNT(*) FROM bookmarks "
				  "WHERE parentid = %" G_GINT64_FORMAT " AND %s",
				  id,
				  condition);
    else
	sqlcmd = g_strdup_printf ("SELECT COUNT(*) FROM bookmarks "
				  "WHERE parentid IS NULL AND %s ",
				  condition);

    if (strstr (condition, "%q"))
    {
        sqlcmd_value = sqlite3_mprintf (sqlcmd, value ? value : "");
        count = count_from_sqlite (db, sqlcmd_value);
        sqlite3_free (sqlcmd_value);
    }
    else
        count = count_from_sqlite (db, sqlcmd);

    g_free (sqlcmd);

    if (!recursive || (count < 0))
        return count;

    ids = NULL;

    if (id > 0)
	sqlcmd_value = sqlite3_mprintf (
	    "SELECT id FROM bookmarks "
	    "WHERE parentid = %" G_GINT64_FORMAT " AND uri = ''", id);
    else
	sqlcmd_value = sqlite3_mprintf (
	    "SELECT id FROM bookmarks "
	    "WHERE parentid IS NULL AND uri = ''");

    if (sqlite3_prepare_v2 (db, sqlcmd_value, -1, &stmt, NULL) == SQLITE_OK)
    {
	g_assert (sqlite3_column_count (stmt) == 1);
    
	if ((result = sqlite3_step (stmt)) == SQLITE_ROW)
	{
	    gint64* pid = g_new (gint64, 1);
	    
	    *pid = sqlite3_column_int64(stmt, 0);
	    ids = g_list_append (ids, pid);
	}
	
	sqlite3_clear_bindings (stmt);
	sqlite3_reset (stmt);
    }

    sqlite3_free (sqlcmd_value);

    iter_ids = ids;
    while (iter_ids)
    {
	gint64 sub_count = midori_array_count_recursive_by_id (bookmarks,
							       condition,
							       value,
							       *(gint64*)(iter_ids->data),
							       recursive);
	
	if (sub_count < 0)
	{
	    g_list_free_full (ids, g_free);
	    return -1;
	}
	
	count += sub_count;
	iter_ids = g_list_next (iter_ids);
    }
	
    g_list_free_full (ids, g_free);
    return count;
}

/**
 * midori_array_count_recursive:
 * @array: the main bookmark array
 * @condition: condition, like "folder = '%q'"
 * @value: a value to be inserted if @condition contains %q
 * @recursive: if %TRUE include children
 *
 * Return value: the number of elements on success, -1 otherwise
 *
 * Since: 0.5.2
 **/
gint64
midori_array_count_recursive (KatzeArray*  bookmarks,
			      const gchar* condition,
                              const gchar* value,
			      KatzeItem*   folder,
                              gboolean     recursive)
{
    gint64 id = -1;

    g_return_val_if_fail (!folder || KATZE_ITEM_IS_FOLDER (folder), -1);
    
    id = folder ? katze_item_get_meta_integer (folder, "id") : 0;

    return midori_array_count_recursive_by_id (bookmarks, condition,
					       value, id,
					       recursive);
}
