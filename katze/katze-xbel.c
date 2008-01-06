/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

/**
 * TODO:
 *  - Support info > metadata, alias, added, modified, visited
 *  - Compatibility: The Nokia 770 *requires* metadata and folder
 *  - Compatibility: Kazehakase's bookmarks
 *  - Compatibility: Epiphany's bookmarks
 *  - XML Indentation
 **/

#include "katze-xbel.h"

#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "katze-utils.h"

struct _KatzeXbelItemPrivate
{
    guint refs;
    KatzeXbelItemKind kind;
    KatzeXbelItem* parent;

    GList* items;      // folder
    gboolean folded;   // foolder
    gchar* title;      // !separator
    gchar* desc;       // folder and bookmark
    gchar* href;       // bookmark
    //time_t added;    // !separator
    //time_t modfied;  // bookmark
    //time_t visited;  // bookmark
} ;

#define KATZE_XBEL_ITEM_GET_PRIVATE(item) \
    item->priv

// Private: Create a new item of a certain type
static KatzeXbelItem*
katze_xbel_item_new (KatzeXbelItemKind kind)
{
    KatzeXbelItem* item = g_new (KatzeXbelItem, 1);
    KATZE_XBEL_ITEM_GET_PRIVATE (item) = g_new (KatzeXbelItemPrivate, 1);
    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);

    priv->refs = 1;
    priv->parent = NULL;
    priv->kind = kind;
    if (kind == KATZE_XBEL_ITEM_KIND_FOLDER)
    {
        priv->items = NULL;
        priv->folded = TRUE;
    }
    if (kind != KATZE_XBEL_ITEM_KIND_SEPARATOR)
    {
        priv->title = NULL;
        priv->desc  = NULL;
    }
    if (kind == KATZE_XBEL_ITEM_KIND_BOOKMARK)
        priv->href = g_strdup ("");
    return item;
}

/**
 * katze_xbel_bookmark_new:
 *
 * Create a new empty bookmark.
 *
 * Return value: a newly allocated bookmark
 **/
KatzeXbelItem*
katze_xbel_bookmark_new (void)
{
    return katze_xbel_item_new (KATZE_XBEL_ITEM_KIND_BOOKMARK);
}

static KatzeXbelItem*
katze_xbel_bookmark_from_xmlNodePtr (xmlNodePtr cur)
{
    g_return_val_if_fail (cur, NULL);

    KatzeXbelItem* bookmark = katze_xbel_bookmark_new ();
    xmlChar* key = xmlGetProp (cur, (xmlChar*)"href");
    katze_xbel_bookmark_set_href (bookmark, (gchar*)key);
    cur = cur->xmlChildrenNode;
    while (cur)
    {
        if (!xmlStrcmp (cur->name, (const xmlChar*)"title"))
        {
         xmlChar* key = xmlNodeGetContent (cur);
         katze_xbel_item_set_title (bookmark, g_strstrip ((gchar*)key));
        }
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"desc"))
        {
         xmlChar* key = xmlNodeGetContent (cur);
         katze_xbel_item_set_desc (bookmark, g_strstrip ((gchar*)key));
        }
        cur = cur->next;
    }
    return bookmark;
}

/**
 * katze_katze_xbel_separator_new:
 *
 * Create a new separator.
 *
 * The returned item must be freed eventually.
 *
 * Return value: a newly allocated separator.
 **/
KatzeXbelItem*
katze_xbel_separator_new (void)
{
    return katze_xbel_item_new (KATZE_XBEL_ITEM_KIND_SEPARATOR);
}

/**
 * katze_xbel_folder_new:
 *
 * Create a new empty folder.
 *
 * The returned item must be freed eventually.
 *
 * Return value: a newly allocated folder.
 **/
KatzeXbelItem*
katze_xbel_folder_new (void)
{
    return katze_xbel_item_new (KATZE_XBEL_ITEM_KIND_FOLDER);
}

