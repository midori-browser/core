/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_XBEL_H__
#define __KATZE_XBEL_H__ 1

#include <glib-object.h>

G_BEGIN_DECLS

#define KATZE_TYPE_XBEL_ITEM \
    (katze_xbel_item_get_type ())
#define KATZE_XBEL_ITEM(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_XBEL_ITEM, KatzeXbelItem))
#define KATZE_IS_XBEL_ITEM(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_XBEL_ITEM))

typedef struct _KatzeXbelItem                KatzeXbelItem;
typedef struct _KatzeXbelItemClass           KatzeXbelItemClass;

struct _KatzeXbelItemClass
{
    GObjectClass parent_class;
};

#define KATZE_XBEL_ERROR g_quark_from_string("KATZE_XBEL_ERROR")

typedef enum
{
    KATZE_XBEL_ERROR_INVALID_URI,        /* Malformed uri */
    KATZE_XBEL_ERROR_INVALID_VALUE,      /* Requested field not found */
    KATZE_XBEL_ERROR_URI_NOT_FOUND,      /* Requested uri not found */
    KATZE_XBEL_ERROR_READ,               /* Malformed document */
    KATZE_XBEL_ERROR_UNKNOWN_ENCODING,   /* Parsed text was in an unknown encoding */
    KATZE_XBEL_ERROR_WRITE,              /* Writing failed. */
} KatzeXbelError;

typedef enum
{
    KATZE_XBEL_ITEM_KIND_FOLDER,
    KATZE_XBEL_ITEM_KIND_BOOKMARK,
    KATZE_XBEL_ITEM_KIND_SEPARATOR
} KatzeXbelItemKind;

GType
katze_xbel_item_get_type         (void) G_GNUC_CONST;

KatzeXbelItem*
katze_xbel_bookmark_new          (void);

KatzeXbelItem*
katze_xbel_separator_new         (void);

KatzeXbelItem*
katze_xbel_folder_new            (void);

void
katze_xbel_item_ref              (KatzeXbelItem*   item);

void
katze_xbel_item_unref            (KatzeXbelItem*   item);

KatzeXbelItem*
katze_xbel_item_copy             (KatzeXbelItem*   item);

void
katze_xbel_folder_append_item    (KatzeXbelItem*   folder,
                                  KatzeXbelItem*   item);

void
katze_xbel_folder_prepend_item   (KatzeXbelItem*   folder,
                                  KatzeXbelItem*   item);

void
katze_xbel_folder_remove_item    (KatzeXbelItem*   folder,
                                  KatzeXbelItem*   item);

guint
katze_xbel_folder_get_n_items    (KatzeXbelItem*   folder);

KatzeXbelItem*
katze_xbel_folder_get_nth_item   (KatzeXbelItem*   folder,
                                  guint            n);

gboolean
katze_xbel_folder_is_empty       (KatzeXbelItem*   folder);

gboolean
katze_xbel_folder_get_folded     (KatzeXbelItem*   folder);

KatzeXbelItemKind
katze_xbel_item_get_kind         (KatzeXbelItem*   item);

KatzeXbelItem*
katze_xbel_item_get_parent       (KatzeXbelItem*   item);

G_CONST_RETURN gchar*
katze_xbel_item_get_title        (KatzeXbelItem*   item);

G_CONST_RETURN gchar*
katze_xbel_item_get_desc         (KatzeXbelItem*   item);

G_CONST_RETURN gchar*
katze_xbel_bookmark_get_href     (KatzeXbelItem*   bookmark);

/*time_t
katze_xbel_bookmark_get_added    (KatzeXbelItem*   bookmark);

time_t
katze_xbel_bookmark_get_modified (KatzeXbelItem*   bookmark);

time_t
katze_xbel_bookmark_get_visited  (KatzeXbelItem*   bookmark);*/

gboolean
katze_xbel_item_is_bookmark      (KatzeXbelItem*   bookmark);

gboolean
katze_xbel_item_is_separator     (KatzeXbelItem*   bookmark);

gboolean
katze_xbel_item_is_folder        (KatzeXbelItem*   bookmark);

void
katze_xbel_folder_set_folded     (KatzeXbelItem*   folder,
                                  gboolean         folded);

void
katze_xbel_item_set_title        (KatzeXbelItem*   item,
                                  const gchar*     title);

void
katze_xbel_item_set_desc         (KatzeXbelItem*   item,
                                  const gchar*     desc);

void
katze_xbel_bookmark_set_href     (KatzeXbelItem*   bookmark,
                                  const gchar*     href);

gboolean
katze_xbel_folder_from_data      (KatzeXbelItem*   folder,
                                  const gchar*     data,
                                  GError**         error);

gboolean
katze_xbel_folder_from_file      (KatzeXbelItem*   folder,
                                  const gchar*     file,
                                  GError**         error);

gboolean
katze_xbel_folder_from_data_dirs (KatzeXbelItem*   folder,
                                  const gchar*     file,
                                  gchar**          full_path,
                                  GError**         error);

gchar*
katze_xbel_item_to_data          (KatzeXbelItem*   item);

gchar*
katze_xbel_folder_to_data        (KatzeXbelItem*   folder,
                                  gsize*           length,
                                  GError**         error);

gboolean
katze_xbel_folder_to_file        (KatzeXbelItem*   folder,
                                  const gchar*     file,
                                  GError**         error);

#endif /* !__KATZE_XBEL_H__ */
