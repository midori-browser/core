/*
 Copyright (C) 2009-2010 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "feed-parse.h"
#include <time.h>

gchar*
feed_get_element_markup (FeedParser* fparser)
{
    xmlNodePtr node;

    node = fparser->node;

    if (node->children &&
        !xmlIsBlankNode (node->children) &&
        node->children->type == XML_ELEMENT_NODE)
    {
        return ((gchar*) xmlNodeGetContent (node->children));
    }

    if (!node->children ||
        xmlIsBlankNode (node->children) ||
        (node->children->type != XML_TEXT_NODE &&
         node->children->type != XML_CDATA_SECTION_NODE)
       )
    {
        /* Some servers add required elements with no content,
         * create a dummy string to handle it.
         */
        return g_strdup (" ");
    }
    return (gchar*)xmlNodeListGetString (fparser->doc, node->children, 1);
}

static void
handle_markup_chars (void*          user_data,
                     const xmlChar* ch,
                     int            len)
{
    if (len > 0)
    {
        gchar** markup;
        gchar* temp;

        markup = (gchar**)user_data;
        temp = g_strndup ((gchar*)ch, len);
        *markup = (*markup) ? g_strconcat (*markup, temp, NULL) : g_strdup (temp);
        g_free (temp);
    }
}

gchar*
feed_remove_markup (gchar* markup)
{
    const xmlChar* stag;
    if (((stag = xmlStrchr (BAD_CAST markup, '<')) && xmlStrchr (stag, '>')) ||
         xmlStrchr (BAD_CAST markup, '&'))
    {
        gchar* text = NULL;
        htmlSAXHandlerPtr psax;

        psax = g_new0 (htmlSAXHandler, 1);
        psax->characters = handle_markup_chars;
        htmlSAXParseDoc (BAD_CAST markup, "UTF-8", psax, &text);
        g_free (psax);
        g_free (markup);
        return text;
    }
    return markup;
}

gchar*
feed_get_element_string (FeedParser* fparser)
{
    gchar* markup;

    markup = feed_get_element_markup (fparser);
    return feed_remove_markup (markup);
}

gint64
feed_get_element_date (FeedParser* fparser)
{
    time_t date;
    gchar* content;

    date = 0;
    content = feed_get_element_string (fparser);

    if (content)
    {
        SoupDate* sdate;

        sdate = soup_date_new_from_string (content);
        if (sdate)
        {
            date = soup_date_to_time_t (sdate);
            soup_date_free (sdate);
        }
        g_free (content);
    }
    return ((gint64)date);
}

KatzeItem*
feed_item_exists (KatzeArray* array,
                  KatzeItem*  item)
{
    const gchar* guid;
    gchar* hstr;
    guint hash;

    guid = katze_item_get_token (item);
    if (!guid)
    {
        hstr = g_strjoin (NULL,
                          katze_item_get_name (item),
                          katze_item_get_uri (item),
                          katze_item_get_text (item),
                          NULL);
        hash = g_str_hash (hstr);
        g_free (hstr);

        hstr = g_strdup_printf ("%u", hash);
        katze_item_set_token (item, hstr);
        g_free (hstr);

        guid = katze_item_get_token (item);
    }

    return (katze_array_find_token (array, guid));
}

void
feed_parse_node (FeedParser* fparser)
{
    xmlNodePtr node;
    xmlNodePtr child;

    if (!*fparser->error)
    {
        if (fparser->preparse)
            (*fparser->preparse) (fparser);

        if (fparser->parse)
        {
            node = fparser->node;
            child = node->last;

            while (child)
            {
                if (child->type == XML_ELEMENT_NODE)
                {
                    fparser->node = child;

                    (*fparser->parse) (fparser);

                    if (*fparser->error)
                        break;
                }
                child = child->prev;
            }
            fparser->node = node;
        }

        if (fparser->postparse)
            (*fparser->postparse) (fparser);
    }
}

static void
feed_parse_doc (xmlDocPtr   doc,
                GSList*     parsers,
                KatzeArray* array,
                GError**    error)
{
    FeedParser* fparser;
    xmlNodePtr root;
    gboolean isvalid;

    root = xmlDocGetRootElement (doc);

    if (!root)
    {
        *error = g_error_new (FEED_PARSE_ERROR,
                              FEED_PARSE_ERROR_MISSING_ELEMENT,
                              _("Failed to find root element in feed XML data."));
        return;
    }

    while (parsers)
    {
        fparser = (FeedParser*)parsers->data;
        fparser->error = error;
        fparser->doc = doc;
        fparser->node = root;

        if (fparser && fparser->isvalid)
        {
            isvalid = (*fparser->isvalid) (fparser);

            if (*fparser->error)
                return;

            if (isvalid)
            {
                fparser->item = KATZE_ITEM (array);

                if (fparser->update &&
                    (*fparser->update) (fparser))
                    feed_parse_node (fparser);
            }
        }
        else
            isvalid = FALSE;

        fparser->error = NULL;
        fparser->doc = NULL;
        fparser->node = NULL;

        if (isvalid)
            return;

        parsers = g_slist_next (parsers);
    }

    *error = g_error_new (FEED_PARSE_ERROR,
                          FEED_PARSE_ERROR_INVALID_FORMAT,
                          _("Unsupported feed format."));
}

gboolean
parse_feed (gchar*      data,
            gint64      length,
            GSList*     parsers,
            KatzeArray* array,
            GError**    error)
{
    xmlDocPtr doc;
    xmlErrorPtr xerror;

    LIBXML_TEST_VERSION

    doc = xmlReadMemory (
                data, length, "feedfile.xml", NULL,
                XML_PARSE_NOWARNING | XML_PARSE_NOERROR /*| XML_PARSE_RECOVER*/
                );

    if (doc)
    {
        feed_parse_doc (doc, parsers, array, error);
        xmlFreeDoc (doc);
    }
    else
    {
        xerror = xmlGetLastError ();
        *error = g_error_new (FEED_PARSE_ERROR,
                              FEED_PARSE_ERROR_PARSE,
                              _("Failed to parse XML feed: %s"),
                              xerror->message);
        xmlResetLastError ();
    }
    xmlMemoryDump ();

    return *error ? FALSE : TRUE;
}

