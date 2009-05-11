/*
 Copyright (C) 2009 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __FEED_PARSE_H__
#define __FEED_PARSE_H__

#include <midori/midori.h>

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include <libsoup/soup.h>
#include <libxml/parser.h>
#include <libxml/HTMLparser.h>

G_BEGIN_DECLS

#define FEED_PARSE_ERROR g_quark_from_string("FEED_PARSE_ERROR")

typedef enum
{
    FEED_PARSE_ERROR_PARSE,
    FEED_PARSE_ERROR_INVALID_FORMAT,
    FEED_PARSE_ERROR_INVALID_VERSION,
    FEED_PARSE_ERROR_MISSING_ELEMENT

} FeedBarError;

typedef struct _FeedParser
{
    xmlDocPtr   doc;   /* The XML document */
    xmlNodePtr  node;  /* The XML node at a specific point */
    KatzeItem*  item;
    GError**    error;

    gboolean (*isvalid)   (struct _FeedParser* fparser);
    gboolean (*update)    (struct _FeedParser* fparser);
    void     (*preparse)  (struct _FeedParser* fparser);
    void     (*parse)     (struct _FeedParser* fparser);
    void     (*postparse) (struct _FeedParser* fparser);

} FeedParser;

#define feed_parser_set_error(fparser, err, msg) \
    *(fparser)->error = g_error_new ( \
            FEED_PARSE_ERROR, (err), (msg))

gchar*
feed_get_element_string (FeedParser* fparser);

gchar*
feed_remove_markup (gchar* markup);

gchar*
feed_get_element_markup (FeedParser* fparser);

gint64
feed_get_element_date   (FeedParser* fparser);

KatzeItem*
feed_item_exists        (KatzeArray* array,
                         KatzeItem*  item);
void
feed_parse_node         (FeedParser* fparser);

gboolean
parse_feed              (gchar*      data,
                         gint64      length,
                         GSList*     parsers,
                         KatzeArray* array,
                         GError**    error);

G_END_DECLS

#endif /* __FEED_PARSE_H__ */

