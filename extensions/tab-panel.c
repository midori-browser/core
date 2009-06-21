/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>
#include <midori/sokoke.h>

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
    GtkTreeModel* model;
    MidoriBrowser* browser;
    GtkWidget* notebook;

    model = g_object_get_data (G_OBJECT (extension), "treemodel");
    g_object_unref (model);
    browser = midori_browser_get_for_widget (panel);
    notebook = katze_object_get_object (browser, "notebook");
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), TRUE);
    g_object_unref (notebook);

    gtk_widget_destroy (panel);
    g_signal_handlers_disconnect_by_func (
        extension, tab_panel_deactivate_cb, panel);
    g_signal_handlers_disconnect_by_func (
        app, tab_panel_app_add_browser_cb, extension);
}

static void
midori_extension_cursor_or_row_changed_cb (GtkTreeView*     treeview,
                                           MidoriExtension* extension)
{
    /* Nothing to do */
}

static void
midori_extension_treeview_render_icon_cb (GtkTreeViewColumn* column,
                                          GtkCellRenderer*   renderer,
                                          GtkTreeModel*      model,
                                          GtkTreeIter*       iter,
                                          GtkWidget*         treeview)
{
    MidoriView* view;
    GdkPixbuf* pixbuf;

    gtk_tree_model_get (model, iter, 0, &view, -1);

    if ((pixbuf = midori_view_get_icon (view)))
        g_object_set (renderer, "pixbuf", pixbuf, NULL);

    g_object_unref (view);
}

static void
midori_extension_treeview_render_text_cb (GtkTreeViewColumn* column,
                                          GtkCellRenderer*   renderer,
                                          GtkTreeModel*      model,
                                          GtkTreeIter*       iter,
                                          GtkWidget*         treeview)
{
    MidoriView* view;

    gtk_tree_model_get (model, iter, 0, &view, -1);

    g_object_set (renderer, "text", midori_view_get_display_title (view), NULL);

    g_object_unref (view);
}

static void
midori_extension_row_activated_cb (GtkTreeView*       treeview,
                                   GtkTreePath*       path,
                                   GtkTreeViewColumn* column,
                                   MidoriExtension*   extension)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model (treeview);

    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        GtkWidget* view;
        MidoriBrowser* browser;

        gtk_tree_model_get (model, &iter, 0, &view, -1);
        browser = midori_browser_get_for_widget (GTK_WIDGET (treeview));
        midori_browser_set_current_tab (browser, view);

        g_object_unref (view);
    }
}

static void
midori_extension_popup_item (GtkWidget*       menu,
                             const gchar*     stock_id,
                             const gchar*     label,
                             GtkWidget*       view,
                             gpointer         callback,
                             MidoriExtension* extension)
{
    GtkWidget* menuitem;

    menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);
    if (label)
        gtk_label_set_text_with_mnemonic (GTK_LABEL (gtk_bin_get_child (
        GTK_BIN (menuitem))), label);
    g_object_set_data (G_OBJECT (menuitem), "MidoriView", view);
    g_signal_connect (menuitem, "activate", G_CALLBACK (callback), extension);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
}

static void
midori_extension_open_activate_cb (GtkWidget*       menuitem,
                                   MidoriExtension* extension)
{
    GtkWidget* view;
    MidoriBrowser* browser;

    view = (GtkWidget*)g_object_get_data (G_OBJECT (menuitem), "MidoriView");

    browser = midori_browser_get_for_widget (view);
    midori_browser_set_current_tab (browser, view);
}

static void
midori_extension_open_in_window_activate_cb (GtkWidget*       menuitem,
                                             MidoriExtension* extension)
{
    GtkWidget* view;
    MidoriBrowser* new_browser;

    view = (GtkWidget*)g_object_get_data (G_OBJECT (menuitem), "MidoriView");

    new_browser = midori_app_create_browser (midori_extension_get_app (extension));
    midori_app_add_browser (midori_extension_get_app (extension), new_browser);
    gtk_widget_show (GTK_WIDGET (new_browser));
    midori_browser_add_tab (new_browser, view);
}

static void
midori_extension_popup (GtkWidget*       widget,
                        GdkEventButton*  event,
                        GtkWidget*       view,
                        MidoriExtension* extension)
{
    GtkWidget* menu;

    menu = gtk_menu_new ();
    midori_extension_popup_item (menu, GTK_STOCK_OPEN, NULL,
        view, midori_extension_open_activate_cb, extension);
    midori_extension_popup_item (menu, STOCK_WINDOW_NEW, _("Open in New _Window"),
        view, midori_extension_open_in_window_activate_cb, extension);

    sokoke_widget_popup (widget, GTK_MENU (menu),
                         event, SOKOKE_MENU_POSITION_CURSOR);
}

