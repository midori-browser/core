/*
 Copyright (C) 2010 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2010 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_BOOKMARKS_DB_H__
#define __MIDORI_BOOKMARKS_DB_H__ 1

#include <sqlite3.h>
#include <katze/katze.h>

G_BEGIN_DECLS

#define TYPE_MIDORI_BOOKMARKS_DB \
    (midori_bookmarks_db_get_type ())
#define MIDORI_BOOKMARKS_DB(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_MIDORI_BOOKMARKS_DB, MidoriBookmarksDb))
#define MIDORI_BOOKMARKS_DB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_MIDORI_BOOKMARKS_DB, MidoriBookmarksDbClass))
#define IS_MIDORI_BOOKMARKS_DB(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_MIDORI_BOOKMARKS_DB))
#define IS_MIDORI_BOOKMARKS_DB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_MIDORI_BOOKMARKS_DB))
#define MIDORI_BOOKMARKS_DB_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_MIDORI_BOOKMARKS_DB, MidoriBookmarksDbClass))

typedef struct _MidoriBookmarksDb                       MidoriBookmarksDb;
typedef struct _MidoriBookmarksDbClass                  MidoriBookmarksDbClass;

GType
midori_bookmarks_db_get_type               (void) G_GNUC_CONST;

MidoriBookmarksDb*
midori_bookmarks_db_new (char** errmsg);

void
midori_bookmarks_db_on_quit (MidoriBookmarksDb* bookmarks);

void
midori_bookmarks_db_add_item (MidoriBookmarksDb* bookmarks, KatzeItem* item);

void
midori_bookmarks_db_update_item (MidoriBookmarksDb* bookmarks, KatzeItem* item);

void
midori_bookmarks_db_remove_item (MidoriBookmarksDb* bookmarks, KatzeItem* item);

void
midori_bookmarks_db_import_array (MidoriBookmarksDb* bookmarks,
                                  KatzeArray* array,
                                  gint64      parentid);

KatzeArray*
midori_bookmarks_db_query_recursive (MidoriBookmarksDb*  bookmarks,
                                     const gchar* fields,
                                     const gchar* condition,
                                     const gchar* value,
                                     gboolean     recursive);

gint64
midori_bookmarks_db_count_recursive (MidoriBookmarksDb*  bookmarks,
				     const gchar*        condition,
                                     const gchar*        value,
				     KatzeItem*          folder,
				     gboolean            recursive);

void
midori_bookmarks_db_populate_folder (MidoriBookmarksDb* bookmarks,
    KatzeArray *folder);

#endif /* !__MIDORI_BOOKMARKS_DB_H__ */

