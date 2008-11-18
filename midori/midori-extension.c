/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-extension.h"

#include <katze/katze.h>

G_DEFINE_TYPE (MidoriExtension, midori_extension, G_TYPE_OBJECT);

struct _MidoriExtensionPrivate
{
    gchar* name;
    gchar* description;
    gchar* version;
    gchar* authors;
};

enum
{
    PROP_0,

    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_VERSION,
    PROP_AUTHORS
};

static void
midori_extension_finalize (GObject* object);

static void
midori_extension_set_property (GObject*      object,
                               guint         prop_id,
                               const GValue* value,
                               GParamSpec*   pspec);

static void
midori_extension_get_property (GObject*    object,
                               guint       prop_id,
                               GValue*     value,
                               GParamSpec* pspec);

static void
midori_extension_class_init (MidoriExtensionClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_extension_finalize;
    gobject_class->set_property = midori_extension_set_property;
    gobject_class->get_property = midori_extension_get_property;

    flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    g_object_class_install_property (gobject_class,
                                     PROP_NAME,
                                     g_param_spec_string (
                                     "name",
                                     "Name",
                                     "The name of the extension",
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_DESCRIPTION,
                                     g_param_spec_string (
                                     "description",
                                     "Description",
                                     "The description of the extension",
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_VERSION,
                                     g_param_spec_string (
                                     "version",
                                     "Version",
                                     "The version of the extension",
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_AUTHORS,
                                     g_param_spec_string (
                                     "authors",
                                     "Authors",
                                     "The authors of the extension",
                                     NULL,
                                     flags));

    g_type_class_add_private (class, sizeof (MidoriExtensionPrivate));
}

static void
midori_extension_init (MidoriExtension* extension)
{
    extension->priv = G_TYPE_INSTANCE_GET_PRIVATE (extension,
        MIDORI_TYPE_EXTENSION, MidoriExtensionPrivate);
}

static void
midori_extension_finalize (GObject* object)
{
    MidoriExtension* extension = MIDORI_EXTENSION (object);

    katze_assign (extension->priv->name, NULL);
    katze_assign (extension->priv->description, NULL);
    katze_assign (extension->priv->version, NULL);
    katze_assign (extension->priv->authors, NULL);
}

static void
midori_extension_set_property (GObject*      object,
                               guint         prop_id,
                               const GValue* value,
                               GParamSpec*   pspec)
{
    MidoriExtension* extension = MIDORI_EXTENSION (object);

    switch (prop_id)
    {
    case PROP_NAME:
        katze_assign (extension->priv->name, g_value_dup_string (value));
        break;
    case PROP_DESCRIPTION:
        katze_assign (extension->priv->description, g_value_dup_string (value));
        break;
    case PROP_VERSION:
        katze_assign (extension->priv->version, g_value_dup_string (value));
        break;
    case PROP_AUTHORS:
        katze_assign (extension->priv->authors, g_value_dup_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_extension_get_property (GObject*    object,
                               guint       prop_id,
                               GValue*     value,
                               GParamSpec* pspec)
{
    MidoriExtension* extension = MIDORI_EXTENSION (object);

    switch (prop_id)
    {
    case PROP_NAME:
        g_value_set_string (value, extension->priv->name);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, extension->priv->description);
        break;
    case PROP_VERSION:
        g_value_set_string (value, extension->priv->version);
        break;
    case PROP_AUTHORS:
        g_value_set_string (value, extension->priv->authors);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}
