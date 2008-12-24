/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze-array.h"

#include "katze-utils.h"

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
    KatzeList parent_instance;

    GType type;
};

struct _KatzeArrayClass
{
    KatzeListClass parent_class;
};

G_DEFINE_TYPE (KatzeArray, katze_array, KATZE_TYPE_LIST)

static void
katze_array_finalize (GObject* object);

static void
_katze_array_add_item (KatzeList* list,
                       gpointer   item)
{
    if (katze_array_is_a ((KatzeArray*)list, G_TYPE_OBJECT))
    {
        GType type = G_OBJECT_TYPE (item);

        g_return_if_fail (katze_array_is_a ((KatzeArray*)list, type));
        g_object_ref (item);
        if (g_type_is_a (type, KATZE_TYPE_ITEM))
            katze_item_set_parent (item, list);
    }
    KATZE_LIST_CLASS (katze_array_parent_class)->add_item (list, item);
}

static void
_katze_array_remove_item (KatzeList* list,
                          gpointer   item)
{
    KATZE_LIST_CLASS (katze_array_parent_class)->remove_item (list, item);
    if (katze_array_is_a ((KatzeArray*)list, G_TYPE_OBJECT))
    {
        if (KATZE_IS_ITEM (item))
            katze_item_set_parent (item, NULL);
        g_object_unref (item);
    }
}

static void
katze_array_class_init (KatzeArrayClass* class)
{
    GObjectClass* gobject_class;
    KatzeListClass* katzelist_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = katze_array_finalize;

    katzelist_class = KATZE_LIST_CLASS (class);
    katzelist_class->add_item = _katze_array_add_item;
    katzelist_class->remove_item = _katze_array_remove_item;
}

static void
katze_array_init (KatzeArray* array)
{
    array->type = G_TYPE_NONE;
}

static void
katze_array_finalize (GObject* object)
{
    KatzeArray* array;
    guint n, i;

    array = KATZE_ARRAY (object);
    if (katze_array_is_a (array, G_TYPE_OBJECT))
    {
        n = katze_list_get_length ((KatzeList*)array);
        for (i = 0; i < n; i++)
            g_object_unref (katze_list_get_nth_item ((KatzeList*)array, i));
    }

    G_OBJECT_CLASS (katze_array_parent_class)->finalize (object);
}

/**
 * katze_array_new:
 * @type: the expected item type
 *
 * Creates a new #KatzeArray for @type items.
 *
 * You may only add items of the given type or inherited
 * from it to this array *if* @type is an #GObject type.
 * The array will keep a reference on each object until
 * it is removed from the array.
 *
 * If @type is *not* a #GObject type, #KatzeArray behaves
 * pretty much like #KatzeList.
 *
 * Note: Since 0.1.2 you may use #KatzeList accessors to
 * work with #KatzeArray if you want to.
 *
 * Return value: a new #KatzeArray
 **/
KatzeArray*
katze_array_new (GType type)
{
    KatzeArray* array = g_object_new (KATZE_TYPE_ARRAY, NULL);
    array->type = type;

    return array;
}

/**
 *
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
 * @item: a #GObject
 *
 * Adds an item to the array.
 *
 * If @item is a #KatzeItem its parent is set accordingly.
 **/
void
katze_array_add_item (KatzeArray* array,
                      gpointer    item)
{
    /* g_return_if_fail (KATZE_IS_ARRAY (array)); */

    katze_list_add_item (KATZE_LIST (array), item);
}

/**
 * katze_array_remove_item:
 * @array: a #KatzeArray
 * @item: a #GObject
 *
 * Removes an item from the array.
 *
 * If @item is a #KatzeItem its parent is unset accordingly.
 **/
void
katze_array_remove_item (KatzeArray* array,
                         gpointer    item)
{
    /* g_return_if_fail (KATZE_IS_ARRAY (array)); */

    katze_list_remove_item (KATZE_LIST (array), item);
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
    /* g_return_val_if_fail (KATZE_IS_ARRAY (array), NULL); */

    return katze_list_get_nth_item (KATZE_LIST (array), n);
}

/**
 * katze_array_is_empty:
 * @array: a #KatzeArray
 *
 * Determines if @array is empty.
 *
 * Return value: an item, or %NULL
 **/
gboolean
katze_array_is_empty (KatzeArray* array)
{
    /* g_return_val_if_fail (KATZE_IS_ARRAY (array), TRUE); */

    return katze_list_is_empty (KATZE_LIST (array));
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
    /* g_return_val_if_fail (KATZE_IS_ARRAY (array), -1); */

    return katze_list_get_item_index (KATZE_LIST (array), item);
}

/**
 * katze_array_find_token:
 * @array: a #KatzeArray
 * @token: a token string
 *
 * Looks up an item in the array which has the specified token.
 *
 * This function will silently fail if the type of the list
 * is not based on #GObject and only #KatzeItem children
 * are checked for their token, any other objects are skipped.
 *
 * Note that @token is by definition unique to one item.
 *
 * Return value: an item, or %NULL
 **/
gpointer
katze_array_find_token (KatzeArray*  array,
                        const gchar* token)
{
    guint n, i;
    gpointer item;
    const gchar* found_token;

    g_return_val_if_fail (KATZE_IS_ARRAY (array), NULL);

    if (!katze_array_is_a (array, G_TYPE_OBJECT))
        return NULL;

    n = katze_list_get_length ((KatzeList*)array);
    for (i = 0; i < n; i++)
    {
        item = katze_list_get_nth_item ((KatzeList*)array, i);
        if (!g_type_is_a (G_OBJECT_TYPE (item), KATZE_TYPE_ITEM))
            continue;
        found_token = katze_item_get_token ((KatzeItem*)item);
        if (found_token && !strcmp (found_token, token))
            return item;
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
    /* g_return_val_if_fail (KATZE_IS_ARRAY (array), 0); */

    return katze_list_get_length (KATZE_LIST (array));
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
    /* g_return_if_fail (KATZE_IS_ARRAY (array)); */

    katze_list_clear (KATZE_LIST (array));
}
