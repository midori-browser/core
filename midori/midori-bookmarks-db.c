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

/**
 * SECTION:midory-bookmarks-db
 * @short_description: A #KatzeArray connected to a database
 * @see_also: #KatzeArray
 *
 * #MidoriBookmarksDb is a #KatzeArray specialized for database
 * interraction.
 */

struct _MidoriBookmarksDb
{
    KatzeArray  parent_instance;

    sqlite3*    db;
    GList*      pending_inserts;
    GHashTable* pending_updates;
    GHashTable* pending_deletes;
    GHashTable* all_items;
    gboolean    in_idle_func;
};

struct _MidoriBookmarksDbClass
{
    KatzeArrayClass parent_class;

    /* Signals */
    void
    (*update_item)            (MidoriBookmarksDb* bookmarks,
                               gpointer    item);
};

G_DEFINE_TYPE (MidoriBookmarksDb, midori_bookmarks_db, KATZE_TYPE_ARRAY);

enum {
    UPDATE_ITEM,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
_midori_bookmarks_db_add_item (KatzeArray* array,
                               gpointer    item);

static void
_midori_bookmarks_db_update_item (MidoriBookmarksDb* bookmarks,
                                  gpointer    item);

static void
_midori_bookmarks_db_remove_item (KatzeArray* array,
                                  gpointer   item);

static void
_midori_bookmarks_db_move_item (KatzeArray* array,
                                gpointer    item,
                                gint        position);

static void
_midori_bookmarks_db_clear (KatzeArray* array);

static void
midori_bookmarks_db_force_idle (MidoriBookmarksDb* bookmarks);

static void
midori_bookmarks_db_finalize (GObject* object);

static gint64
midori_bookmarks_db_insert_item_db (sqlite3*   db,
                                    KatzeItem* item,
                                    gint64     parentid);

static gboolean
midori_bookmarks_db_update_item_db (sqlite3*   db,
                                    KatzeItem* item);

static gboolean
midori_bookmarks_db_remove_item_db (sqlite3*   db,
                                    KatzeItem* item);

static guint
item_hash (gconstpointer item)
{
    gint64 id = katze_item_get_meta_integer (KATZE_ITEM (item), "id");
    return g_int64_hash (&id);
}

static gboolean
item_equal (gconstpointer item_a, gconstpointer item_b)
{
    gint64 id_a = katze_item_get_meta_integer (KATZE_ITEM (item_a), "id");
    gint64 id_b = katze_item_get_meta_integer (KATZE_ITEM (item_b), "id");
    return (id_a == id_b)? TRUE : FALSE;
}

static void
midori_bookmarks_db_class_init (MidoriBookmarksDbClass* class)
{
    GObjectClass* gobject_class;
    KatzeArrayClass* katze_array_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_bookmarks_db_finalize;

    signals[UPDATE_ITEM] = g_signal_new (
        "update-item",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriBookmarksDbClass, update_item),
        0,
        NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE, 1,
        G_TYPE_POINTER);

    katze_array_class = KATZE_ARRAY_CLASS (class);

    katze_array_class->add_item = _midori_bookmarks_db_add_item;
    katze_array_class->remove_item = _midori_bookmarks_db_remove_item;
    katze_array_class->move_item = _midori_bookmarks_db_move_item;
    katze_array_class->clear = _midori_bookmarks_db_clear;

    class->update_item = _midori_bookmarks_db_update_item;
}

static void
midori_bookmarks_db_init (MidoriBookmarksDb* bookmarks)
{
    bookmarks->db = NULL;
    bookmarks->pending_inserts = NULL;
    bookmarks->pending_updates = g_hash_table_new (item_hash, item_equal);
    bookmarks->pending_deletes = g_hash_table_new (item_hash, item_equal);
    bookmarks->all_items = g_hash_table_new (item_hash, item_equal);

    bookmarks->in_idle_func = FALSE;

    katze_item_set_meta_integer (KATZE_ITEM (bookmarks), "id", -1);
    katze_item_set_name (KATZE_ITEM (bookmarks), _("Bookmarks"));
    g_hash_table_insert (bookmarks->all_items, bookmarks, bookmarks);
    /* g_object_ref (bookmarks); */
}

