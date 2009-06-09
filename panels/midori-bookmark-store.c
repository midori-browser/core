/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-bookmark-store.h"

struct _MidoriBookmarkStore
{
    GtkTreeStore parent_instance;
};

struct _MidoriBookmarkStoreClass
{
    GtkTreeStoreClass parent_class;
};

static void
midori_bookmark_store_drag_source_iface_init (GtkTreeDragSourceIface* iface);

static void
midori_bookmark_store_drag_dest_iface_init (GtkTreeDragDestIface* iface);

G_DEFINE_TYPE_WITH_CODE (MidoriBookmarkStore, midori_bookmark_store, GTK_TYPE_TREE_STORE,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
                                                midori_bookmark_store_drag_source_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST,
                                                midori_bookmark_store_drag_dest_iface_init));

static void
midori_bookmark_store_finalize (GObject* object);

static void
midori_bookmark_store_class_init (MidoriBookmarkStoreClass* class)
{
    GObjectClass* gobject_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_bookmark_store_finalize;
}

static void
midori_bookmark_store_init (MidoriBookmarkStore* bookmark_store)
{
    /* Nothing to do */
}

static void
midori_bookmark_store_finalize (GObject* object)
{
    MidoriBookmarkStore* bookmark_store = MIDORI_BOOKMARK_STORE (object);

    /* Nothing to do */
}

static void
midori_bookmark_store_drag_source_iface_init (GtkTreeDragSourceIface* iface)
{
  /*iface->row_draggable = real_gtk_tree_store_row_draggable;
  iface->drag_data_delete = gtk_tree_store_drag_data_delete;
  iface->drag_data_get = gtk_tree_store_drag_data_get;*/
}

static void
midori_bookmark_store_drag_dest_iface_init (GtkTreeDragDestIface* iface)
{
  /*iface->drag_data_received = gtk_tree_store_drag_data_received;
  iface->row_drop_possible = gtk_tree_store_row_drop_possible;*/
}

/**
 * midori_bookmark_store_new:
 *
 * Creates a new empty bookmark_store.
 *
 * Return value: a new #MidoriBookmarkStore
 *
 * Since: 0.1.8
 **/
GtkTreeStore*
midori_bookmark_store_new (gint n_columns,
                           ...)
{
    GtkTreeStore* treestore;
    va_list args;
    gint i;
    GType* types;

    g_return_val_if_fail (n_columns > 0, NULL);

    treestore = g_object_new (GTK_TYPE_TREE_STORE, NULL);

    va_start (args, n_columns);

    types = g_new (gint, n_columns);
    for (i = 0; i < n_columns; i++)
    {
        GType type = va_arg (args, GType);
        types[i] = type;
    }
    va_end (args);

    gtk_tree_store_set_column_types (treestore, i, types);

    return treestore;
}