// Private: Create a folder from an xmlNodePtr
static KatzeXbelItem*
katze_xbel_folder_from_xmlNodePtr (xmlNodePtr cur)
{
    g_return_val_if_fail (cur, NULL);

    KatzeXbelItem* folder = katze_xbel_folder_new ();
    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (folder);

    xmlChar* key = xmlGetProp (cur, (xmlChar*)"folded");
    if (key)
    {
        if (!g_ascii_strncasecmp ((gchar*)key, "yes", 3))
            priv->folded = TRUE;
        else if (!g_ascii_strncasecmp ((gchar*)key, "no", 2))
            priv->folded = FALSE;
        else
            g_warning ("XBEL: Unknown value for folded.");
        xmlFree (key);
    }
    cur = cur->xmlChildrenNode;
    while (cur)
    {
        if (!xmlStrcmp (cur->name, (const xmlChar*)"title"))
        {
            xmlChar* key = xmlNodeGetContent (cur);
            katze_assign (priv->title, g_strstrip ((gchar*)key));
        }
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"desc"))
        {
            xmlChar* key = xmlNodeGetContent (cur);
            katze_assign (priv->desc, g_strstrip ((gchar*)key));
        }
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"folder"))
        {
            KatzeXbelItem* item = katze_xbel_folder_from_xmlNodePtr (cur);
            KATZE_XBEL_ITEM_GET_PRIVATE (item)->parent = folder;
            priv->items = g_list_prepend (priv->items, item);
        }
     else if (!xmlStrcmp (cur->name, (const xmlChar*)"bookmark"))
     {
         KatzeXbelItem* item = katze_xbel_bookmark_from_xmlNodePtr (cur);
         priv->parent = folder;
         priv->items = g_list_prepend (priv->items, item);
     }
     else if (!xmlStrcmp (cur->name, (const xmlChar*)"separator"))
     {
         KatzeXbelItem* item = katze_xbel_separator_new ();
         priv->parent = folder;
         priv->items = g_list_prepend (priv->items, item);
     }
        cur = cur->next;
    }
    // Prepending and reversing is faster than appending
    priv->items = g_list_reverse (priv->items);
    return folder;
}

// Private: Loads the contents from an xmlNodePtr into a folder.
static gboolean
katze_xbel_folder_from_xmlDocPtr (KatzeXbelItem* folder,
                                  xmlDocPtr      doc)
{
    g_return_val_if_fail (katze_xbel_folder_is_empty (folder), FALSE);
    g_return_val_if_fail (doc, FALSE);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (folder);

    xmlNodePtr cur = xmlDocGetRootElement (doc);
    xmlChar* version = xmlGetProp (cur, (xmlChar*)"version");
    if (xmlStrcmp (version, (xmlChar*)"1.0"))
        g_warning ("XBEL version is not 1.0.");
    xmlFree (version);

    katze_assign (priv->title, (gchar*)xmlGetProp (cur, (xmlChar*)"title"));
    katze_assign (priv->desc, (gchar*)xmlGetProp (cur, (xmlChar*)"desc"));
    if ((cur = xmlDocGetRootElement (doc)) == NULL)
    {
        // Empty document
        return FALSE;
    }
    if (xmlStrcmp (cur->name, (const xmlChar*)"xbel"))
    {
        // Wrong document kind
        return FALSE;
    }
    cur = cur->xmlChildrenNode;
    while (cur)
    {
        KatzeXbelItem* item = NULL;
        if (!xmlStrcmp (cur->name, (const xmlChar*)"folder"))
            item = katze_xbel_folder_from_xmlNodePtr (cur);
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"bookmark"))
            item = katze_xbel_bookmark_from_xmlNodePtr (cur);
        else if (!xmlStrcmp (cur->name, (const xmlChar*)"separator"))
            item = katze_xbel_separator_new ();
        /*else if (!xmlStrcmp (cur->name, (const xmlChar*)"info"))
            item = katze_xbel_parse_info (xbel, cur);*/
        if (item)
        {
            KATZE_XBEL_ITEM_GET_PRIVATE (item)->parent = folder;
            priv->items = g_list_prepend (priv->items, item);
        }
        cur = cur->next;
    }
    // Prepending and reversing is faster than appending
    priv->items = g_list_reverse (priv->items);
    return TRUE;
}

