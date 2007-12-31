/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

/**
 * This is an implementation of XBEL based on Glib and libXML2.
 *
 * Design Goals:
 *  - XbelItem is the only opaque public data structure.
 *  - The interface should be intuitive and familiar to Glib users.
 *  - There should be no public exposure of libXML2 specific code.
 *  - Bookmarks should actually be easily exchangeable between programs.
 *
 * TODO:
 *  - Support info > metadata, alias, added, modified, visited
 *  - Compatibility: The Nokia 770 *requires* metadata and folder
 *  - Compatibility: Kazehakase's bookmarks
 *  - Compatibility: Epiphany's bookmarks
 *  - XML Indentation
 *  - Export and import to other formats
 **/

#include "xbel.h"

#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

// Private: Create a new item of a certain type
static XbelItem* xbel_item_new(XbelItemKind kind)
{
    XbelItem* item = g_new(XbelItem, 1);
    item->refs = 1;
    item->parent = NULL;
    item->kind = kind;
    if(kind == XBEL_ITEM_FOLDER)
    {
        item->items = NULL;
        item->folded = TRUE;
    }
    if(kind != XBEL_ITEM_SEPARATOR)
    {
        item->title = NULL;
        item->desc  = NULL;
    }
    if(kind == XBEL_ITEM_BOOKMARK)
        item->href = g_strdup("");
    return item;
}

/**
 * xbel_bookmark_new:
 *
 * Create a new empty bookmark.
 *
 * Return value: a newly allocated bookmark
 **/
XbelItem* xbel_bookmark_new(void)
{
    return xbel_item_new(XBEL_ITEM_BOOKMARK);
}

static XbelItem* xbel_bookmark_from_xmlNodePtr(xmlNodePtr cur)
{
    g_return_val_if_fail(cur != NULL, NULL);
    XbelItem* bookmark = xbel_bookmark_new();
    xmlChar* key = xmlGetProp(cur, (xmlChar*)"href");
    xbel_bookmark_set_href(bookmark, (gchar*)key);
    cur = cur->xmlChildrenNode;
    while(cur != NULL)
    {
        if(!xmlStrcmp(cur->name, (const xmlChar*)"title"))
        {
         xmlChar* key = xmlNodeGetContent(cur);
         bookmark->title = (gchar*)g_strstrip((gchar*)key);
        }
        else if(!xmlStrcmp(cur->name, (const xmlChar*)"desc"))
        {
         xmlChar* key = xmlNodeGetContent(cur);
         bookmark->desc = (gchar*)g_strstrip((gchar*)key);
        }
        cur = cur->next;
    }
    return bookmark;
}

/**
 * xbel_separator_new:
 *
 * Create a new separator.
 *
 * The returned item must be freed eventually.
 *
 * Return value: a newly allocated separator.
 **/
XbelItem* xbel_separator_new(void)
{
    return xbel_item_new(XBEL_ITEM_SEPARATOR);
}

/**
 * xbel_folder_new:
 *
 * Create a new empty folder.
 *
 * The returned item must be freed eventually.
 *
 * Return value: a newly allocated folder.
 **/
XbelItem* xbel_folder_new(void)
{
    return xbel_item_new(XBEL_ITEM_FOLDER);
}

