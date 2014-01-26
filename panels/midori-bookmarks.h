/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_BOOKMARKS_PANEL_H__
#define __MIDORI_BOOKMARKS_PANEL_H__

#include <sqlite3.h>
#include <gtk/gtk.h>
#include <katze/katze.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_BOOKMARKS \
    (midori_bookmarks_get_type ())
#define MIDORI_BOOKMARKS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_BOOKMARKS, MidoriBookmarks))
#define MIDORI_BOOKMARKS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_BOOKMARKS, MidoriBookmarksClass))
#define MIDORI_IS_BOOKMARKS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_BOOKMARKS))
#define MIDORI_IS_BOOKMARKS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_BOOKMARKS))
#define MIDORI_BOOKMARKS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_BOOKMARKS, MidoriBookmarksClass))

typedef struct _MidoriBookmarks                MidoriBookmarks;
typedef struct _MidoriBookmarksClass           MidoriBookmarksClass;

GType
midori_bookmarks_get_type               (void);

G_END_DECLS

#endif /* __MIDORI_BOOKMARKS_PANEL_H__ */
