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
#include "midori-bookmarks.h"

static void
bookmarks_panel_create (void)
{
    MidoriApp* app;
    MidoriBookmarks* bookmarks;
    gpointer value;

    app = g_object_new (MIDORI_TYPE_APP, NULL);

    bookmarks = g_object_new (MIDORI_TYPE_BOOKMARKS, NULL);
    value = katze_object_get_object (bookmarks, "app");
    g_assert (value == NULL);
    gtk_widget_destroy (GTK_WIDGET (bookmarks));

    bookmarks = g_object_new (MIDORI_TYPE_BOOKMARKS, "app", app, NULL);
    value = katze_object_get_object (bookmarks, "app");
    g_assert (value == app);
    gtk_widget_destroy (GTK_WIDGET (bookmarks));

    bookmarks = g_object_new (MIDORI_TYPE_BOOKMARKS, NULL);
    g_object_set (bookmarks, "app", app, NULL);
    value = katze_object_get_object (bookmarks, "app");
    g_assert (value == app);
    gtk_widget_destroy (GTK_WIDGET (bookmarks));
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
bookmarks_panel_fill (void)
{
    MidoriApp* app;
    KatzeArray* array;
    MidoriBookmarks* bookmarks;
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
    g_object_set (app, "bookmarks", array, NULL);
    value = katze_object_get_object (app, "bookmarks");
    g_assert (value == array);
    bookmarks = g_object_new (MIDORI_TYPE_BOOKMARKS, "app", app, NULL);
    children = gtk_container_get_children (GTK_CONTAINER (bookmarks));
    treeview = g_list_nth_data (children, 0);
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

int
main (int    argc,
      char** argv)
{
    /* libSoup uses threads, therefore if WebKit is built with libSoup
       or Midori is using it, we need to initialize threads. */
    if (!g_thread_supported ()) g_thread_init (NULL);
    g_test_init (&argc, &argv, NULL);
    gtk_init_check (&argc, &argv);

    g_test_add_func ("/bookmarks/panel/create", bookmarks_panel_create);
    g_test_add_func ("/bookmarks/panel/fill", bookmarks_panel_fill);

    return g_test_run ();
}
