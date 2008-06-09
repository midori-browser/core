/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_WEB_ITEM_H__
#define __MIDORI_WEB_ITEM_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_WEB_ITEM \
    (midori_web_item_get_type ())
#define MIDORI_WEB_ITEM(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_WEB_ITEM, MidoriWebItem))
#define MIDORI_WEB_ITEM_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_WEB_ITEM, MidoriWebItemClass))
#define MIDORI_IS_WEB_ITEM(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_WEB_ITEM))
#define MIDORI_IS_WEB_ITEM_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_WEB_ITEM))
#define MIDORI_WEB_ITEM_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_WEB_ITEM, MidoriWebItemClass))

typedef struct _MidoriWebItem                MidoriWebItem;
typedef struct _MidoriWebItemClass           MidoriWebItemClass;

struct _MidoriWebItemClass
{
    GObjectClass parent_class;
};

GType
midori_web_item_get_type               (void);

MidoriWebItem*
midori_web_item_new                    (void);

const gchar*
midori_web_item_get_name               (MidoriWebItem*  web_item);

void
midori_web_item_set_name               (MidoriWebItem*  web_item,
                                        const gchar*    name);

const gchar*
midori_web_item_get_description        (MidoriWebItem*  web_item);

void
midori_web_item_set_description        (MidoriWebItem*  web_item,
                                        const gchar*    description);

const gchar*
midori_web_item_get_uri                (MidoriWebItem*  web_item);

void
midori_web_item_set_uri                (MidoriWebItem*  web_item,
                                        const gchar*    uri);

const gchar*
midori_web_item_get_icon               (MidoriWebItem*  web_item);

void
midori_web_item_set_icon               (MidoriWebItem*  web_item,
                                        const gchar*    icon);

const gchar*
midori_web_item_get_token              (MidoriWebItem*  web_item);

void
midori_web_item_set_token              (MidoriWebItem*  web_item,
                                        const gchar*    token);

G_END_DECLS

#endif /* __MIDORI_WEB_ITEM_H__ */
