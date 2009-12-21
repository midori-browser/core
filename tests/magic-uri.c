/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "compat.h"
#include "sokoke.h"

#define SM "http://www.searchmash.com/search/"

static void
sokoke_assert_str_equal (const gchar* input,
                         const gchar* result,
                         const gchar* expected)
{
    if (g_strcmp0 (result, expected))
    {
        g_error ("Input: %s\nExpected: %s\nResult: %s",
                 input ? input : "NULL",
                 expected ? expected : "NULL",
                 result ? result : "NULL");
    }
}

static void
test_input (const gchar* input,
            const gchar* expected)
{
    static KatzeArray* search_engines = NULL;
    if (G_UNLIKELY (!search_engines))
    {
        KatzeItem* item;

        search_engines = katze_array_new (KATZE_TYPE_ITEM);
        item = g_object_new (KATZE_TYPE_ITEM,
                             "uri", SM "%s",
                             "token", "sm", NULL);
        katze_array_add_item (search_engines, item);
        g_object_unref (item);
        item = g_object_new (KATZE_TYPE_ITEM,
                             "uri", SM,
                             "token", "se", NULL);
        katze_array_add_item (search_engines, item);
        g_object_unref (item);
    }

    gchar* uri = sokoke_magic_uri (input, search_engines);
    sokoke_assert_str_equal (input, uri, expected);
    g_free (uri);
}

static void
magic_uri_uri (void)
{
    test_input ("ftp://ftp.mozilla.org", "ftp://ftp.mozilla.org");
    test_input ("ftp://ftp.mozilla.org/pub", "ftp://ftp.mozilla.org/pub");
    test_input ("http://www.example.com", "http://www.example.com");
    test_input ("http://example.com", "http://example.com");
    test_input ("example.com", "http://example.com");
    test_input ("example.com", "http://example.com");
    test_input ("www.google..com", "http://www.google..com");
    test_input ("/home/user/midori.html", "file:///home/user/midori.html");
    test_input ("localhost", "http://localhost");
    test_input ("localhost:8000", "http://localhost:8000");
    test_input ("localhost/rss", "http://localhost/rss");
    test_input ("10.0.0.1", "http://10.0.0.1");
    test_input ("192.168.1.1", "http://192.168.1.1");
    test_input ("192.168.1.1:8000", "http://192.168.1.1:8000");
    test_input ("file:///home/mark/foo/bar.html",
                "file:///home/mark/foo/bar.html");
    test_input ("foo:123@bar.baz", "http://foo:123@bar.baz");
    /* test_input ("foo:f1o2o3@bar.baz", "http://f1o2o3:foo@bar.baz"); */
    /* test_input ("foo:foo@bar.baz", "http://foo:foo@bar.baz"); */
}

static void
magic_uri_idn (void)
{
    typedef struct
    {
        const gchar* before;
        const gchar* after;
    } URIItem;

    static const URIItem items[] = {
    #if HAVE_LIBIDN || defined (HAVE_LIBSOUP_2_27_90)
     { "http://www.münchhausen.at", "http://www.xn--mnchhausen-9db.at" },
     { "http://www.خداوند.com/", "http://www.xn--mgbndb8il.com/" },
     { "айкидо.com", "xn--80aildf0a.com" },
     { "http://東京理科大学.jp", "http://xn--1lq68wkwbj6ugkpigi.jp" },
     { "https://青のネコ",  "https://xn--u9jthzcs263c" },
    #else
     { "http://www.münchhausen.at", NULL },
     { "http://www.خداوند.com/", NULL },
     { "айкидо.com", NULL },
     { "http://東京理科大学.jp", NULL },
     { "https://青のネコ.co.jp",  NULL },
    #endif
    { "http://en.wikipedia.org/wiki/Kölsch_language", NULL },
    { "file:///home/mark/frühstück", NULL },
     };
    guint i;

    for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
        gchar* result = sokoke_uri_to_ascii (items[i].before);
        const gchar* after = items[i].after ? items[i].after : items[i].before;
        sokoke_assert_str_equal (items[i].before, result, after);
        g_free (result);
    }

    #if HAVE_LIBIDN
    test_input ("айкидо.com", "http://xn--80aildf0a.com");
    #else
    test_input ("айкидо.com", "http://айкидо.com");
    #endif
    test_input ("sm Küchenzubehör", SM "Küchenzubehör");
    test_input ("sm 東京理科大学", SM "東京理科大学");
}

