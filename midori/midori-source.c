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

#include <katze/katze.h>

#include <string.h>
#include <glib/gi18n.h>

struct _MidoriSource
{
    GtkTextView parent_instance;

    KatzeNet* net;
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

    source->net = katze_net_new ();
}

static void
midori_source_finalize (GObject* object)
{
    katze_object_assign (MIDORI_SOURCE (object)->net, NULL);

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

static void
midori_source_transfer_cb (KatzeNetRequest* request,
                           MidoriSource*    source)
{
    gchar** mimev;
    gchar* charset;
    gchar* contents_utf8;
    GtkTextBuffer* buffer;

    if (request->data)
    {
        if (!g_utf8_validate (request->data, request->length, NULL))
        {
            charset = NULL;
            if (request->mime_type)
            {
                mimev = g_strsplit (request->mime_type, " ", 2);
                if (mimev[0] && mimev[1] &&
                    g_str_has_prefix (mimev[1], "charset="))
                    charset = g_strdup (&mimev[1][8]);
                g_strfreev (mimev);
            }
            contents_utf8 = g_convert (request->data, -1, "UTF-8",
                charset ? charset : "ISO-8859-1", NULL, NULL, NULL);
        }
        else
            contents_utf8 = (gchar*)request->data;
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source));
        if (contents_utf8)
            gtk_text_buffer_set_text (buffer, contents_utf8, -1);
        g_object_unref (buffer);
        if (contents_utf8 != request->data)
            g_free (contents_utf8);
    }
}

void
midori_source_set_uri (MidoriSource* source,
                       const gchar*  uri)
{
    g_return_if_fail (MIDORI_IS_SOURCE (source));
    g_return_if_fail (uri != NULL);

    katze_net_load_uri (source->net, uri,
        NULL, (KatzeNetTransferCb)midori_source_transfer_cb, source);
}
