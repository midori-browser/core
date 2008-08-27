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

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include <glib.h>
#if HAVE_GIO
#include <gio/gio.h>
#endif
#include <webkit/webkit.h>

G_BEGIN_DECLS

#if !GTK_CHECK_VERSION(2, 14, 0)

#if HAVE_GIO

GdkPixbuf*
gdk_pixbuf_new_from_stream (GInputStream* stream,
                            GCancellable* cancellable,
                            GError**      error);

#endif

#endif

#if !GTK_CHECK_VERSION(2, 12, 0)

void
gtk_widget_set_has_tooltip             (GtkWidget*         widget,
                                        gboolean           has_tooltip);

void
gtk_widget_set_tooltip_text            (GtkWidget*         widget,
                                        const gchar*       text);

void
gtk_tool_item_set_tooltip_text         (GtkToolItem*       toolitem,
                                        const gchar*       text);

#endif

#ifndef WEBKIT_CHECK_VERSION

gfloat
webkit_web_view_get_zoom_level         (WebKitWebView*     web_view);

void
webkit_web_view_set_zoom_level         (WebKitWebView*     web_view,
                                        gfloat             zoom_level);

void
webkit_web_view_zoom_in                (WebKitWebView*     web_view);

void
webkit_web_view_zoom_out               (WebKitWebView*     web_view);

#endif

G_END_DECLS

#endif /* __COMPAT_H__ */
