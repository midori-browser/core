/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_TRANSFERS_H__
#define __MIDORI_TRANSFERS_H__

#include <gtk/gtk.h>

#include <katze/katze.h>

#include "midori-viewable.h"

G_BEGIN_DECLS

#define MIDORI_TYPE_TRANSFERS \
    (midori_transfers_get_type ())
#define MIDORI_TRANSFERS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_TRANSFERS, MidoriTransfers))
#define MIDORI_TRANSFERS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_TRANSFERS, MidoriTransfersClass))
#define MIDORI_IS_TRANSFERS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_TRANSFERS))
#define MIDORI_IS_TRANSFERS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_TRANSFERS))
#define MIDORI_TRANSFERS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_TRANSFERS, MidoriTransfersClass))

typedef struct _MidoriTransfers                MidoriTransfers;
typedef struct _MidoriTransfersClass           MidoriTransfersClass;

GType
midori_transfers_get_type               (void);

GtkWidget*
midori_transfers_new                    (void);

G_END_DECLS

#endif /* __MIDORI_TRANSFERS_H__ */
