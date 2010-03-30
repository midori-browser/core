/*
 Copyright (C) 2009 André Stösel <Midori-Plugin@PyIT.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

enum { TAB_ICON, TAB_NAME, TAB_POINTER, TAB_CELL_COUNT };

static MidoriExtension *thisExtension;
static gboolean switchEvent;

static GdkPixbuf* tab_selector_get_snapshot(MidoriView* view,
                                            gint       maxwidth,
                                            gint       maxheight)
{
    GtkWidget* web_view;
    guint width, height;
    gfloat factor;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    web_view = midori_view_get_web_view (view);

    if(maxwidth < 0) {
        maxwidth *= -1;
    }
    if(maxheight < 0) {
        maxheight *= -1;
    }

    factor = MIN((gfloat) maxwidth / web_view->allocation.width, (gfloat) maxheight / web_view->allocation.height);
    width = (int)(factor * web_view->allocation.width);
    height = (int)(factor * web_view->allocation.height);

    return midori_view_get_snapshot(view, width, height);
}

static void tab_selector_list_foreach (GtkWidget    *view,
                                       GtkListStore *store)
{
    GtkTreeIter it;
    GdkPixbuf* icon = midori_view_get_icon (MIDORI_VIEW (view));
    const gchar *title = midori_view_get_display_title (MIDORI_VIEW (view));
    gtk_list_store_append (store, &it);
    gtk_list_store_set (store, &it, TAB_ICON, icon, -1);
    gtk_list_store_set (store, &it, TAB_NAME, title, -1);
    gtk_list_store_set (store, &it, TAB_POINTER, view, -1);
}

static GtkWidget* tab_selector_init_window (MidoriBrowser   *browser)
{
    GList *list;
    gint col_offset;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkWidget *window, *treeview, *sw, *hbox;
    GtkListStore *store;
    GtkWidget *page;
    GtkWidget *image;
    GdkPixbuf *snapshot;

    window = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_default_size(GTK_WINDOW(window), 320, 20);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);


    hbox = gtk_hbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(window), hbox);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 1);

    sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
            GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
            GTK_POLICY_NEVER,
            GTK_POLICY_AUTOMATIC);

    gtk_box_pack_start (GTK_BOX (hbox), sw, TRUE, TRUE, 0);

    store = gtk_list_store_new(TAB_CELL_COUNT, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_set_data(G_OBJECT(window), "tab_selector_treeview", treeview);

    list = g_object_get_data(G_OBJECT(browser), "tab_selector_list");
    g_list_foreach(list, (GFunc) tab_selector_list_foreach, store);

    g_object_unref(store);
    g_object_set(treeview, "headers-visible", FALSE, NULL);

    renderer = gtk_cell_renderer_pixbuf_new();

    gtk_tree_view_insert_column_with_attributes(
            GTK_TREE_VIEW(treeview), -1, "Icon", renderer, "pixbuf", TAB_ICON, NULL);

    renderer = gtk_cell_renderer_text_new();

    col_offset = gtk_tree_view_insert_column_with_attributes(
            GTK_TREE_VIEW(treeview), -1, "Title", renderer, "text", TAB_NAME, NULL);
    column = gtk_tree_view_get_column (GTK_TREE_VIEW (treeview), col_offset - 1);
    gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),
            GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (column),
            midori_extension_get_integer(thisExtension, "TitleColumnWidth"));

    gtk_container_add (GTK_CONTAINER (sw), treeview);

    page = katze_object_get_object(browser, "tab");
    snapshot = tab_selector_get_snapshot(MIDORI_VIEW(page),
            midori_extension_get_integer(thisExtension, "TabPreviewWidth"),
            midori_extension_get_integer(thisExtension, "TabPreviewHeight"));
    image = gtk_image_new_from_pixbuf (snapshot);
    gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, TRUE, 0);
    g_object_set_data(G_OBJECT(window), "tab_selector_image", image);

    gtk_widget_show_all(window);

    return window;
}

static void tab_selector_window_walk (  GtkWidget       *window,
                                        GdkEventKey     *event,
                                        MidoriBrowser   *browser)
{
    gint *pindex, iindex, items;
    GtkWidget *view;
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeView *treeview;
    GtkTreeModel *model;
    GtkTreeViewColumn *column;

    treeview = g_object_get_data (G_OBJECT (window), "tab_selector_treeview");
    model = gtk_tree_view_get_model (treeview);
    items = gtk_tree_model_iter_n_children (model, NULL) -1;
    gtk_tree_view_get_cursor (treeview, &path, &column);
    pindex = gtk_tree_path_get_indices (path);
    if(!pindex)
        return;
    iindex = *pindex;
    gtk_tree_path_free(path);

    if (event->state & GDK_SHIFT_MASK)
        iindex = iindex == 0 ? items : iindex-1;
    else
        iindex = iindex == items ? 0 : iindex+1;

    path = gtk_tree_path_new_from_indices(iindex, -1);
    column = gtk_tree_view_get_column (GTK_TREE_VIEW (treeview), 1);
    gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, column, FALSE);

    gtk_tree_model_get_iter (model, &iter, path);
    gtk_tree_model_get (model, &iter, TAB_POINTER, &view, -1);

    if (midori_extension_get_boolean (thisExtension, "ShowTabInBackground")) {
        midori_browser_set_current_tab (browser, view);
    } else {
        GtkImage *image;
        GdkPixbuf *snapshot = tab_selector_get_snapshot(MIDORI_VIEW(view),
                midori_extension_get_integer(thisExtension, "TabPreviewWidth"),
                midori_extension_get_integer(thisExtension, "TabPreviewHeight"));
        image = g_object_get_data(G_OBJECT(window), "tab_selector_image");
        gtk_image_set_from_pixbuf(image, snapshot);
    }
    gtk_tree_path_free(path);
}

static gboolean tab_selector_handle_events (GtkWidget       *widget,
                                            GdkEventKey     *event,
                                            MidoriBrowser   *browser)
{
    /* tab -> 23
       ctrl -> 37 */
    gint treeitems;
    static GtkWidget *window;
    if(event->type == GDK_KEY_PRESS && event->hardware_keycode == 23 && event->state & GDK_CONTROL_MASK) {
        treeitems = gtk_notebook_get_n_pages (GTK_NOTEBOOK (
                katze_object_get_object(browser, "notebook")));
        if(treeitems > 1) {
            if(!GTK_IS_WINDOW(window)) {
                switchEvent = FALSE;
                window = tab_selector_init_window(browser);
            }
            tab_selector_window_walk(window, event, browser);
        }
        return TRUE;
    } else if(event->type == GDK_KEY_RELEASE && event->hardware_keycode == 37 && GTK_IS_WINDOW(window)) {
        switchEvent = TRUE;
        if(midori_extension_get_boolean(thisExtension, "ShowTabInBackground")) {
            GtkWidget *page;
            page = katze_object_get_object(browser, "tab");

            GList *list = g_object_get_data(G_OBJECT(browser), "tab_selector_list");
            list = g_list_remove(list, page);
            list = g_list_prepend(list, page);
            g_object_set_data(G_OBJECT(browser), "tab_selector_list", list);
        } else {
            GtkTreePath *path;
            GtkTreeViewColumn *column;
            GtkTreeIter iter;
            GtkWidget *view, *treeview;
            GtkTreeModel *model;

            treeview = g_object_get_data(G_OBJECT(window), "tab_selector_treeview");
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

            gtk_tree_view_get_cursor (
                    GTK_TREE_VIEW(treeview), &path, &column);
            gtk_tree_model_get_iter (
                    model, &iter, path);
            gtk_tree_model_get (
                    model, &iter, TAB_POINTER, &view, -1);
            midori_browser_set_current_tab (browser, view);
            gtk_tree_path_free (path);
        }
        gtk_widget_destroy(window);
        window = NULL;
        return TRUE;
    }
    return FALSE;
}

