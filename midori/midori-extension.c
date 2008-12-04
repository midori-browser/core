/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-extension.h"

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "midori-app.h"

#include <katze/katze.h>

G_DEFINE_TYPE (MidoriExtension, midori_extension, G_TYPE_OBJECT);

struct _MidoriExtensionPrivate
{
    gchar* name;
    gchar* description;
    gchar* version;
    gchar* authors;

    gboolean active;
    gchar* config_dir;
};

enum
{
    PROP_0,

    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_VERSION,
    PROP_AUTHORS
};

enum {
    ACTIVATE,
    DEACTIVATE,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

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

    signals[ACTIVATE] = g_signal_new (
        "activate",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        MIDORI_TYPE_APP);

    signals[DEACTIVATE] = g_signal_new (
        "deactivate",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0,
        G_TYPE_NONE);

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

void
midori_extension_activate_cb (MidoriExtension* extension,
                              MidoriApp*       app)
{
    extension->priv->active = TRUE;
    /* FIXME: Disconnect all signal handlers */
}

static void
midori_extension_init (MidoriExtension* extension)
{
    extension->priv = G_TYPE_INSTANCE_GET_PRIVATE (extension,
        MIDORI_TYPE_EXTENSION, MidoriExtensionPrivate);

    extension->priv->active = FALSE;
    extension->priv->config_dir = NULL;

    g_signal_connect (extension, "activate",
        G_CALLBACK (midori_extension_activate_cb), NULL);
}

static void
midori_extension_finalize (GObject* object)
{
    MidoriExtension* extension = MIDORI_EXTENSION (object);

    katze_assign (extension->priv->name, NULL);
    katze_assign (extension->priv->description, NULL);
    katze_assign (extension->priv->version, NULL);
    katze_assign (extension->priv->authors, NULL);

    katze_assign (extension->priv->config_dir, NULL);
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

/**
 * midori_extension_is_prepared:
 * @extension: a #MidoriExtension
 *
 * Determines if @extension is prepared for use, for instance
 * by ensuring that all required values are set and that it
 * is actually activatable.
 *
 * Return value: %TRUE if @extension is ready for use
 **/
gboolean
midori_extension_is_prepared (MidoriExtension* extension)
{
    g_return_val_if_fail (MIDORI_IS_EXTENSION (extension), FALSE);

    if (extension->priv->name && extension->priv->description
        && extension->priv->version && extension->priv->authors
        && g_signal_has_handler_pending (extension, signals[ACTIVATE], 0, FALSE))
        return TRUE;
    return FALSE;
}

/**
 * midori_extension_is_active:
 * @extension: a #MidoriExtension
 *
 * Determines if @extension is active.
 *
 * Return value: %TRUE if @extension is active
 *
 * Since: 0.1.2
 **/
gboolean
midori_extension_is_active (MidoriExtension* extension)
{
    g_return_val_if_fail (MIDORI_IS_EXTENSION (extension), FALSE);

    return extension->priv->active;
}

/**
 * midori_extension_deactivate:
 * @extension: a #MidoriExtension
 *
 * Attempts to deactivate @extension in a way that the instance
 * is actually finished irreversibly.
 **/
void
midori_extension_deactivate (MidoriExtension* extension)
{
    g_return_if_fail (MIDORI_IS_EXTENSION (extension));

    g_signal_emit (extension, signals[DEACTIVATE], 0);
    g_signal_handlers_destroy (extension);
    g_object_run_dispose (G_OBJECT (extension));
}

/**
 * midori_extension_get_config_dir:
 * @extension: a #MidoriExtension
 *
 * Retrieves the path to a directory reserved for configuration
 * files specific to the extension. For that purpose the 'name'
 * of the extension is actually part of the path.
 *
 * The path is actually created if it doesn't already exist.
 *
 * Return value: a path, such as ~/.config/midori/extensions/name
 **/
const gchar*
midori_extension_get_config_dir (MidoriExtension* extension)
{
    g_return_val_if_fail (MIDORI_IS_EXTENSION (extension), NULL);
    g_return_val_if_fail (midori_extension_is_prepared (extension), NULL);

    if (!extension->priv->config_dir)
        extension->priv->config_dir = g_build_filename (
            g_get_user_config_dir (), PACKAGE_NAME, "extensions",
            extension->priv->name, NULL);

    g_mkdir_with_parents (extension->priv->config_dir, 0700);
    return extension->priv->config_dir;
}
