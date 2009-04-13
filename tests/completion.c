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
 { "http://one.com/one/", "One off" },
 { "http://four.org", "One" },
 { "https://four.org", "Four" },
 { "ftp://four.org/", "ごオルゴ" },
 { "http://muenchen.de/weißwürste/", "Münchner Weißwürste" }, /* Umlauts */
 };
static const guint items_n = 9;

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
 "", "www.", "ww2.", "ftp."
 };
static const guint subs_n = 4;
static const gchar* slds[] = {
 "one", "two", "four", "six", "seven"/*, "Seven", "SEVEN"*/
 };
static const guint slds_n = 5;
static const gchar* tlds[] = {
 "com", "org", "net", "de", "co.uk", "com.au"/*, "Com.Au", "COM.AU"*/
 };
static const guint tlds_n = 6;
static const gchar* files[] = {
 "/", "/index.html", "/img.png", /*"/weißwürste",*/ "/images/"
 /*, "/Images", "/IMAGES/"*/
 };
static const guint files_n = 4;

static const gchar* inputs[] = {
 "http://www.one.com/index", "http://two.de/images", "http://six.com.au/img"/*,
 "http://muenchen.de/weißwürste/"*/
 };
static const gchar* additions[] = {
 "http://www.one.com/invention", "http://two.de/island", "http://six.com.au/ish"
 };

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

    items_added = G_N_ELEMENTS (protocols) * G_N_ELEMENTS (subs)
        * G_N_ELEMENTS (slds) * G_N_ELEMENTS (tlds) * G_N_ELEMENTS (files);
    items_added_effective = protocols_n * subs_n * slds_n * tlds_n * files_n;
    g_print ("Adding %d items, effectively %d items:\n",
             items_added, items_added_effective);

    /* Since adding items when the action is frozen is very fast,
       we run it 5 times and take the average time. */
    elapsed = 0.0;
    for (i = 0; i < 5; i++)
    {
        midori_location_action_clear (action);
        midori_location_action_freeze (action);
        g_test_timer_start ();
        location_action_fill (action);
        elapsed += g_test_timer_elapsed ();
        midori_location_action_thaw (action);
    }
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, items_added_effective);
    g_print ("%f seconds, the action is frozen\n", elapsed / 5.0);

    midori_location_action_clear (action);
    g_test_timer_start ();
    location_action_fill (action);
    elapsed = g_test_timer_elapsed ();
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, items_added_effective);
    g_print ("%f seconds, the action updates normally\n", elapsed);

    /* We don't clear the action intentionally in order to see
       how long adding only duplicates takes. */
    g_test_timer_start ();
    location_action_fill (action);
    elapsed = g_test_timer_elapsed ();
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));
    n = gtk_tree_model_iter_n_children (model, NULL);
    g_assert_cmpint (n, ==, items_added_effective);
    g_print ("%f seconds, adding exact duplicates\n", elapsed);

    g_print ("...");
}

static guint matches = 0;
static guint matches_expected = 0;

static gboolean
entry_completion_insert_prefix_cb (GtkEntryCompletion*   completion,
                                   const gchar*          prefix,
                                   MidoriLocationAction* action)
{
    GtkWidget* entry = gtk_entry_completion_get_entry (completion);
    const gchar* text = gtk_entry_get_text (GTK_ENTRY (entry));

    if (!g_strrstr (prefix, text))
        g_print ("Match failed, input: %s, result: %s\n",
                 text, prefix);
    g_assert (g_strrstr (prefix, text));
    midori_location_action_delete_item_from_uri (action, text);
    midori_location_action_add_uri (action, text);

    matches++;

    return FALSE;
}

static void
completion_match (void)
{
    MidoriLocationAction* action;
    GtkWidget* toolitem;
    GtkWidget* alignment;
    GtkWidget* location_entry;
    GtkWidget* entry;
    GtkEntryCompletion* completion;
    guint i;

    action = g_object_new (MIDORI_TYPE_LOCATION_ACTION, NULL);

    midori_location_action_freeze (action);
    location_action_fill (action);
    midori_location_action_thaw (action);

    toolitem = gtk_action_create_tool_item (GTK_ACTION (action));
    alignment = gtk_bin_get_child (GTK_BIN (toolitem));
    location_entry = gtk_bin_get_child (GTK_BIN (alignment));
    entry = gtk_bin_get_child (GTK_BIN (location_entry));
    completion = gtk_entry_get_completion (GTK_ENTRY (entry));
    g_signal_connect (completion, "insert-prefix",
                      G_CALLBACK (entry_completion_insert_prefix_cb), action);
    gtk_entry_completion_set_inline_completion (completion, TRUE);
    gtk_entry_completion_set_popup_single_match (completion, FALSE);

    for (i = 0; i < G_N_ELEMENTS (inputs); i++)
    {
        matches_expected++;
        gtk_entry_set_text (GTK_ENTRY (entry), inputs[i]);
        gtk_entry_completion_complete (completion);
        gtk_entry_completion_insert_prefix (completion);
        if (matches != matches_expected)
            g_print ("Match failed, input: %s, result: %s\n",
                inputs[i], gtk_entry_get_text (GTK_ENTRY (entry)));
        g_assert_cmpint (matches, ==, matches_expected);
    }
    for (i = 0; i < G_N_ELEMENTS (additions); i++)
    {
        midori_location_action_add_uri (action, additions[i]);
        midori_location_action_delete_item_from_uri (action, additions[i]);
        midori_location_action_add_uri (action, additions[i]);
    }
    for (i = 0; i < G_N_ELEMENTS (inputs); i++)
    {
        matches_expected++;
        gtk_entry_set_text (GTK_ENTRY (entry), inputs[i]);
        gtk_entry_completion_complete (completion);
        gtk_entry_completion_insert_prefix (completion);
        if (matches != matches_expected)
            g_print ("Match failed, input: %s, result: %s\n",
                inputs[i], gtk_entry_get_text (GTK_ENTRY (entry)));
        g_assert_cmpint (matches, ==, matches_expected);
    }
    for (i = 0; i < G_N_ELEMENTS (additions); i++)
    {
        matches_expected++;
        gtk_entry_set_text (GTK_ENTRY (entry), additions[i]);
        gtk_entry_completion_complete (completion);
        gtk_entry_completion_insert_prefix (completion);
        if (matches != matches_expected)
            g_print ("Match failed, input: %s, result: %s\n",
                additions[i], gtk_entry_get_text (GTK_ENTRY (entry)));
        g_assert_cmpint (matches, ==, matches_expected);
    }
}

int
main (int    argc,
      char** argv)
{
    g_test_init (&argc, &argv, NULL);
    gtk_init_check (&argc, &argv);

    g_test_add_func ("/completion/count", completion_count);
    g_test_add_func ("/completion/fill", completion_fill);
    g_test_add_func ("/completion/match", completion_match);

    return g_test_run ();
}
