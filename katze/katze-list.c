/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze-list.h"

#include "katze-utils.h"

#include <glib/gi18n.h>
#include <string.h>

/**
 * SECTION:katze-list
 * @short_description: A verbose and versatile item container
 * @see_also: #KatzeItem
 *
 * #KatzeList is a verbose and versatile container for items.
 */

G_DEFINE_TYPE (KatzeList, katze_list, KATZE_TYPE_ITEM)

enum {
    ADD_ITEM,
    REMOVE_ITEM,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
katze_list_finalize (GObject* object);

static void
_katze_list_add_item (KatzeList* list,
                      gpointer   item)
{
    list->items = g_list_append (list->items, item);
}

static void
_katze_list_remove_item (KatzeList* list,
                         gpointer   item)
{
    list->items = g_list_remove (list->items, item);
}

static void
katze_list_class_init (KatzeListClass* class)
{
    GObjectClass* gobject_class;

    signals[ADD_ITEM] = g_signal_new (
        "add-item",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (KatzeListClass, add_item),
        0,
        NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE, 1,
        G_TYPE_POINTER);

    signals[REMOVE_ITEM] = g_signal_new (
        "remove-item",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (KatzeListClass, remove_item),
        0,
        NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE, 1,
        G_TYPE_POINTER);

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = katze_list_finalize;

    class->add_item = _katze_list_add_item;
    class->remove_item = _katze_list_remove_item;
}

static void
katze_list_init (KatzeList* list)
{
    list->items = NULL;
}

static void
katze_list_finalize (GObject* object)
{
    KatzeList* list;

    list = KATZE_LIST (object);
    g_list_free (list->items);

    G_OBJECT_CLASS (katze_list_parent_class)->finalize (object);
}

/**
 * katze_list_new:
 *
 * Creates a new #KatzeList.
 *
 * Return value: a new #KatzeList
 **/
KatzeList*
katze_list_new (void)
{
    KatzeList* list = g_object_new (KATZE_TYPE_LIST, NULL);

    return list;
}

/**
 * katze_list_add_item:
 * @list: a #KatzeList
 * @item: a #GObject
 *
 * Adds an item to the list.
 **/
void
katze_list_add_item (KatzeList* list,
                     gpointer   item)
{
    g_return_if_fail (KATZE_IS_LIST (list));

    g_signal_emit (list, signals[ADD_ITEM], 0, item);
}

/**
 * katze_list_add_item:
 * @list: a #KatzeList
 * @item: a #GObject
 *
 * Removes an item from the list.
 **/
void
katze_list_remove_item (KatzeList* list,
                        gpointer   item)
{
    g_return_if_fail (KATZE_IS_LIST (list));

    g_signal_emit (list, signals[REMOVE_ITEM], 0, item);
}

/**
 * katze_list_get_nth_item:
 * @list: a #KatzeList
 * @n: an index in the list
 *
 * Retrieves the item in @list at the position @n.
 *
 * Return value: an item, or %NULL
 **/
gpointer
katze_list_get_nth_item (KatzeList* list,
                         guint      n)
{
    g_return_val_if_fail (KATZE_IS_LIST (list), NULL);

    return g_list_nth_data (list->items, n);
}

/**
 * katze_list_is_empty:
 * @list: a #KatzeList
 *
 * Determines if @list is empty.
 *
 * Return value: an item, or %NULL
 **/
gboolean
katze_list_is_empty (KatzeList* list)
{
    g_return_val_if_fail (KATZE_IS_LIST (list), TRUE);

    return !g_list_nth_data (list->items, 0);
}

/**
 * katze_list_get_item_position:
 * @list: a #KatzeList
 * @item: an item in the list
 *
 * Retrieves the index of the item in @list.
 *
 * Return value: an item, or -1
 **/
gint
katze_list_get_item_index (KatzeList* list,
                           gpointer   item)
{
    g_return_val_if_fail (KATZE_IS_LIST (list), -1);

    return g_list_index (list->items, item);
}

/**
 * katze_list_get_length:
 * @list: a #KatzeList
 *
 * Retrieves the number of items in @list.
 *
 * Return value: the length of the list
 **/
guint
katze_list_get_length (KatzeList* list)
{
    g_return_val_if_fail (KATZE_IS_LIST (list), 0);

    return g_list_length (list->items);
}

/**
 * katze_list_clear:
 * @list: a #KatzeList
 *
 * Deletes all items currently contained in @list.
 **/
void
katze_list_clear (KatzeList* list)
{
    guint n;
    guint i;
    GObject* item;

    g_return_if_fail (KATZE_IS_LIST (list));

    n = g_list_length (list->items);
    for (i = 0; i < n; i++)
    {
        if ((item = g_list_nth_data (list->items, i)))
            katze_list_remove_item (list, item);
    }
    g_list_free (list->items);
    list->items = NULL;
}