static void
magic_uri_search (void)
{
    test_input ("sm midori", SM "midori");
    test_input ("sm cats dogs", SM "cats dogs");
    test_input ("se cats dogs", SM "cats dogs");
    test_input ("dict midori", NULL);
    test_input ("cats", NULL);
    test_input ("cats dogs", NULL);
    test_input ("gtk 2.0", NULL);
    test_input ("gtk2.0", NULL);
    test_input ("pcre++", NULL);
    test_input ("sm pcre++", SM "pcre%2B%2B");
    test_input ("5580", NULL);
    test_input ("sm 5580", SM "5580");
    test_input ("midori0.1.0", NULL);
    test_input ("midori 0.1.0", NULL);
    test_input ("search:cats", NULL);
    test_input ("search:twotoasts.de", NULL);
    test_input ("g cache:127.0.0.1", NULL);
    test_input ("g cache:127.0.0.1/foo", NULL);
    test_input ("g cache:twotoasts.de/foo", NULL);
    test_input ("sm cache:127.0.0.1", SM "cache:127.0.0.1");
    test_input ("sm cache:127.0.0.1/foo", SM "cache:127.0.0.1/foo");
    test_input ("sm cache:twotoasts.de/foo", SM "cache:twotoasts.de/foo");
    test_input ("de.po verbose", NULL);
    test_input ("verbose de.po", NULL);
    test_input ("g de.po verbose", NULL);
    test_input ("sm de.po verbose", SM "de.po verbose");
    test_input ("sm warning: configure /dev/net: virtual",
                SM "warning: configure /dev/net: virtual");
    test_input ("g \"ISO 9001:2000 certified\"", NULL);
    test_input ("g conference \"April 2, 7:00 am\"", NULL);
    test_input ("max@mustermann.de", NULL);
    test_input ("g max@mustermann.de", NULL);
    test_input ("g inurl:http://twotoasts.de bug", NULL);
    test_input ("sm", SM);
}

static void
magic_uri_pseudo (void)
{
    test_input ("javascript:alert(1)", "javascript:alert(1)");
    test_input ("mailto:christian@twotoasts.de", "mailto:christian@twotoasts.de");
    test_input ("data:text/html;charset=utf-8,<title>Test</title>Test",
                "data:text/html;charset=utf-8,<title>Test</title>Test");
}

static void
magic_uri_performance (void)
{
    gsize i;

    g_test_timer_start ();

    for (i = 0; i < 1000; i++)
    {
        magic_uri_uri ();
        magic_uri_idn ();
        magic_uri_search ();
        magic_uri_pseudo ();
    }

    g_print ("\nTime needed for URI tests: %f ", g_test_timer_elapsed ());
}

static void
magic_uri_format (void)
{
    typedef struct
    {
        const gchar* before;
        const gchar* after;
    } URIItem;

    static const URIItem items[] = {
     { "http://www.csszengarden.com", NULL },
     { "http://live.gnome.org/GTK+/3.0/Tasks", NULL },
     { "http://www.johannkönig.com/index.php?ausw=home", NULL },
     { "http://digilife.bz/wiki/index.php?Python%E3%81%AE%E9%96%8B%E7%99%BA%E6%89%8B%E9%A0%86",
       "http://digilife.bz/wiki/index.php?Pythonの開発手順" },
     { "http://die-welt.net/~evgeni/LenovoBatteryLinux/", NULL },
     { "http://wiki.c3sl.ufpr.br/multiseat/index.php/Xephyr_Solution", NULL },
     { "http://şøñđëřżēıċħęŋđőmæîņĭśŧşũþėŗ.de/char.jpg", NULL },
     { "http://www.ⓖⓝⓞⓜⓔ.org/", "http://www.gnome.org/" },
     };
    guint i;

    for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
        gchar* result = sokoke_format_uri_for_display (items[i].before);
        const gchar* after = items[i].after ? items[i].after : items[i].before;
        sokoke_assert_str_equal (items[i].before, result, after);
        g_free (result);
    }
}

int
main (int    argc,
      char** argv)
{
    g_test_init (&argc, &argv, NULL);
    gtk_init_check (&argc, &argv);

    g_test_add_func ("/magic-uri/uri", magic_uri_uri);
    g_test_add_func ("/magic-uri/idn", magic_uri_idn);
    g_test_add_func ("/magic-uri/search", magic_uri_search);
    g_test_add_func ("/magic-uri/pseudo", magic_uri_pseudo);
    g_test_add_func ("/magic-uri/performance", magic_uri_performance);
    g_test_add_func ("/magic-uri/format", magic_uri_format);

    return g_test_run ();
}
