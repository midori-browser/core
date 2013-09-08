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

KatzeArray*
midori_bookmarks_new (char** errmsg);

void
midori_bookmarks_on_quit (KatzeArray* array);

void
midori_array_update_item (KatzeArray* bookmarks, KatzeItem* item);

void
midori_bookmarks_import_array (KatzeArray* bookmarks,
                               KatzeArray* array,
                               gint64      parentid);

KatzeArray*
midori_array_query_recursive (KatzeArray*  bookmarks,
			      const gchar* fields,
			      const gchar* condition,
			      const gchar* value,
			      gboolean     recursive);

gint64
midori_array_count_recursive (KatzeArray*  bookmarks,
			      const gchar*        condition,
			      const gchar*        value,
			      KatzeItem*          folder,
			      gboolean            recursive);

gint64
midori_bookmarks_insert_item_db (sqlite3*   db,
                                 KatzeItem* item,
                                 gint64     parentid);

#endif /* !__MIDORI_BOOKMARKS_DB_H__ */

