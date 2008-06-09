/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-trash.h"

#include "sokoke.h"
#include <glib/gi18n.h>

G_DEFINE_TYPE (MidoriTrash, midori_trash, G_TYPE_OBJECT)

struct _MidoriTrashPrivate
{
    guint limit;
    KatzeXbelItem* xbel_folder;
};

#define MIDORI_TRASH_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
     MIDORI_TYPE_TRASH, MidoriTrashPrivate))

enum
{
    PROP_0,

    PROP_LIMIT
};

enum {
    INSERTED,
    REMOVED,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
midori_trash_finalize (GObject* object);

static void
midori_trash_set_property (GObject*      object,
                           guint         prop_id,
                           const GValue* value,
                           GParamSpec*   pspec);

static void
midori_trash_get_property (GObject*    object,
                           guint       prop_id,
                           GValue*     value,
                           GParamSpec* pspec);

static void
midori_trash_class_init (MidoriTrashClass* class)
{
    signals[INSERTED] = g_signal_new (
        "inserted",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriTrashClass, inserted),
        0,
        NULL,
        g_cclosure_marshal_VOID__UINT,
        G_TYPE_NONE, 1,
        G_TYPE_UINT);

    signals[REMOVED] = g_signal_new (
        "removed",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriTrashClass, removed),
        0,
        NULL,
        g_cclosure_marshal_VOID__UINT,
        G_TYPE_NONE, 1,
        G_TYPE_UINT);

    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_trash_finalize;
    gobject_class->set_property = midori_trash_set_property;
    gobject_class->get_property = midori_trash_get_property;

    GParamFlags flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    g_object_class_install_property (gobject_class,
                                     PROP_LIMIT,
                                     g_param_spec_uint (
                                     "limit",
                                     _("Limit"),
                                     _("The maximum number of items"),
                                     0, G_MAXUINT, 10,
                                     flags));

    g_type_class_add_private (class, sizeof (MidoriTrashPrivate));
}



static void
midori_trash_init (MidoriTrash* trash)
{
    trash->priv = MIDORI_TRASH_GET_PRIVATE (trash);

    MidoriTrashPrivate* priv = trash->priv;

    priv->xbel_folder = katze_xbel_folder_new ();
}

static void
midori_trash_finalize (GObject* object)
{
    MidoriTrash* trash = MIDORI_TRASH (object);
    MidoriTrashPrivate* priv = trash->priv;

    katze_xbel_item_unref (priv->xbel_folder);

    G_OBJECT_CLASS (midori_trash_parent_class)->finalize (object);
}

