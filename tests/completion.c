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

#if GLIB_CHECK_VERSION(2, 16, 0)

typedef struct
{
    const gchar* uri;
    const gchar* name;
} CompletionItem;

static const CompletionItem items[] = {
 { "http://one.com", "One" },
 { "http://one.com/", "One" }, /* Duplicate */
 { "http://one.com/", "One One" }, /* Duplicate */
 { "http://one.com", "One Two" }, /* Duplicate */
 { "http://two.com", "Two" },
 { "http://three.com", "Three" },
 { "http://one.com/one", "One one" },
 { "http://one.com/one/", "One off" }, /* Duplicate */
 { "http://four.org", "One" },
 { "https://four.org", "Four" },
 { "ftp://four.org/", "Five" },
 };
static const guint items_n = 7;

static void
completion_count (void)
{
    MidoriLocationAction* action;
    GtkWidget* toolitem;
    GtkWidget* alignment;
    GtkWidget* location_entry;
    GtkWidget* entry;
    GtkTreeModel* model;
    guint n, i;

    action = g_object_new (MIDORI_TYPE_LOCATION_ACTION, NULL);
    toolitem = gtk_action_create_tool_item (GTK_ACTION (action));
    alignment = gtk_bin_get_child (GTK_BIN (toolitem));
    location_entry = gtk_bin_get_child (GTK_BIN (alignment));
    entry = gtk_bin_get_child (GTK_BIN (location_entry));
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));

    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, 0);
    midori_location_action_add_item (action, "http://one.com", NULL, "One");
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, 1);
    midori_location_action_clear (action);
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, 0);
    for (i = 0; i < G_N_ELEMENTS (items); i++)
        midori_location_action_add_item (action, items[i].uri,
                                         NULL, items[i].name);
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, items_n);
    /* Adding the exact same items again shouldn't increase the total amount */
    for (i = 0; i < G_N_ELEMENTS (items); i++)
        midori_location_action_add_item (action, items[i].uri,
                                         NULL, items[i].name);
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, items_n);
}

/* Components to construct test addresses, the numbers take into account
   how many items minus duplicates are unique. */
static const gchar* protocols[] = {
 "http", "https", "ftp"
 };
static const guint protocols_n = 3;
static const gchar* subs[] = {
 "", "www.", "ww2.", "ftp.", "sub."
 };
static const guint subs_n = 5;
static const gchar* slds[] = {
 "one", "two", "four", "six", "seven"/*, "Seven", "SEVEN"*/
 };
static const guint slds_n = 5;
static const gchar* tlds[] = {
 "com", "org", "net", "de", "co.uk", "com.au"/*, "Com.Au", "COM.AU"*/
 };
static const guint tlds_n = 6;
static const gchar* files[] = {
 "", "/", "/index.html", "/images", "/images/", "/Images", "/IMAGES"
 };
static const guint files_n = 5;

static void location_action_fill (MidoriLocationAction* action)
{
    guint i, j, k, l, m;

    for (i = 0; i < G_N_ELEMENTS (protocols); i++)
        for (j = 0; j < G_N_ELEMENTS (subs); j++)
            for (k = 0; k < G_N_ELEMENTS (slds); k++)
                for (l = 0; l < G_N_ELEMENTS (tlds); l++)
                    for (m = 0; m < G_N_ELEMENTS (files); m++)
                    {
                        gchar* uri = g_strdup_printf ("%s://%s%s.%s%s",
                            protocols[i], subs[j], slds[k], tlds[l], files[m]);
                        midori_location_action_add_item (action, uri, NULL, uri);
                        g_free (uri);
                    }
}

static void
completion_fill (void)
{
    MidoriLocationAction* action;
    GtkWidget* toolitem;
    GtkWidget* alignment;
    GtkWidget* location_entry;
    GtkWidget* entry;
    GtkTreeModel* model;
    guint i, items_added, items_added_effective, n;
    gdouble elapsed;

    action = g_object_new (MIDORI_TYPE_LOCATION_ACTION, NULL);
    toolitem = gtk_action_create_tool_item (GTK_ACTION (action));
    alignment = gtk_bin_get_child (GTK_BIN (toolitem));
    location_entry = gtk_bin_get_child (GTK_BIN (alignment));
    entry = gtk_bin_get_child (GTK_BIN (location_entry));

    g_print ("...\n");

    /* Since adding items when the action is frozen is very fast,
       we run it 10 times and take the average time. */
    elapsed = 0.0;
    for (i = 0; i < 10; i++)
    {
        midori_location_action_clear (action);
        midori_location_action_freeze (action);
        g_test_timer_start ();
        location_action_fill (action);
        elapsed += g_test_timer_elapsed ();
        midori_location_action_thaw (action);
    }
    items_added = G_N_ELEMENTS (protocols) * G_N_ELEMENTS (subs)
        * G_N_ELEMENTS (slds) * G_N_ELEMENTS (tlds) * G_N_ELEMENTS (files);
    items_added_effective = protocols_n * subs_n * slds_n * tlds_n * files_n;
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, items_added_effective);
    g_print ("Added %d items, effectively %d items, in %f seconds (frozen)\n",
             items_added, items_added_effective, elapsed / 10.0);

    midori_location_action_clear (action);
    g_test_timer_start ();
    location_action_fill (action);
    elapsed = g_test_timer_elapsed ();
    items_added = G_N_ELEMENTS (protocols) * G_N_ELEMENTS (subs)
        * G_N_ELEMENTS (slds) * G_N_ELEMENTS (tlds) * G_N_ELEMENTS (files);
    items_added_effective = protocols_n * subs_n * slds_n * tlds_n * files_n;
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, items_added_effective);
    g_print ("Added %d items, effectively %d items, in %f seconds\n",
             items_added, items_added_effective, elapsed);

    g_print ("...");
}

int
main (int    argc,
      char** argv)
{
    g_test_init (&argc, &argv, NULL);
    gtk_init (&argc, &argv);

    g_test_add_func ("/completion/count", completion_count);
    g_test_add_func ("/completion/fill", completion_fill);

    return g_test_run ();
}

#endif
