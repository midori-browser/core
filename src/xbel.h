/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __XBEL_H__
#define __XBEL_H__ 1

#include <glib.h>
#include <glib-object.h>

#define XBEL_ERROR g_quark_from_string("XBEL_ERROR")

typedef enum
{
    XBEL_ERROR_INVALID_URI,        /* Malformed uri */
    XBEL_ERROR_INVALID_VALUE,      /* Requested field not found */
    XBEL_ERROR_URI_NOT_FOUND,      /* Requested uri not found */
    XBEL_ERROR_READ,               /* Malformed document */
    XBEL_ERROR_UNKNOWN_ENCODING,   /* Parsed text was in an unknown encoding */
    XBEL_ERROR_WRITE,              /* Writing failed. */
} XBELError;

typedef enum
{
    XBEL_ITEM_FOLDER,
    XBEL_ITEM_BOOKMARK,
    XBEL_ITEM_SEPARATOR
} XbelItemKind;

// Note: This structure is entirely private.
typedef struct
{
    XbelItemKind kind;
    struct XbelItem* parent;

    GList* items; // folder
    gboolean folded; // foolder
    gchar* title;    // !separator
    gchar* desc;     // folder and bookmark
    gchar* href;     // bookmark
    //time_t added;    // !separator
    //time_t modfied;    // bookmark
    //time_t visited;    // bookmark
} XbelItem;

XbelItem*
xbel_bookmark_new(void);

XbelItem*
xbel_separator_new(void);

XbelItem*
xbel_folder_new(void);

void
xbel_item_free(XbelItem*);

XbelItem*
xbel_item_copy(XbelItem*);

GType
xbel_item_get_type();

#define G_TYPE_XBEL_ITEM xbel_item_get_type()

void
xbel_folder_append_item(XbelItem*, XbelItem*);

void
xbel_folder_prepend_item(XbelItem*, XbelItem*);

void
xbel_folder_remove_item(XbelItem*, XbelItem*);

guint
xbel_folder_get_n_items(XbelItem*);

XbelItem*
xbel_folder_get_nth_item(XbelItem*, guint);

gboolean
xbel_folder_is_empty(XbelItem*);

gboolean
xbel_folder_get_folded(XbelItem*);

XbelItemKind
xbel_item_get_kind(XbelItem*);

XbelItem*
xbel_item_get_parent(XbelItem*);

G_CONST_RETURN gchar*
xbel_item_get_title(XbelItem*);

G_CONST_RETURN gchar*
xbel_item_get_desc(XbelItem*);

G_CONST_RETURN gchar*
xbel_bookmark_get_href(XbelItem*);

/*time_t
xbel_bookmark_get_added(XbelItem*);

time_t
xbel_bookmark_get_modified(XbelItem*);

time_t
xbel_bookmark_get_visited(XbelItem*);*/

gboolean
xbel_item_is_bookmark(XbelItem*);

gboolean
xbel_item_is_separator(XbelItem*);

gboolean
xbel_item_is_folder(XbelItem*);

void
xbel_folder_set_folded(XbelItem*, gboolean);

void
xbel_item_set_title(XbelItem*, const gchar*);

void
xbel_item_set_desc(XbelItem*, const gchar*);

void
xbel_bookmark_set_href(XbelItem*, const gchar*);

gboolean
xbel_folder_from_data(XbelItem*, const gchar*, GError**);

gboolean
xbel_folder_from_file(XbelItem*, const gchar*, GError**);

gboolean
xbel_folder_from_data_dirs(XbelItem*, const gchar*, gchar**, GError**);

gchar*
xbel_folder_to_data(XbelItem*, gsize*, GError**);

gboolean
xbel_folder_to_file(XbelItem*, const gchar*, GError**);

#endif /* !__XBEL_H__ */
