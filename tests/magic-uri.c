/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

#define SM "http://www.searchmash.com/search/"

static void
test_input (const gchar* input,
            const gchar* expected)
{
    static KatzeArray* search_engines = NULL;
    gchar* uri;

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

    uri = sokoke_magic_uri (input);
    if (!uri)
    {
        gchar** parts;
        gchar* keywords = NULL;
        const gchar* search_uri = NULL;

        /* Do we have a keyword and a string? */
        parts = g_strsplit (input, " ", 2);
        if (parts[0])
        {
            KatzeItem* item;
            if ((item = katze_array_find_token (search_engines, parts[0])))
            {
                keywords = g_strdup (parts[1] ? parts[1] : "");
                search_uri = katze_item_get_uri (item);
            }
        }
        g_strfreev (parts);

        uri = keywords ? midori_uri_for_search (search_uri, keywords) : NULL;

        g_free (keywords);
    }
    katze_assert_str_equal (input, uri, expected);
    g_free (uri);
}

static void
magic_uri_uri (void)
{
    const gchar* uri;
    gchar* path;

    test_input ("ftp://ftp.mozilla.org", "ftp://ftp.mozilla.org");
    test_input ("ftp://ftp.mozilla.org/pub", "ftp://ftp.mozilla.org/pub");
    test_input ("http://www.example.com", "http://www.example.com");
    test_input ("http://example.com", "http://example.com");
    test_input ("example.com", "http://example.com");
    test_input ("example.com", "http://example.com");
    test_input ("www.google..com", "http://www.google..com");
    test_input ("/home/user/midori.html", "file:///home/user/midori.html");
    test_input ("http://www.google.com/search?q=query test",
                "http://www.google.com/search?q=query test");
    if (sokoke_resolve_hostname ("localhost"))
    {
        test_input ("localhost", "http://localhost");
        test_input ("localhost:8000", "http://localhost:8000");
        test_input ("localhost/rss", "http://localhost/rss");
    }
    test_input ("10.0.0.1", "http://10.0.0.1");
    test_input ("192.168.1.1", "http://192.168.1.1");
    test_input ("192.168.1.1:8000", "http://192.168.1.1:8000");
    test_input ("file:///home/mark/foo/bar.html",
                "file:///home/mark/foo/bar.html");
    test_input ("foo:123@bar.baz", "http://foo:123@bar.baz");
    /* test_input ("foo:f1o2o3@bar.baz", "http://f1o2o3:foo@bar.baz"); */
    /* test_input ("foo:foo@bar.baz", "http://foo:foo@bar.baz"); */

    uri = "http://bugs.launchpad.net/midori";
    g_assert_cmpstr ("bugs.launchpad.net", ==, midori_uri_parse_hostname (uri, NULL));
    uri = "https://bugs.launchpad.net/midori";
    g_assert_cmpstr ("bugs.launchpad.net", ==, midori_uri_parse_hostname (uri, NULL));
    g_assert_cmpstr ("bugs.launchpad.net", ==, midori_uri_parse_hostname (uri, &path));
    g_assert_cmpstr ("/midori", ==, path);
    uri = "http://айкидо.ru/users/kotyata";
    g_assert_cmpstr ("айкидо.ru", ==, midori_uri_parse_hostname (uri, &path));
    g_assert_cmpstr ("/users/kotyata", ==, path);
    uri = "invalid:/uri.like/thing";
    g_assert_cmpstr (NULL, ==, midori_uri_parse_hostname (uri, NULL));
    uri = "invalid-uri.like:thing";
    g_assert_cmpstr (NULL, ==, midori_uri_parse_hostname (uri, NULL));
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
     { "http://www.münchhausen.at", "http://www.xn--mnchhausen-9db.at" },
     { "http://www.خداوند.com/", "http://www.xn--mgbndb8il.com/" },
     { "айкидо.com", "xn--80aildf0a.com" },
     { "http://東京理科大学.jp", "http://xn--1lq68wkwbj6ugkpigi.jp" },
     { "https://青のネコ",  "https://xn--u9jthzcs263c" },
    { "http://en.wikipedia.org/wiki/Kölsch_language", NULL },
    { "file:///home/mark/frühstück", NULL },
    { "about:version", NULL },
     };
    guint i;

    for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
        gchar* result = midori_uri_to_ascii (items[i].before);
        const gchar* after = items[i].after ? items[i].after : items[i].before;
        katze_assert_str_equal (items[i].before, result, after);
        g_free (result);
    }

    test_input ("айкидо.com", "http://айкидо.com");
    test_input ("sm Küchenzubehör", SM "Küchenzubehör");
    test_input ("sm 東京理科大学", SM "東京理科大学");
}