static void tab_selector_switch_page (GtkNotebook     *notebook,
                                      GtkNotebookPage *page_,
                                      guint            page_num,
                                      MidoriBrowser   *browser)
{
    if(switchEvent) {
        /* Don't know why *page_ points to the wrong address */
        GtkWidget *page;
        page = katze_object_get_object(browser, "tab");

        GList *list = g_object_get_data(G_OBJECT(browser), "tab_selector_list");
        list = g_list_remove(list, page);
        list = g_list_prepend(list, page);
        g_object_set_data(G_OBJECT(browser), "tab_selector_list", list);
    }
}

static void
tab_selector_browser_add_tab_cb (MidoriBrowser      *browser,
                                 GtkWidget          *view,
                                 MidoriExtension    *extension)
{
    g_signal_connect (view, "key_press_event",
            G_CALLBACK (tab_selector_handle_events), browser);
    g_signal_connect (view, "key_release_event",
            G_CALLBACK (tab_selector_handle_events), browser);

    GList *list = g_object_get_data(G_OBJECT(browser), "tab_selector_list");
    list = g_list_append(list, view);
    g_object_set_data(G_OBJECT(browser), "tab_selector_list", list);
}

static void
tab_selector_browser_remove_tab_cb (MidoriBrowser      *browser,
                                    GtkWidget          *view,
                                    MidoriExtension    *extension)
{
    GList *list = g_object_get_data(G_OBJECT(browser), "tab_selector_list");
    list = g_list_remove(list, view);
    g_object_set_data(G_OBJECT(browser), "tab_selector_list", list);
}

