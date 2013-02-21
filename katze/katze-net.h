/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_NET_H__
#define __KATZE_NET_H__

#ifndef HAVE_WEBKIT2
    #include <webkit/webkit.h>
#else
    #include <webkit2/webkit2.h>
#endif
#include "katze-utils.h"

G_BEGIN_DECLS

#define KATZE_TYPE_NET \
    (katze_net_get_type ())
#define KATZE_NET(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_NET, KatzeNet))
#define KATZE_NET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), KATZE_TYPE_NET, KatzeNetClass))
#define KATZE_IS_NET(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_NET))
#define KATZE_IS_NET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), KATZE_TYPE_NET))
#define KATZE_NET_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), KATZE_TYPE_NET, KatzeNetClass))

typedef struct _KatzeNet                KatzeNet;
typedef struct _KatzeNetClass           KatzeNetClass;

GType
katze_net_get_type                       (void) G_GNUC_CONST;

typedef enum
{
    KATZE_NET_VERIFIED,
    KATZE_NET_MOVED,
    KATZE_NET_NOT_FOUND,
    KATZE_NET_FAILED,
    KATZE_NET_DONE
} KatzeNetStatus;

typedef struct
{
    gchar* uri;
    KatzeNetStatus status;
    gchar* mime_type;
    gchar* data;
    gint64 length;
} KatzeNetRequest;

typedef gboolean (*KatzeNetStatusCb)     (KatzeNetRequest*   request,
                                          gpointer           user_data);

typedef void     (*KatzeNetTransferCb)   (KatzeNetRequest*   request,
                                          gpointer           user_data);

void
katze_net_load_uri                       (KatzeNet*          net,
                                          const gchar*       uri,
                                          KatzeNetStatusCb   status_cb,
                                          KatzeNetTransferCb transfer_cb,
                                          gpointer           user_data);

#if !WEBKIT_CHECK_VERSION (1, 3, 13)
gchar*
katze_net_get_cached_path                (KatzeNet*          net,
                                          const gchar*       uri,
                                          const gchar*       subfolder);
#endif

G_END_DECLS

#endif /* __KATZE_NET_H__ */
