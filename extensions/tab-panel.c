/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

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
tab_panel_settings_notify_cb (MidoriWebSettings* settings,
                              GParamSpec*        pspec,
                              GtkTreeModel*      model);

static void
tab_panel_browser_add_tab_cb (MidoriBrowser*   browser,
                              GtkWidget*       view,
                              MidoriExtension* extension);

static void
tab_panel_browser_remove_tab_cb (MidoriBrowser*   browser,
                                 GtkWidget*       view,
                                 MidoriExtension* extension);

static void
tab_panel_browser_notify_tab_cb (MidoriBrowser* browser,
                                 GParamSpec*    pspec,
                                 GtkTreeView*   treeview);
static void
tab_panel_browser_move_tab_cb (MidoriBrowser* browser,
                               GtkWidget*     notebook,
                               gint           cur_pos,
                               gint           new_pos,
                               gpointer       user_data);

static void
tab_panel_view_notify_minimized_cb (GtkWidget*       view,
                                    GParamSpec*      pspec,
                                    MidoriExtension* extension);

static void
tab_panel_view_notify_icon_cb (GtkWidget*       view,
                               GParamSpec*      pspec,
                               MidoriExtension* extension);

static void
tab_panel_view_notify_title_cb (GtkWidget*       view,
                                GParamSpec*      pspec,
                                MidoriExtension* extension);

static GtkTreeModel*
tab_panel_get_model_for_browser (MidoriBrowser* browser)
{
    return g_object_get_data (G_OBJECT (browser), "tab-panel-ext-model");
}

static GtkWidget*
tab_panel_get_toolbar_for_browser (MidoriBrowser* browser)
{
    return g_object_get_data (G_OBJECT (browser), "tab-panel-ext-toolbar");
}

static GtkToolItem*
tab_panel_get_toolitem_for_view (GtkWidget* view)
{
    return g_object_get_data (G_OBJECT (view), "tab-panel-ext-toolitem");
}

static gboolean
tab_panel_get_iter_for_view (GtkTreeModel* model,
                             GtkTreeIter*  iter,
                             gpointer      view)
{
    guint i = 0;

    while (gtk_tree_model_iter_nth_child (model, iter, NULL, i))
    {
        MidoriView* view_;

        gtk_tree_model_get (model, iter, 0, &view_, -1);
        g_object_unref (view_);
        if (view_ == view)
            return TRUE;
        i++;
    }

    return FALSE;
}

static void
tab_panel_deactivate_cb (MidoriExtension* extension,
                         GtkWidget*       treeview)
{
    MidoriApp* app = midori_extension_get_app (extension);
    MidoriBrowser* browser = midori_browser_get_for_widget (treeview);
    GtkTreeModel* model = tab_panel_get_model_for_browser (browser);
    GList* tabs = midori_browser_get_tabs (browser);
    for (; tabs; tabs = g_list_next (tabs))
    {
        g_signal_handlers_disconnect_by_func (
            tabs->data, tab_panel_view_notify_minimized_cb, extension);
        g_signal_handlers_disconnect_by_func (
            tabs->data, tab_panel_view_notify_icon_cb, extension);
        g_signal_handlers_disconnect_by_func (
            tabs->data, tab_panel_view_notify_title_cb, extension);
    }
    g_list_free (tabs);

    g_signal_handlers_disconnect_by_func (
        extension, tab_panel_deactivate_cb, treeview);
    g_signal_handlers_disconnect_by_func (
        app, tab_panel_app_add_browser_cb, extension);
    g_signal_handlers_disconnect_by_func (
        browser, tab_panel_browser_add_tab_cb, extension);
    g_signal_handlers_disconnect_by_func (
        browser, tab_panel_browser_remove_tab_cb, extension);
    g_signal_handlers_disconnect_by_func (
        browser, tab_panel_browser_notify_tab_cb, treeview);
    g_signal_handlers_disconnect_by_func (
        browser, tab_panel_settings_notify_cb, model);
    g_signal_handlers_disconnect_by_func (
        browser, tab_panel_browser_move_tab_cb, NULL);

    gtk_widget_destroy (treeview);
    g_object_unref (model);
    g_object_set_data (G_OBJECT (browser), "tab-panel-ext-model", NULL);
    g_object_set (browser, "show-tabs", TRUE, NULL);

}