// Private: Create a folder from an xmlNodePtr
static XbelItem* xbel_folder_from_xmlNodePtr(xmlNodePtr cur)
{
    g_return_val_if_fail(cur != NULL, NULL);
    XbelItem* folder = xbel_folder_new();
    xmlChar* key = xmlGetProp(cur, (xmlChar*)"folded");
    if(key)
    {
        if(!g_ascii_strncasecmp((gchar*)key, "yes", 3))
            folder->folded = TRUE;
        else if(!g_ascii_strncasecmp((gchar*)key, "no", 2))
            folder->folded = FALSE;
        else
            g_warning("XBEL: Unknown value for folded.");
        xmlFree(key);
    }
    cur = cur->xmlChildrenNode;
    while(cur)
    {
        if(!xmlStrcmp(cur->name, (const xmlChar*)"title"))
        {
            xmlChar* key = xmlNodeGetContent(cur);
            folder->title = (gchar*)g_strstrip((gchar*)key);
        }
        else if(!xmlStrcmp(cur->name, (const xmlChar*)"desc"))
        {
            xmlChar* key = xmlNodeGetContent(cur);
            folder->desc = (gchar*)g_strstrip((gchar*)key);
        }
        else if(!xmlStrcmp(cur->name, (const xmlChar*)"folder"))
        {
            XbelItem* item = xbel_folder_from_xmlNodePtr(cur);
            item->parent = (struct XbelItem*)folder;
            folder->items = g_list_prepend(folder->items, item);
        }
     else if(!xmlStrcmp(cur->name, (const xmlChar*)"bookmark"))
     {
         XbelItem* item = xbel_bookmark_from_xmlNodePtr(cur);
         item->parent = (struct XbelItem*)folder;
         folder->items = g_list_prepend(folder->items, item);
     }
     else if(!xmlStrcmp(cur->name, (const xmlChar*)"separator"))
     {
         XbelItem* item = xbel_separator_new();
         item->parent = (struct XbelItem*)folder;
         folder->items = g_list_prepend(folder->items, item);
     }
        cur = cur->next;
    }
    // Prepending and reversing is faster than appending
    folder->items = g_list_reverse(folder->items);
    return folder;
}

// Private: Loads the contents from an xmlNodePtr into a folder.
static gboolean xbel_folder_from_xmlDocPtr(XbelItem* folder, xmlDocPtr doc)
{
    g_return_val_if_fail(xbel_folder_is_empty(folder), FALSE);
    g_return_val_if_fail(doc != NULL, FALSE);
    xmlNodePtr cur = xmlDocGetRootElement(doc);
    xmlChar* version = xmlGetProp(cur, (xmlChar*)"version");
    if(xmlStrcmp(version, (xmlChar*)"1.0"))
        g_warning("XBEL version is not 1.0.");
    xmlFree(version);
    folder->title = (gchar*)xmlGetProp(cur, (xmlChar*)"title");
    folder->desc = (gchar*)xmlGetProp(cur, (xmlChar*)"desc");
    if((cur = xmlDocGetRootElement(doc)) == NULL)
    {
        // Empty document
        return FALSE;
    }
    if(xmlStrcmp(cur->name, (const xmlChar*)"xbel"))
    {
        // Wrong document kind
        return FALSE;
    }
    cur = cur->xmlChildrenNode;
    while(cur != NULL)
    {
        XbelItem* item = NULL;
        if(!xmlStrcmp(cur->name, (const xmlChar*)"folder"))
         item = xbel_folder_from_xmlNodePtr(cur);
        else if(!xmlStrcmp(cur->name, (const xmlChar*)"bookmark"))
         item = xbel_bookmark_from_xmlNodePtr(cur);
        else if(!xmlStrcmp(cur->name, (const xmlChar*)"separator"))
         item = xbel_separator_new();
        /*else if(!xmlStrcmp(cur->name, (const xmlChar*)"info"))
         item = xbel_parse_info(xbel, cur);*/
        if(item != NULL)
        {
            item->parent = (struct XbelItem*)folder;
            folder->items = g_list_prepend(folder->items, item);
        }
        cur = cur->next;
    }
    // Prepending and reversing is faster than appending
    folder->items = g_list_reverse(folder->items);
    return TRUE;
}

/**
 * xbel_item_ref:
 * @item: a valid item
 *
 * Ref an XbelItem.
 *
 * Ref means that the reference count is increased by one.
 *
 * This has no effect on children of a folder.
 **/
void xbel_item_ref(XbelItem* item)
{
    g_return_if_fail(item);
    item->refs++;
}

/**
 * xbel_item_unref:
 * @item: a valid item
 *
 * Unref an XbelItem. If @item is a folder all of its children will also
 *  be unreffed automatically.
 *
 * Unref means that the reference count is decreased. If there are no
 * references left, the memory will be freed and if needed removed from
 * its containing folder.
 **/
