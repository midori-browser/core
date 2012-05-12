/*
 Copyright (C) 2012 Vincent Cappe <vcappe@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
 */

#include "midori.h"
#include "panels/midori-bookmarks.h"

typedef struct
{
    KatzeArray* db_bookmarks;
    KatzeArray* test_bookmarks;
} BookmarksFixture;

typedef struct
{
    char *dbfile;           /* usually ":memory:" */
    gboolean verbose;       /* print debug stuff if TRUE */
    char* infile;           /* (e.g. to test import), usually NULL */
    char* outfile;          /* (e.g. to test export), if it can be avoided it's
                                better to not write anything to disk, though */
} TestParameters;

typedef void (*FixtureFunc)(BookmarksFixture*, const void*);


static void
fixture_setup (BookmarksFixture* fixture,
               const TestParameters* params)
{
    KatzeItem* item;
    KatzeArray* folder;
    sqlite3* db;
    gchar *errmsg = NULL;

    fixture->db_bookmarks = katze_array_new (KATZE_TYPE_ARRAY);
    db = midori_bookmarks_initialize (fixture->db_bookmarks, params->dbfile, &errmsg);
    if (db == NULL)
        g_error ("Bookmarks couldn't be loaded: %s\n", errmsg);
    g_assert (errmsg == NULL);
    g_object_set_data ( G_OBJECT (fixture->db_bookmarks), "db", db);


    fixture->test_bookmarks = katze_array_new (KATZE_TYPE_ARRAY);

    item = (KatzeItem*)katze_array_new (KATZE_TYPE_ARRAY);
    item->name = "i am a folder";
    katze_array_add_item (fixture->test_bookmarks, item);
    folder = (KatzeArray *) item;

    item = (KatzeItem*)katze_array_new (KATZE_TYPE_ARRAY);
    item->name = "i am a folder inside a folder";
    katze_array_add_item (folder, item);
    folder = (KatzeArray *) item;

    item = g_object_new (KATZE_TYPE_ITEM,
        "uri", "http://xyzzy.invalid",
        "name", "xyzzy", NULL);
    katze_item_set_meta_integer (item, "app", TRUE);
    katze_array_add_item (folder, item);

    /* level up */
    folder = katze_item_get_parent ((KatzeItem*)folder);

    item = g_object_new (KATZE_TYPE_ITEM,
        "uri", "http://zyxxy.invalid",
        "name", "zyxxy",
        "text", "i have a description and am in a folder", NULL);
    katze_item_set_meta_integer(item, "toolbar", TRUE);
    katze_array_add_item (folder, item);

    folder = katze_item_get_parent ((KatzeItem*)folder);
    /* we should be at toplevel, now */
    g_assert (folder == fixture->test_bookmarks);

    item = g_object_new (KATZE_TYPE_ITEM,
        "uri", "http://foobarbaz.invalid",
        "name", "i am in the toplevel folder", NULL);
    katze_array_add_item (folder, item);
}

static void
fixture_teardown (BookmarksFixture* fixture,
                  const TestParameters *params)
{
    sqlite3* db = g_object_get_data (G_OBJECT (fixture->db_bookmarks), "db");
    sqlite3_close (db);
    g_object_unref (fixture->db_bookmarks);
    g_object_unref (fixture->test_bookmarks);
}

static void
print_bookmark (KatzeItem *bookmark)
{
    g_print ("title  : '%s'\n", katze_item_get_name (bookmark));
    g_print ("uri    : '%s'\n", katze_item_get_uri (bookmark));
    g_print ("desc   : '%s'\n", katze_item_get_text (bookmark));
    g_print ("app    : %d\n", katze_item_get_meta_boolean (bookmark, "app"));
    g_print ("toolbar: %d\n", katze_item_get_meta_boolean (bookmark, "toolbar"));
}

static void
compare_items (KatzeItem *a, KatzeItem *b)
{
    g_assert_cmpstr ( katze_item_get_uri (a), ==, katze_item_get_uri (b));
    g_assert_cmpstr ( katze_item_get_name (a), ==, katze_item_get_name (b));
    g_assert_cmpstr ( katze_str_non_null (katze_item_get_text (a)), ==, katze_str_non_null (katze_item_get_text (b)));
    g_assert_cmpint ( katze_item_get_meta_boolean (a, "app"), ==, katze_item_get_meta_boolean (b, "app"));
    g_assert_cmpint ( katze_item_get_meta_boolean (a, "toolbar"), ==, katze_item_get_meta_boolean (b, "toolbar"));
}

/* NB: assumes "title" is unique in a set */
static void
compare_test_and_db (KatzeArray* test_bookmarks,
                     KatzeArray* db_bookmarks,
                     gboolean verbose)
{
    KatzeArray* db_items;
    KatzeItem *test_item, *db_item;
    GList* list;

    KATZE_ARRAY_FOREACH_ITEM_L (test_item, test_bookmarks, list)
    {
        if (verbose)
        {
            print_bookmark (test_item);
            g_print ("----------\n");
        }

        db_items = midori_array_query_recursive (db_bookmarks,
                           "*", "title='%q'", katze_item_get_name (test_item), FALSE);

        g_assert_cmpint (katze_array_get_length (db_items), ==, 1);
        db_item = katze_array_get_nth_item (db_items, 0);

        compare_items (db_item, test_item);

        if (KATZE_ITEM_IS_FOLDER(test_item))
            compare_test_and_db ( KATZE_ARRAY (test_item), db_bookmarks, verbose);
    }
    g_list_free (list);
}

static void
insert_bookmarks (KatzeArray* test_bookmarks,
                  KatzeArray* db_bookmarks,
                  gboolean verbose)
{
    KatzeItem* item;
    GList* list;
    sqlite3 *db = g_object_get_data (G_OBJECT (db_bookmarks), "db");

    KATZE_ARRAY_FOREACH_ITEM_L (item, test_bookmarks, list)
    {
        if (verbose)
        {
            print_bookmark (item);
            g_print ("----------\n");
        }

        midori_bookmarks_insert_item_db (db, item, 0);

        if (KATZE_ITEM_IS_FOLDER(item))
            insert_bookmarks (KATZE_ARRAY (item), db_bookmarks, verbose);
    }
    g_list_free (list);
}

static void
simple_test (BookmarksFixture* fixture,
             const TestParameters* params)
{
    if (params->verbose)
        g_print ("\n===== inserting items in the database =====\n");
    insert_bookmarks (fixture->test_bookmarks, fixture->db_bookmarks, params->verbose);

    if (params->verbose)
        g_print ("===== comparing database with the original =====\n");
    compare_test_and_db (fixture->test_bookmarks, fixture->db_bookmarks, params->verbose);
}


int
main (int    argc,
      char** argv)
{
    //TestParameters default_params = {"/a/path/unlikely/to/exists/bookmarks.db", TRUE, NULL, NULL};
    //TestParameters default_params = {"/tmp/bookmarks.db", TRUE, NULL, NULL};
    //TestParameters default_params = {":memory:", TRUE, NULL, NULL};
    TestParameters default_params = {":memory:", FALSE, NULL, NULL};

    midori_app_setup (argv);
    g_test_init (&argc, &argv, NULL);
    gtk_init_check (&argc, &argv);

    g_test_add ("/bookmarks/simple test",
                    BookmarksFixture, &default_params,
                    (FixtureFunc) fixture_setup,
                    (FixtureFunc) simple_test,
                    (FixtureFunc) fixture_teardown);

    return g_test_run ();
}