static gboolean
midori_extension_button_release_event_cb (GtkWidget*       widget,
                                          GdkEventButton*  event,
                                          MidoriExtension* extension)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (event->button != 2 && event->button != 3)
        return FALSE;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        GtkWidget* view;

        gtk_tree_model_get (model, &iter, 0, &view, -1);

        if (event->button == 2)
        {
            MidoriBrowser* browser = midori_browser_get_for_widget (widget);
            midori_browser_set_current_tab (browser, view);
        }
        else
            midori_extension_popup (widget, event, view, extension);

        g_object_unref (view);
        return TRUE;
    }
    return FALSE;
}

static gboolean
midori_extension_key_release_event_cb (GtkWidget*       widget,
                                       GdkEventKey*     event,
                                       MidoriExtension* extension)
{
    /* Nothing to do */

    return FALSE;
}

static void
midori_extension_popup_menu_cb (GtkWidget*       widget,
                                MidoriExtension* extension)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        GtkWidget* view;

        gtk_tree_model_get (model, &iter, 0, &view, -1);
        midori_extension_popup (widget, NULL, view, extension);
        g_object_unref (view);
    }
}

static void
tab_panel_browser_add_tab_cb (MidoriBrowser*   browser,
                              MidoriView*      view,
                              MidoriExtension* extension)
{
    GtkTreeModel* model = g_object_get_data (G_OBJECT (extension), "treemodel");
    GtkTreeIter iter;
    gtk_tree_store_insert_with_values (GTK_TREE_STORE (model),
        &iter, NULL, G_MAXINT, 0, view, -1);
}

static void
tab_panel_browser_foreach_cb (GtkWidget*       view,
                              MidoriExtension* extension)
{
    tab_panel_browser_add_tab_cb (midori_browser_get_for_widget (view),
                                  MIDORI_VIEW (view), extension);
}

static void
tab_panel_browser_remove_tab_cb (MidoriBrowser*   browser,
                                 MidoriView*      view,
                                 MidoriExtension* extension)
{

}

static void
tab_panel_app_add_browser_cb (MidoriApp*       app,
                              MidoriBrowser*   browser,
                              MidoriExtension* extension)
{
    GtkTreeStore* model;
    GtkWidget* treeview;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_pixbuf;
    GtkCellRenderer* renderer_text;
    GtkWidget* panel;
    GtkWidget* notebook;
    GtkWidget* toolbar;
    /* GtkToolItem* toolitem; */

    notebook = katze_object_get_object (browser, "notebook");
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
    g_object_unref (notebook);

    panel = katze_object_get_object (browser, "panel");

    model = g_object_get_data (G_OBJECT (extension), "treemodel");
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_extension_treeview_render_icon_cb,
        treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_extension_treeview_render_text_cb,
        treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    g_object_connect (treeview,
                      "signal::row-activated",
                      midori_extension_row_activated_cb, extension,
                      "signal::cursor-changed",
                      midori_extension_cursor_or_row_changed_cb, extension,
                      "signal::columns-changed",
                      midori_extension_cursor_or_row_changed_cb, extension,
                      "signal::button-release-event",
                      midori_extension_button_release_event_cb, extension,
                      "signal::key-release-event",
                      midori_extension_key_release_event_cb, extension,
                      "signal::popup-menu",
                      midori_extension_popup_menu_cb, extension,
                      NULL);
    gtk_widget_show (treeview);

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

    midori_panel_append_widget (MIDORI_PANEL (panel), treeview,
                                STOCK_TAB_PANEL, _("Tab Panel"), toolbar);
    g_object_unref (panel);

    midori_browser_foreach (browser,
        (GtkCallback)tab_panel_browser_foreach_cb, treeview);

    g_signal_connect (browser, "add-tab",
        G_CALLBACK (tab_panel_browser_add_tab_cb), extension);
    g_signal_connect (browser, "remove-tab",
        G_CALLBACK (tab_panel_browser_remove_tab_cb), extension);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (tab_panel_deactivate_cb), notebook);
}

static void
tab_panel_activate_cb (MidoriExtension* extension,
                       MidoriApp*       app)
{
    GtkTreeStore* model;
    KatzeArray* browsers;
    MidoriBrowser* browser;
    guint i;

    model = gtk_tree_store_new (1, MIDORI_TYPE_VIEW);
    g_object_set_data (G_OBJECT (extension), "treemodel", model);

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
