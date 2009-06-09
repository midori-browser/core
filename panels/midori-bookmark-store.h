/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_BOOKMARK_STORE_H__
#define __MIDORI_BOOKMARK_STORE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_BOOKMARK_STORE \
    (midori_bookmark_store_get_type ())
#define MIDORI_BOOKMARK_STORE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_BOOKMARK_STORE, MidoriBookmarkStore))
#define MIDORI_BOOKMARK_STORE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_BOOKMARK_STORE, MidoriBookmarkStoreClass))
#define MIDORI_IS_BOOKMARK_STORE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_BOOKMARK_STORE))
#define MIDORI_IS_BOOKMARK_STORE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_BOOKMARK_STORE))
#define MIDORI_BOOKMARK_STORE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_BOOKMARK_STORE, MidoriBookmarkStoreClass))

typedef struct _MidoriBookmarkStore                MidoriBookmarkStore;
typedef struct _MidoriBookmarkStoreClass           MidoriBookmarkStoreClass;

GType
midori_bookmark_store_get_type               (void);

GtkTreeStore*
midori_bookmark_store_new                    (gint n_columns,
                                              ...);

G_END_DECLS

#endif /* __MIDORI_BOOKMARK_STORE_H__ */
