/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

#define STOCK_TAB_PANEL "tab-panel"

static void
tab_panel_app_add_browser_cb (MidoriApp*       app,
                              MidoriBrowser*   browser,
                              MidoriExtension* extension);

static void
tab_panel_deactivate_cb (MidoriExtension* extension,
                         GtkWidget*       panel)
{
    MidoriApp* app = midori_extension_get_app (extension);

    gtk_widget_destroy (panel);
    g_signal_handlers_disconnect_by_func (
        extension, tab_panel_deactivate_cb, panel);
    g_signal_handlers_disconnect_by_func (
        app, tab_panel_app_add_browser_cb, extension);
}

static void
tab_panel_app_add_browser_cb (MidoriApp*       app,
                              MidoriBrowser*   browser,
                              MidoriExtension* extension)
{
    GtkWidget* panel;
    GtkWidget* notebook;
    GtkWidget* toolbar;
    /* GtkToolItem* toolitem; */

    panel = katze_object_get_object (browser, "panel");
    notebook = gtk_notebook_new ();
    gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_RIGHT);
    gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
    gtk_widget_show (notebook);
    toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
    gtk_widget_show (toolbar);

    /*
    TODO: Implement optional thumbnail images
    toolitem = gtk_toggle_tool_button_new_from_stock (STOCK_IMAGE);
    gtk_tool_item_set_is_important (toolitem, TRUE);
    g_signal_connect (toolitem, "toggled",
            G_CALLBACK (tab_panel_button_thumbnail_toggled_cb), notebook);
    gtk_widget_show (GTK_WIDGET (toolitem));
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1); */

    midori_panel_append_widget (MIDORI_PANEL (panel), notebook,
                                STOCK_TAB_PANEL, _("Tab Panel"), toolbar);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (tab_panel_deactivate_cb), notebook);

    g_object_unref (panel);
}

static void
tab_panel_activate_cb (MidoriExtension* extension,
                       MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;
    guint i;

    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++)))
        tab_panel_app_add_browser_cb (app, browser, extension);
    g_object_unref (browsers);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (tab_panel_app_add_browser_cb), extension);
}

MidoriExtension*
extension_init (void)
{
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

    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Tab Panel"),
        "description", _("Show tabs in a vertical panel"),
        "version", "0.1",
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (tab_panel_activate_cb), NULL);

    return extension;
}
