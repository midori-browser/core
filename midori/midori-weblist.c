/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-weblist.h"

#include <glib/gi18n.h>
#include <string.h>

struct _MidoriWebList
{
    GObject parent_instance;

    GList* items;
};

G_DEFINE_TYPE (MidoriWebList, midori_web_list, G_TYPE_OBJECT)

enum {
    ADD_ITEM,
    REMOVE_ITEM,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
midori_web_list_finalize (GObject* object);

static void
midori_web_list_class_init (MidoriWebListClass* class)
{
    signals[ADD_ITEM] = g_signal_new (
        "add-item",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriWebListClass, add_item),
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        MIDORI_TYPE_WEB_ITEM);

    signals[REMOVE_ITEM] = g_signal_new (
        "remove-item",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriWebListClass, remove_item),
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        MIDORI_TYPE_WEB_ITEM);

    class->add_item = midori_web_list_add_item;
    class->remove_item = midori_web_list_remove_item;

    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_web_list_finalize;
}

static void
midori_web_list_init (MidoriWebList* web_list)
{
    web_list->items = NULL;
}

static void
midori_web_list_finalize (GObject* object)
{
    MidoriWebList* web_list = MIDORI_WEB_LIST (object);

    g_list_foreach (web_list->items, (GFunc)g_object_unref, NULL);
    g_list_free (web_list->items);

    G_OBJECT_CLASS (midori_web_list_parent_class)->finalize (object);
}

/**
 * midori_web_list_new:
 *
 * Creates a new #MidoriWebList.
 *
 * Return value: a new #MidoriWebList
 **/
MidoriWebList*
midori_web_list_new (void)
{
    MidoriWebList* web_list = g_object_new (MIDORI_TYPE_WEB_LIST,
                                            NULL);

    return web_list;
}

/**
 * midori_web_list_add_item:
 * @web_list: a #MidoriWebList
 * @web_item: a #MidoriWebItem
 *
 * Adds an item to the list.
 **/
void
midori_web_list_add_item (MidoriWebList* web_list,
                          MidoriWebItem* web_item)
{
    g_object_ref (web_item);
    web_list->items = g_list_append (web_list->items, web_item);
}

/**
 * midori_web_list_add_item:
 * @web_list: a #MidoriWebList
 * @web_item: a #MidoriWebItem
 *
 * Removes an item from the list.
 **/
void
midori_web_list_remove_item (MidoriWebList* web_list,
                             MidoriWebItem* web_item)
{
    web_list->items = g_list_remove (web_list->items, web_item);
}

/**
 * midori_web_list_get_nth_item:
 * @web_list: a #MidoriWebList
 * @n: an index in the list
 *
 * Retrieves the item in @web_list at the index @n.
 *
 * Return value: an item, or %NULL
 **/
MidoriWebItem*
midori_web_list_get_nth_item (MidoriWebList* web_list,
                              guint          n)
{
    g_return_val_if_fail (MIDORI_IS_WEB_LIST (web_list), 0);

    return g_list_nth_data (web_list->items, n);
}

/**
 * midori_web_list_find_token:
 * @web_list: a #MidoriWebList
 * @token: a token string
 *
 * Looks up an item in the list which has the specified token.
 *
 * Note that @token is by definition unique to one item.
 *
 * Return value: an item, or %NULL
 **/
MidoriWebItem*
midori_web_list_find_token (MidoriWebList* web_list,
                            const gchar*   token)
{
    guint n, i;
    MidoriWebItem* web_item;

    g_return_val_if_fail (MIDORI_IS_WEB_LIST (web_list), NULL);

    n = g_list_length (web_list->items);
    for (i = 0; i < n; i++)
    {
        web_item = (MidoriWebItem*)g_list_nth_data (web_list->items, i);
        if (!strcmp (midori_web_item_get_token (web_item), token))
            return web_item;
    }
    return NULL;
}

/**
 * midori_web_list_get_length:
 * @web_list: a #MidoriWebList
 *
 * Retrieves the number of items in @web_list.
 *
 * Return value: the length of the list
 **/
guint
midori_web_list_get_length (MidoriWebList* web_list)
{
    g_return_val_if_fail (MIDORI_IS_WEB_LIST (web_list), 0);

    return g_list_length (web_list->items);
}