static void
midori_extension_cursor_or_row_changed_cb (GtkTreeView*     treeview,
                                           MidoriExtension* extension)
{
    /* Nothing to do */
}

static gboolean
tab_panel_treeview_query_tooltip_cb (GtkWidget*  treeview,
                                     gint        x,
                                     gint        y,
                                     gboolean    keyboard_tip,
                                     GtkTooltip* tooltip,
                                     gpointer    user_data)
{
    GtkTreeIter iter;
    GtkTreePath* path;
    GtkTreeModel* model;
    MidoriView* view;

    if (!gtk_tree_view_get_tooltip_context (GTK_TREE_VIEW (treeview),
        &x, &y, keyboard_tip, &model, &path, &iter))
        return FALSE;

    gtk_tree_model_get (model, &iter, 0, &view, -1);

    gtk_tooltip_set_text (tooltip, midori_view_get_display_title (view));
    gtk_tree_view_set_tooltip_row (GTK_TREE_VIEW (treeview), tooltip, path);

    gtk_tree_path_free (path);
    g_object_unref (view);

    return TRUE;
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
tab_panel_popup (GtkWidget*      widget,
                 GdkEventButton* event,
                 GtkWidget*      view)
{
    GtkWidget* menu = midori_view_get_tab_menu (MIDORI_VIEW (view));

    katze_widget_popup (widget, GTK_MENU (menu), event, KATZE_MENU_POSITION_CURSOR);
}

static gboolean
midori_extension_button_release_event_cb (GtkWidget*       widget,
                                          GdkEventButton*  event,
                                          MidoriExtension* extension)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (event->button < 1 || event->button > 3)
        return FALSE;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        GtkWidget* view;

        gtk_tree_model_get (model, &iter, 0, &view, -1);

        if (event->button == 1)
        {
            MidoriBrowser* browser = midori_browser_get_for_widget (widget);
            GtkTreeViewColumn* column;
            if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                event->x, event->y, NULL, &column, NULL, NULL)
                && column == gtk_tree_view_get_column (GTK_TREE_VIEW (widget), 1))
                midori_browser_close_tab (browser, view);
            else
                midori_browser_set_current_tab (browser, view);
        }
        else if (event->button == 2)
            midori_browser_close_tab (midori_browser_get_for_widget (widget), view);
        else
            tab_panel_popup (widget, event, view);

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
        tab_panel_popup (widget, NULL, view);
        g_object_unref (view);
    }
}

static void
tab_panel_settings_notify_cb (MidoriWebSettings* settings,
                              GParamSpec*        pspec,
                              GtkTreeModel*      model)
{
    gboolean buttons = katze_object_get_boolean (settings, "close-buttons-on-tabs");
    guint i;
    GtkTreeIter iter;

    i = 0;
    while (gtk_tree_model_iter_nth_child (model, &iter, NULL, i++))
        gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 2, buttons, -1);
}

static void
tab_panel_toggle_toolbook (GtkWidget* toolbar)
{
        /* Hack to ensure correct toolbar visibility */
        GtkWidget* toolbook = gtk_widget_get_parent (toolbar);
        if (gtk_notebook_get_current_page (GTK_NOTEBOOK (toolbook))
         == gtk_notebook_page_num (GTK_NOTEBOOK (toolbook), toolbar))
        {
            GList* items = gtk_container_get_children (GTK_CONTAINER (toolbar));
            sokoke_widget_set_visible (toolbook, items != NULL);
            g_list_free (items);
        }
}

