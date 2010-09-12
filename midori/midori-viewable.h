/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_VIEWABLE_H__
#define __MIDORI_VIEWABLE_H__

#include <katze/katze.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_VIEWABLE \
    (midori_viewable_get_type ())
#define MIDORI_VIEWABLE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_VIEWABLE, MidoriViewable))
#define MIDORI_IS_VIEWABLE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_VIEWABLE))
#define MIDORI_VIEWABLE_GET_IFACE(inst) \
    (G_TYPE_INSTANCE_GET_INTERFACE ((inst), MIDORI_TYPE_VIEWABLE, \
    MidoriViewableIface))

typedef struct _MidoriViewable                MidoriViewable;
typedef struct _MidoriViewableIface           MidoriViewableIface;

struct _MidoriViewableIface
{
    GTypeInterface base_iface;

    /* Virtual functions */
    const gchar*
    (*get_stock_id)           (MidoriViewable*             viewable);

    const gchar*
    (*get_label)              (MidoriViewable*             viewable);

    GtkWidget*
    (*get_toolbar)            (MidoriViewable*             viewable);
};

GType
midori_viewable_get_type               (void) G_GNUC_CONST;

const gchar*
midori_viewable_get_stock_id           (MidoriViewable*        viewable);

const gchar*
midori_viewable_get_label              (MidoriViewable*        viewable);

GtkWidget*
midori_viewable_get_toolbar            (MidoriViewable*        viewable);

G_END_DECLS

#endif /* __MIDORI_VIEWABLE_H__ */
