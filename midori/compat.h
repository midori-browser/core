/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __COMPAT_H__
#define __COMPAT_H__

#include <webkit/webkit.h>

G_BEGIN_DECLS

#ifndef WEBKIT_CHECK_VERSION

gfloat
webkit_web_view_get_zoom_level         (WebKitWebView*     web_view);

void
webkit_web_view_set_zoom_level         (WebKitWebView*     web_view);

void
webkit_web_view_zoom_in                (WebKitWebView*     web_view);

void
webkit_web_view_zoom_out               (WebKitWebView*     web_view);

#endif

G_END_DECLS

#endif /* __COMPAT_H__ */