/**
 * katze_xbel_item_ref:
 * @item: a valid item
 *
 * Ref an KatzeXbelItem.
 *
 * Ref means that the reference count is increased by one.
 *
 * This has no effect on children of a folder.
 **/
void
katze_xbel_item_ref (KatzeXbelItem* item)
{
    g_return_if_fail (item);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);
    priv->refs++;
}

/**
 * katze_xbel_item_unref:
 * @item: a valid item
 *
 * Unref an KatzeXbelItem. If @item is a folder all of its children will also
 *  be unreffed automatically.
 *
 * Unref means that the reference count is decreased. If there are no
 * references left, the memory will be freed and if needed removed from
 * its containing folder.
 **/
void
katze_xbel_item_unref (KatzeXbelItem* item)
{
    g_return_if_fail (item);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);
    priv->refs--;
    if (priv->refs)
        return;

    if (priv->parent)
        katze_xbel_folder_remove_item (priv->parent, item);

    if (priv->kind == KATZE_XBEL_ITEM_KIND_FOLDER)
    {
        guint n = katze_xbel_folder_get_n_items (item);
        guint i;
        for (i = 0; i < n; i++)
        {
            KatzeXbelItem* _item = katze_xbel_folder_get_nth_item (item, i);
            KATZE_XBEL_ITEM_GET_PRIVATE (_item)->parent = NULL;
            katze_xbel_item_unref (_item);
        }
        g_list_free (priv->items);
    }
    if (priv->kind != KATZE_XBEL_ITEM_KIND_SEPARATOR)
    {
        g_free (priv->title);
        g_free (priv->desc);
    }
    if (priv->kind == KATZE_XBEL_ITEM_KIND_BOOKMARK)
        g_free (priv->href);
    g_free (item);
}

/**
 * katze_xbel_item_copy:
 * @item: the item to copy
 *
 * Copy an KatzeXbelItem.
 *
 * The returned item must be unreffed eventually.
 *
 * Return value: a copy of @item
 **/
KatzeXbelItem*
katze_xbel_item_copy (KatzeXbelItem* item)
{
    g_return_val_if_fail (item, NULL);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);
    KatzeXbelItem* copy = katze_xbel_item_new (priv->kind);

    if (katze_xbel_item_is_folder (item))
    {
        guint n = katze_xbel_folder_get_n_items (item);
        guint i;
        for (i = 0; i < n; i++)
        {
            KatzeXbelItem* _item = katze_xbel_folder_get_nth_item (item, i);
            katze_xbel_folder_append_item (copy, katze_xbel_item_copy (_item));
        }
    }
    if (priv->kind != KATZE_XBEL_ITEM_KIND_SEPARATOR)
    {
        katze_xbel_item_set_title (copy, priv->title);
        katze_xbel_item_set_desc (copy, priv->desc);
    }
    if (priv->kind == KATZE_XBEL_ITEM_KIND_BOOKMARK)
        katze_xbel_bookmark_set_href (copy, priv->href);
    return copy;
}

GType
katze_xbel_item_get_type (void)
{
    static GType type = 0;
    if (!type)
        type = g_pointer_type_register_static ("katze_xbel_item");
    return type;
}

/**
 * katze_xbel_folder_append_item:
 * @folder: a folder
 * @item: the item to append
 *
 * Append the given item to a folder.
 *
 * The item is actually moved and must not be contained in another folder.
 *
 **/
void
katze_xbel_folder_append_item (KatzeXbelItem* folder,
                               KatzeXbelItem* item)
{
    g_return_if_fail (!katze_xbel_item_get_parent (item));
    g_return_if_fail (katze_xbel_item_is_folder (folder));

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (folder);
    priv->items = g_list_append (priv->items, item);

    KATZE_XBEL_ITEM_GET_PRIVATE (item)->parent = folder;
}

/**
 * katze_xbel_folder_prepend_item:
 * @folder: a folder
 * @item: the item to prepend
 *
 * Prepend the given item to a folder.
 *
 * The item is actually moved and must not be contained in another folder.
 *
 **/
