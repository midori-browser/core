/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_WEB_LIST_H__
#define __MIDORI_WEB_LIST_H__

#include "midori-webitem.h"

G_BEGIN_DECLS

#define MIDORI_TYPE_WEB_LIST \
    (midori_web_list_get_type ())
#define MIDORI_WEB_LIST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_WEB_LIST, MidoriWebList))
#define MIDORI_WEB_LIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_WEB_LIST, MidoriWebListClass))
#define MIDORI_IS_WEB_LIST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_WEB_LIST))
#define MIDORI_IS_WEB_LIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_WEB_LIST))
#define MIDORI_WEB_LIST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_WEB_LIST, MidoriWebListClass))

typedef struct _MidoriWebList                MidoriWebList;
typedef struct _MidoriWebListClass           MidoriWebListClass;

struct _MidoriWebListClass
{
    GObjectClass parent_class;

    /* Signals */
    void
    (*add_item)               (MidoriWebList* web_list,
                               GObject*       item);
    void
    (*remove_item)            (MidoriWebList* web_list,
                               GObject*       item);
};

GType
midori_web_list_get_type               (void);

MidoriWebList*
midori_web_list_new                    (void);

void
midori_web_list_add_item               (MidoriWebList* web_list,
                                        gpointer       item);

void
midori_web_list_remove_item            (MidoriWebList* web_list,
                                        gpointer       item);

gpointer
midori_web_list_get_nth_item           (MidoriWebList* web_list,
                                        guint          n);

gboolean
midori_web_list_is_empty               (MidoriWebList* web_list);

gint
midori_web_list_get_item_index         (MidoriWebList* web_list,
                                        gpointer       item);

gpointer
midori_web_list_find_token             (MidoriWebList* web_list,
                                        const gchar*   token);

guint
midori_web_list_get_length             (MidoriWebList* web_list);

void
midori_web_list_clear                  (MidoriWebList* web_list);

G_END_DECLS

#endif /* __MIDORI_WEB_LIST_H__ */
