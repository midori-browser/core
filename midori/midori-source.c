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
#if HAVE_GTKSOURCEVIEW
    #include <gtksourceview/gtksourceview.h>
    #include <gtksourceview/gtksourcelanguagemanager.h>

    #define MidoriSourceView GtkSourceView
    #define MidoriSourceViewClass GtkSourceViewClass
    #define MIDORI_TYPE_SOURCE_VIEW GTK_TYPE_SOURCE_VIEW
#else
    #define MidoriSourceView GtkTextView
    #define MidoriSourceViewClass GtkTextViewClass
    #define MIDORI_TYPE_SOURCE_VIEW GTK_TYPE_TEXT_VIEW
#endif

struct _MidoriSource
{
    MidoriSourceView parent_instance;
};

struct _MidoriSourceClass
{
    MidoriSourceViewClass parent_class;
};

G_DEFINE_TYPE (MidoriSource, midori_source, MIDORI_TYPE_SOURCE_VIEW);

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
    #if HAVE_GTKSOURCEVIEW
    GtkSourceBuffer* buffer;
    #else
    GtkTextBuffer* buffer;
    #endif

    #if HAVE_GTKSOURCEVIEW
    buffer = gtk_source_buffer_new (NULL);
    gtk_source_buffer_set_highlight_syntax (buffer, TRUE);
    gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (source), TRUE);
    #else
    buffer = gtk_text_buffer_new (NULL);
    #endif
    gtk_text_view_set_buffer (GTK_TEXT_VIEW (source), GTK_TEXT_BUFFER (buffer));
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
    #if HAVE_GTKSOURCEVIEW
    GFileInfo* info;
    const gchar* content_type;
    #endif
    #endif
    gchar* contents;
    gchar* contents_utf8;
    GtkTextBuffer* buffer;
    #if HAVE_GTKSOURCEVIEW
    #if HAVE_GIO
    GtkSourceLanguageManager* language_manager;
    GtkSourceLanguage* language;
    #endif
    #endif

    g_return_if_fail (MIDORI_IS_SOURCE (source));

    contents = NULL;

    #if HAVE_GIO
    file = g_file_new_for_uri (uri);
    tag = NULL;
    #if HAVE_GTKSOURCEVIEW
    content_type = NULL;
    #endif
    if (g_file_load_contents (file, NULL, &contents, NULL, &tag, NULL))
    {
        #if HAVE_GTKSOURCEVIEW
        info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                  G_FILE_QUERY_INFO_NONE, NULL, NULL);
        content_type = g_file_info_get_content_type (info);
        #endif
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
    #if HAVE_GTKSOURCEVIEW
    #if HAVE_GIO
    if (content_type)
    {
        language_manager = gtk_source_language_manager_get_default ();
        if (!strcmp (content_type, "text/html"))
        {
            language = gtk_source_language_manager_get_language (
                language_manager, "html");
            gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), language);
        }
        else if (!strcmp (content_type, "text/css"))
        {
            language = gtk_source_language_manager_get_language (
                language_manager, "css");
            gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), language);
        }
        else if (!strcmp (content_type, "text/javascript"))
        {
            language = gtk_source_language_manager_get_language (
                language_manager, "js");
            gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), language);
        }
    }
    #endif
    #endif
    if (contents_utf8)
        gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), contents_utf8, -1);

    g_object_unref (buffer);
    g_free (contents_utf8);
    #if HAVE_GIO
    g_free (tag);
    #endif
}