void
katze_xbel_folder_prepend_item (KatzeXbelItem* folder,
                                KatzeXbelItem* item)
{
    g_return_if_fail (!katze_xbel_item_get_parent (item));
    g_return_if_fail (katze_xbel_item_is_folder (folder));

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (folder);
    priv->items = g_list_prepend (priv->items, item);

    KATZE_XBEL_ITEM_GET_PRIVATE (item)->parent = folder;
}

/**
 * katze_xbel_folder_remove_item:
 * @folder: a folder
 * @item:   the item to remove
 *
 * Remove the given @item from a @folder.
 **/
void
katze_xbel_folder_remove_item (KatzeXbelItem* folder,
                               KatzeXbelItem* item)
{
    g_return_if_fail (item);
    g_return_if_fail (katze_xbel_item_get_parent(folder) != item);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (folder);
    // Fortunately we know that items are unique
    priv->items = g_list_remove (priv->items, item);

    KATZE_XBEL_ITEM_GET_PRIVATE (item)->parent = NULL;
}

/**
 * katze_xbel_folder_get_n_items:
 * @folder: a folder
 *
 * Retrieve the number of items contained in the given @folder.
 *
 * Return value: number of items
 **/
guint
katze_xbel_folder_get_n_items (KatzeXbelItem* folder)
{
    g_return_val_if_fail (katze_xbel_item_is_folder (folder), 0);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (folder);
    return g_list_length (priv->items);
}

/**
 * katze_xbel_folder_get_nth_item:
 * @folder: a folder
 * @n: the index of an item
 *
 * Retrieve an item contained in the given @folder by its index.
 *
 * Return value: the item at the given index or %NULL
 **/
KatzeXbelItem*
katze_xbel_folder_get_nth_item (KatzeXbelItem* folder,
                                guint          n)
{
    g_return_val_if_fail (katze_xbel_item_is_folder(folder), NULL);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (folder);
    return (KatzeXbelItem*) g_list_nth_data (priv->items, n);
}

/**
 * katze_xbel_folder_is_empty:
 * @folder: A folder.
 *
 * Determines wether or not a folder contains no items. This is significantly
 *  faster than katze_xbel_folder_get_n_items for this particular purpose.
 *
 * Return value: Wether the given @folder is folded.
 **/
gboolean
katze_xbel_folder_is_empty (KatzeXbelItem* folder)
{
    return !katze_xbel_folder_get_nth_item (folder, 0);
}

/**
 * katze_xbel_folder_get_folded:
 * @folder: A folder.
 *
 * Determines wether or not a folder is folded. If a folder is not folded
 *  it should not be exposed in a user interface.
 *
 * New folders are folded by default.
 *
 * Return value: Wether the given @folder is folded.
 **/
gboolean
katze_xbel_folder_get_folded (KatzeXbelItem* folder)
{
    g_return_val_if_fail (katze_xbel_item_is_folder (folder), TRUE);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (folder);
    return priv->folded;
}

/**
 * katze_xbel_item_get_kind:
 * @item: A item.
 *
 * Determines the kind of an item.
 *
 * Return value: The kind of the given @item.
 **/
KatzeXbelItemKind
katze_xbel_item_get_kind (KatzeXbelItem* item)
{
    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);
    return priv->kind;
}

/**
 * katze_xbel_item_get_parent:
 * @item: A valid item.
 *
 * Retrieves the parent folder of an item.
 *
 * Return value: The parent folder of the given @item or %NULL.
 **/
KatzeXbelItem*
katze_xbel_item_get_parent (KatzeXbelItem* item)
{
    g_return_val_if_fail (item, NULL);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);
    return priv->parent;
}

/**
 * katze_xbel_item_get_title:
 * @item: A valid item.
 *
 * Retrieves the title of an item.
 *
 * Return value: The title of the given @item or %NULL.
 **/
G_CONST_RETURN gchar*
katze_xbel_item_get_title (KatzeXbelItem* item)
{
    g_return_val_if_fail (!katze_xbel_item_is_separator (item), NULL);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);
    return priv->title;
}

/**
 * katze_xbel_item_get_desc:
 * @item: A valid item.
 *
 * Retrieves the description of an item.
 *
 * Return value: The description of the @item or %NULL.
 **/