static void
midori_bookmarks_db_finalize (GObject* object)
{
    MidoriBookmarksDb* bookmarks = MIDORI_BOOKMARKS_DB (object);

    if (bookmarks->db)
    {
        midori_bookmarks_db_force_idle (bookmarks);
        sqlite3_close (bookmarks->db);
    }

    g_list_free (bookmarks->pending_inserts);
    g_hash_table_unref (bookmarks->pending_updates);
    g_hash_table_unref (bookmarks->pending_deletes);
    g_hash_table_unref (bookmarks->all_items);

    G_OBJECT_CLASS (midori_bookmarks_db_parent_class)->finalize (object);
}

/**
 * midori_bookmarks_db_get_item_parent:
 * @bookmarks: the main bookmarks array
 * @item: a #KatzeItem
 *
 * Internal function that find the parent of the @item thanks to its %parentid
 **/
static KatzeArray*
midori_bookmarks_db_get_item_parent (MidoriBookmarksDb* bookmarks,
                                     gpointer    item)
{
    KatzeArray* parent;
    gint64 parentid;

    parentid = katze_item_get_meta_integer (KATZE_ITEM (item), "parentid");

    if (parentid == 0)
    {
        parent = KATZE_ARRAY (bookmarks);
    }
    else
    {
        KatzeItem *search = katze_item_new ();

        katze_item_set_meta_integer(search, "id", parentid);

        parent = KATZE_ARRAY (g_hash_table_lookup (bookmarks->all_items, search));

        g_object_unref (search);
    }

    return parent;
}

/**
 * _midori_bookmarks_db_add_item:
 * @array: the main bookmarks array
 * @item: a #KatzeItem
 *
 * Internal function that overloads the #KatzeArray %katze_array_add_item().
 * It relays the add item to the appropriate #KatzeArray.
 **/
static void
_midori_bookmarks_db_add_item (KatzeArray* array,
                               gpointer    item)
{
    MidoriBookmarksDb *bookmarks;
    KatzeArray* parent;
    KatzeArray* db_parent;

    g_return_if_fail (IS_MIDORI_BOOKMARKS_DB (array));
    g_return_if_fail (KATZE_IS_ITEM (item));

    bookmarks = MIDORI_BOOKMARKS_DB (array);
    g_return_if_fail (bookmarks->in_idle_func);

    parent = katze_item_get_parent (KATZE_ITEM (item));

    db_parent = midori_bookmarks_db_get_item_parent (bookmarks, item);

    if (parent == db_parent)
    {
        if (IS_MIDORI_BOOKMARKS_DB (parent))
            KATZE_ARRAY_CLASS (midori_bookmarks_db_parent_class)->update (parent);
        else if (KATZE_IS_ARRAY (parent))
            katze_array_update (parent);
        return;
    }

    if (IS_MIDORI_BOOKMARKS_DB (parent))
        KATZE_ARRAY_CLASS (midori_bookmarks_db_parent_class)->add_item (parent, item);
    else if (KATZE_IS_ARRAY (parent))
        katze_array_add_item (parent, item);

    g_assert (parent == katze_item_get_parent (KATZE_ITEM (item)));
}

/**
 * _midori_bookmarks_db_update_item:
 * @array: the main bookmarks array
 * @item: a #KatzeItem
 *
 * Internal function that implements the %midori_bookmarks_db_update_item() post-processing.
 * It relays an update to the appropriate #KatzeArray.
 **/
static void
_midori_bookmarks_db_update_item (MidoriBookmarksDb* bookmarks,
                                  gpointer    item)
{
    KatzeArray* parent;

    g_return_if_fail (IS_MIDORI_BOOKMARKS_DB (bookmarks));
    g_return_if_fail (KATZE_IS_ITEM (item));

    g_return_if_fail (bookmarks->in_idle_func);

    parent = katze_item_get_parent (KATZE_ITEM (item));

    g_return_if_fail (parent);

    katze_array_update (parent);
}

/**
 * _midori_bookmarks_db_remove_item:
 * @array: the main bookmarks array
 * @item: a #KatzeItem
 *
 * Internal function that overloads the #KatzeArray %katze_array_remove_item().
 * It relays the remove item to the appropriate #KatzeArray.
 **/
