/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_HTTP_AUTH_H__
#define __KATZE_HTTP_AUTH_H__

#include "katze-utils.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define KATZE_TYPE_HTTP_AUTH \
    (katze_http_auth_get_type ())
#define KATZE_HTTP_AUTH(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_HTTP_AUTH, KatzeHttpAuth))
#define KATZE_HTTP_AUTH_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), KATZE_TYPE_HTTP_AUTH, KatzeHttpAuthClass))
#define KATZE_IS_HTTP_AUTH(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_HTTP_AUTH))
#define KATZE_IS_HTTP_AUTH_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), KATZE_TYPE_HTTP_AUTH))
#define KATZE_HTTP_AUTH_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), KATZE_TYPE_HTTP_AUTH, KatzeHttpAuthClass))

typedef struct _KatzeHttpAuth                KatzeHttpAuth;
typedef struct _KatzeHttpAuthClass           KatzeHttpAuthClass;

GType
katze_http_auth_get_type                       (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __KATZE_HTTP_AUTH_H__ */
