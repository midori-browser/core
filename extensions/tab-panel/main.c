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

void
tab_panel_app_add_browser_cb (MidoriApp*     app,
                              MidoriBrowser* browser)
{
    GtkWidget* panel;
    GtkWidget* child;

    /* FIXME: Actually provide a tree view listing all views. */

    panel = katze_object_get_object (browser, "panel");
    child = midori_view_new (NULL);
    gtk_widget_show (child);
    midori_panel_append_page (MIDORI_PANEL (panel), child,
                              NULL, GTK_STOCK_INDEX, "Tab Panel");
}

MidoriExtension* extension_main (MidoriApp* app)
{
    MidoriExtension* extension = g_object_new (TAB_PANEL_TYPE_EXTENSION,
        "name", "Tab Panel",
        "description", "",
        "version", "0.1",
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);

    g_signal_connect (app, "add-browser",
        G_CALLBACK (tab_panel_app_add_browser_cb), extension);

    return extension;
}