void xbel_item_unref(XbelItem* item)
{
    g_return_if_fail(item);
    item->refs--;
    if(item->refs)
        return;
    XbelItem* parent = xbel_item_get_parent(item);
    if(parent)
        xbel_folder_remove_item(parent, item);
    if(xbel_item_is_folder(item))
    {
        guint n = xbel_folder_get_n_items(item);
        guint i;
        for(i = 0; i < n; i++)
        {
            XbelItem* _item = xbel_folder_get_nth_item(item, i);
            _item->parent = NULL;
            xbel_item_unref(_item);
        }
        g_list_free(item->items);
    }
    if(item->kind != XBEL_ITEM_SEPARATOR)
    {
        g_free(item->title);
        g_free(item->desc);
    }
    if(item->kind == XBEL_ITEM_BOOKMARK)
        g_free(item->href);
    g_free(item);
}

/**
 * xbel_item_copy:
 * @item: the item to copy
 *
 * Copy an XbelItem.
 *
 * The returned item must be unreffed eventually.
 *
 * Return value: a copy of @item
 **/
XbelItem* xbel_item_copy(XbelItem* item)
{
    g_return_val_if_fail(item, NULL);
    XbelItem* copy = xbel_folder_new();
    if(xbel_item_is_folder(item))
    {
        guint n = xbel_folder_get_n_items(item);
        guint i;
        for(i = 0; i < n; i++)
        {
            XbelItem* _item = xbel_folder_get_nth_item(item, i);
            xbel_folder_append_item(copy, xbel_item_copy(_item));
        }
    }
    if(item->kind != XBEL_ITEM_SEPARATOR)
    {
        xbel_item_set_title(copy, item->title);
        xbel_item_set_desc(copy, item->desc);
    }
    if(item->kind == XBEL_ITEM_BOOKMARK)
        xbel_bookmark_set_href(copy, item->href);
    return copy;
}

GType xbel_item_get_type()
{
    static GType type = 0;
    if(!type)
        type = g_pointer_type_register_static("xbel_item");
    return type;
}

/**
 * xbel_folder_append_item:
 * @folder: a folder
 * @item: the item to append
 *
 * Append the given item to a folder.
 *
 * The item is actually moved and must not be contained in another folder.
 *
 **/
void xbel_folder_append_item(XbelItem* folder, XbelItem* item)
{
    g_return_if_fail(!xbel_item_get_parent(item));
    g_return_if_fail(xbel_item_is_folder(folder));
    item->parent = (struct XbelItem*)folder;
    folder->items = g_list_append(folder->items, item);
}

/**
 * xbel_folder_prepend_item:
 * @folder: a folder
 * @item: the item to prepend
 *
 * Prepend the given item to a folder.
 *
 * The item is actually moved and must not be contained in another folder.
 *
 **/
void xbel_folder_prepend_item(XbelItem* folder, XbelItem* item)
{
    g_return_if_fail(!xbel_item_get_parent(item));
    g_return_if_fail(xbel_item_is_folder(folder));
    item->parent = (struct XbelItem*)folder;
    folder->items = g_list_prepend(folder->items, item);
}

/**
 * xbel_folder_remove_item:
 * @folder: a folder
 * @item:   the item to remove
 *
 * Remove the given @item from a @folder.
 **/
void xbel_folder_remove_item(XbelItem* folder, XbelItem* item)
{
    g_return_if_fail(item);
    g_return_if_fail(xbel_item_get_parent(folder) != item);
    item->parent = NULL;
    // Fortunately we know that items are unique
    folder->items = g_list_remove(folder->items, item);
}

/**
 * xbel_folder_get_n_items:
 * @folder: a folder
 *
 * Retrieve the number of items contained in the given @folder.
 *
 * Return value: number of items
 **/
guint xbel_folder_get_n_items(XbelItem* folder)
{
    g_return_val_if_fail(xbel_item_is_folder(folder), 0);
    return g_list_length(folder->items);
}

/**
 * xbel_folder_get_nth_item:
 * @folder: a folder
 * @n: the index of an item
 *
 * Retrieve an item contained in the given @folder by its index.
 *
 * Return value: the item at the given index or %NULL
 **/
XbelItem* xbel_folder_get_nth_item(XbelItem* folder, guint n)
{
    g_return_val_if_fail(xbel_item_is_folder(folder), NULL);
    return (XbelItem*)g_list_nth_data(folder->items, n);
}

/**
 * xbel_folder_is_empty:
 * @folder: A folder.
 *
 * Determines wether or not a folder contains no items. This is significantly
 *  faster than xbel_folder_get_n_items for this particular purpose.
 *
 * Return value: Wether the given @folder is folded.
 **/
