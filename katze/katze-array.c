/*
 Copyright (C) 2008-2011 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze-array.h"

#include "katze-utils.h"
#include "marshal.h"

#include <glib/gi18n.h>
#include <string.h>

/**
 * SECTION:katze-array
 * @short_description: A type aware item container
 * @see_also: #KatzeList
 *
 * #KatzeArray is a type aware container for items.
 */

struct _KatzeArray
{
    KatzeItem parent_instance;

    GType type;
    GList* items;
};

struct _KatzeArrayClass
{
    KatzeItemClass parent_class;

    /* Signals */
    void
    (*add_item)               (KatzeArray* array,
                               gpointer    item);
    void
    (*remove_item)            (KatzeArray* array,
                               gpointer    item);
    void
    (*move_item)              (KatzeArray* array,
                               gpointer    item,
                               gint        index);
    void
    (*clear)                  (KatzeArray* array);

    void
    (*update)                 (KatzeArray* array);
};

G_DEFINE_TYPE (KatzeArray, katze_array, KATZE_TYPE_ITEM);

enum {
    ADD_ITEM,
    REMOVE_ITEM,
    MOVE_ITEM,
    CLEAR,
    UPDATE,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

GList* kalistglobal;

static void
katze_array_finalize (GObject* object);

static void
_katze_array_update (KatzeArray* array)
{
    g_object_set_data (G_OBJECT (array), "last-update",
                       GINT_TO_POINTER (time (NULL)));
}

static void
_katze_array_add_item (KatzeArray* array,
                       gpointer    item)
{
    GType type = G_OBJECT_TYPE (item);
    g_object_ref (item);
    if (g_type_is_a (type, KATZE_TYPE_ITEM))
        katze_item_set_parent (item, array);

    array->items = g_list_append (array->items, item);
    _katze_array_update (array);
}

static void
_katze_array_remove_item (KatzeArray* array,
                          gpointer   item)
{
    array->items = g_list_remove (array->items, item);

    if (KATZE_IS_ITEM (item))
        katze_item_set_parent (item, NULL);
    g_object_unref (item);
    _katze_array_update (array);
}

static void
_katze_array_move_item (KatzeArray* array,
                        gpointer    item,
                        gint        position)
{
    array->items = g_list_remove (array->items, item);
    array->items = g_list_insert (array->items, item, position);
    _katze_array_update (array);
}

static void
_katze_array_clear (KatzeArray* array)
{
    GObject* item;

    while ((item = g_list_nth_data (array->items, 0)))
        g_signal_emit (array, signals[REMOVE_ITEM], 0, item);
    g_list_free (array->items);
    array->items = NULL;
    _katze_array_update (array);
}

static void
katze_array_class_init (KatzeArrayClass* class)
{
    GObjectClass* gobject_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = katze_array_finalize;

    signals[ADD_ITEM] = g_signal_new (
        "add-item",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (KatzeArrayClass, add_item),
        0,
        NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE, 1,
        G_TYPE_POINTER);

    signals[REMOVE_ITEM] = g_signal_new (
        "remove-item",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (KatzeArrayClass, remove_item),
        0,
        NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE, 1,
        G_TYPE_POINTER);

    /**
     * KatzeArray::move-item:
     * @array: the object on which the signal is emitted
     * @item: the item being moved
     * @position: the new position of the item
     *
     * An item is moved to a new position.
     *
     * Since: 0.1.6
     **/
    signals[MOVE_ITEM] = g_signal_new (
        "move-item",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (KatzeArrayClass, move_item),
        0,
        NULL,
        midori_cclosure_marshal_VOID__POINTER_INT,
        G_TYPE_NONE, 2,
        G_TYPE_POINTER,
        G_TYPE_INT);

    signals[CLEAR] = g_signal_new (
        "clear",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (KatzeArrayClass, clear),
        0,
        NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    /**
     * KatzeArray::update:
     * @array: the object on which the signal is emitted
     *
     * The array changed and any display widgets should
     * be updated.
     *
     * Since: 0.3.0
     **/
    signals[UPDATE] = g_signal_new (
        "update",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (KatzeArrayClass, update),
        0,
        NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = katze_array_finalize;

    class->add_item = _katze_array_add_item;
    class->remove_item = _katze_array_remove_item;
    class->move_item = _katze_array_move_item;
    class->clear = _katze_array_clear;
    class->update = _katze_array_update;
}

static void
katze_array_init (KatzeArray* array)
{
    array->type = G_TYPE_OBJECT;
    array->items = NULL;
}

static void
katze_array_finalize (GObject* object)
{
    KatzeArray* array = KATZE_ARRAY (object);
    GList* items;

    for (items = array->items; items; items = g_list_next (items))
        g_object_unref (items->data);
    g_list_free (array->items);

    G_OBJECT_CLASS (katze_array_parent_class)->finalize (object);
}

/**
 * katze_array_new:
 * @type: the expected item type
 *
 * Creates a new #KatzeArray for @type items.
 *
 * The array will keep a reference on each object until
 * it is removed from the array.
 *
 * Return value: a new #KatzeArray
 **/
KatzeArray*
katze_array_new (GType type)
{
    KatzeArray* array;

    g_return_val_if_fail (g_type_is_a (type, G_TYPE_OBJECT), NULL);

    array = g_object_new (KATZE_TYPE_ARRAY, NULL);
    array->type = type;

    return array;
}

/**
 * katze_array_is_a:
 * @array: a #KatzeArray
 * @is_a_type: type to compare with
 *
 * Checks whether the array is compatible
 * with items of the specified type.
 *
 * Retur value: %TRUE if @array is compatible with @is_a_type
 **/
gboolean
katze_array_is_a (KatzeArray* array,
                  GType       is_a_type)
{
    g_return_val_if_fail (KATZE_IS_ARRAY (array), FALSE);

    return g_type_is_a (array->type, is_a_type);
}

/**
 * katze_array_add_item:
 * @array: a #KatzeArray
 * @item: an item
 *
 * Adds an item to the array.
 *
 * If @item is a #KatzeItem its parent is set accordingly.
 **/
void
katze_array_add_item (KatzeArray* array,
                      gpointer    item)
{
    g_return_if_fail (KATZE_IS_ARRAY (array));

    g_signal_emit (array, signals[ADD_ITEM], 0, item);
}

/**
 * katze_array_remove_item:
 * @array: a #KatzeArray
 * @item: an item
 *
 * Removes an item from the array.
 *
 * If @item is a #KatzeItem its parent is unset accordingly.
 **/
void
katze_array_remove_item (KatzeArray* array,
                         gpointer    item)
{
    g_return_if_fail (KATZE_IS_ARRAY (array));

    g_signal_emit (array, signals[REMOVE_ITEM], 0, item);
}

/**
 * katze_array_get_nth_item:
 * @array: a #KatzeArray
 * @n: an index in the array
 *
 * Retrieves the item in @array at the position @n.
 *
 * Return value: an item, or %NULL
 **/
gpointer
katze_array_get_nth_item (KatzeArray* array,
                          guint       n)
{
    g_return_val_if_fail (KATZE_IS_ARRAY (array), NULL);

    return g_list_nth_data (array->items, n);
}

/**
 * katze_array_is_empty:
 * @array: a #KatzeArray
 *
 * Determines whether @array is empty.
 *
 * Return value: an item, or %NULL
 **/
gboolean
katze_array_is_empty (KatzeArray* array)
{
    g_return_val_if_fail (KATZE_IS_ARRAY (array), TRUE);

    return !g_list_nth_data (array->items, 0);
}

/**
 * katze_array_get_item_index:
 * @array: a #KatzeArray
 * @item: an item in the array
 *
 * Retrieves the index of the item in @array.
 *
 * Return value: an item, or -1
 **/
gint
katze_array_get_item_index (KatzeArray* array,
                            gpointer    item)
{
    g_return_val_if_fail (KATZE_IS_ARRAY (array), -1);

    return g_list_index (array->items, item);
}

/**
 * katze_array_find_token:
 * @array: a #KatzeArray
 * @token: a token string, or "token keywords" string
 *
 * Looks up an item in the array which has the specified token.
 *
 * This function will fail if the type of the list
 * is not based on #KatzeItem children.
 *
 * Note that @token is by definition unique to one item.
 *
 * Since 0.4.4 @token can be a "token keywords" string.
 *
 * Return value: an item, or %NULL
 **/
gpointer
katze_array_find_token (KatzeArray*  array,
                        const gchar* token)
{
    goffset token_length;
    GList* items;

    g_return_val_if_fail (KATZE_IS_ARRAY (array), NULL);
    g_return_val_if_fail (katze_array_is_a (array, KATZE_TYPE_ITEM), NULL);
    g_return_val_if_fail (token != NULL, NULL);

    token_length = strchr (token, ' ') - token;
    if (token_length < 1)
        token_length = strlen (token);

    for (items = array->items; items; items = g_list_next (items))
    {
        const gchar* found_token = ((KatzeItem*)items->data)->token;
        if (found_token != NULL)
        {
            guint bigger_item = strlen (found_token) > token_length ? strlen (found_token) : token_length;

            if (strncmp (token, found_token, bigger_item) == 0)
                return items->data;
        }
    }
    return NULL;
}

/**
 * katze_array_find_uri:
 * @array: a #KatzeArray
 * @uri: an URI
 *
 * Looks up an item in the array which has the specified URI.
 *
 * This function will fail if the type of the list
 * is not based on #KatzeItem children.
 *
 * Return value: an item, or %NULL
 *
 * Since: 0.2.0
 **/
gpointer
katze_array_find_uri (KatzeArray*  array,
                      const gchar* uri)
{
    GList* items;

    g_return_val_if_fail (KATZE_IS_ARRAY (array), NULL);
    g_return_val_if_fail (katze_array_is_a (array, KATZE_TYPE_ITEM), NULL);
    g_return_val_if_fail (uri != NULL, NULL);

    for (items = array->items; items; items = g_list_next (items))
    {
        const gchar* found_uri = ((KatzeItem*)items->data)->uri;
        if (found_uri != NULL && !strcmp (found_uri, uri))
            return items->data;
    }
    return NULL;
}

/**
 * katze_array_get_length:
 * @array: a #KatzeArray
 *
 * Retrieves the number of items in @array.
 *
 * Return value: the length of the list
 **/
guint
katze_array_get_length (KatzeArray* array)
{
    g_return_val_if_fail (KATZE_IS_ARRAY (array), 0);

    return g_list_length (array->items);
}

/**
 * katze_array_move_item:
 * @array: a #KatzeArray
 * @item: the item being moved
 * @position: the new position of the item
 *
 * Moves @item to the position @position.
 *
 * Since: 0.1.6
 **/
void
katze_array_move_item (KatzeArray* array,
                       gpointer    item,
                       gint        position)
{
    g_return_if_fail (KATZE_IS_ARRAY (array));

    g_signal_emit (array, signals[MOVE_ITEM], 0, item, position);
}

/**
 * katze_array_get_items:
 * @array: a #KatzeArray
 *
 * Retrieves the items as a list.
 *
 * Return value: a newly allocated #GList of items
 *
 * Since: 0.2.5
 **/
GList*
katze_array_get_items (KatzeArray* array)
{
    g_return_val_if_fail (KATZE_IS_ARRAY (array), NULL);

    return g_list_copy (array->items);
}

GList*
katze_array_peek_items (KatzeArray* array)
{
    g_return_val_if_fail (KATZE_IS_ARRAY (array), NULL);

    return array->items;
}

/**
 * katze_array_clear:
 * @array: a #KatzeArray
 *
 * Deletes all items currently contained in @array.
 **/
void
katze_array_clear (KatzeArray* array)
{
    g_return_if_fail (KATZE_IS_ARRAY (array));

    g_signal_emit (array, signals[CLEAR], 0);
}

/**
 * katze_array_update:
 * @array: a #KatzeArray
 *
 * Indicates that the array changed and any display
 * widgets should be updated.
 *
 * Since: 0.3.0
 **/
void
katze_array_update (KatzeArray* array)
{
    g_return_if_fail (KATZE_IS_ARRAY (array));

    g_signal_emit (array, signals[UPDATE], 0);
}