static void
_midori_bookmarks_db_remove_item (KatzeArray* array,
                                  gpointer   item)
{
    MidoriBookmarksDb *bookmarks;
    KatzeArray* parent;

    g_return_if_fail (IS_MIDORI_BOOKMARKS_DB (array));
    g_return_if_fail (KATZE_IS_ITEM (item));

    bookmarks = MIDORI_BOOKMARKS_DB (array);
    g_return_if_fail (bookmarks->in_idle_func);

    parent = katze_item_get_parent (KATZE_ITEM (item));

    g_return_if_fail (parent);

    if (IS_MIDORI_BOOKMARKS_DB (parent))
        KATZE_ARRAY_CLASS (midori_bookmarks_db_parent_class)->remove_item (parent, item);
    else if (KATZE_IS_ARRAY (parent))
        katze_array_remove_item (parent, item);
}

/**
 * _midori_bookmarks_db_move_item:
 * @array: the main bookmarks array
 * @item: a #KatzeItem
 * @position: the new @item position
 *
 * Internal function that overloads the #KatzeArray %katze_array_move_item().
 * It relays the move @item to the appropriate #KatzeArray.
 **/
static void
_midori_bookmarks_db_move_item (KatzeArray* array,
                                gpointer    item,
                                gint        position)
{
    MidoriBookmarksDb *bookmarks;
    KatzeArray* parent;

    g_return_if_fail (IS_MIDORI_BOOKMARKS_DB (array));
    g_return_if_fail (KATZE_IS_ITEM (item));

    parent = katze_item_get_parent (KATZE_ITEM (item));

    g_return_if_fail (parent);

    KATZE_ARRAY_CLASS (midori_bookmarks_db_parent_class)->move_item (parent, item, position);
}

/**
 * _midori_bookmarks_db_clear:
 * @array: the main bookmarks array
 *
 * Internal function that overloads the #KatzeArray %katze_array_clear().
 * It deletes the whole bookmarks data.
 **/
static void
_midori_bookmarks_db_clear (KatzeArray* array)
{
    g_return_if_fail (IS_MIDORI_BOOKMARKS_DB (array));

    g_critical ("_midori_bookmarks_db_clear: not implemented\n");
}

/**
 * midori_bookmarks_db_signal_update_item:
 * @array: a #KatzeArray
 * @item: an item
 *
 * Notify an update of the item of the array.
 *
 **/
static void
midori_bookmarks_db_signal_update_item (MidoriBookmarksDb* array,
                                        gpointer    item)
{
    g_return_if_fail (IS_MIDORI_BOOKMARKS_DB (array));

    g_signal_emit (array, signals[UPDATE_ITEM], 0, item);
}

/**
 * midori_bookmarks_db_begin_transaction:
 * @db: the removed #KatzeItem
 *
 * Internal function that starts an SQL transaction.
 **/
static gboolean
midori_bookmarks_db_begin_transaction (sqlite3* db)
{
    char* errmsg = NULL;

    if (sqlite3_exec (db, "BEGIN TRANSACTION;", NULL, NULL, &errmsg) != SQLITE_OK)
    {
        g_printerr (_("Failed to begin transaction: %s\n"), errmsg);
        sqlite3_free (errmsg);
        return FALSE;
    }

    return TRUE;
}

/**
 * midori_bookmarks_db_end_transaction:
 * @db: the removed #KatzeItem
 * @commit : boolean
 *
 * Internal function that ends an SQL transaction.
 * If @commit is %TRUE, the transaction is ended by a COMMIT.
 * It is ended by a ROLLBACK otherwise.
 **/
static void
midori_bookmarks_db_end_transaction (sqlite3* db, gboolean commit)
{
    char* errmsg = NULL;
    if (sqlite3_exec (db, (commit ? "COMMIT;" : "ROLLBACK;"), NULL, NULL, &errmsg) != SQLITE_OK)
    {
        if (commit)
            g_printerr (_("Failed to end transaction: %s\n"), errmsg);
        else
            g_printerr (_("Failed to cancel transaction: %s\n"), errmsg);
        sqlite3_free (errmsg);
    }
}

/**
 * midori_bookmarks_db_add_item_recursive:
 * @item: the removed #KatzeItem
 * @bookmarks : the main bookmarks array
 *
 * Internal function that creates memory records of the added @item.
 * If @item is a #KatzeArray, the function recursiveley adds records
 * of all its childs.
 **/