static void
tab_panel_remove_view (MidoriBrowser* browser,
                       GtkWidget*     view,
                       gboolean       minimized)
{
    if (minimized)
    {
        GtkToolItem* toolitem = tab_panel_get_toolitem_for_view (view);
        GtkWidget* toolbar = tab_panel_get_toolbar_for_browser (browser);
        gtk_widget_destroy (GTK_WIDGET (toolitem));
        tab_panel_toggle_toolbook (toolbar);
    }
    else
    {
        GtkTreeModel* model = tab_panel_get_model_for_browser (browser);
        GtkTreeIter iter;
        if (tab_panel_get_iter_for_view (model, &iter, view))
            gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
    }
}

static void
tab_panel_view_notify_minimized_cb (GtkWidget*       view,
                                    GParamSpec*      pspec,
                                    MidoriExtension* extension)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (view);
    gboolean minimized = katze_object_get_boolean (view, "minimized");

    tab_panel_remove_view (browser, view, !minimized);
    tab_panel_browser_add_tab_cb (browser, view, extension);
}

static void
tab_panel_view_notify_icon_cb (GtkWidget*       view,
                               GParamSpec*      pspec,
                               MidoriExtension* extension)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (view);
    gboolean minimized = katze_object_get_boolean (view, "minimized");
    GdkPixbuf* icon = midori_view_get_icon (MIDORI_VIEW (view));

    if (minimized)
    {
        GtkToolItem* toolitem = tab_panel_get_toolitem_for_view (view);
        GtkWidget* image = gtk_tool_button_get_icon_widget (GTK_TOOL_BUTTON (toolitem));
        gtk_image_set_from_pixbuf (GTK_IMAGE (image), icon);
    }
    else
    {
        GtkTreeModel* model = tab_panel_get_model_for_browser (browser);
        GtkTreeIter iter;
        GdkColor* fg = midori_tab_get_fg_color (MIDORI_TAB (view));
        GdkColor* bg = midori_tab_get_bg_color (MIDORI_TAB (view));

        if (tab_panel_get_iter_for_view (model, &iter, view))
            gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
                3, icon,
                6, bg,
                7, fg,
                -1);
    }
}

static void
tab_panel_view_notify_title_cb (GtkWidget*       view,
                                GParamSpec*      pspec,
                                MidoriExtension* extension)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (view);
    gboolean minimized = katze_object_get_boolean (view, "minimized");
    const gchar* title = midori_view_get_display_title (MIDORI_VIEW (view));

    if (minimized)
    {
        GtkToolItem* toolitem = tab_panel_get_toolitem_for_view (view);
        gtk_tool_item_set_tooltip_text (toolitem, title);
    }
    else
    {
        GtkTreeModel* model = tab_panel_get_model_for_browser (browser);
        GtkTreeIter iter;
        GdkColor* fg = midori_tab_get_fg_color (MIDORI_TAB (view));
        GdkColor* bg = midori_tab_get_bg_color (MIDORI_TAB (view));
        if (tab_panel_get_iter_for_view (model, &iter, view))
        {
            gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
                4, title,
                5, midori_view_get_label_ellipsize (MIDORI_VIEW (view)),
                6, bg,
                7, fg,
                -1);
        }
    }
}

static void
tab_panel_toolitem_clicked_cb (GtkToolItem* toolitem,
                               GtkWidget*   view)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (view);
    midori_browser_set_current_tab (browser, view);
}

static gboolean
tab_panel_toolitem_button_press_event_cb (GtkToolItem*    toolitem,
                                          GdkEventButton* event,
                                          GtkWidget*      view)
{
    if (MIDORI_EVENT_CONTEXT_MENU (event))
    {
        tab_panel_popup (GTK_WIDGET (toolitem), event, view);
        return TRUE;
    }

    return FALSE;
}