gboolean xbel_folder_is_empty(XbelItem* folder)
{
    return !xbel_folder_get_nth_item(folder, 0);
}

/**
 * xbel_folder_get_folded:
 * @folder: A folder.
 *
 * Determines wether or not a folder is folded. If a folder is not folded
 *  it should not be exposed in a user interface.
 *
 * New folders are folded by default.
 *
 * Return value: Wether the given @folder is folded.
 **/
gboolean xbel_folder_get_folded(XbelItem* folder)
{
    g_return_val_if_fail(xbel_item_is_folder(folder), TRUE);
    return folder->folded;
}

/**
 * xbel_item_get_kind:
 * @item: A item.
 *
 * Determines the kind of an item.
 *
 * Return value: The kind of the given @item.
 **/
XbelItemKind xbel_item_get_kind(XbelItem* item)
{
    return item->kind;
}

/**
 * xbel_item_get_parent:
 * @item: A valid item.
 *
 * Retrieves the parent folder of an item.
 *
 * Return value: The parent folder of the given @item or %NULL.
 **/
XbelItem* xbel_item_get_parent(XbelItem* item)
{
    g_return_val_if_fail(item, NULL);
    return (XbelItem*)item->parent;
}

/**
 * xbel_item_get_title:
 * @item: A valid item.
 *
 * Retrieves the title of an item.
 *
 * Return value: The title of the given @item or %NULL.
 **/
G_CONST_RETURN gchar* xbel_item_get_title(XbelItem* item)
{
    g_return_val_if_fail(!xbel_item_is_separator(item), NULL);
    return item->title;
}

/**
 * xbel_item_get_desc:
 * @item: A valid item.
 *
 * Retrieves the description of an item.
 *
 * Return value: The description of the @item or %NULL.
 **/
G_CONST_RETURN gchar* xbel_item_get_desc(XbelItem* item)
{
    g_return_val_if_fail(!xbel_item_is_separator(item), NULL);
    return item->desc;
}

/**
 * xbel_bookmark_get_href:
 * @bookmark: A bookmark.
 *
 * Retrieves the uri of a bookmark. The value is guaranteed to not be %NULL.
 *
 * Return value: The uri of the @bookmark.
 **/
G_CONST_RETURN gchar* xbel_bookmark_get_href(XbelItem* bookmark)
{
    g_return_val_if_fail(xbel_item_is_bookmark(bookmark), NULL);
    return bookmark->href;
}

/**
 * xbel_item_is_bookmark:
 * @item: A valid item.
 *
 * Determines wether or not an item is a bookmark.
 *
 * Return value: %TRUE if the @item is a bookmark.
 **/
gboolean xbel_item_is_bookmark(XbelItem* item)
{
    g_return_val_if_fail(item, FALSE);
    return item->kind == XBEL_ITEM_BOOKMARK;
}

/**
 * xbel_item_is_separator:
 * @item: A valid item.
 *
 * Determines wether or not an item is a separator.
 *
 * Return value: %TRUE if the @item is a separator.
 **/
gboolean xbel_item_is_separator(XbelItem* item)
{
    g_return_val_if_fail(item, FALSE);
    return item->kind == XBEL_ITEM_SEPARATOR;
}

/**
 * xbel_item_is_folder:
 * @item: A valid item.
 *
 * Determines wether or not an item is a folder.
 *
 * Return value: %TRUE if the item is a folder.
 **/
gboolean xbel_item_is_folder(XbelItem* item)
{
    g_return_val_if_fail(item, FALSE);
    return item->kind == XBEL_ITEM_FOLDER;
}

/**
 * xbel_folder_set_folded:
 * @folder: A folder.
 * @folded: TRUE if the folder is folded.
 *
 * Sets the foldedness of the @folder.
 **/
void xbel_folder_set_folded(XbelItem* folder, gboolean folded)
{
    g_return_if_fail(xbel_item_is_folder(folder));
    folder->folded = folded;
}

/**
 * xbel_item_set_title:
 * @item: A valid item.
 * @title: A string to use for the title.
 *
 * Sets the title of the @item.
 **/
void xbel_item_set_title(XbelItem* item, const gchar* title)
{
    g_return_if_fail(!xbel_item_is_separator(item));
    g_free(item->title);
    item->title = g_strdup(title);
}

