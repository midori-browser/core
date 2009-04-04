/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "tab-panel-extension.h"

#include <midori/midori.h>

#define STOCK_TAB_PANEL "tab-panel"

static void
tab_panel_app_add_browser_cb (MidoriApp*     app,
                              MidoriBrowser* browser)
{
    GtkWidget* panel;
    GtkWidget* child;

    /* FIXME: Actually provide a tree view listing all views. */

    panel = katze_object_get_object (browser, "panel");
    child = midori_view_new (NULL);
    gtk_widget_show (child);
    midori_panel_append_widget (MIDORI_PANEL (panel), child,
                                STOCK_TAB_PANEL, _("Tab Panel"), NULL);
}

static void
tab_panel_activate_cb (MidoriExtension* extension,
                       MidoriApp*       app)
{
    g_signal_connect (app, "add-browser",
        G_CALLBACK (tab_panel_app_add_browser_cb), NULL);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension;
    GtkIconFactory* factory;
    GtkIconSource* icon_source;
    GtkIconSet* icon_set;
    static GtkStockItem items[] =
    {
        { STOCK_TAB_PANEL, N_("T_ab Panel"), 0, 0, NULL },
    };

    factory = gtk_icon_factory_new ();
    gtk_stock_add (items, G_N_ELEMENTS (items));
    icon_set = gtk_icon_set_new ();
    icon_source = gtk_icon_source_new ();
    gtk_icon_source_set_icon_name (icon_source, GTK_STOCK_INDEX);
    gtk_icon_set_add_source (icon_set, icon_source);
    gtk_icon_source_free (icon_source);
    gtk_icon_factory_add (factory, STOCK_TAB_PANEL, icon_set);
    gtk_icon_set_unref (icon_set);
    gtk_icon_factory_add_default (factory);
    g_object_unref (factory);

    extension = g_object_new (TAB_PANEL_TYPE_EXTENSION,
        "name", _("Tab Panel"),
        "description", "",
        "version", "0.1",
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (tab_panel_activate_cb), NULL);

    return extension;
}
