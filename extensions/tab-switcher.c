/*
 Copyright (C) 2009 André Stösel <Midori-Plugin@PyIT.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

enum { TAB_NAME, TAB_POINTER, TAB_CELL_COUNT };

static MidoriExtension *thisExtension;
static gboolean switchEvent;

static void tab_selector_list_foreach (GtkWidget    *view,
                                       GtkListStore *store)
{
    GtkTreeIter it;
    const gchar *title = midori_view_get_display_title (MIDORI_VIEW (view));
    gtk_list_store_append (store, &it);
    gtk_list_store_set (store, &it, TAB_NAME, title, -1);
    gtk_list_store_set (store, &it, TAB_POINTER, view, -1);
}

static GtkWidget* tab_selector_init_window (MidoriBrowser   *browser)
{
    GList *list;
    GtkCellRenderer *renderer;
    GtkWidget *window, *treeview;
    GtkListStore *store;

    window = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_default_size(GTK_WINDOW(window), 320, 20);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    store = gtk_list_store_new(TAB_CELL_COUNT, G_TYPE_STRING, G_TYPE_POINTER);
    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_set_data(G_OBJECT(window), "tab_selector_treeview", treeview);

    list = g_object_get_data(G_OBJECT(browser), "tab_selector_list");
    g_list_foreach(list, (GFunc) tab_selector_list_foreach, store);

    g_object_unref(store);
    g_object_set(treeview, "headers-visible", FALSE, NULL);
    renderer = gtk_cell_renderer_text_new();

    gtk_tree_view_insert_column_with_attributes(
            GTK_TREE_VIEW(treeview), -1, "Title", renderer, "text", TAB_NAME, NULL);

    gtk_container_add(GTK_CONTAINER(window), treeview);

    gtk_widget_show_all(window);

    return window;
}

static void tab_selector_window_walk (  GtkWidget       *window,
                                        GdkEventKey     *event,
                                        MidoriBrowser   *browser)
{
    GtkTreeIter iter;
    GtkWidget *view, *treeview;
    GtkTreePath *path, *start, *end;
    GtkTreeViewColumn *column;

    treeview = g_object_get_data (G_OBJECT (window), "tab_selector_treeview");
    if (gtk_tree_view_get_visible_range (GTK_TREE_VIEW (treeview), &start, &end)) {
        gtk_tree_view_get_cursor (GTK_TREE_VIEW (treeview), &path, &column);
        if (event->state & GDK_SHIFT_MASK) {
            if(gtk_tree_path_compare (path, start) == 0)
                path = gtk_tree_path_copy (end);
            else
                gtk_tree_path_prev (path);
        } else {
            if (gtk_tree_path_compare (path, end) == 0)
                path = gtk_tree_path_copy (start);
            else
                gtk_tree_path_next (path);
        }
        column = gtk_tree_view_get_column (GTK_TREE_VIEW (treeview), 1);
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, column, FALSE);
        if (midori_extension_get_boolean (thisExtension, "ShowTabInBackground")) {
            GtkTreeModel *model;
            model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

            gtk_tree_model_get_iter (model, &iter, path);
            gtk_tree_model_get (model, &iter, TAB_POINTER, &view, -1);
            midori_browser_set_current_tab (browser, view);
        }
        gtk_tree_path_free (path);
    }
    gtk_tree_path_free (end);
    gtk_tree_path_free (start);
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

static void tab_selector_page_added (GtkNotebook     *notebook,
                                     GtkWidget       *child,
                                     guint            page_num,
                                     MidoriBrowser   *browser)
{
    GList *list = g_object_get_data(G_OBJECT(browser), "tab_selector_list");
    list = g_list_append(list, child);
    g_object_set_data(G_OBJECT(browser), "tab_selector_list", list);
}

static void tab_selector_page_removed (GtkNotebook     *notebook,
                                       GtkWidget       *child,
                                       guint            page_num,
                                       MidoriBrowser   *browser)
{
    GList *list = g_object_get_data(G_OBJECT(browser), "tab_selector_list");
    list = g_list_remove(list, child);
    g_object_set_data(G_OBJECT(browser), "tab_selector_list", list);
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

    g_signal_connect_after (browser, "add-tab",
        G_CALLBACK (tab_selector_browser_add_tab_cb), extension);

    navigationbar = katze_object_get_object(browser, "navigationbar");
    g_signal_connect (navigationbar, "key_press_event",
            G_CALLBACK (tab_selector_handle_events), browser);
    g_signal_connect (navigationbar, "key_release_event",
            G_CALLBACK (tab_selector_handle_events), browser);
    g_object_unref(navigationbar);

    notebook = katze_object_get_object(browser, "notebook");
    g_signal_connect_after (notebook, "switch-page",
            G_CALLBACK (tab_selector_switch_page), browser);
    g_signal_connect (notebook, "page-added",
            G_CALLBACK (tab_selector_page_added), browser);
    g_signal_connect (notebook, "page-removed",
            G_CALLBACK (tab_selector_page_removed), browser);
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
    g_signal_handlers_disconnect_by_func (notebook,
            tab_selector_page_added, browser);
    g_signal_handlers_disconnect_by_func (notebook,
            tab_selector_page_removed, browser);
    g_object_unref (notebook);
}

static void
tab_selector_activate_cb (MidoriExtension   *extension,
                          MidoriApp         *app)
{
    KatzeArray *browsers;
    MidoriBrowser *browser;
    guint i;

    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++)))
        tab_selector_app_add_browser_cb (app, browser, extension);
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
    thisExtension = extension;
    switchEvent = TRUE;

    return extension;
}