/**
 * xbel_item_set_desc:
 * @item: A valid item.
 * @title: A string to use for the description.
 *
 * Sets the description of the @item.
 **/
void xbel_item_set_desc(XbelItem* item, const gchar* desc)
{
    g_return_if_fail(!xbel_item_is_separator(item));
    g_free(item->desc);
    item->desc = g_strdup(desc);
}

/**
 * xbel_bookmark_set_href:
 * @bookmark: A bookmark.
 * @href: A string containing a valid uri.
 *
 * Sets the uri of the bookmark.
 *
 * The uri must not be %NULL.
 *
 * This uri is not currently validated in any way. This may change in the future.
 **/
void xbel_bookmark_set_href(XbelItem* bookmark, const gchar* href)
{
    g_return_if_fail(xbel_item_is_bookmark(bookmark));
    g_return_if_fail(href);
    g_free(bookmark->href);
    bookmark->href = g_strdup(href);
}

gboolean xbel_folder_from_data(XbelItem* folder, const gchar* data, GError** error)
{
    g_return_val_if_fail(xbel_folder_is_empty(folder), FALSE);
    g_return_val_if_fail(data, FALSE);
    xmlDocPtr doc;
    if((doc = xmlParseMemory(data, strlen(data))) == NULL)
    {
        // No valid xml or broken encoding
        *error = g_error_new(XBEL_ERROR, XBEL_ERROR_READ
        , "Malformed document.");
        return FALSE;
    }
    if(!xbel_folder_from_xmlDocPtr(folder, doc))
    {
        // Parsing failed
        xmlFreeDoc(doc);
        *error = g_error_new(XBEL_ERROR, XBEL_ERROR_READ
         , "Malformed document.");
        return FALSE;
    }
    xmlFreeDoc(doc);
    return TRUE;
}

/**
 * xbel_folder_from_file:
 * @folder: An empty folder.
 * @file: A relative path to a file.
 * @error: return location for a GError or %NULL
 *
 * Tries to load @file.
 *
 * Return value: %TRUE on success or %FALSE when an error occured.
 **/
gboolean xbel_folder_from_file(XbelItem* folder, const gchar* file, GError** error)
{
    g_return_val_if_fail(xbel_folder_is_empty(folder), FALSE);
    g_return_val_if_fail(file, FALSE);
    if(!g_file_test(file, G_FILE_TEST_EXISTS))
    {
        // File doesn't exist
        *error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_NOENT
         , "File not found.");
        return FALSE;
    }
    xmlDocPtr doc;
    if((doc = xmlParseFile(file)) == NULL)
    {
        // No valid xml or broken encoding
        *error = g_error_new(XBEL_ERROR, XBEL_ERROR_READ
         , "Malformed document.");
        return FALSE;
    }
    if(!xbel_folder_from_xmlDocPtr(folder, doc))
    {
        // Parsing failed
        xmlFreeDoc(doc);
        *error = g_error_new(XBEL_ERROR, XBEL_ERROR_READ
         , "Malformed document.");
        return FALSE;
    }
    xmlFreeDoc(doc);
    return TRUE;
}

/**
 * xbel_folder_from_data_dirs:
 * @folder: An empty folder.
 * @file: A relative path to a file.
 * @full_path: return location for the full path of the file or %NULL
 * @error: return location for a GError or %NULL
 *
 * Tries to load @file from the user data dir or any of the system data dirs.
 *
 * Return value: %TRUE on success or %FALSE when an error occured.
 **/
gboolean xbel_folder_from_data_dirs(XbelItem* folder, const gchar* file
 , gchar** full_path, GError** error)
{
    g_return_val_if_fail(xbel_folder_is_empty(folder), FALSE);
    g_return_val_if_fail(file, FALSE);
    // FIXME: Essentially unimplemented

    *error = g_error_new(XBEL_ERROR, XBEL_ERROR_READ
     , "Malformed document.");
    return FALSE;
}

static gchar* xbel_xml_element(const gchar* name, const gchar* value)
{
    if(!value)
        return g_strdup("");
    gchar* valueEscaped = g_markup_escape_text(value, -1);
    gchar* XML = g_strdup_printf("<%s>%s</%s>\n", name, valueEscaped, name);
    g_free(valueEscaped);
    return XML;
}