G_CONST_RETURN gchar*
katze_xbel_item_get_desc (KatzeXbelItem* item)
{
    g_return_val_if_fail (!katze_xbel_item_is_separator (item), NULL);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);
    return priv->desc;
}

/**
 * katze_xbel_bookmark_get_href:
 * @bookmark: A bookmark.
 *
 * Retrieves the uri of a bookmark. The value is guaranteed to not be %NULL.
 *
 * Return value: The uri of the @bookmark.
 **/
G_CONST_RETURN gchar*
katze_xbel_bookmark_get_href (KatzeXbelItem* bookmark)
{
    g_return_val_if_fail (katze_xbel_item_is_bookmark (bookmark), NULL);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (bookmark);
    return priv->href;
}

/**
 * katze_xbel_item_is_bookmark:
 * @item: A valid item.
 *
 * Determines wether or not an item is a bookmark.
 *
 * Return value: %TRUE if the @item is a bookmark.
 **/
gboolean
katze_xbel_item_is_bookmark (KatzeXbelItem* item)
{
    g_return_val_if_fail (item, FALSE);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);
    return priv->kind == KATZE_XBEL_ITEM_KIND_BOOKMARK;
}

/**
 * katze_xbel_item_is_separator:
 * @item: A valid item.
 *
 * Determines wether or not an item is a separator.
 *
 * Return value: %TRUE if the @item is a separator.
 **/
gboolean katze_xbel_item_is_separator (KatzeXbelItem* item)
{
    g_return_val_if_fail (item, FALSE);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);
    return priv->kind == KATZE_XBEL_ITEM_KIND_SEPARATOR;
}

/**
 * katze_xbel_item_is_folder:
 * @item: A valid item.
 *
 * Determines wether or not an item is a folder.
 *
 * Return value: %TRUE if the item is a folder.
 **/
gboolean
katze_xbel_item_is_folder (KatzeXbelItem* item)
{
    g_return_val_if_fail (item, FALSE);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);
    return priv->kind == KATZE_XBEL_ITEM_KIND_FOLDER;
}

/**
 * katze_xbel_folder_set_folded:
 * @folder: A folder.
 * @folded: TRUE if the folder is folded.
 *
 * Sets the foldedness of the @folder.
 **/
void
katze_xbel_folder_set_folded (KatzeXbelItem* folder,
                              gboolean folded)
{
    g_return_if_fail (katze_xbel_item_is_folder (folder));

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (folder);
    priv->folded = folded;
}

/**
 * katze_xbel_item_set_title:
 * @item: A valid item.
 * @title: A string to use for the title.
 *
 * Sets the title of the @item.
 **/
void
katze_xbel_item_set_title (KatzeXbelItem* item,
                           const gchar*   title)
{
    g_return_if_fail (!katze_xbel_item_is_separator (item));

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);
    katze_assign (priv->title, g_strdup (title));
}

/**
 * katze_xbel_item_set_desc:
 * @item: A valid item.
 * @title: A string to use for the description.
 *
 * Sets the description of the @item.
 **/
void
katze_xbel_item_set_desc (KatzeXbelItem* item,
                          const gchar*   desc)
{
    g_return_if_fail (!katze_xbel_item_is_separator (item));

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);
    katze_assign (priv->desc, g_strdup (desc));
}

/**
 * katze_xbel_bookmark_set_href:
 * @bookmark: A bookmark.
 * @href: A string containing a valid uri.
 *
 * Sets the uri of the bookmark.
 *
 * The uri must not be %NULL.
 *
 * This uri is not currently validated in any way. This may change in the future.
 **/
void
katze_xbel_bookmark_set_href (KatzeXbelItem* bookmark,
                              const gchar*   href)
{
    g_return_if_fail (katze_xbel_item_is_bookmark (bookmark));
    g_return_if_fail (href);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (bookmark);
    katze_assign (priv->href, g_strdup (href));
}