static gint
midori_bookmarks_db_add_item_recursive (MidoriBookmarksDb* bookmarks,
                                        KatzeItem* item)
{
    GList* list;
    KatzeArray* array;
    gint64 id = 0;
    gint count = 0;
    gint64 parentid = katze_item_get_meta_integer (item, "parentid");

    id = midori_bookmarks_db_insert_item_db (bookmarks->db, item, parentid);
    count++;

    g_object_ref (item);
    g_hash_table_insert (bookmarks->all_items, item, item);

    if (!KATZE_IS_ARRAY (item))
        return count;

    array = KATZE_ARRAY (item);

    KATZE_ARRAY_FOREACH_ITEM_L (item, array, list)
    {
        katze_item_set_meta_integer (item, "parentid", id);
        count += midori_bookmarks_db_add_item_recursive (bookmarks, item);
    }

    g_list_free (list);
    return count;
}

/**
 * midori_bookmarks_db_remove_item_recursive:
 * @item: the removed #KatzeItem
 * @bookmarks : the main bookmarks array
 *
 * Internal function that removes memory records of the removed @item.
 * If @item is a #KatzeArray, the function recursiveley removes records
 * of all its childs.
 **/
static void
midori_bookmarks_db_remove_item_recursive (KatzeItem*  item,
                                           MidoriBookmarksDb* bookmarks)
{
    GHashTableIter hash_iter;
    gpointer key, value;
    gpointer found;
    KatzeArray* array;
    KatzeItem* child;
    GList* list;

    if (NULL != (found = g_list_find (bookmarks->pending_inserts, item)))
    {
        g_object_unref (((GList*)found)->data);
        bookmarks->pending_inserts = g_list_delete_link (bookmarks->pending_inserts,
            ((GList*)found));
    }

    if (NULL != (found = g_hash_table_lookup (bookmarks->pending_updates, item)))
    {
        g_hash_table_remove (bookmarks->pending_updates, found);
        g_object_unref (found);
    }

    if (NULL != (found = g_hash_table_lookup (bookmarks->all_items, item)))
    {
        g_hash_table_remove (bookmarks->all_items, found);
        g_object_unref (found);
    }

    if (!KATZE_IS_ARRAY (item))
        return;

    array = KATZE_ARRAY (item);

    KATZE_ARRAY_FOREACH_ITEM_L (child, array, list)
    {
        midori_bookmarks_db_remove_item_recursive (child, bookmarks);
    }

    g_list_free (list);
}

/**
 * midori_bookmarks_db_idle_func:
 * @data: the main bookmark array
 *
 * Internal function executed during idle time that Packs pending database 
 * operations in one transaction.
 *
 * Pending operations are either:
 * a. a list of pending add items,
 *    all child #KatzeItem of a #KatzeArray are recursively added.
 *    Each added #KatzeItem is memorized for future use.
 *    (See %midori_bookmarks_db_array_from_statement())
 * b. a hash table of items to update,
 * c. or a hash table of items to remove
 *    the database CASCADE on delete takes care of removal of the childs 
 *    #KatzeItem of a #KatzeArray in the database 
 *
 * When database operations are done, the #KatzeArray equivalent operations
 * are called to:
 * 1. update the #KatzeArray tree content
 * 2. signal the client views of the  #KatzeArray tree content change.
 **/
