/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_ITEM_H__
#define __KATZE_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define KATZE_TYPE_ITEM \
    (katze_item_get_type ())
#define KATZE_ITEM(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_ITEM, KatzeItem))
#define KATZE_ITEM_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), KATZE_TYPE_ITEM, KatzeItemClass))
#define KATZE_IS_ITEM(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_ITEM))
#define KATZE_IS_ITEM_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), KATZE_TYPE_ITEM))
#define KATZE_ITEM_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), KATZE_TYPE_ITEM, KatzeItemClass))
#define KATZE_ITEM_IS_BOOKMARK(item) (item && katze_item_get_uri (item))
#define KATZE_ITEM_IS_FOLDER(item) (item && !katze_item_get_uri (item))
#define KATZE_ITEM_IS_SEPARATOR(item) (item == NULL)

typedef struct _KatzeItem                KatzeItem;
typedef struct _KatzeItemClass           KatzeItemClass;

struct _KatzeItem
{
    GObject parent_instance;

    gchar* name;
    gchar* text;
    gchar* uri;
    gchar* token;
    gint64 added;
    GHashTable* metadata;

    KatzeItem* parent;
};

struct _KatzeItemClass
{
    GObjectClass parent_class;

    gpointer
    (*copy)                       (KatzeItem*      item);
};

GType
katze_item_get_type               (void) G_GNUC_CONST;

KatzeItem*
katze_item_new                    (void);

const gchar*
katze_item_get_name               (KatzeItem*      item);

void
katze_item_set_name               (KatzeItem*      item,
                                   const gchar*    name);

const gchar*
katze_item_get_text               (KatzeItem*      item);

void
katze_item_set_text               (KatzeItem*      item,
                                   const gchar*    text);

const gchar*
katze_item_get_uri                (KatzeItem*      item);

void
katze_item_set_uri                (KatzeItem*      item,
                                   const gchar*    uri);

const gchar*
katze_item_get_icon               (KatzeItem*      item);

void
katze_item_set_icon               (KatzeItem*      item,
                                   const gchar*    icon);

GdkPixbuf*
katze_item_get_pixbuf             (KatzeItem*      item,
                                   GtkWidget*      widget);

GtkWidget*
katze_item_get_image              (KatzeItem*      item);

const gchar*
katze_item_get_token              (KatzeItem*      item);

void
katze_item_set_token              (KatzeItem*      item,
                                   const gchar*    token);

gint64
katze_item_get_added              (KatzeItem*      item);

void
katze_item_set_added              (KatzeItem*      item,
                                   gint64          added);

GList*
katze_item_get_meta_keys          (KatzeItem*      item);

const gchar*
katze_item_get_meta_string        (KatzeItem*      item,
                                   const gchar*    key);

void
katze_item_set_meta_string        (KatzeItem*      item,
                                   const gchar*    key,
                                   const gchar*    value);

gint64
katze_item_get_meta_integer       (KatzeItem*      item,
                                   const gchar*    key);

gboolean
katze_item_get_meta_boolean       (KatzeItem*      item,
                                   const gchar*    key);

void
katze_item_set_meta_integer       (KatzeItem*      item,
                                   const gchar*    key,
                                   gint64          value);

gpointer
katze_item_get_parent             (KatzeItem*      item);

void
katze_item_set_parent             (KatzeItem*      item,
                                   gpointer        parent);

KatzeItem*
katze_item_copy                   (KatzeItem*      item);

G_END_DECLS

#endif /* __MIDORI_WEB_ITEM_H__ */
