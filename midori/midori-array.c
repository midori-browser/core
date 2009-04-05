/*
 Copyright (C) 2007-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <katze/katze.h>

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include <glib/gi18n.h>

#if HAVE_LIBXML
    #include <libxml/parser.h>
    #include <libxml/tree.h>
#endif

#if HAVE_LIBXML
static KatzeItem*
katze_item_from_xmlNodePtr (xmlNodePtr cur)
{
    KatzeItem* item;
    xmlChar* key;

    item = katze_item_new ();
    key = xmlGetProp (cur, (xmlChar*)"href");
    katze_item_set_uri (item, (gchar*)key);
    g_free (key);

    cur = cur->xmlChildrenNode;
    while (cur)
    {
        if (!xmlStrcmp (cur->name, (const xmlChar*)"title"))
        {
            key = xmlNodeGetContent (cur);
            katze_item_set_name (item, g_strstrip ((gchar*)key));
            g_free (key);
        }
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"desc"))
        {
            key = xmlNodeGetContent (cur);
            katze_item_set_text (item, g_strstrip ((gchar*)key));
            g_free (key);
        }
        cur = cur->next;
    }
    return item;
}

/* Create an array from an xmlNodePtr */
static KatzeArray*
katze_array_from_xmlNodePtr (xmlNodePtr cur)
{
    KatzeArray* array;
    xmlChar* key;
    KatzeItem* item;

    array = katze_array_new (KATZE_TYPE_ARRAY);

    key = xmlGetProp (cur, (xmlChar*)"folded");
    if (key)
    {
        /* if (!g_ascii_strncasecmp ((gchar*)key, "yes", 3))
            folder->folded = TRUE;
        else if (!g_ascii_strncasecmp ((gchar*)key, "no", 2))
            folder->folded = FALSE;
        else
            g_warning ("XBEL: Unknown value for folded."); */
        xmlFree (key);
    }

    cur = cur->xmlChildrenNode;
    while (cur)
    {
        if (!xmlStrcmp (cur->name, (const xmlChar*)"title"))
        {
            key = xmlNodeGetContent (cur);
            katze_item_set_name (KATZE_ITEM (array), g_strstrip ((gchar*)key));
        }
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"desc"))
        {
            key = xmlNodeGetContent (cur);
            katze_item_set_text (KATZE_ITEM (array), g_strstrip ((gchar*)key));
        }
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"folder"))
        {
            item = (KatzeItem*)katze_array_from_xmlNodePtr (cur);
            katze_array_add_item (array, item);
        }
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"bookmark"))
        {
            item = katze_item_from_xmlNodePtr (cur);
            katze_array_add_item (array, item);
        }
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"separator"))
        {
            item = katze_item_new ();
            katze_array_add_item (array, item);
        }
        cur = cur->next;
    }
    return array;
}

/* Loads the contents from an xmlNodePtr into an array. */
static gboolean
katze_array_from_xmlDocPtr (KatzeArray* array,
                            xmlDocPtr   doc)
{
    xmlNodePtr cur;
    xmlChar* version;
    gchar* value;
    KatzeItem* item;

    cur = xmlDocGetRootElement (doc);
    version = xmlGetProp (cur, (xmlChar*)"version");
    if (xmlStrcmp (version, (xmlChar*)"1.0"))
        g_warning ("XBEL version is not 1.0.");
    xmlFree (version);

    value = (gchar*)xmlGetProp (cur, (xmlChar*)"title");
    katze_item_set_name (KATZE_ITEM (array), value);
    g_free (value);

    value = (gchar*)xmlGetProp (cur, (xmlChar*)"desc");
    katze_item_set_text (KATZE_ITEM (array), value);
    g_free (value);

    if ((cur = xmlDocGetRootElement (doc)) == NULL)
    {
        /* Empty document */
        return FALSE;
    }
    if (xmlStrcmp (cur->name, (const xmlChar*)"xbel"))
    {
        /* Wrong document kind */
        return FALSE;
    }
    cur = cur->xmlChildrenNode;
    while (cur)
    {
        item = NULL;
        if (!xmlStrcmp (cur->name, (const xmlChar*)"folder"))
            item = (KatzeItem*)katze_array_from_xmlNodePtr (cur);
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"bookmark"))
            item = katze_item_from_xmlNodePtr (cur);
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"separator"))
            item = katze_item_new ();
        /*else if (!xmlStrcmp (cur->name, (const xmlChar*)"info"))
            item = katze_xbel_parse_info (xbel, cur);*/
        if (item)
            katze_array_add_item (array, item);
        cur = cur->next;
    }
    return TRUE;
}

/**
 * midori_array_from_file:
 * @array: a #KatzeArray
 * @filename: a filename to load from
 * @format: the desired format
 * @error: a #GError or %NULL
 *
 * Loads the contents of a file in the specified format.
 *
 * Return value: %TRUE on success, %FALSE otherwise
 *
 * Since: 0.1.6
 **/
gboolean
midori_array_from_file (KatzeArray*  array,
                        const gchar* filename,
                        const gchar* format,
                        GError**     error)
{
    xmlDocPtr doc;

    g_return_val_if_fail (katze_array_is_a (array, KATZE_TYPE_ITEM), FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);
    g_return_val_if_fail (!g_strcmp0 (format, "xbel"), FALSE);
    g_return_val_if_fail (!error || !*error, FALSE);

    if (!g_file_test (filename, G_FILE_TEST_EXISTS))
    {
        /* File doesn't exist */
        *error = g_error_new_literal (G_FILE_ERROR, G_FILE_ERROR_NOENT,
                                      _("File not found."));
        return FALSE;
    }

    if ((doc = xmlParseFile (filename)) == NULL)
    {
        /* No valid xml or broken encoding */
        *error = g_error_new_literal (G_FILE_ERROR, G_FILE_ERROR_FAILED,
                                      _("Malformed document."));
        return FALSE;
    }

    if (!katze_array_from_xmlDocPtr (array, doc))
    {
        /* Parsing failed */
        xmlFreeDoc (doc);
        *error = g_error_new_literal (G_FILE_ERROR, G_FILE_ERROR_FAILED,
                                      _("Malformed document."));
        return FALSE;
    }
    xmlFreeDoc (doc);
    return TRUE;
}
#endif