static void
midori_trash_set_property (GObject*      object,
                           guint         prop_id,
                           const GValue* value,
                           GParamSpec*   pspec)
{
    MidoriTrash* trash = MIDORI_TRASH (object);
    MidoriTrashPrivate* priv = trash->priv;

    switch (prop_id)
    {
    case PROP_LIMIT:
        priv->limit = g_value_get_uint (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_trash_get_property (GObject*    object,
                           guint       prop_id,
                           GValue*     value,
                           GParamSpec* pspec)
{
    MidoriTrash* trash = MIDORI_TRASH (object);
    MidoriTrashPrivate* priv = trash->priv;

    switch (prop_id)
    {
    case PROP_LIMIT:
        g_value_set_uint (value, priv->limit);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * midori_trash_new:
 * @limit: the maximum number of items
 *
 * Creates a new #MidoriTrash that can contain a specified number of items,
 * meaning that each additional item will replace the oldest existing item.
 *
 * The value 0 for @limit actually means that there is no limit.
 *
 * You will typically want to assign this to a #MidoriBrowser.
 *
 * Return value: a new #MidoriTrash
 **/
MidoriTrash*
midori_trash_new (guint limit)
{
    MidoriTrash* trash = g_object_new (MIDORI_TYPE_TRASH,
                                       "limit", limit,
                                       NULL);

    return trash;
}

/**
 * midori_trash_is_empty:
 * @trash: a #MidoriTrash
 *
 * Determines whether the @trash contains no items.
 *
 * Return value: %TRUE if there are no items, %FALSE otherwise
 **/
gboolean
midori_trash_is_empty (MidoriTrash* trash)
{
    g_return_val_if_fail (MIDORI_IS_TRASH (trash), FALSE);

    MidoriTrashPrivate* priv = trash->priv;

    return katze_xbel_folder_is_empty (priv->xbel_folder);
}

/**
 * midori_trash_get_n_items:
 * @trash: a #MidoriTrash
 *
 * Determines the number of items in @trash.
 *
 * Return value: the current number of items
 **/
guint
midori_trash_get_n_items (MidoriTrash* trash)
{
    g_return_val_if_fail (MIDORI_IS_TRASH (trash), 0);

    MidoriTrashPrivate* priv = trash->priv;

    return katze_xbel_folder_get_n_items (priv->xbel_folder);
}

/**
 * midori_trash_get_nth_xbel_item:
 * @trash: a #MidoriTrash
 * @n: the index of an item
 *
 * Retrieve an item contained in @trash by its index.
 *
 * Note that you mustn't unref this item.
 *
 * Return value: the index at the given index or %NULL
 **/
KatzeXbelItem*
midori_trash_get_nth_xbel_item (MidoriTrash* trash,
                                guint        n)
{
    g_return_val_if_fail (MIDORI_IS_TRASH (trash), 0);

    MidoriTrashPrivate* priv = trash->priv;

    return katze_xbel_folder_get_nth_item (priv->xbel_folder, n);
}

/**
 * midori_trash_prepend_xbel_item:
 * @trash: a #MidoriTrash
 * @xbel_item: a #KatzeXbelItem
 *
 * Prepends a #KatzeXbelItem to @trash.
 *
 * The item is copied. If there is a limit set, the oldest item is
 * removed automatically.
 *
 * Return value: %TRUE if there are no items, %FALSE otherwise
 **/
void
midori_trash_prepend_xbel_item (MidoriTrash*   trash,
                                KatzeXbelItem* xbel_item)
{
    g_return_if_fail (MIDORI_IS_TRASH (trash));

    MidoriTrashPrivate* priv = trash->priv;

    KatzeXbelItem* copy = katze_xbel_item_copy (xbel_item);
    katze_xbel_folder_prepend_item (priv->xbel_folder, copy);
    g_signal_emit (trash, signals[INSERTED], 0, 0);
    guint n = katze_xbel_folder_get_n_items (priv->xbel_folder);
    if (n > 10)
    {
        KatzeXbelItem* item = katze_xbel_folder_get_nth_item (priv->xbel_folder,
                                                              n - 1);
        g_signal_emit (trash, signals[REMOVED], 0, n - 1);
        katze_xbel_item_unref (item);
    }
}

/**
 * midori_trash_remove_nth_item:
 * @trash: a #MidoriTrash
 * @n: the index of an item
 *
 * Removes the item at the specified position from @trash.
 *
 * Nothing happens if the function fails.
 **/
void
midori_trash_remove_nth_item (MidoriTrash* trash,
                              guint        n)
{
    g_return_if_fail (MIDORI_IS_TRASH (trash));

    MidoriTrashPrivate* priv = trash->priv;

    KatzeXbelItem* item = katze_xbel_folder_get_nth_item (priv->xbel_folder, n);
    if (!n)
        return;
    katze_xbel_folder_remove_item (priv->xbel_folder, item);
    g_signal_emit (trash, signals[REMOVED], 0, n);
    katze_xbel_item_unref (item);
}

/**
 * midori_trash_empty:
 * @trash: a #MidoriTrash
 *
 * Deletes all items currently contained in @trash.
 **/
void
midori_trash_empty (MidoriTrash* trash)
{
    g_return_if_fail (MIDORI_IS_TRASH (trash));

    MidoriTrashPrivate* priv = trash->priv;

    guint n = katze_xbel_folder_get_n_items (priv->xbel_folder);
    guint i;
    for (i = 0; i < n; i++)
    {
        KatzeXbelItem* item = katze_xbel_folder_get_nth_item (priv->xbel_folder,
                                                              i);
        g_signal_emit (trash, signals[REMOVED], 0, i);
        katze_xbel_item_unref (item);
    }
}