static void
magic_uri_search (void)
{
    test_input ("sm midori", SM "midori");
    test_input ("sm cats dogs", SM "cats%20dogs");
    test_input ("se cats dogs", SM "cats%20dogs");
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
    test_input ("sm de.po verbose", SM "de.po%20verbose");
    test_input ("sm warning: configure /dev/net: virtual",
                SM "warning:%20configure%20/dev/net:%20virtual");
    test_input ("g \"ISO 9001:2000 certified\"", NULL);
    test_input ("g conference \"April 2, 7:00 am\"", NULL);
    test_input ("max@mustermann.de", NULL);
    test_input ("g max@mustermann.de", NULL);
    test_input ("g inurl:http://twotoasts.de bug", NULL);
    test_input ("sm", SM);
    /* test_input ("LT_PREREQ(2.2)", NULL); */
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
magic_uri_fingerprint (void)
{
    const gchar* uri;
    uri = "http://midori-0.4.1.tar.bz2#!md5!33dde203cd71ae2b1d2adcc7f5739f65";
    g_assert_cmpint (midori_uri_get_fingerprint (uri, NULL, NULL), ==, G_CHECKSUM_MD5);
    uri = "http://midori-0.4.1.tar.bz2#!md5!33DDE203CD71AE2B1D2ADCC7F5739F65";
    g_assert_cmpint (midori_uri_get_fingerprint (uri, NULL, NULL), ==, G_CHECKSUM_MD5);
    uri = "http://midori-0.4.1.tar.bz2#!sha1!0c499459b1049feabf86dce89f49020139a9efd9";
    g_assert_cmpint (midori_uri_get_fingerprint (uri, NULL, NULL), ==, G_CHECKSUM_SHA1);
    uri = "http://midori-0.4.1.tar.bz2#!sha256!123456";
    g_assert_cmpint (midori_uri_get_fingerprint (uri, NULL, NULL), ==, G_MAXINT);
    uri = "http://midori-0.4.1.tar.bz2#abcdefg";
    g_assert_cmpint (midori_uri_get_fingerprint (uri, NULL, NULL), ==, G_MAXINT);
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
        gchar* result = midori_uri_format_for_display (items[i].before);
        const gchar* after = items[i].after ? items[i].after : items[i].before;
        katze_assert_str_equal (items[i].before, result, after);
        g_free (result);
    }
}

static void
magic_uri_prefetch (void)
{
    g_assert (!sokoke_prefetch_uri (NULL, NULL, NULL, NULL));
    g_assert (sokoke_prefetch_uri (NULL, "http://google.com", NULL, NULL));
    g_assert (sokoke_prefetch_uri (NULL, "http://google.com", NULL, NULL));
    g_assert (sokoke_prefetch_uri (NULL, "http://googlecom", NULL, NULL));
    g_assert (sokoke_prefetch_uri (NULL, "http://1kino.com", NULL, NULL));
    g_assert (sokoke_prefetch_uri (NULL, "http://", NULL, NULL));
    g_assert (!sokoke_prefetch_uri (NULL, "http:/", NULL, NULL));
    g_assert (!sokoke_prefetch_uri (NULL, "http", NULL, NULL));
    g_assert (!sokoke_prefetch_uri (NULL, "ftp://ftphost.org", NULL, NULL));
    g_assert (!sokoke_prefetch_uri (NULL, "http://10.0.0.1", NULL, NULL));
    g_assert (!sokoke_prefetch_uri (NULL, "about:blank", NULL, NULL));
    g_assert (!sokoke_prefetch_uri (NULL, "javascript: alert()", NULL, NULL));
}

int
main (int    argc,
      char** argv)
{
    midori_app_setup (argv);
    g_test_init (&argc, &argv, NULL);
    gtk_init_check (&argc, &argv);

    g_test_add_func ("/magic-uri/uri", magic_uri_uri);
    g_test_add_func ("/magic-uri/idn", magic_uri_idn);
    g_test_add_func ("/magic-uri/search", magic_uri_search);
    g_test_add_func ("/magic-uri/pseudo", magic_uri_pseudo);
    g_test_add_func ("/magic-uri/performance", magic_uri_performance);
    g_test_add_func ("/magic-uri/fingerprint", magic_uri_fingerprint);
    g_test_add_func ("/magic-uri/format", magic_uri_format);
    g_test_add_func ("/magic-uri/prefetch", magic_uri_prefetch);

    return g_test_run ();
}