static gboolean
midori_bookmarks_db_idle_func (gpointer data)
{
    GTimer *timer = g_timer_new();
    gint count = 0;
    gulong microseconds;
    gboolean with_transaction;
    MidoriBookmarksDb* bookmarks = MIDORI_BOOKMARKS_DB (data);
    GList* list_iter;
    GHashTableIter hash_iter;
    gpointer key, value;

    bookmarks->in_idle_func = TRUE;

    g_timer_start (timer);

    with_transaction = midori_bookmarks_db_begin_transaction (bookmarks->db);

    for (list_iter = bookmarks->pending_inserts; list_iter; list_iter = g_list_next (list_iter))
    {
        KatzeItem *item = KATZE_ITEM (list_iter->data);

        count += midori_bookmarks_db_add_item_recursive (bookmarks, item);
        katze_array_add_item (KATZE_ARRAY (bookmarks), item);

        g_object_unref (item);
    }

    g_hash_table_iter_init (&hash_iter, bookmarks->pending_updates);

    while (g_hash_table_iter_next (&hash_iter, &key, &value))
    {
        KatzeItem *item = KATZE_ITEM (value);

        midori_bookmarks_db_update_item_db (bookmarks->db, item);
        midori_bookmarks_db_signal_update_item (bookmarks, item);
        g_object_unref (item);
        count++;
    }

    g_hash_table_iter_init (&hash_iter, bookmarks->pending_deletes);

    while (g_hash_table_iter_next (&hash_iter, &key, &value))
    {
        KatzeItem *item = KATZE_ITEM (value);

        midori_bookmarks_db_remove_item_db (bookmarks->db, item);
        katze_array_remove_item (KATZE_ARRAY (bookmarks), item);
        g_object_unref (item);
        count++;
    }

    if (with_transaction)
        midori_bookmarks_db_end_transaction (bookmarks->db, TRUE);

    g_timer_elapsed (timer, &microseconds);
    g_print ("midori_bookmarks_db_idle: %d DB operation(s) in %lu micro-seconds\n",
        count, microseconds);

    g_timer_destroy (timer);

    g_hash_table_remove_all (bookmarks->pending_deletes);
    g_hash_table_remove_all (bookmarks->pending_updates);
    g_list_free (bookmarks->pending_inserts);
    bookmarks->pending_inserts = NULL;

    bookmarks->in_idle_func = FALSE;

    return FALSE;
}

/**
 * midori_bookmarks_db_idle_start:
 * @bookmarks: the main bookmark array
 *
 * Internal function that checks whether idle processing is pending,
 * if not, add a new one.
 **/
static void
midori_bookmarks_db_idle_start (MidoriBookmarksDb* bookmarks)
{
    g_return_if_fail (bookmarks->db != NULL);

    if (bookmarks->pending_inserts
        || g_hash_table_size (bookmarks->pending_updates)
        || g_hash_table_size (bookmarks->pending_deletes))
        return;

    g_idle_add (midori_bookmarks_db_idle_func, bookmarks);
}

/**
 * midori_bookmarks_db_insert_item_db:
 * @db: the #sqlite3
 * @item: #KatzeItem the item to insert
 *
 * Internal function that does the actual SQL INSERT of the @item in @db.
 *
 * Since: 0.5.2
 **/