static void
tab_panel_browser_add_tab_cb (MidoriBrowser*   browser,
                              GtkWidget*       view,
                              MidoriExtension* extension)
{
    gint page = midori_browser_page_num (browser, view);
    MidoriWebSettings* settings = midori_browser_get_settings (browser);
    gboolean minimized = katze_object_get_boolean (view, "minimized");
    GdkPixbuf* icon = midori_view_get_icon (MIDORI_VIEW (view));
    const gchar* title = midori_view_get_display_title (MIDORI_VIEW (view));
    GtkTreeModel* model = tab_panel_get_model_for_browser (browser);

    if (minimized)
    {
        GtkWidget* toolbar = tab_panel_get_toolbar_for_browser (browser);
        GtkWidget* image = gtk_image_new_from_pixbuf (
            midori_view_get_icon (MIDORI_VIEW (view)));
        GtkToolItem* toolitem = gtk_tool_button_new (image, NULL);
        gtk_tool_item_set_tooltip_text (toolitem, title);
        gtk_widget_show (image);
        g_object_set_data (G_OBJECT (view), "tab-panel-ext-toolitem", toolitem);
        gtk_widget_show (GTK_WIDGET (toolitem));
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        tab_panel_toggle_toolbook (toolbar);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (tab_panel_toolitem_clicked_cb), view);
        g_signal_connect (gtk_bin_get_child (GTK_BIN (toolitem)), "button-press-event",
            G_CALLBACK (tab_panel_toolitem_button_press_event_cb), view);
    }
    else
    {
        GtkTreeIter iter;
        gboolean buttons = katze_object_get_boolean (settings, "close-buttons-on-tabs");
        gint ellipsize = midori_view_get_label_ellipsize (MIDORI_VIEW (view));
        GdkColor* fg = midori_tab_get_fg_color (MIDORI_TAB (view));
        GdkColor* bg = midori_tab_get_bg_color (MIDORI_TAB (view));

        gtk_tree_store_insert_with_values (GTK_TREE_STORE (model),
            &iter, NULL, page, 0, view, 1, GTK_STOCK_CLOSE, 2, buttons,
            3, icon, 4, title, 5, ellipsize, 6, bg, 7, fg, -1);
    }

    if (!g_signal_handler_find (view, G_SIGNAL_MATCH_FUNC,
        g_signal_lookup ("notify", MIDORI_TYPE_VIEW), 0, NULL,
        tab_panel_view_notify_minimized_cb, extension))
    {
        g_signal_connect (settings, "notify::close-buttons-on-tabs",
            G_CALLBACK (tab_panel_settings_notify_cb), model);
        g_signal_connect (view, "notify::minimized",
            G_CALLBACK (tab_panel_view_notify_minimized_cb), extension);
        g_signal_connect (view, "notify::icon",
            G_CALLBACK (tab_panel_view_notify_icon_cb), extension);
        g_signal_connect (view, "notify::title",
            G_CALLBACK (tab_panel_view_notify_title_cb), extension);
    }
}

static void
tab_panel_browser_remove_tab_cb (MidoriBrowser*   browser,
                                 GtkWidget*       view,
                                 MidoriExtension* extension)
{
    gboolean minimized = katze_object_get_boolean (view, "minimized");

    if (!g_object_get_data (G_OBJECT (browser), "midori-browser-destroyed"))
        tab_panel_remove_view (browser, view, minimized);
}

