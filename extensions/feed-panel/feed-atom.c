/*
 Copyright (C) 2009-2010 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "feed-atom.h"

static gboolean
atom_is_valid (FeedParser* fparser)
{
    xmlNodePtr node;

    node = fparser->node;

    if (!(xmlStrcmp (node->name, BAD_CAST "feed")) &&
        !(xmlStrcmp (node->ns->href, BAD_CAST "http://www.w3.org/2005/Atom"))
       )
        return TRUE;

    return FALSE;
}

static gboolean
atom_update (FeedParser* fparser)
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
            if (!(xmlStrcmp (child->name, BAD_CAST "updated")))
            {
                fparser->node = child;
                newdate = feed_get_element_date (fparser);
                fparser->node = node;
                return (date != newdate);
            }
        }
        child = child->next;
    }
    return TRUE;
}

static gboolean
atom_preferred_link (const gchar* old,
                     const gchar* new)
{
    guint i;
    gint iold;
    gint inew;
    gchar* rels[5] =
    {
        "enclosure",
        "via",
        "related",
        "alternate",
        "self",
    };

    iold = inew = -1;
    for (i = 0; i < 5; i++)
    {
        if (old && g_str_equal (old, rels[i]))
            iold = i;
        if (new && g_str_equal (new, rels[i]))
            inew = i;
    }
    return (inew > iold);
}

static void
atom_get_link (KatzeItem* item,
               xmlNodePtr node)
{
    gchar* oldtype;
    gchar* newtype;
    gchar* oldrel;
    gchar* newrel;
    gchar* href;
    gboolean oldishtml;
    gboolean newishtml;
    gboolean newlink;

    oldtype = (gchar*)katze_item_get_meta_string (item, "feedpanel:linktype");
    oldrel = (gchar*)katze_item_get_meta_string (item, "feedpanel:linkrel");

    newtype = (gchar*)xmlGetProp (node, BAD_CAST "type");
    newrel = (gchar*)xmlGetProp (node, BAD_CAST "rel");
    href = (gchar*)xmlGetProp (node, BAD_CAST "href");

    if (!newrel)
        newrel = g_strdup ("alternate");

    oldishtml = (oldtype && g_str_equal (oldtype, "text/html"));
    newishtml = (newtype && g_str_equal (newtype, "text/html"));

    /* prefer HTML links over anything else.
     * if the previous link was already HTML, decide which link
     * we prefer.
     */
    if ((newishtml && oldishtml) || (!newishtml && !oldishtml))
        newlink = atom_preferred_link (oldrel, newrel);
    else
        newlink = newishtml;

    if (newlink)
    {
        katze_item_set_uri (item, href);
        katze_item_set_meta_string (item, "feedpanel:linkrel", newrel);
        katze_item_set_meta_string (item, "feedpanel:linktype", newtype);
    }

    xmlFree (href);
    xmlFree (newrel);
    xmlFree (newtype);
}

static void
atom_preparse_entry (FeedParser* fparser)
{
    fparser->item = katze_item_new ();
}

static void
atom_parse_entry (FeedParser* fparser)
{
    xmlNodePtr node;
    gchar* content;
    gint64 date;

    node = fparser->node;
    content = NULL;

    if (!xmlStrcmp (node->name, BAD_CAST "id"))
    {
        content = feed_get_element_string (fparser);
        katze_item_set_token (fparser->item, content);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "title"))
    {
        content = feed_get_element_string (fparser);
        katze_item_set_name (fparser->item, content);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "summary"))
    {
        content = feed_get_element_markup (fparser);
        katze_item_set_text (fparser->item, content);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "updated"))
    {
        date = feed_get_element_date (fparser);
        katze_item_set_added (fparser->item, date);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "icon"))
    {
        content = feed_get_element_string (fparser);
        katze_item_set_icon (fparser->item, content);
    }
    /* FIXME content can be used in some cases where there
     * is no summary, but it needs additional work,
     * as it can be HTML, or base64 encoded.
     * see the spec.
     */
    else if (!xmlStrcmp (node->name, BAD_CAST "content"))
    {
        /* Only retrieve content if there is no summary */
        if (!katze_item_get_text (fparser->item))
        {
            content = feed_get_element_markup (fparser);
            katze_item_set_text (fparser->item, content);
        }
    }
    else if (!(xmlStrcmp (node->name, BAD_CAST "link")))
        atom_get_link (fparser->item, node);

    g_free (content);
}

