/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-viewable.h"

#include <glib/gi18n.h>

enum {
    POPULATE_OPTION_MENU,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
midori_viewable_base_init (MidoriViewableIface* iface);

static void
midori_viewable_base_finalize (MidoriViewableIface* iface);

GType
midori_viewable_get_type (void)
{
  static GType viewable_type = 0;

  if (!viewable_type)
    {
      const GTypeInfo viewable_info =
      {
        sizeof (MidoriViewableIface),
        (GBaseInitFunc)     midori_viewable_base_init,
        (GBaseFinalizeFunc) midori_viewable_base_finalize,
      };

      viewable_type = g_type_register_static (G_TYPE_INTERFACE,
                                              "MidoriViewable",
                                              &viewable_info, 0);
      g_type_interface_add_prerequisite (viewable_type, GTK_TYPE_WIDGET);
    }

  return viewable_type;
}

static const gchar*
midori_viewable_default_get_stock_id (MidoriViewable* viewable)
{
    return NULL;
}

static const gchar*
midori_viewable_default_get_label (MidoriViewable* viewable)
{
    return NULL;
}

static GtkWidget*
midori_viewable_default_get_toolbar (MidoriViewable* viewable)
{
    return NULL;
}

static void
midori_viewable_base_init (MidoriViewableIface* iface)
{
    static gboolean initialized = FALSE;

    if (initialized)
        return;

    /**
     * MidoriViewable::populate-option-menu:
     * @viewable: the object on which the signal is emitted
     * @menu: the #GtkMenu to populate
     *
     * Emitted when an Option menu is displayed, for instance
     * when the user clicks the Options button in the panel.
     *
     * Deprecated: 0.2.3
     */
    signals[POPULATE_OPTION_MENU] = g_signal_new (
        "populate-option-menu",
        G_TYPE_FROM_INTERFACE (iface),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        GTK_TYPE_MENU);

    iface->p = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    iface->get_stock_id = midori_viewable_default_get_stock_id;
    iface->get_label = midori_viewable_default_get_label;
    iface->get_toolbar = midori_viewable_default_get_toolbar;

    initialized = TRUE;
}

static void
midori_viewable_base_finalize (MidoriViewableIface* iface)
{
    g_hash_table_destroy (iface->p);
}

/**
 * midori_viewable_new_from_uri:
 * @uri: an URI
 *
 * Attempts to create a new #MidoriViewable from the specified URI.
 *
 * The protocol of @uri must previously have been registered by
 * the #MidoriViewable via midori_viewable_register_protocol().
 *
 * Return value: a new #MidoriViewable, or %NULL
 **/
GtkWidget*
midori_viewable_new_from_uri (const gchar* uri)
{
    MidoriViewableIface* iface;
    gchar** parts;
    gchar* type_name;
    GType type;

    if (!(iface = g_type_default_interface_peek (MIDORI_TYPE_VIEWABLE)))
    {
        g_warning ("No viewable interface available");
        return NULL;
    }

    g_return_val_if_fail (uri != NULL, NULL);

    if (!g_hash_table_size (iface->p))
        return NULL;

    if ((parts = g_strsplit (uri, "://", 2)))
    {
        if (!(type_name = g_hash_table_lookup (iface->p, parts[0])))
        {
            /* FIXME: Support midori://dummy/foo */

            type_name = g_hash_table_lookup (iface->p, uri);
        }
        g_strfreev (parts);
        if (type_name)
        {
            type = g_type_from_name (type_name);
            g_free (type_name);
            if (type)
                return g_object_new (type, "uri", uri, NULL);
        }
    }
    else if ((parts = g_strsplit_set (uri, ":", 2)))
    {
        type_name = g_hash_table_lookup (iface->p, parts[0]);
        g_strfreev (parts);
        if (type_name)
        {
            type = g_type_from_name (type_name);
            g_free (type_name);
            if (type)
                return g_object_new (type, "uri", uri, NULL);
        }
    }
    return NULL;
}

static gboolean
viewable_type_implements (GType type,
                          GType interface)
{
    GType *interfaces;
    guint i;

    if (!(interfaces = g_type_interfaces (type, NULL)))
        return FALSE;
    for (i = 0; interfaces[i]; i++)
    {
        if (interfaces[i] == interface)
        {
            g_free (interfaces);
            return TRUE;
        }
    }
    g_free (interfaces);
    return FALSE;
}

/**
 * midori_viewable_register_protocol:
 * @type: a type that implements #MidoriViewable
 * @protocol: a protocol
 *
 * Registers the specified protocol as supported by @type.
 *
 * The following kinds of protocols are supported:
 *
 * "dummy":       support URIs like "dummy://foo/bar"
 * "about:dummy": support URIs like "about:dummy"
 * FIXME: The following is not yet fully supported
 * "midori://dummy": support URIs like "midori://dummy/foo"
 *
 * Return value: a new #MidoriViewable, or %NULL
 **/
void
midori_viewable_register_protocol (GType        type,
                                   const gchar* protocol)
{
    MidoriViewableIface* iface;
    GObjectClass* class;

    if (!(iface = g_type_default_interface_peek (MIDORI_TYPE_VIEWABLE)))
    {
        g_warning ("No viewable interface available");
        return;
    }

    g_return_if_fail (viewable_type_implements (type, MIDORI_TYPE_VIEWABLE));

    if (!(class = g_type_class_peek (type)))
    {
        g_warning ("No class for %s available", g_type_name (type));
        return;
    }
    g_return_if_fail (g_object_class_find_property (class, "uri"));
    /* FIXME: Verify the syntax of protocol */

    g_hash_table_insert (iface->p, g_strdup (protocol),
                         g_strdup (g_type_name (type)));
}

/**
 * midori_viewable_get_stock_id:
 * @viewable: a #MidoriViewable
 *
 * Retrieves the stock ID of the viewable.
 *
 * Return value: a stock ID
 **/
const gchar*
midori_viewable_get_stock_id (MidoriViewable* viewable)
{
    g_return_val_if_fail (MIDORI_IS_VIEWABLE (viewable), NULL);

    return MIDORI_VIEWABLE_GET_IFACE (viewable)->get_stock_id (viewable);
}

/**
 * midori_viewable_get_label:
 * @viewable: a #MidoriViewable
 *
 * Retrieves the label of the viewable.
 *
 * Return value: a label string
 **/
const gchar*
midori_viewable_get_label (MidoriViewable* viewable)
{
    g_return_val_if_fail (MIDORI_IS_VIEWABLE (viewable), NULL);

    return MIDORI_VIEWABLE_GET_IFACE (viewable)->get_label (viewable);
}

/**
 * midori_viewable_get_toolbar:
 * @viewable: a #MidoriViewable
 *
 * Retrieves the toolbar of the viewable.
 *
 * Return value: a toolbar
 **/
GtkWidget*
midori_viewable_get_toolbar (MidoriViewable* viewable)
{
    GtkWidget* toolbar;

    g_return_val_if_fail (MIDORI_IS_VIEWABLE (viewable), NULL);

    toolbar = MIDORI_VIEWABLE_GET_IFACE (viewable)->get_toolbar (viewable);
    if (!toolbar)
        toolbar = gtk_toolbar_new ();
    return toolbar;
}