static gchar* xbel_item_to_data(XbelItem* item)
{
    g_return_val_if_fail(item, NULL);
    gchar* XML = NULL;
    switch(xbel_item_get_kind(item))
    {
    case XBEL_ITEM_FOLDER:
    {
        GString* _XML = g_string_new(NULL);
        guint n = xbel_folder_get_n_items(item);
        guint i;
        for(i = 0; i < n; i++)
        {
            XbelItem* _item = xbel_folder_get_nth_item(item, i);
            gchar* itemXML = xbel_item_to_data(_item);
            g_string_append(_XML, itemXML);
            g_free(itemXML);
        }
        gchar* folded = item->folded ? NULL : g_strdup_printf(" folded=\"no\"");
        gchar* title = xbel_xml_element("title", item->title);
        gchar* desc = xbel_xml_element("desc", item->desc);
        XML = g_strdup_printf("<folder%s>\n%s%s%s</folder>\n"
         , folded ? folded : ""
         , title
         , desc
         , g_string_free(_XML, FALSE));
        g_free(folded);
        g_free(title);
        g_free(desc);
        break;
        }
    case XBEL_ITEM_BOOKMARK:
    {
        gchar* hrefEscaped = g_markup_escape_text(item->href, -1);
        gchar* href = g_strdup_printf(" href=\"%s\"", hrefEscaped);
        g_free(hrefEscaped);
        gchar* title = xbel_xml_element("title", item->title);
        gchar* desc = xbel_xml_element("desc", item->desc);
        XML = g_strdup_printf("<bookmark%s>\n%s%s%s</bookmark>\n"
         , href
         , title
         , desc
         , "");
        g_free(href);
        g_free(title);
        g_free(desc);
        break;
    }
    case XBEL_ITEM_SEPARATOR:
        XML = g_strdup("<separator/>\n");
        break;
    default:
        g_warning("XBEL: Unknown item kind");
    }
    return XML;
}

/**
 * xbel_folder_to_data:
 * @folder: A folder.
 * @length: return location for the length of the created string or %NULL
 * @error: return location for a GError or %NULL
 *
 * Retrieve the contents of @folder as a string.
 *
 * Return value: %TRUE on success or %FALSE when an error occured.
 **/
gchar* xbel_folder_to_data(XbelItem* folder, gsize* length, GError** error)
{
    g_return_val_if_fail(xbel_item_is_folder(folder), FALSE);
    // FIXME: length is never filled
    GString* innerXML = g_string_new(NULL);
    guint n = xbel_folder_get_n_items(folder);
    guint i;
    for(i = 0; i < n; i++)
    {
        gchar* sItem = xbel_item_to_data(xbel_folder_get_nth_item(folder, i));
        g_string_append(innerXML, sItem);
        g_free(sItem);
    }
    gchar* title = xbel_xml_element("title", folder->title);
    gchar* desc = xbel_xml_element("desc", folder->desc);
    gchar* outerXML;
    outerXML = g_strdup_printf("%s%s<xbel version=\"1.0\">\n%s%s%s</xbel>\n"
     , "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
     , "<!DOCTYPE xbel PUBLIC \"+//IDN python.org//DTD XML Bookmark Exchange Language 1.0//EN//XML\" \"http://www.python.org/topics/xml/dtds/xbel-1.0.dtd\">\n"
     , title
     , desc
     , g_string_free(innerXML, FALSE));
    g_free(title);
    g_free(desc);
    return outerXML;
}

/**
 * xbel_folder_to_file:
 * @folder: A folder.
 * @file: The destination filename.
 * @error: return location for a GError or %NULL
 *
 * Write the contents of @folder to the specified file, creating it if necessary.
 *
 * Return value: %TRUE on success or %FALSE when an error occured.
 **/
gboolean xbel_folder_to_file(XbelItem* folder, const gchar* file, GError** error)
{
    g_return_val_if_fail(file, FALSE);
    gchar* data;
    if(!(data = xbel_folder_to_data(folder, NULL, error)))
        return FALSE;
    FILE* fp;
    if(!(fp = fopen(file, "w")))
    {
        *error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_ACCES
         , "Writing failed.");
        return FALSE;
    }
    fputs(data, fp);
    fclose(fp);
    g_free(data);
    return TRUE;
}