gboolean
katze_xbel_folder_from_data (KatzeXbelItem* folder,
                             const gchar*   data,
                             GError**       error)
{
    g_return_val_if_fail (katze_xbel_folder_is_empty (folder), FALSE);
    g_return_val_if_fail (data, FALSE);
    xmlDocPtr doc;
    if((doc = xmlParseMemory (data, strlen (data))) == NULL)
    {
        // No valid xml or broken encoding
        *error = g_error_new (KATZE_XBEL_ERROR, KATZE_XBEL_ERROR_READ,
                              "Malformed document.");
        return FALSE;
    }
    if (!katze_xbel_folder_from_xmlDocPtr (folder, doc))
    {
        // Parsing failed
        xmlFreeDoc(doc);
        *error = g_error_new (KATZE_XBEL_ERROR, KATZE_XBEL_ERROR_READ,
                              "Malformed document.");
        return FALSE;
    }
    xmlFreeDoc(doc);
    return TRUE;
}

/**
 * katze_xbel_folder_from_file:
 * @folder: An empty folder.
 * @file: A relative path to a file.
 * @error: return location for a GError or %NULL
 *
 * Tries to load @file.
 *
 * Return value: %TRUE on success or %FALSE when an error occured.
 **/
gboolean
katze_xbel_folder_from_file (KatzeXbelItem* folder,
                             const gchar*   file,
                             GError**       error)
{
    g_return_val_if_fail (katze_xbel_folder_is_empty (folder), FALSE);
    g_return_val_if_fail (file, FALSE);
    if (!g_file_test (file, G_FILE_TEST_EXISTS))
    {
        // File doesn't exist
        *error = g_error_new (G_FILE_ERROR, G_FILE_ERROR_NOENT,
                              "File not found.");
        return FALSE;
    }
    xmlDocPtr doc;
    if ((doc = xmlParseFile (file)) == NULL)
    {
        // No valid xml or broken encoding
        *error = g_error_new (KATZE_XBEL_ERROR, KATZE_XBEL_ERROR_READ,
                              "Malformed document.");
        return FALSE;
    }
    if (!katze_xbel_folder_from_xmlDocPtr (folder, doc))
    {
        // Parsing failed
        xmlFreeDoc (doc);
        *error = g_error_new (KATZE_XBEL_ERROR, KATZE_XBEL_ERROR_READ,
                             "Malformed document.");
        return FALSE;
    }
    xmlFreeDoc (doc);
    return TRUE;
}

/**
 * katze_xbel_folder_from_data_dirs:
 * @folder: An empty folder.
 * @file: A relative path to a file.
 * @full_path: return location for the full path of the file or %NULL
 * @error: return location for a GError or %NULL
 *
 * Tries to load @file from the user data dir or any of the system data dirs.
 *
 * Return value: %TRUE on success or %FALSE when an error occured.
 **/
gboolean
katze_xbel_folder_from_data_dirs (KatzeXbelItem* folder,
                                  const gchar*   file,
                                  gchar**        full_path,
                                  GError**       error)
{
    g_return_val_if_fail (katze_xbel_folder_is_empty (folder), FALSE);
    g_return_val_if_fail (file, FALSE);
    // FIXME: Essentially unimplemented

    *error = g_error_new (KATZE_XBEL_ERROR, KATZE_XBEL_ERROR_READ,
                          "Malformed document.");
    return FALSE;
}

static gchar*
katze_xbel_xml_element (const gchar* name,
                        const gchar* value)
{
    if (!value)
        return g_strdup ("");
    gchar* valueEscaped = g_markup_escape_text (value, -1);
    gchar* markup = g_strdup_printf ("<%s>%s</%s>\n",
                                     name, valueEscaped, name);
    g_free (valueEscaped);
    return markup;
}

