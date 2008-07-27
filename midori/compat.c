/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "compat.h"

#if !GTK_CHECK_VERSION(2, 14, 0)

#if GLIB_CHECK_VERSION(2, 16, 0)

/* GTK+/ GdkPixbuf internal helper function
   Copyright (C) 2008 Matthias Clasen <mclasen@redhat.com>
   Copied from Gtk+ 2.13, coding style adjusted */

static GdkPixbuf*
load_from_stream (GdkPixbufLoader* loader,
                  GInputStream*    stream,
                  GCancellable*    cancellable,
                  GError**         error)
{
    GdkPixbuf* pixbuf;
    gssize n_read;
    guchar buffer[65536];
    gboolean res;

    res = TRUE;
    while (1)
    {
        n_read = g_input_stream_read (stream, buffer, sizeof (buffer),
                                      cancellable, error);
        if (n_read < 0)
        {
            res = FALSE;
            error = NULL; /* Ignore further errors */
            break;
        }

        if (!n_read)
            break;

        if (!gdk_pixbuf_loader_write (loader, buffer, n_read,
                                      error))
        {
            res = FALSE;
            error = NULL;
            break;
        }
    }

    if (!gdk_pixbuf_loader_close (loader, error))
    {
        res = FALSE;
        error = NULL;
    }

    pixbuf = NULL;
    if (res)
    {
        pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
        if (pixbuf)
        g_object_ref (pixbuf);
    }

    return pixbuf;
}

/* GTK+/ GdkPixbuf stream loading function
   Copyright (C) 2008 Matthias Clasen <mclasen@redhat.com>
   Copied from Gtk+ 2.13, coding style adjusted */
GdkPixbuf*
gdk_pixbuf_new_from_stream (GInputStream* stream,
                            GCancellable* cancellable,
                            GError**      error)
{
    GdkPixbuf* pixbuf;
    GdkPixbufLoader* loader;

    loader = gdk_pixbuf_loader_new ();
    pixbuf = load_from_stream (loader, stream, cancellable, error);
    g_object_unref (loader);

    return pixbuf;
}

#endif

#endif

#if !GTK_CHECK_VERSION(2, 12, 0)

void
gtk_widget_set_has_tooltip (GtkWidget* widget,
                            gboolean   has_tooltip)
{
    /* Do nothing */
}

void
gtk_widget_set_tooltip_text (GtkWidget*   widget,
                             const gchar* text)
{
    static GtkTooltips* tooltips;
    if (!tooltips)
        tooltips = gtk_tooltips_new ();
    gtk_tooltips_set_tip (tooltips, widget, text, NULL);
}

void
gtk_tool_item_set_tooltip_text (GtkToolItem* toolitem,
                                const gchar* text)
{
    if (text && *text)
    {
        static GtkTooltips* tooltips = NULL;
        if (G_UNLIKELY (!tooltips))
            tooltips = gtk_tooltips_new();

        gtk_tool_item_set_tooltip (toolitem, tooltips, text, NULL);
    }
}

#endif

#ifndef WEBKIT_CHECK_VERSION

gfloat
webkit_web_view_get_zoom_level (WebKitWebView* web_view)
{
    g_return_val_if_fail (WEBKIT_IS_WEB_VIEW (web_view), 1.0);

    return 1.0;
}

void
webkit_web_view_set_zoom_level (WebKitWebView* web_view,
                                gfloat         zoom_level)
{
    g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
}

void
webkit_web_view_zoom_in (WebKitWebView* web_view)
{
    g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
}

void
webkit_web_view_zoom_out (WebKitWebView* web_view)
{
    g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
}

#endif