static gint64
midori_bookmarks_db_insert_item_db (sqlite3*   db,
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

/**
 * midori_bookmarks_db_update_item_db:
 * @db: the #sqlite3
 * @item: #KatzeItem the item to update
 *
 * Internal function that does the actual SQL UPDATE of the @item in @db.
 *
 * Since: 0.5.2
 **/
static gboolean
midori_bookmarks_db_update_item_db (sqlite3*   db,
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
 * midori_bookmarks_db_remove_item_db:
 * @db: the #sqlite3
 * @item: #KatzeItem the item to delete
 *
 * Internal function that does the actual SQL DELETE of the @item in @db.
 *
 * Since: 0.5.2
 **/
static gboolean
midori_bookmarks_db_remove_item_db (sqlite3*    db,
                                    KatzeItem*  item)
{
    char* errmsg = NULL;
    gchar* sqlcmd;
    gboolean removed = TRUE;
    gchar* id;

    id = g_strdup_printf ("%" G_GINT64_FORMAT,
                          katze_item_get_meta_integer (item, "id"));

    sqlcmd = sqlite3_mprintf ("DELETE FROM bookmarks WHERE id = %q", id);

    if (sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        g_printerr (_("Failed to remove bookmark item: %s\n"), errmsg);
        sqlite3_free (errmsg);
        removed = FALSE;
    }

    sqlite3_free (sqlcmd);
    g_free (id);
    return removed;
}

/**
 * midori_bookmarks_db_add_item:
 * @bookmarks: the main bookmark array
 * @item: #KatzeItem the item to update
 *
 * Adds the @item in the bookmark data base.
 *
 * Since: 0.5.2
 **/
void
midori_bookmarks_db_add_item (MidoriBookmarksDb* bookmarks, KatzeItem* item)
{
    g_return_if_fail (IS_MIDORI_BOOKMARKS_DB (bookmarks));
    g_return_if_fail (KATZE_IS_ITEM (item));

    /* Force NULL id for database addition */
    if (NULL != katze_item_get_meta_string (item, "id"))
    {
	katze_item_set_meta_string (item, "id", NULL);
    }

    gpointer found = g_list_find (bookmarks->pending_inserts, item);

    if (found)
        return;

    midori_bookmarks_db_idle_start (bookmarks);

    g_object_ref (item);
    bookmarks->pending_inserts = g_list_append (bookmarks->pending_inserts, item);
}

/**
 * midori_bookmarks_db_update_item:
 * @bookmarks: the main bookmark array
 * @item: #KatzeItem the item to update
 *
 * Updates the @item in the bookmark data base.
 *
 * Since: 0.5.2
 **/
void
midori_bookmarks_db_update_item (MidoriBookmarksDb* bookmarks, KatzeItem* item)
{
    g_return_if_fail (IS_MIDORI_BOOKMARKS_DB (bookmarks));
    g_return_if_fail (KATZE_IS_ITEM (item));
    g_return_if_fail (katze_item_get_meta_string (item, "id"));
    g_return_if_fail (0 != katze_item_get_meta_integer (item, "id"));

    gpointer found = g_hash_table_lookup (bookmarks->pending_updates, item);

    if (found)
        return;

    midori_bookmarks_db_idle_start (bookmarks);

    g_object_ref (item);
    g_hash_table_insert (bookmarks->pending_updates, item, item);
}

/**
 * midori_bookmarks_db_remove_item:
 * @bookmarks: the main bookmark array
 * @item: #KatzeItem the item to remove
 *
 * Removes the @item from the bookmark data base.
 *
 * Since: 0.5.2
 **/
void
midori_bookmarks_db_remove_item (MidoriBookmarksDb* bookmarks, KatzeItem* item)
{
    g_return_if_fail (IS_MIDORI_BOOKMARKS_DB (bookmarks));
    g_return_if_fail (KATZE_IS_ITEM (item));
    g_return_if_fail (katze_item_get_meta_string (item, "id"));
    g_return_if_fail (0 != katze_item_get_meta_integer (item, "id"));

    gpointer found = g_hash_table_lookup (bookmarks->pending_deletes, item);

    if (found)
        return;

    midori_bookmarks_db_idle_start (bookmarks);

    midori_bookmarks_db_remove_item_recursive (item, bookmarks);

    g_object_ref (item);
    g_hash_table_insert (bookmarks->pending_deletes, item, item);
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

static gboolean
midori_bookmarks_db_import_from_old_db (sqlite3*     db,
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

static void
midori_bookmarks_db_dbtracer (void*       dummy,
                              const char* query)
{
    g_printerr ("%s\n", query);
}

/**
 * midori_bookmarks_db_new:
 *
 * Initializes the bookmark data base.
 *
 * Returns: the main bookmarks array
 *
 * Since: 0.5.2
 **/
MidoriBookmarksDb*
midori_bookmarks_db_new (char** errmsg)
{
    sqlite3* db;
    gchar* oldfile;
    gchar* newfile;
    gboolean newfile_did_exist, oldfile_exists;
    const gchar* create_stmt;
    gchar* sql_errmsg = NULL;
    gchar* import_errmsg = NULL;
    KatzeArray* array;
    MidoriBookmarksDb* bookmarks;

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
        sqlite3_trace (db, midori_bookmarks_db_dbtracer, NULL);

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
        if (!midori_bookmarks_db_import_from_old_db (db, oldfile, &import_errmsg))
        {
            *errmsg = g_strdup_printf (_("Couldn't import from old database: %s\n"),
                import_errmsg ? import_errmsg : "(err = NULL)");
            g_free (import_errmsg);
        }

    init_success:
        g_free (newfile);
        g_free (oldfile);
        bookmarks = MIDORI_BOOKMARKS_DB (g_object_new (TYPE_MIDORI_BOOKMARKS_DB, NULL));
        bookmarks->db = db;

        g_object_set_data (G_OBJECT (bookmarks), "db", db);
        return bookmarks;

    init_failed:
        g_free (newfile);
        g_free (oldfile);

        if (db)
            sqlite3_close (db);

        return NULL;
}

/**
 * midori_bookmarks_db_on_quit:
 * @bookmarks: the main bookmark array
 *
 * Delete the main bookmark array.
 *
 * Since: 0.5.2
 **/
void
midori_bookmarks_db_on_quit (MidoriBookmarksDb* bookmarks)
{
    g_return_if_fail (IS_MIDORI_BOOKMARKS_DB (bookmarks));

    g_object_unref (bookmarks);
}

/**
 * midori_bookmarks_db_import_array:
 * @array: the main bookmark array
 * @array: #KatzeArray containing the items to import
 * @parentid: the id of folder
 *
 * Imports the items of @array as childs of the folder
 * identfied by @parentid.
 *
 * Since: 0.5.2
 **/
void
midori_bookmarks_db_import_array (MidoriBookmarksDb* bookmarks,
                               KatzeArray* array,
                               gint64      parentid)
{
    GList* list;
    KatzeItem* item;

    g_return_if_fail (IS_MIDORI_BOOKMARKS_DB (bookmarks));
    g_return_if_fail (KATZE_IS_ARRAY (array));

    KATZE_ARRAY_FOREACH_ITEM_L (item, array, list)
    {
        katze_item_set_meta_integer (item, "parentid", parentid);
        midori_bookmarks_db_add_item (bookmarks, item);
    }

    g_list_free (list);
}

/**
 * midori_bookmarks_db_force_idle:
 * @array: the main bookmark array
 *
 * Internal function that checks if idle processing is pending.
 * If it is the case, removes it from idle time processing and
 * executes it immediately.
 **/
static void
midori_bookmarks_db_force_idle (MidoriBookmarksDb* bookmarks)
{
    if (bookmarks->in_idle_func)
	return;

    if (g_idle_remove_by_data (bookmarks))
        midori_bookmarks_db_idle_func (bookmarks);
}

/**
 * midori_bookmarks_db_array_from_statement:
 * @stmt: the sqlite returned statement
 * @bookmarks: the database controller
 *
 * Internal function that populate a #KatzeArray by processing the @stmt
 * rows identifying:
 * a- if the item is already in memory
 *    in this case the item data is updated with retreived database content
 *    and the already existing item is populated in the returned #KatzeArray
 * b- if the data is a folder
 *    a new #KatzeArray item is populated in the returned  #KatzeArray and
 *    memorized for future use.
 * c- if the data is a bookmark
 *    a new #KatzeItem item is populated in the returned  #KatzeArray and
 *    memorized for furure use.
 *
 * Return value: the populated #KatzeArray
 **/
static KatzeArray*
midori_bookmarks_db_array_from_statement (sqlite3_stmt* stmt,
                                       MidoriBookmarksDb* bookmarks)
{
    KatzeArray *array;
    gint result;
    gint cols;

    array = katze_array_new (KATZE_TYPE_ITEM);
    cols = sqlite3_column_count (stmt);

    while ((result = sqlite3_step (stmt)) == SQLITE_ROW)
    {
        gint i;
        KatzeItem* item;
        KatzeItem* found;

        item = katze_item_new ();
        for (i = 0; i < cols; i++)
            katze_item_set_value_from_column (stmt, i, item);

        if (NULL != (found = g_hash_table_lookup (bookmarks->all_items, item)))
        {
            for (i = 0; i < cols; i++)
                katze_item_set_value_from_column (stmt, i, found);

            g_object_unref (item);

            item = found;
        }
        else if (KATZE_ITEM_IS_FOLDER (item))
        {
            g_object_unref (item);

            item = KATZE_ITEM (katze_array_new (KATZE_TYPE_ITEM));

            for (i = 0; i < cols; i++)
                katze_item_set_value_from_column (stmt, i, item);

            g_object_ref (item);
            g_hash_table_insert (bookmarks->all_items, item, item);
        }
        else
        {
            g_object_ref (item);
            g_hash_table_insert (bookmarks->all_items, item, item);
        }

        katze_array_add_item (array, item);
    }

    sqlite3_clear_bindings (stmt);
    sqlite3_reset (stmt);
    return array;
}

/**
 * midori_bookmarks_db_array_from_sqlite:
 * @array: the main bookmark array
 * @sqlcmd: the sqlcmd to execute
 *
 * Internal function that first forces pending idle processing to update the
 * database then process the requested @sqlcmd.
 *
 * Return value: a #KatzeArray on success, %NULL otherwise
 **/
static KatzeArray*
midori_bookmarks_db_array_from_sqlite (MidoriBookmarksDb* bookmarks,
                                    const gchar* sqlcmd)
{
    sqlite3_stmt* stmt;
    gint result;

    g_return_val_if_fail (bookmarks->db != NULL, NULL);

    midori_bookmarks_db_force_idle (bookmarks);

    result = sqlite3_prepare_v2 (bookmarks->db, sqlcmd, -1, &stmt, NULL);
    if (result != SQLITE_OK)
        return NULL;

    return midori_bookmarks_db_array_from_statement (stmt, bookmarks);
}

/**
 * midori_bookmarks_db_query_recursive:
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
 * Since: 0.5.2
 **/
KatzeArray*
midori_bookmarks_db_query_recursive (MidoriBookmarksDb*  bookmarks,
                                  const gchar* fields,
                                  const gchar* condition,
                                  const gchar* value,
                                  gboolean     recursive)
{
    gchar* sqlcmd;
    char* sqlcmd_value;
    KatzeArray* array;
    KatzeItem* item;
    GList* list;

    g_return_val_if_fail (IS_MIDORI_BOOKMARKS_DB (bookmarks), NULL);
    g_return_val_if_fail (fields, NULL);
    g_return_val_if_fail (condition, NULL);

    sqlcmd = g_strdup_printf ("SELECT %s FROM bookmarks WHERE %s "
                              "ORDER BY (uri='') ASC, title DESC", fields, condition);
    if (strstr (condition, "%q"))
    {
        sqlcmd_value = sqlite3_mprintf (sqlcmd, value ? value : "");
        array = midori_bookmarks_db_array_from_sqlite (bookmarks, sqlcmd_value);
        sqlite3_free (sqlcmd_value);
    }
    else
        array = midori_bookmarks_db_array_from_sqlite (bookmarks, sqlcmd);
    g_free (sqlcmd);

    if (!recursive)
        return array;

    KATZE_ARRAY_FOREACH_ITEM_L (item, array, list)
    {
        if (KATZE_ITEM_IS_FOLDER (item))
        {
            gchar* parentid = g_strdup_printf ("%" G_GINT64_FORMAT,
                                               katze_item_get_meta_integer (item, "id"));
            KatzeArray* subarray = midori_bookmarks_db_query_recursive (bookmarks,
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
midori_bookmarks_db_count_from_sqlite (sqlite3*     db,
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
midori_bookmarks_db_count_recursive_by_id (MidoriBookmarksDb*  bookmarks,
                                           const gchar*        condition,
                                           const gchar*        value,
                                           gint64              id,
                                           gboolean            recursive)
{
    gint64 count = -1;
    gchar* sqlcmd;
    char* sqlcmd_value;
    sqlite3_stmt* stmt;
    gint result;
    GList* ids;
    GList* iter_ids;

    g_return_val_if_fail (condition, -1);
    g_return_val_if_fail (MIDORI_BOOKMARKS_DB (bookmarks), -1);
    g_return_val_if_fail (bookmarks->db != NULL, -1);

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
        count = midori_bookmarks_db_count_from_sqlite (bookmarks->db, sqlcmd_value);
        sqlite3_free (sqlcmd_value);
    }
    else
        count = midori_bookmarks_db_count_from_sqlite (bookmarks->db, sqlcmd);

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

    if (sqlite3_prepare_v2 (bookmarks->db, sqlcmd_value, -1, &stmt, NULL) == SQLITE_OK)
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
        gint64 sub_count = midori_bookmarks_db_count_recursive_by_id (bookmarks,
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
 * midori_bookmarks_db_count_recursive:
 * @bookmarks: the main bookmark array
 * @condition: condition, like "folder = '%q'"
 * @value: a value to be inserted if @condition contains %q
 * @recursive: if %TRUE include children
 *
 * Return value: the number of elements on success, -1 otherwise
 *
 * Since: 0.5.2
 **/
gint64
midori_bookmarks_db_count_recursive (MidoriBookmarksDb*  bookmarks,
                                     const gchar*        condition,
                                     const gchar*        value,
                                     KatzeItem*          folder,
                                     gboolean            recursive)
{
    gint64 id = -1;

    g_return_val_if_fail (!folder || KATZE_ITEM_IS_FOLDER (folder), -1);

    id = folder ? katze_item_get_meta_integer (folder, "id") : 0;

    return midori_bookmarks_db_count_recursive_by_id (bookmarks, condition,
                                                      value, id,
                                                      recursive);
}
