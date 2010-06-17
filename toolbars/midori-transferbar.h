/*
 Copyright (C) 2010 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_TRANSFERBAR_H__
#define __MIDORI_TRANSFERBAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_TRANSFERBAR \
    (midori_transferbar_get_type ())
#define MIDORI_TRANSFERBAR(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_TRANSFERBAR, MidoriTransferbar))
#define MIDORI_TRANSFERBAR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_TRANSFERBAR, MidoriTransferbarClass))
#define MIDORI_IS_TRANSFERBAR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_TRANSFERBAR))
#define MIDORI_IS_TRANSFERBAR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_TRANSFERBAR))
#define MIDORI_TRANSFERBAR_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_TRANSFERBAR, MidoriTransferbarClass))

typedef struct _MidoriTransferbar                MidoriTransferbar;
typedef struct _MidoriTransferbarClass           MidoriTransferbarClass;

GType
midori_transferbar_get_type               (void);

G_END_DECLS

#endif /* __MIDORI_TRANSFERBAR_H__ */
