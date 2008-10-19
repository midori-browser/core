/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "midori-source.h"

#include <string.h>
#include <glib/gi18n.h>

#if HAVE_LIBSOUP
    #include <libsoup/soup.h>
#endif

struct _MidoriSource
{
    GtkTextView parent_instance;

    #if HAVE_LIBSOUP
    SoupSession* session;
    #endif
};

struct _MidoriSourceClass
{
    GtkTextViewClass parent_class;
};

G_DEFINE_TYPE (MidoriSource, midori_source, GTK_TYPE_TEXT_VIEW);

static void
midori_source_finalize (GObject* object);

static void
midori_source_class_init (MidoriSourceClass* class)
{
    GObjectClass* gobject_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_source_finalize;
}

static void
midori_source_init (MidoriSource* source)
{
    GtkTextBuffer* buffer;

    buffer = gtk_text_buffer_new (NULL);
    gtk_text_view_set_buffer (GTK_TEXT_VIEW (source), buffer);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (source), FALSE);

    #if HAVE_LIBSOUP
    source->session = soup_session_async_new ();
    #endif
}

static void
midori_source_finalize (GObject* object)
{
    #if HAVE_LIBSOUP
    g_object_unref (MIDORI_SOURCE (object)->session);
    #endif

    G_OBJECT_CLASS (midori_source_parent_class)->finalize (object);
}

/**
 * midori_source_new:
 * @uri: a view-source: URI
 *
 * Creates a new source widget.
 *
 * Return value: a new #MidoriSource
 **/
GtkWidget*
midori_source_new (const gchar* uri)
{
    MidoriSource* source = g_object_new (MIDORI_TYPE_SOURCE,
                                         /*"uri", uri,*/
                                         NULL);
    midori_source_set_uri (source, uri);

    return GTK_WIDGET (source);
}

#if HAVE_LIBSOUP
static void
midori_source_got_body_cb (SoupMessage*  msg,
                           MidoriSource* source)
{
    const gchar* contents;
    const gchar* mime;
    gchar** mimev;
    gchar* charset;
    gchar* contents_utf8;
    GtkTextBuffer* buffer;

    if (msg->response_body->length > 0)
    {
        contents = msg->response_body->data;
        if (contents && !g_utf8_validate (contents, -1, NULL))
        {
            charset = NULL;
            if (msg->response_headers)
            {
                mime = soup_message_headers_get (msg->response_headers,
                                                 "content-type");
                if (mime)
                {
                    mimev = g_strsplit (mime, " ", 2);
                    if (mimev[0] && mimev[1] &&
                        g_str_has_prefix (mimev[1], "charset="))
                        charset = g_strdup (&mimev[1][8]);
                    g_strfreev (mimev);
                }
            }
            contents_utf8 = g_convert (contents, -1, "UTF-8",
                charset ? charset : "ISO-8859-1", NULL, NULL, NULL);
        }
        else
            contents_utf8 = (gchar*)contents;
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source));
        if (contents_utf8)
            gtk_text_buffer_set_text (buffer, contents_utf8, -1);
        g_object_unref (buffer);
        if (contents != contents_utf8)
            g_free (contents_utf8);
    }
}
#endif

void
midori_source_set_uri (MidoriSource* source,
                       const gchar*  uri)
{
    gchar* contents;
    gchar* contents_utf8;
    GtkTextBuffer* buffer;
    #if HAVE_LIBSOUP
    SoupMessage* msg;
    #endif
    gchar* filename;

    g_return_if_fail (MIDORI_IS_SOURCE (source));
    g_return_if_fail (uri != NULL);

    contents = NULL;

    #if HAVE_LIBSOUP
    if (g_str_has_prefix (uri, "http://") || g_str_has_prefix (uri, "https://"))
    {
        msg = soup_message_new ("GET", uri);
        g_signal_connect (msg, "got-body",
            G_CALLBACK (midori_source_got_body_cb), source);
        soup_session_queue_message (source->session, msg, NULL, NULL);
        return;
    }
    #endif
    if (g_str_has_prefix (uri, "file://"))
    {
        contents = NULL;
        filename = g_filename_from_uri (uri, NULL, NULL);
        if (!filename || !g_file_get_contents (filename, &contents, NULL, NULL))
            return;
        if (contents && !g_utf8_validate (contents, -1, NULL))
        {
            contents_utf8 = g_convert (contents, -1, "UTF-8", "ISO-8859-1",
                                       NULL, NULL, NULL);
            g_free (contents);
        }
        else
            contents_utf8 = contents;
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source));
        if (contents_utf8)
            gtk_text_buffer_set_text (buffer, contents_utf8, -1);
        g_object_unref (buffer);
        g_free (contents_utf8);
    }
}
