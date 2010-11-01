/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

#define STOCK_PAGE_HOLDER "page-holder"

static void
page_holder_app_add_browser_cb (MidoriApp*       app,
                                MidoriBrowser*   browser,
                                MidoriExtension* extension);

static gint
page_holder_notebook_append_view (GtkWidget* notebook)
{
    GtkWidget* view;
    MidoriBrowser* browser;
    MidoriWebSettings *settings;
    GtkWidget* label;

    view = midori_view_new (NULL);
    browser = midori_browser_get_for_widget (notebook);
    settings = midori_browser_get_settings (browser);
    midori_view_set_settings (MIDORI_VIEW (view), settings);
    gtk_widget_show (view);
    label = midori_view_get_proxy_tab_label (MIDORI_VIEW (view));
    return gtk_notebook_append_page (GTK_NOTEBOOK (notebook), view, label);
}

static void
page_holder_button_jump_to_clicked_cb (GtkWidget* button,
                                       GtkWidget* notebook)
{
    gint n;
    MidoriBrowser* browser;
    const gchar* uri;
    GtkWidget* view;

    n = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
    if (n < 0)
        n = page_holder_notebook_append_view (notebook);

    browser = midori_browser_get_for_widget (notebook);
    uri = midori_browser_get_current_uri (browser);
    view = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), n);
    midori_view_set_uri (MIDORI_VIEW (view), uri);
}

static void
page_holder_button_add_clicked_cb (GtkWidget* button,
                                   GtkWidget* notebook)
{
    gint n;
    GtkWidget* view;
    MidoriBrowser* browser;
    const gchar* uri;

    n = page_holder_notebook_append_view (notebook);
    view = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), n);
    browser = midori_browser_get_for_widget (notebook);
    uri = midori_browser_get_current_uri (browser);
    midori_view_set_uri (MIDORI_VIEW (view), uri);
}

static void
page_holder_deactivate_cb (MidoriExtension* extension,
                           GtkWidget*       notebook)
{
    MidoriApp* app = midori_extension_get_app (extension);

    gtk_widget_destroy (notebook);
    g_signal_handlers_disconnect_by_func (
        extension, page_holder_deactivate_cb, notebook);
    g_signal_handlers_disconnect_by_func (
        app, page_holder_app_add_browser_cb, extension);
}

static void
page_holder_app_add_browser_cb (MidoriApp*       app,
                                MidoriBrowser*   browser,
                                MidoriExtension* extension)
{
    GtkWidget* panel;
    GtkWidget* notebook;
    GtkWidget* toolbar;
    GtkToolItem* toolitem;

    panel = katze_object_get_object (browser, "panel");
    notebook = gtk_notebook_new ();
    gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_RIGHT);
    gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
    gtk_widget_show (notebook);
    toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
    gtk_widget_show (toolbar);

    toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_JUMP_TO);
    gtk_tool_item_set_is_important (toolitem, TRUE);
    g_signal_connect (toolitem, "clicked",
            G_CALLBACK (page_holder_button_jump_to_clicked_cb), notebook);
    gtk_widget_show (GTK_WIDGET (toolitem));
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);

    toolitem = gtk_separator_tool_item_new ();
    gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (toolitem), FALSE);
    gtk_tool_item_set_expand (toolitem, TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
    gtk_widget_show (GTK_WIDGET (toolitem));

    toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_ADD);
    gtk_tool_item_set_is_important (toolitem, TRUE);
    g_signal_connect (toolitem, "clicked",
            G_CALLBACK (page_holder_button_add_clicked_cb), notebook);
    gtk_widget_show (GTK_WIDGET (toolitem));
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);

    midori_panel_append_widget (MIDORI_PANEL (panel), notebook,
        /* i18n: A panel showing a user specified web page */
                                STOCK_PAGE_HOLDER, _("Pageholder"), toolbar);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (page_holder_deactivate_cb), notebook);

    g_object_unref (panel);
}

static void
page_holder_activate_cb (MidoriExtension* extension,
                         MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        page_holder_app_add_browser_cb (app, browser, extension);
    g_object_unref (browsers);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (page_holder_app_add_browser_cb), extension);
}

MidoriExtension*
extension_init (void)
{
    GtkIconFactory* factory;
    GtkIconSource* icon_source;
    GtkIconSet* icon_set;
    static GtkStockItem items[] =
    {
        { STOCK_PAGE_HOLDER, N_("_Pageholder"), 0, 0, NULL },
    };

    factory = gtk_icon_factory_new ();
    gtk_stock_add (items, G_N_ELEMENTS (items));
    icon_set = gtk_icon_set_new ();
    icon_source = gtk_icon_source_new ();
    gtk_icon_source_set_icon_name (icon_source, GTK_STOCK_ORIENTATION_PORTRAIT);
    gtk_icon_set_add_source (icon_set, icon_source);
    gtk_icon_source_free (icon_source);
    gtk_icon_factory_add (factory, STOCK_PAGE_HOLDER, icon_set);
    gtk_icon_set_unref (icon_set);
    gtk_icon_factory_add_default (factory);
    g_object_unref (factory);

    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Pageholder"),
        "description", _("Keep one or multiple pages open in parallel to your tabs"),
        "version", "0.1",
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (page_holder_activate_cb), NULL);

    return extension;
}
