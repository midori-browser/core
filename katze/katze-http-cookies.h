/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_HTTP_COOKIES_H__
#define __KATZE_HTTP_COOKIES_H__

#include "katze-utils.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define KATZE_TYPE_HTTP_COOKIES \
    (katze_http_cookies_get_type ())
#define KATZE_HTTP_COOKIES(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_HTTP_COOKIES, KatzeHttpCookies))
#define KATZE_HTTP_COOKIES_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), KATZE_TYPE_HTTP_COOKIES, KatzeHttpCookiesClass))
#define KATZE_IS_HTTP_COOKIES(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_HTTP_COOKIES))
#define KATZE_IS_HTTP_COOKIES_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), KATZE_TYPE_HTTP_COOKIES))
#define KATZE_HTTP_COOKIES_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), KATZE_TYPE_HTTP_COOKIES, KatzeHttpCookiesClass))

typedef struct _KatzeHttpCookies                KatzeHttpCookies;
typedef struct _KatzeHttpCookiesClass           KatzeHttpCookiesClass;

GType
katze_http_cookies_get_type                       (void);

G_END_DECLS

#endif /* __KATZE_HTTP_COOKIES_H__ */
