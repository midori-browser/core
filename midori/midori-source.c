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
#if HAVE_GIO
    #include <gio/gio.h>
#endif
#include <glib/gi18n.h>

struct _MidoriSource
{
    GtkTextView parent_instance;
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
}

static void
midori_source_finalize (GObject* object)
{
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

void
midori_source_set_uri (MidoriSource* source,
                       const gchar*  uri)
{
    #if HAVE_GIO
    GFile* file;
    gchar* tag;
    #endif
    gchar* contents;
    gchar* contents_utf8;
    GtkTextBuffer* buffer;

    g_return_if_fail (MIDORI_IS_SOURCE (source));

    contents = NULL;

    #if HAVE_GIO
    file = g_file_new_for_uri (uri);
    tag = NULL;
    if (g_file_load_contents (file, NULL, &contents, NULL, &tag, NULL))
    {
        g_object_unref (file);
    }
    if (contents && !g_utf8_validate (contents, -1, NULL))
    {
        contents_utf8 = g_convert (contents, -1, "UTF-8", "ISO-8859-1",
                                   NULL, NULL, NULL);
        g_free (contents);
    }
    else
    #endif
        contents_utf8 = contents;

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source));
    if (contents_utf8)
        gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), contents_utf8, -1);

    g_object_unref (buffer);
    g_free (contents_utf8);
    #if HAVE_GIO
    g_free (tag);
    #endif
}
