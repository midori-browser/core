/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_HISTORY_PANEL_H__
#define __MIDORI_HISTORY_PANEL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_HISTORY \
    (midori_history_get_type ())
#define MIDORI_HISTORY(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_HISTORY, MidoriHistory))
#define MIDORI_HISTORY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_HISTORY, MidoriHistoryClass))
#define MIDORI_IS_HISTORY(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_HISTORY))
#define MIDORI_IS_HISTORY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_HISTORY))
#define MIDORI_HISTORY_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_HISTORY, MidoriHistoryClass))

typedef struct _MidoriHistory                MidoriHistory;
typedef struct _MidoriHistoryClass           MidoriHistoryClass;

GType
midori_history_get_type               (void);

G_END_DECLS

#endif /* __MIDORI_HISTORY_PANEL_H__ */
