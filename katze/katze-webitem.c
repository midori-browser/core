/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze-webitem.h"

#include "katze-utils.h"

#include <glib/gi18n.h>

struct _MidoriWebItem
{
    GObject parent_instance;

    gchar* name;
    gchar* description;
    gchar* uri;
    gchar* icon;
    gchar* token;
};

G_DEFINE_TYPE (MidoriWebItem, midori_web_item, G_TYPE_OBJECT)

enum
{
    PROP_0,

    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_URI,
    PROP_ICON,
    PROP_TOKEN
};

static void
midori_web_item_finalize (GObject* object);

static void
midori_web_item_set_property (GObject*      object,
                              guint         prop_id,
                              const GValue* value,
                              GParamSpec*   pspec);

static void
midori_web_item_get_property (GObject*    object,
                              guint       prop_id,
                              GValue*     value,
                              GParamSpec* pspec);

static void
midori_web_item_class_init (MidoriWebItemClass* class)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_web_item_finalize;
    gobject_class->set_property = midori_web_item_set_property;
    gobject_class->get_property = midori_web_item_get_property;

    GParamFlags flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    g_object_class_install_property (gobject_class,
                                     PROP_NAME,
                                     g_param_spec_string (
                                     "name",
                                     _("Name"),
                                     _("The name of the web item"),
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_DESCRIPTION,
                                     g_param_spec_string (
                                     "description",
                                     _("Description"),
                                     _("The description of the web item"),
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_URI,
                                     g_param_spec_string (
                                     "uri",
                                     _("URI"),
                                     _("The URI of the web item"),
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_ICON,
                                     g_param_spec_string (
                                     "icon",
                                     _("Icon"),
                                     _("The icon of the web item"),
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_TOKEN,
                                     g_param_spec_string (
                                     "token",
                                     _("Token"),
                                     _("The token of the web item"),
                                     NULL,
                                     flags));
}



static void
midori_web_item_init (MidoriWebItem* web_item)
{
    /* Nothing to do here */
}

static void
midori_web_item_finalize (GObject* object)
{
    MidoriWebItem* web_item = MIDORI_WEB_ITEM (object);

    g_free (web_item->name);
    g_free (web_item->description);
    g_free (web_item->uri);
    g_free (web_item->icon);
    g_free (web_item->token);

    G_OBJECT_CLASS (midori_web_item_parent_class)->finalize (object);
}

static void
midori_web_item_set_property (GObject*      object,
                              guint         prop_id,
                              const GValue* value,
                              GParamSpec*   pspec)
{
    MidoriWebItem* web_item = MIDORI_WEB_ITEM (object);

    switch (prop_id)
    {
    case PROP_NAME:
        web_item->name = g_value_dup_string (value);
        break;
    case PROP_DESCRIPTION:
        web_item->description = g_value_dup_string (value);
        break;
    case PROP_URI:
        web_item->uri = g_value_dup_string (value);
        break;
    case PROP_ICON:
        web_item->icon = g_value_dup_string (value);
        break;
    case PROP_TOKEN:
        web_item->token = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_web_item_get_property (GObject*    object,
                              guint       prop_id,
                              GValue*     value,
                              GParamSpec* pspec)
{
    MidoriWebItem* web_item = MIDORI_WEB_ITEM (object);

    switch (prop_id)
    {
    case PROP_NAME:
        g_value_set_string (value, web_item->name);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, web_item->description);
        break;
    case PROP_URI:
        g_value_set_string (value, web_item->uri);
        break;
    case PROP_ICON:
        g_value_set_string (value, web_item->icon);
        break;
    case PROP_TOKEN:
        g_value_set_string (value, web_item->token);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * midori_web_item_new:
 *
 * Creates a new #MidoriWebItem.
 *
 * Return value: a new #MidoriWebItem
 **/
MidoriWebItem*
midori_web_item_new (void)
{
    MidoriWebItem* web_item = g_object_new (MIDORI_TYPE_WEB_ITEM,
                                            NULL);

    return web_item;
}

/**
 * midori_web_item_get_name:
 * @web_item: a #MidoriWebItem
 *
 * Retrieves the name of @web_item.
 *
 * Return value: the name of the web item
 **/
const gchar*
midori_web_item_get_name (MidoriWebItem* web_item)
{
    g_return_val_if_fail (MIDORI_IS_WEB_ITEM (web_item), NULL);

    return web_item->name;
}

/**
 * midori_web_item_set_name:
 * @web_item: a #MidoriWebItem
 * @name: a string
 *
 * Sets the name of @web_item.
 **/
void
midori_web_item_set_name (MidoriWebItem* web_item,
                          const gchar*   name)
{
    g_return_if_fail (MIDORI_IS_WEB_ITEM (web_item));

    katze_assign (web_item->name, g_strdup (name));
    g_object_notify (G_OBJECT (web_item), "name");
}

/**
 * midori_web_item_get_description:
 * @web_item: a #MidoriWebItem
 *
 * Retrieves the description of @web_item.
 *
 * Return value: the description of the web item
 **/
const gchar*
midori_web_item_get_description (MidoriWebItem* web_item)
{
    g_return_val_if_fail (MIDORI_IS_WEB_ITEM (web_item), NULL);

    return web_item->description;
}

/**
 * midori_web_item_set_description:
 * @web_item: a #MidoriWebItem
 * @description: a string
 *
 * Sets the description of @web_item.
 **/
void
midori_web_item_set_description (MidoriWebItem* web_item,
                                 const gchar*   description)
{
    g_return_if_fail (MIDORI_IS_WEB_ITEM (web_item));

    katze_assign (web_item->description, g_strdup (description));
    g_object_notify (G_OBJECT (web_item), "description");
}

/**
 * midori_web_item_get_uri:
 * @web_item: a #MidoriWebItem
 *
 * Retrieves the URI of @web_item.
 *
 * Return value: the URI of the web item
 **/
const gchar*
midori_web_item_get_uri (MidoriWebItem* web_item)
{
    g_return_val_if_fail (MIDORI_IS_WEB_ITEM (web_item), NULL);

    return web_item->uri;
}

/**
 * midori_web_item_set_uri:
 * @web_item: a #MidoriWebItem
 * @uri: a string
 *
 * Sets the URI of @web_item.
 **/
void
midori_web_item_set_uri (MidoriWebItem* web_item,
                         const gchar*   uri)
{
    g_return_if_fail (MIDORI_IS_WEB_ITEM (web_item));

    katze_assign (web_item->uri, g_strdup (uri));
    g_object_notify (G_OBJECT (web_item), "uri");
}

/**
 * midori_web_item_get_icon:
 * @web_item: a #MidoriWebItem
 *
 * Retrieves the icon of @web_item.
 *
 * Return value: the icon of the web item
 **/
const gchar*
midori_web_item_get_icon (MidoriWebItem* web_item)
{
    g_return_val_if_fail (MIDORI_IS_WEB_ITEM (web_item), NULL);

    return web_item->icon;
}

/**
 * midori_web_item_set_icon:
 * @web_item: a #MidoriWebItem
 * @icon: a string
 *
 * Sets the icon of @web_item.
 **/
void
midori_web_item_set_icon (MidoriWebItem* web_item,
                          const gchar*   icon)
{
    g_return_if_fail (MIDORI_IS_WEB_ITEM (web_item));

    katze_assign (web_item->icon, g_strdup (icon));
    g_object_notify (G_OBJECT (web_item), "icon");
}

/**
 * midori_web_item_get_token:
 * @web_item: a #MidoriWebItem
 *
 * Retrieves the token of @web_item.
 *
 * Return value: the token of the web item
 **/
const gchar*
midori_web_item_get_token (MidoriWebItem* web_item)
{
    g_return_val_if_fail (MIDORI_IS_WEB_ITEM (web_item), NULL);

    return web_item->token;
}

/**
 * midori_web_item_set_token:
 * @web_item: a #MidoriWebItem
 * @token: a string
 *
 * Sets the token of @web_item.
 **/
void
midori_web_item_set_token (MidoriWebItem* web_item,
                           const gchar*   token)
{
    g_return_if_fail (MIDORI_IS_WEB_ITEM (web_item));

    katze_assign (web_item->token, g_strdup (token));
    g_object_notify (G_OBJECT (web_item), "token");
}