static void
tab_selector_disconnect_tab_cb (GtkWidget     *view,
                                MidoriBrowser *browser)
{
    g_signal_handlers_disconnect_by_func (
        view, tab_selector_handle_events, browser);
}

static void
tab_selector_app_add_browser_cb (MidoriApp       *app,
                                 MidoriBrowser   *browser,
                                 MidoriExtension *extension)
{
    GtkWidget *navigationbar, *notebook;

    g_object_set_data(G_OBJECT(browser), "tab_selector_list", NULL);

    g_signal_connect (browser, "add-tab",
        G_CALLBACK (tab_selector_browser_add_tab_cb), extension);
    g_signal_connect (browser, "remove-tab",
        G_CALLBACK (tab_selector_browser_remove_tab_cb), extension);

    navigationbar = katze_object_get_object(browser, "navigationbar");
    g_signal_connect (navigationbar, "key_press_event",
            G_CALLBACK (tab_selector_handle_events), browser);
    g_signal_connect (navigationbar, "key_release_event",
            G_CALLBACK (tab_selector_handle_events), browser);
    g_object_unref(navigationbar);

    notebook = katze_object_get_object(browser, "notebook");
    g_signal_connect_after (notebook, "switch-page",
            G_CALLBACK (tab_selector_switch_page), browser);
    g_object_unref(notebook);
}

static void
tab_selector_app_remove_browser_cb (MidoriApp       *app,
                                    MidoriBrowser   *browser,
                                    MidoriExtension *extension)
{
    GList *list = g_object_get_data (G_OBJECT (browser), "tab_selector_list");
    g_list_free (list);
}

static void
tab_selector_disconnect_browser_cb (MidoriApp       *app,
                                    MidoriBrowser   *browser,
                                    MidoriExtension *extension)
{
    GtkWidget *navigationbar, *notebook;

    midori_browser_foreach (browser,
        (GtkCallback)tab_selector_disconnect_tab_cb, browser);

    g_signal_handlers_disconnect_by_func (
        browser, tab_selector_browser_add_tab_cb, extension);
    g_signal_handlers_disconnect_by_func (
        browser, tab_selector_browser_remove_tab_cb, extension);
    g_signal_handlers_disconnect_by_func (
        katze_object_get_object (browser, "navigationbar"),
        tab_selector_handle_events, browser);

    navigationbar = katze_object_get_object (browser, "navigationbar");
    g_signal_handlers_disconnect_by_func (navigationbar,
            tab_selector_handle_events, browser);
    g_signal_handlers_disconnect_by_func (navigationbar,
            tab_selector_handle_events, browser);
    g_object_unref (navigationbar);

    notebook = katze_object_get_object (browser, "notebook");
    g_signal_handlers_disconnect_by_func (notebook,
            tab_selector_switch_page, browser);
    g_object_unref (notebook);
}

static void
tab_selector_activate_cb (MidoriExtension   *extension,
                          MidoriApp         *app)
{
    GtkWidget *view;
    KatzeArray *browsers;
    MidoriBrowser *browser;
    guint i, j;

    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++))) {
        j = 0;
        tab_selector_app_add_browser_cb (app, browser, extension);
        while((view = midori_browser_get_nth_tab(browser, j++)))
            tab_selector_browser_add_tab_cb(browser, view, extension);
    }
    g_object_unref (browsers);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (tab_selector_app_add_browser_cb), extension);
    g_signal_connect (app, "remove-browser",
        G_CALLBACK (tab_selector_app_remove_browser_cb), extension);
}

static void
tab_selector_deactivate_cb (MidoriExtension *extension,
                            GtkWidget *foo)
{
    MidoriApp* app = midori_extension_get_app (extension);
    KatzeArray *browsers;
    MidoriBrowser *browser;
    guint i;

    g_signal_handlers_disconnect_by_func (
        app, tab_selector_app_add_browser_cb, extension);
    g_signal_handlers_disconnect_by_func (
        app, tab_selector_app_remove_browser_cb, extension);

    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++)))
        tab_selector_disconnect_browser_cb (app, browser, extension);
    g_object_unref (browsers);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension *extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Tab History List"),
        "description", _("Allows to switch tabs by choosing from a "
                         "list sorted by last usage"),
        "version", "0.1",
        "authors", "André Stösel <Midori-Plugin@PyIT.de>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (tab_selector_activate_cb), NULL);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (tab_selector_deactivate_cb), NULL);

    midori_extension_install_boolean (extension, "ShowTabInBackground", FALSE);
    midori_extension_install_integer (extension, "TitleColumnWidth", 300);
    midori_extension_install_integer (extension, "TabPreviewWidth", 200);
    midori_extension_install_integer (extension, "TabPreviewHeight", 200);
    thisExtension = extension;
    switchEvent = TRUE;

    return extension;
}

