/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-pane.h"

#include <glib/gi18n.h>

struct _MidoriPane
{
    GtkHBox parent_instance;
};

static void
midori_pane_base_init (MidoriPaneIface* iface);

GType
midori_pane_get_type (void)
{
  static GType pane_type = 0;

  if (!pane_type)
    {
      const GTypeInfo pane_info =
      {
        sizeof (MidoriPaneIface),
        (GBaseInitFunc)     midori_pane_base_init,
        (GBaseFinalizeFunc) NULL,
      };

      pane_type = g_type_register_static (G_TYPE_INTERFACE,
                                          "MidoriPane",
                                          &pane_info, 0);
      g_type_interface_add_prerequisite (pane_type, GTK_TYPE_WIDGET);
    }

  return pane_type;
}

static const gchar*
midori_pane_default_get_stock_id (MidoriPane* pane)
{
    return NULL;
}

static const gchar*
midori_pane_default_get_label (MidoriPane* pane)
{
    return NULL;
}

static GtkWidget*
midori_pane_default_get_toolbar (MidoriPane* pane)
{
    return NULL;
}

static void
midori_pane_base_init (MidoriPaneIface* iface)
{
    static gboolean initialized = FALSE;

    if (initialized)
        return;

    iface->get_stock_id = midori_pane_default_get_stock_id;
    iface->get_label = midori_pane_default_get_label;
    iface->get_toolbar = midori_pane_default_get_toolbar;

    initialized = TRUE;
}

/**
 * midori_pane_get_stock_id:
 * @pane: a #MidoriPane
 *
 * Retrieves the stock ID of the pane.
 *
 * Return value: a stock ID
 **/
const gchar*
midori_pane_get_stock_id (MidoriPane* pane)
{
    g_return_val_if_fail (MIDORI_IS_PANE (pane), NULL);

    return MIDORI_PANE_GET_IFACE (pane)->get_stock_id (pane);
}

/**
 * midori_pane_get_label:
 * @pane: a #MidoriPane
 *
 * Retrieves the label of the pane.
 *
 * Return value: a label string
 **/
const gchar*
midori_pane_get_label (MidoriPane* pane)
{
    g_return_val_if_fail (MIDORI_IS_PANE (pane), NULL);

    return MIDORI_PANE_GET_IFACE (pane)->get_label (pane);
}

/**
 * midori_pane_get_toolbar:
 * @pane: a #MidoriPane
 *
 * Retrieves the toolbar of the pane.
 *
 * Return value: a toolbar
 **/
GtkWidget*
midori_pane_get_toolbar (MidoriPane* pane)
{
    GtkWidget* toolbar;

    g_return_val_if_fail (MIDORI_IS_PANE (pane), NULL);

    toolbar = MIDORI_PANE_GET_IFACE (pane)->get_toolbar (pane);
    if (!toolbar)
        toolbar = gtk_event_box_new ();
    return toolbar;
}