static void
tab_panel_browser_notify_tab_cb (MidoriBrowser* browser,
                                 GParamSpec*    pspec,
                                 GtkTreeView*   treeview)
{
    GtkTreeModel* model = tab_panel_get_model_for_browser (browser);
    GtkTreeIter iter;
    GtkWidget* view;

    if (g_object_get_data (G_OBJECT (browser), "midori-browser-destroyed"))
        return;

    view = midori_browser_get_current_tab (browser);
    if (tab_panel_get_iter_for_view (model, &iter, view))
    {
        GtkTreeSelection* selection = gtk_tree_view_get_selection (treeview);
        gtk_tree_selection_select_iter (selection, &iter);
    }
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
    GtkWidget* toolbar;
    gint i;
    /* GtkToolItem* toolitem; */

    g_object_set (browser, "show-tabs", FALSE, NULL);

    panel = katze_object_get_object (browser, "panel");

    model = gtk_tree_store_new (8, MIDORI_TYPE_VIEW,
        G_TYPE_STRING, G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF, G_TYPE_STRING,
        G_TYPE_INT, GDK_TYPE_COLOR, GDK_TYPE_COLOR);
    g_object_set_data (G_OBJECT (browser), "tab-panel-ext-model", model);
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    gtk_tree_view_set_show_expanders (GTK_TREE_VIEW (treeview), FALSE);
    g_signal_connect (treeview, "query-tooltip",
        G_CALLBACK (tab_panel_treeview_query_tooltip_cb), NULL);
    gtk_widget_set_has_tooltip (treeview, TRUE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer_pixbuf,
        "pixbuf", 3, "cell-background-gdk", 6, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer_text,
        "text", 4, "ellipsize", 5,
        "cell-background-gdk", 6, "foreground-gdk", 7, NULL);
    gtk_tree_view_column_set_expand (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer_pixbuf,
        "stock-id", 1, "follow-state", 2,
        "visible", 2, "cell-background-gdk", 6, NULL);
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
    g_object_set_data (G_OBJECT (browser), "tab-panel-ext-toolbar", toolbar);
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

    i = midori_panel_append_widget (MIDORI_PANEL (panel), treeview,
                                    STOCK_TAB_PANEL, _("Tab Panel"), toolbar);
    if (gtk_widget_get_visible (GTK_WIDGET (browser)))
        midori_panel_set_current_page (MIDORI_PANEL (panel), i);
    g_object_unref (panel);

    GList* tabs = midori_browser_get_tabs (browser);
    for (; tabs; tabs = g_list_next (tabs))
        tab_panel_browser_add_tab_cb (browser, tabs->data, extension);
    g_list_free (tabs);

    g_signal_connect_after (browser, "add-tab",
        G_CALLBACK (tab_panel_browser_add_tab_cb), extension);
    g_signal_connect (browser, "remove-tab",
        G_CALLBACK (tab_panel_browser_remove_tab_cb), extension);
    g_signal_connect (browser, "notify::tab",
        G_CALLBACK (tab_panel_browser_notify_tab_cb), treeview);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (tab_panel_deactivate_cb), treeview);
    g_signal_connect (browser, "move-tab",
        G_CALLBACK (tab_panel_browser_move_tab_cb), NULL);
}

static void
tab_panel_activate_cb (MidoriExtension* extension,
                       MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        tab_panel_app_add_browser_cb (app, browser, extension);
    g_object_unref (browsers);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (tab_panel_app_add_browser_cb), extension);
}

static void
tab_panel_browser_move_tab_cb (MidoriBrowser* browser,
                               GtkWidget*     notebook,
                               gint           cur_pos,
                               gint           new_pos,
                               gpointer       user_data)
{
    GtkTreeIter cur, new;
    gint last_page;
    GtkTreeModel *model;

    last_page = midori_browser_get_n_pages (browser) - 1;
    model = tab_panel_get_model_for_browser (browser);

    gtk_tree_model_iter_nth_child (model, &cur, NULL, cur_pos);

    if (cur_pos == 0 && new_pos == last_page)
        gtk_tree_store_move_before (GTK_TREE_STORE (model), &cur, NULL);
    else if (cur_pos == last_page && new_pos == 0)
        gtk_tree_store_move_after (GTK_TREE_STORE (model), &cur, NULL);
    else
    {
        gtk_tree_model_iter_nth_child (model, &new, NULL, new_pos);
        gtk_tree_store_swap (GTK_TREE_STORE (model), &cur, &new);
    }
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
    MidoriExtension* extension;

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

    extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Tab Panel"),
        "description", _("Show tabs in a vertical panel"),
        "version", "0.1" MIDORI_VERSION_SUFFIX,
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (tab_panel_activate_cb), NULL);

    return extension;
}
