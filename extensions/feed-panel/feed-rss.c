/*
 Copyright (C) 2009-2010 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "feed-rss.h"

static gboolean
rss_is_valid (FeedParser* fparser)
{
    xmlNodePtr node;
    xmlNodePtr child;
    xmlChar* str;
    gboolean valid;

    node = fparser->node;

    if (!(xmlStrcmp (node->name, BAD_CAST "rss")))
    {
        if ((str = xmlGetProp (node, BAD_CAST "version")))
        {
            if (!xmlStrcmp (str, BAD_CAST "2.0") || !xmlStrcmp (str, BAD_CAST "0.92"))
                valid = TRUE;
            else
                valid = FALSE;

            xmlFree (str);

            if (valid)
            {
                child = node->children;
                while (child)
                {
                    if (child->type == XML_ELEMENT_NODE &&
                        !(xmlStrcmp (child->name, BAD_CAST "channel")))
                    {
                        fparser->node = child;
                        return TRUE;
                    }
                    child = child->next;
                }

                feed_parser_set_error (fparser, FEED_PARSE_ERROR_MISSING_ELEMENT,
                                       _("Failed to find \"channel\" element in RSS XML data."));
            }
            else
            {
                feed_parser_set_error (fparser, FEED_PARSE_ERROR_INVALID_VERSION,
                                       _("Unsupported RSS version found."));
            }
        }
    }
    return FALSE;
}

static gboolean
rss_update (FeedParser* fparser)
{
    xmlNodePtr node;
    xmlNodePtr child;
    gint64 date;
    gint64 newdate;

    date = katze_item_get_added (fparser->item);

    node = fparser->node;
    child = node->children;
    while (child)
    {
        if (child->type == XML_ELEMENT_NODE)
        {
            if (!(xmlStrcmp (child->name, BAD_CAST "lastBuildDate")))
            {
                fparser->node = child;
                newdate = feed_get_element_date (fparser);
                fparser->node = node;
                return (date != newdate || date == 0);
            }
        }
        child = child->next;
    }
    return TRUE;
}

static void
rss_preparse_item (FeedParser* fparser)
{
    fparser->item = katze_item_new ();
}

static void
rss_parse_item (FeedParser* fparser)
{
    xmlNodePtr node;
    gchar* content;
    gint64 date;

    node = fparser->node;
    content = NULL;

    if (!xmlStrcmp (node->name, BAD_CAST "guid"))
    {
        content = feed_get_element_string (fparser);
        katze_item_set_token (fparser->item, content);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "title"))
    {
        content = feed_get_element_string (fparser);
        katze_item_set_name (fparser->item, content);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "description"))
    {
        content = feed_get_element_markup (fparser);
        katze_item_set_text (fparser->item, content);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "pubDate"))
    {
        date = feed_get_element_date (fparser);
        katze_item_set_added (fparser->item, date);
    }
    else if (!(xmlStrcmp (node->name, BAD_CAST "link")))
    {
        content = feed_get_element_string (fparser);
        katze_item_set_uri (fparser->item, content);
    }
    g_free (content);
}

static void
rss_postparse_item (FeedParser* fparser)
{
    if (!*fparser->error)
    {
        /*
        * Verify that the required RSS elements are added
        * (as per the spec)
        */
        if (!katze_item_get_name (fparser->item))
        {
            gchar* desc;

            desc = (gchar*)katze_item_get_text (fparser->item);
            if (!desc)
            {
                feed_parser_set_error (fparser, FEED_PARSE_ERROR_MISSING_ELEMENT,
                                       _("Failed to find required RSS \"item\" elements in XML data."));
            }
            else
            {
                desc = feed_remove_markup (g_strdup (desc));
                if (desc)
                {
                    katze_item_set_name (fparser->item, desc);
                    g_free (desc);
                }
                else
                {
                    if ((desc = (gchar*)katze_item_get_uri (fparser->item)))
                        katze_item_set_name (fparser->item, desc);
                }
            }
        }
    }

    if (*fparser->error && KATZE_IS_ITEM (fparser->item))
    {
        g_object_unref (fparser->item);
        fparser->item = NULL;
    }
}

static void
rss_parse_channel (FeedParser* fparser)
{
    FeedParser* eparser;
    xmlNodePtr node;
    gchar* content;
    gint64 date;

    node = fparser->node;
    content = NULL;

    if (!xmlStrcmp (node->name, BAD_CAST "title"))
    {
        content = feed_get_element_string (fparser);
        katze_item_set_name (fparser->item, content);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "description"))
    {
        content = feed_get_element_markup (fparser);
        katze_item_set_text (fparser->item, content);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "lastBuildDate"))
    {
        date = feed_get_element_date (fparser);
        katze_item_set_added (fparser->item, date);
    }
    else if (!(xmlStrcmp (node->name, BAD_CAST "link")))
    {
        content = feed_get_element_string (fparser);
        katze_item_set_uri (fparser->item, content);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "item"))
    {
        eparser = g_new0 (FeedParser, 1);
        eparser->doc = fparser->doc;
        eparser->node = fparser->node;
        eparser->error = fparser->error;
        eparser->preparse = rss_preparse_item;
        eparser->parse = rss_parse_item;
        eparser->postparse = rss_postparse_item;

        feed_parse_node (eparser);

        if (KATZE_IS_ITEM (eparser->item))
        {
            KatzeItem* item;
            if (!(item = feed_item_exists (KATZE_ARRAY (fparser->item), eparser->item)))
                katze_array_add_item (KATZE_ARRAY (fparser->item), eparser->item);
            else
            {
                g_object_unref (eparser->item);
                katze_array_move_item (KATZE_ARRAY (fparser->item), item, 0);
            }
        }
        g_free (eparser);

    }
    g_free (content);
}

static void
rss_postparse_channel (FeedParser* fparser)
{
    if (!*fparser->error)
    {
        /*
         * Verify that the required RSS elements are added
         * (as per the spec)
         */
        if (!katze_item_get_name (fparser->item) ||
            !katze_item_get_text (fparser->item) ||
            !katze_item_get_uri (fparser->item))
        {
            feed_parser_set_error (fparser, FEED_PARSE_ERROR_MISSING_ELEMENT,
                                   _("Failed to find required RSS \"channel\" elements in XML data."));
        }
    }
}

FeedParser*
rss_init_parser (void)
{
    FeedParser* fparser;

    fparser = g_new0 (FeedParser, 1);
    g_return_val_if_fail (fparser, NULL);

    fparser->isvalid = rss_is_valid;
    fparser->update = rss_update;
    fparser->parse = rss_parse_channel;
    fparser->postparse = rss_postparse_channel;

    return fparser;
}

