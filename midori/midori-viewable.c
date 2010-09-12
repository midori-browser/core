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

    iface->get_stock_id = midori_viewable_default_get_stock_id;
    iface->get_label = midori_viewable_default_get_label;
    iface->get_toolbar = midori_viewable_default_get_toolbar;

    initialized = TRUE;
}

static void
midori_viewable_base_finalize (MidoriViewableIface* iface)
{
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