static gchar*
_simple_xml_element (const gchar* name,
                     const gchar* value)
{
    gchar* value_escaped;
    gchar* markup;

    if (!value)
        return g_strdup ("");
    value_escaped = g_markup_escape_text (value, -1);
    markup = g_strdup_printf ("<%s>%s</%s>\n", name, value_escaped, name);
    g_free (value_escaped);
    return markup;
}

static gchar*
katze_item_to_data (KatzeItem* item)
{
    gchar* markup;

    g_return_val_if_fail (KATZE_IS_ITEM (item), NULL);

    markup = NULL;
    if (KATZE_IS_ARRAY (item))
    {
        GString* _markup = g_string_new (NULL);
        guint i = 0;
        KatzeItem* _item;
        while ((_item = katze_array_get_nth_item (KATZE_ARRAY (item), i++)))
        {
            gchar* item_markup = katze_item_to_data (_item);
            g_string_append (_markup, item_markup);
            g_free (item_markup);
        }
        /* gchar* folded = item->folded ? NULL : g_strdup_printf (" folded=\"no\""); */
        gchar* title = _simple_xml_element ("title", katze_item_get_name (item));
        gchar* desc = _simple_xml_element ("desc", katze_item_get_text (item));
        markup = g_strdup_printf ("<folder%s>\n%s%s%s</folder>\n",
                                  "" /* folded ? folded : "" */,
                                  title, desc,
                                  _markup->str);
        g_string_free (_markup, TRUE);
        /* g_free (folded); */
        g_free (title);
        g_free (desc);
    }
    else if (katze_item_get_uri (item))
    {
        gchar* href_escaped = g_markup_escape_text (katze_item_get_uri (item), -1);
        gchar* href = g_strdup_printf (" href=\"%s\"", href_escaped);
        g_free (href_escaped);
        gchar* title = _simple_xml_element ("title", katze_item_get_name (item));
        gchar* desc = _simple_xml_element ("desc", katze_item_get_text (item));
        markup = g_strdup_printf ("<bookmark%s>\n%s%s%s</bookmark>\n",
                                  href,
                                  title, desc,
                                  "");
        g_free (href);
        g_free (title);
        g_free (desc);
    }
    else
        markup = g_strdup ("<separator/>\n");
    return markup;
}

static gchar*
katze_array_to_xbel (KatzeArray* array,
                     GError**    error)
{
    GString* inner_markup;
    guint i;
    KatzeItem* item;
    gchar* item_xml;
    gchar* title;
    gchar* desc;
    gchar* outer_markup;

    inner_markup = g_string_new (NULL);
    i = 0;
    while ((item = katze_array_get_nth_item (array, i++)))
    {
        item_xml = katze_item_to_data (item);
        g_string_append (inner_markup, item_xml);
        g_free (item_xml);
    }

    title = _simple_xml_element ("title", katze_item_get_name (KATZE_ITEM (array)));
    desc = _simple_xml_element ("desc", katze_item_get_text (KATZE_ITEM (array)));
    outer_markup = g_strdup_printf (
                   "%s%s<xbel version=\"1.0\">\n%s%s%s</xbel>\n",
                   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n",
                   "<!DOCTYPE xbel PUBLIC \"+//IDN python.org//DTD "
                   "XML Bookmark Exchange Language 1.0//EN//XML\" "
                   "\"http://www.python.org/topics/xml/dtds/xbel-1.0.dtd\">\n",
                   title,
                   desc,
                   inner_markup->str);
    g_string_free (inner_markup, TRUE);
    g_free (title);
    g_free (desc);

    return outer_markup;
}

static gboolean
midori_array_to_file_xbel (KatzeArray*  array,
                           const gchar* filename,
                           GError**     error)
{
    gchar* data;
    FILE* fp;

    if (!(data = katze_array_to_xbel (array, error)))
        return FALSE;
    if (!(fp = fopen (filename, "w")))
    {
        *error = g_error_new_literal (G_FILE_ERROR, G_FILE_ERROR_ACCES,
                                      _("Writing failed."));
        return FALSE;
    }
    fputs (data, fp);
    fclose (fp);
    g_free (data);
    return TRUE;
}

/**
 * midori_array_to_file:
 * @array: a #KatzeArray
 * @filename: a filename to load from
 * @format: the desired format
 * @error: a #GError or %NULL
 *
 * Saves the contents to a file in the specified format.
 *
 * Return value: %TRUE on success, %FALSE otherwise
 *
 * Since: 0.1.6
 **/
gboolean
midori_array_to_file (KatzeArray*  array,
                      const gchar* filename,
                      const gchar* format,
                      GError**     error)
{
    g_return_val_if_fail (katze_array_is_a (array, KATZE_TYPE_ITEM), FALSE);
    g_return_val_if_fail (filename, FALSE);
    g_return_val_if_fail (!error || !*error, FALSE);

    if (!g_strcmp0 (format, "xbel"))
        return midori_array_to_file_xbel (array, filename, error);
    g_critical ("Cannot write KatzeArray to unknown format '%s'.", format);
    return FALSE;
}
