/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_PANE_H__
#define __MIDORI_PANE_H__

#include <gtk/gtk.h>

#include <katze/katze.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_PANE \
    (midori_pane_get_type ())
#define MIDORI_PANE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_PANE, MidoriPane))
#define MIDORI_IS_PANE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_PANE))
#define MIDORI_PANE_GET_IFACE(inst) \
    (G_TYPE_INSTANCE_GET_INTERFACE ((inst), MIDORI_TYPE_PANE, MidoriPaneIface))

typedef struct _MidoriPane                MidoriPane;
typedef struct _MidoriPaneIface           MidoriPaneIface;

struct _MidoriPaneIface
{
    GTypeInterface base_iface;

    /* Virtual functions */
    const gchar*
    (*get_stock_id)           (MidoriPane*             pane);

    const gchar*
    (*get_label)              (MidoriPane*             pane);

    GtkWidget*
    (*get_toolbar)            (MidoriPane*             pane);
};

GType
midori_pane_get_type               (void);

const gchar*
midori_pane_get_stock_id           (MidoriPane*        pane);

const gchar*
midori_pane_get_label              (MidoriPane*        pane);

GtkWidget*
midori_pane_get_toolbar            (MidoriPane*        pane);

G_END_DECLS

#endif /* __MIDORI_PANE_H__ */
