/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

void tab_load_title () {
    /*
    var view = new Midori.View.with_title ();
    view.set_uri ("about:blank");
    do {
    }
    while (view.load_status != Midori.LoadStatus.FINISHED);
    Katze.assert_str_equal ("about:blank", "about:blank", view.uri);
    Katze.assert_str_equal ("about:blank", "", view.get_display_uri ()); */
}

struct TestCaseEllipsize {
    public string uri;
    public string? title;
    public Pango.EllipsizeMode expected_ellipsize;
    public string expected_title;
}

const TestCaseEllipsize[] titles = {
    { "http://secure.wikimedia.org/wikipedia/en/wiki/Cat",
      "Cat - Wikipedia, the free encyclopedia",
      Pango.EllipsizeMode.END, null },
    { "https://ar.wikipedia.org/wiki/%D9%82%D8%B7",
      "قط - ويكيبيديا، الموسوعة الحرة",
      Pango.EllipsizeMode.END, null },
    { "https://ar.wikipedia.org/wiki/قط",
      "قط - ويكيبيديا، الموسوعة الحرة",
      Pango.EllipsizeMode.END, null },
    { "http://help.duckduckgo.com/customer/portal/articles/352255-wordpress",
      "DuckDuckGo | WordPress",
      Pango.EllipsizeMode.START, null },
    { "file:///home/user",
      "OMG!",
      Pango.EllipsizeMode.START, "file:///home/user" },
    { "http://paste.foo/0007-Bump-version-to-0.4.7.patch",
      null,
      Pango.EllipsizeMode.START, "0007-Bump-version-to-0.4.7.patch" },
    { "http://translate.google.com/#en/de/cat%0Adog%0Ahorse",
      "Google Translator",
      Pango.EllipsizeMode.END, null }
};

static void tab_display_title () {
    foreach (var title in titles) {
        string result = Midori.Tab.get_display_title (title.title, title.uri);
        unowned string? expected = title.expected_title ?? "‪" + title.title;
        Katze.assert_str_equal (title.title, expected, result);
    }
}

static void tab_display_ellipsize () {
    foreach (var title in titles) {
        Pango.EllipsizeMode result = Midori.Tab.get_display_ellipsize (
            Midori.Tab.get_display_title (title.title, title.uri), title.uri);
        if (result != title.expected_ellipsize)
            error ("%s expected for %s/ %s but got %s",
                   title.expected_ellipsize.to_string (), title.title, title.uri, result.to_string ());
    }
}

void main (string[] args) {
    Test.init (ref args);
    Test.add_func ("/tab/load-title", tab_load_title);
    Test.add_func ("/tab/display-title", tab_display_title);
    Test.add_func ("/tab/ellipsize", tab_display_ellipsize);
    Test.run ();
}