static gchar*
katze_xbel_item_to_data (KatzeXbelItem* item)
{
    g_return_val_if_fail (item, NULL);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (item);

    gchar* markup = NULL;
    switch (priv->kind)
    {
    case KATZE_XBEL_ITEM_KIND_FOLDER:
    {
        GString* _markup = g_string_new (NULL);
        guint n = katze_xbel_folder_get_n_items (item);
        guint i;
        for (i = 0; i < n; i++)
        {
            KatzeXbelItem* _item = katze_xbel_folder_get_nth_item (item, i);
            gchar* item_markup = katze_xbel_item_to_data (_item);
            g_string_append (_markup, item_markup);
            g_free (item_markup);
        }
        gchar* folded = priv->folded ? NULL : g_strdup_printf (" folded=\"no\"");
        gchar* title = katze_xbel_xml_element ("title", priv->title);
        gchar* desc = katze_xbel_xml_element ("desc", priv->desc);
        markup = g_strdup_printf ("<folder%s>\n%s%s%s</folder>\n",
                                  folded ? folded : "",
                                  title, desc,
                                  g_string_free (_markup, FALSE));
        g_free (folded);
        g_free (title);
        g_free (desc);
        break;
    }
    case KATZE_XBEL_ITEM_KIND_BOOKMARK:
    {
        gchar* href_escaped = g_markup_escape_text (priv->href, -1);
        gchar* href = g_strdup_printf (" href=\"%s\"", href_escaped);
        g_free (href_escaped);
        gchar* title = katze_xbel_xml_element ("title", priv->title);
        gchar* desc = katze_xbel_xml_element ("desc", priv->desc);
        markup = g_strdup_printf ("<bookmark%s>\n%s%s%s</bookmark>\n",
                                  href,
                                  title, desc,
                                  "");
        g_free (href);
        g_free (title);
        g_free (desc);
        break;
    }
    case KATZE_XBEL_ITEM_KIND_SEPARATOR:
        markup = g_strdup ("<separator/>\n");
        break;
    default:
        g_warning ("XBEL: Unknown item kind");
    }
    return markup;
}

/**
 * katze_xbel_folder_to_data:
 * @folder: A folder.
 * @length: return location for the length of the created string or %NULL
 * @error: return location for a GError or %NULL
 *
 * Retrieve the contents of @folder as a string.
 *
 * Return value: %TRUE on success or %FALSE when an error occured.
 **/
gchar*
katze_xbel_folder_to_data (KatzeXbelItem* folder,
                           gsize*         length,
                           GError**       error)
{
    g_return_val_if_fail (katze_xbel_item_is_folder (folder), FALSE);

    KatzeXbelItemPrivate* priv = KATZE_XBEL_ITEM_GET_PRIVATE (folder);

    GString* inner_markup = g_string_new (NULL);
    guint n = katze_xbel_folder_get_n_items (folder);
    guint i;
    for (i = 0; i < n; i++)
    {
        KatzeXbelItem* item = katze_xbel_folder_get_nth_item (folder, i);
        gchar* sItem = katze_xbel_item_to_data (item);
        g_string_append (inner_markup, sItem);
        g_free (sItem);
    }
    gchar* title = katze_xbel_xml_element ("title", priv->title);
    gchar* desc = katze_xbel_xml_element ("desc", priv->desc);
    gchar* outer_markup;
    outer_markup = g_strdup_printf (
                   "%s%s<xbel version=\"1.0\">\n%s%s%s</xbel>\n",
                   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n",
                   "<!DOCTYPE xbel PUBLIC \"+//IDN python.org//DTD "
                   "XML Bookmark Exchange Language 1.0//EN//XML\" "
                   "\"http://www.python.org/topics/xml/dtds/xbel-1.0.dtd\">\n",
                   title,
                   desc,
                   g_string_free (inner_markup, FALSE));
    g_free (title);
    g_free (desc);

    if (length)
        *length = strlen (outer_markup);
    return outer_markup;
}

/**
 * katze_xbel_folder_to_file:
 * @folder: A folder.
 * @file: The destination filename.
 * @error: return location for a GError or %NULL
 *
 * Write the contents of @folder to the specified file, creating it if necessary.
 *
 * Return value: %TRUE on success or %FALSE when an error occured.
 **/
gboolean
katze_xbel_folder_to_file (KatzeXbelItem* folder,
                           const gchar*   file,
                           GError**       error)
{
    g_return_val_if_fail (file, FALSE);
    gchar* data;
    if (!(data = katze_xbel_folder_to_data (folder, NULL, error)))
        return FALSE;
    FILE* fp;
    if(!(fp = fopen (file, "w")))
    {
        *error = g_error_new (G_FILE_ERROR, G_FILE_ERROR_ACCES,
                              "Writing failed.");
        return FALSE;
    }
    fputs (data, fp);
    fclose (fp);
    g_free (data);
    return TRUE;
}