static void
atom_postparse_entry (FeedParser* fparser)
{
    if (!*fparser->error)
    {
        /*
        * Verify that the required Atom elements are added
        * (as per the spec)
        */
        if (!katze_item_get_token (fparser->item) ||
            !katze_item_get_name (fparser->item) ||
            !katze_item_get_uri (fparser->item) ||
            !katze_item_get_added (fparser->item))
        {
            feed_parser_set_error (fparser, FEED_PARSE_ERROR_MISSING_ELEMENT,
                                   _("Failed to find required Atom \"entry\" elements in XML data."));
        }
    }

    if (KATZE_IS_ITEM (fparser->item))
    {
        katze_item_set_meta_string (fparser->item, "feedpanel:linkrel", NULL);
        katze_item_set_meta_string (fparser->item, "feedpanel:linktype", NULL);

        if (*fparser->error)
        {
            g_object_unref (fparser->item);
            fparser->item = NULL;
        }
    }
}

static void
atom_parse_feed (FeedParser* fparser)
{
    FeedParser* eparser;
    xmlNodePtr node;
    gchar* content;
    gint64 date;

    node = fparser->node;
    content = NULL;

    if (!xmlStrcmp (node->name, BAD_CAST "id"))
    {
        content = feed_get_element_string (fparser);
        katze_item_set_token (fparser->item, content);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "title"))
    {
        content = feed_get_element_string (fparser);
        katze_item_set_name (fparser->item, content);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "subtitle"))
    {
        content = feed_get_element_markup (fparser);
        katze_item_set_text (fparser->item, content);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "updated"))
    {
        date = feed_get_element_date (fparser);
        katze_item_set_added (fparser->item, date);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "icon"))
    {
        content = feed_get_element_string (fparser);
        katze_item_set_icon (fparser->item, content);
    }
    else if (!(xmlStrcmp (node->name, BAD_CAST "link")))
    {
        atom_get_link (fparser->item, node);
    }
    else if (!xmlStrcmp (node->name, BAD_CAST "entry"))
    {
        eparser = g_new0 (FeedParser, 1);
        eparser->doc = fparser->doc;
        eparser->node = fparser->node;
        eparser->error = fparser->error;
        eparser->preparse = atom_preparse_entry;
        eparser->parse = atom_parse_entry;
        eparser->postparse = atom_postparse_entry;

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
atom_postparse_feed (FeedParser* fparser)
{
    if (KATZE_IS_ARRAY (fparser->item))
    {
        katze_item_set_meta_string (fparser->item, "feedpanel:linkrel", NULL);
        katze_item_set_meta_string (fparser->item, "feedpanel:linktype", NULL);
    }

    if (!*fparser->error)
    {
        /*
         * Verify that the required Atom elements are added
         * (as per the spec)
         */
        if (!katze_item_get_token (fparser->item) ||
            !katze_item_get_name (fparser->item) ||
            !katze_item_get_added (fparser->item))
        {
            feed_parser_set_error (fparser, FEED_PARSE_ERROR_MISSING_ELEMENT,
                                   _("Failed to find required Atom \"feed\" elements in XML data."));
        }
    }
}

FeedParser*
atom_init_parser (void)
{
    FeedParser* fparser;

    fparser = g_new0 (FeedParser, 1);
    g_return_val_if_fail (fparser, NULL);

    fparser->isvalid = atom_is_valid;
    fparser->update = atom_update;
    fparser->parse = atom_parse_feed;
    fparser->postparse = atom_postparse_feed;

    return fparser;
}

