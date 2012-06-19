/*
 Copyright (C) 2010 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2010 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <sqlite3.h>
#include <katze/katze.h>

void
midori_bookmarks_add_item_cb (KatzeArray* array,
                              KatzeItem*  item,
                              sqlite3*    db);

void
midori_bookmarks_remove_item_cb (KatzeArray* array,
                                 KatzeItem*  item,
                                 sqlite3*    db);

sqlite3*
midori_bookmarks_initialize (KatzeArray*  array,
                             char**       errmsg);

void
midori_bookmarks_import (const gchar* filename,
                         sqlite3*     db);
