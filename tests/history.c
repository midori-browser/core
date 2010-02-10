/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "midori.h"
#include "midori-history.h"
#include "sokoke.h"

static void
history_panel_create (void)
{
    MidoriApp* app;
    MidoriHistory* history;
    gpointer value;

    app = g_object_new (MIDORI_TYPE_APP, NULL);

    history = g_object_new (MIDORI_TYPE_HISTORY, NULL);
    value = katze_object_get_object (history, "app");
    g_assert (value == NULL);
    gtk_widget_destroy (GTK_WIDGET (history));

    history = g_object_new (MIDORI_TYPE_HISTORY, "app", app, NULL);
    value = katze_object_get_object (history, "app");
    g_assert (value == app);
    gtk_widget_destroy (GTK_WIDGET (history));

    history = g_object_new (MIDORI_TYPE_HISTORY, NULL);
    g_object_set (history, "app", app, NULL);
    value = katze_object_get_object (history, "app");
    g_assert (value == app);
    gtk_widget_destroy (GTK_WIDGET (history));
}

static KatzeItem*
bookmark_new (const gchar* uri,
              const gchar* title)
{
    return g_object_new (KATZE_TYPE_ITEM, "uri", uri, "name", title, NULL);
}

static KatzeArray*
folder_new (const gchar* title)
{
    KatzeArray* folder;

    folder = katze_array_new (KATZE_TYPE_ARRAY);
    g_object_set (folder, "name", title, NULL);
    return folder;
}

static void
history_panel_fill (void)
{
    MidoriApp* app;
    KatzeArray* array;
    MidoriHistory* history;
    GList* children;
    GtkWidget* treeview;
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* bookmark;
    KatzeArray* folder;
    guint n;
    gpointer value;

    app = g_object_new (MIDORI_TYPE_APP, NULL);
    array = katze_array_new (KATZE_TYPE_ARRAY);
    g_object_set (app, "history", array, NULL);
    value = katze_object_get_object (app, "history");
    g_assert (value == array);
    history = g_object_new (MIDORI_TYPE_HISTORY, "app", app, NULL);
    children = gtk_container_get_children (GTK_CONTAINER (history));
    treeview = g_list_nth_data (children, 1);
    g_list_free (children);
    g_assert (GTK_IS_TREE_VIEW (treeview));
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
    g_assert (GTK_IS_TREE_MODEL (model));
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, 0);
    bookmark = bookmark_new ("http://www.example.com", "Example");
    katze_array_add_item (array, bookmark);
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, 1);
    katze_array_remove_item (array, bookmark);
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, 0);
    bookmark = bookmark_new ("http://www.example.com", "Example");
    katze_array_add_item (array, bookmark);
    folder = folder_new ("Empty");
    katze_array_add_item (array, folder);
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, 2);
    katze_array_remove_item (array, folder);
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, 1);
    folder = folder_new ("Empty");
    katze_array_add_item (array, folder);
    folder = folder_new ("Kurioses");
    katze_array_add_item (array, folder);
    bookmark = bookmark_new ("http://www.ende.de", "Das Ende");
    katze_array_add_item (folder, bookmark);
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, 3);
    folder = folder_new ("Miscellaneous");
    katze_array_add_item (array, folder);
    gtk_tree_model_iter_nth_child (model, &iter, NULL, 3);
    n = gtk_tree_model_iter_n_children (model, &iter);
    g_assert_cmpint (n, ==, 0);
    bookmark = bookmark_new ("http://thesaurus.reference.com/", "Thesaurus");
    katze_array_add_item (folder, bookmark);
    n = gtk_tree_model_iter_n_children (model, &iter);
    g_assert_cmpint (n, ==, 1);
    bookmark = bookmark_new ("http://en.wikipedia.org/", "Wikipedia");
    katze_array_add_item (folder, bookmark);
    n = gtk_tree_model_iter_n_children (model, &iter);
    g_assert_cmpint (n, ==, 2);
    katze_array_remove_item (folder, bookmark);
    n = gtk_tree_model_iter_n_children (model, &iter);
    g_assert_cmpint (n, ==, 1);
    katze_array_remove_item (array, folder);
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, 3);
    katze_array_add_item (array, folder);
    /* katze_array_clear (folder);
    n = gtk_tree_model_iter_n_children (model, &iter);
    g_assert_cmpint (n, ==, 0); */
    katze_array_clear (array);
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, 0);
}

static void
notify_load_status_cb (GtkWidget*  view,
                       GParamSpec* pspec,
                       guint*      done)
{
    MidoriLoadStatus status;

    status = midori_view_get_load_status (MIDORI_VIEW (view));
    if (*done == 2 && status == MIDORI_LOAD_COMMITTED)
        *done = 1;
    else if (*done == 1 && status == MIDORI_LOAD_FINISHED)
        *done = 0;
}

static void
history_browser_add (void)
{
    MidoriBrowser* browser;
    MidoriWebSettings* settings;
    KatzeArray* array;
    GtkWidget* view;
    guint done;
    KatzeItem* date;
    gsize i;
    GtkActionGroup* action_group;
    GtkAction* action;

    browser = midori_browser_new ();
    settings = midori_web_settings_new ();
    array = katze_array_new (KATZE_TYPE_ARRAY);
    g_object_set (browser, "settings", settings, "history", array, NULL);
    view = midori_view_new (NULL);
    midori_browser_add_tab (browser, view);
    midori_view_set_uri (MIDORI_VIEW (view),
        "data:text/html;charset=utf-8,<title>Test</title>Test");
    g_signal_connect (view, "notify::load-status",
        G_CALLBACK (notify_load_status_cb), &done);
    done = 2;
    while (done)
        gtk_main_iteration ();
    g_assert_cmpint (katze_array_get_length (array), ==, 1);
    date = katze_array_get_nth_item (array, 0);
    g_assert_cmpint (katze_array_get_length (KATZE_ARRAY (date)), ==, 1);
    i = 0;
    gtk_widget_show (view);
    midori_browser_set_current_tab (browser, view);
    g_assert (midori_browser_get_current_tab (browser) == view);
    done = 2;
    action_group = midori_browser_get_action_group (browser);
    action = gtk_action_group_get_action (action_group, "Location");
    midori_location_action_set_uri (MIDORI_LOCATION_ACTION (action),
        "data:text/html;charset=utf-8,<title>Test</title>Test");
    g_signal_emit_by_name (action, "submit-uri",
        "data:text/html;charset=utf-8,<title>Test</title>Test", FALSE);
    while (done)
        gtk_main_iteration ();
    g_assert_cmpint (katze_array_get_length (array), ==, 1);
    date = katze_array_get_nth_item (array, 0);
    g_assert_cmpint (katze_array_get_length (KATZE_ARRAY (date)), ==, 1);
    i = 0;
}

int
main (int    argc,
      char** argv)
{
    /* libSoup uses threads, so we need to initialize threads. */
    if (!g_thread_supported ()) g_thread_init (NULL);
    g_test_init (&argc, &argv, NULL);
    gtk_init_check (&argc, &argv);
    sokoke_register_stock_items ();

    g_test_add_func ("/history/panel/create", history_panel_create);
    g_test_add_func ("/history/panel/fill", history_panel_fill);
    g_test_add_func ("/history/browser/add", history_browser_add);

    return g_test_run ();
}
