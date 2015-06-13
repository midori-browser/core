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
      Pango.EllipsizeMode.START, "/home/user" },
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
#if HAVE_WIN32
        string expected = title.expected_title ?? title.title;
#else
        string expected = title.expected_title ?? "‪" + title.title;
#endif
        if (result != expected)
            error ("%s expected for %s but got %s",
                   expected, title.title, result);
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

void tab_special () {
    Midori.Test.grab_max_timeout ();

    Midori.Test.log_set_fatal_handler_for_icons ();
    var browser = new Midori.Browser ();
    /* FIXME need proper stock extension mechanism */
    browser.activate_action ("libabout.so=true");
    var settings = new Midori.WebSettings ();
    browser.set ("settings", settings);
    var tab = new Midori.View.with_title ();
    tab.settings = new Midori.WebSettings ();
    tab.settings.set ("enable-plugins", false);
    browser.add_tab (tab);
    var loop = MainContext.default ();

    tab.set_uri ("about:blank");
    do { loop.iteration (true); } while (tab.load_status != Midori.LoadStatus.FINISHED);
    assert (tab.is_blank ());
    assert (tab.can_view_source ());
    assert (!tab.can_save ());

    tab.set_uri ("about:private");
    do { loop.iteration (true); } while (tab.load_status != Midori.LoadStatus.FINISHED);
    assert (tab.is_blank ());
    assert (tab.can_view_source ());
    assert (tab.special);
    assert (!tab.can_save ());

    tab.set_uri ("http://.invalid");
    do { loop.iteration (true); } while (tab.load_status != Midori.LoadStatus.FINISHED);
    assert (!tab.is_blank ());
    assert (tab.can_view_source ());
    assert (tab.special);
    assert (!tab.can_save ());

    Midori.Test.release_max_timeout ();
}

void tab_alias () {
    Midori.Test.log_set_fatal_handler_for_icons ();
    var browser = new Midori.Browser ();
    var settings = new Midori.WebSettings ();
    /* FIXME need proper stock extension mechanism */
    browser.activate_action ("libabout.so=true");
    browser.set ("settings", settings);
    var tab = new Midori.View.with_title ();
    tab.settings = new Midori.WebSettings ();
    tab.settings.set ("enable-plugins", false);
    browser.add_tab (tab);
    var loop = MainContext.default ();

    tab.settings.tabhome = "http://.invalid/";
    tab.set_uri ("about:new");
    do { loop.iteration (true); } while (tab.load_status != Midori.LoadStatus.FINISHED);
    // FIXME: Katze.assert_str_equal ("about:new", tab.uri, tab.settings.tabhome);
#if !HAVE_WEBKIT2
    // Check that this is the real page, not white page with a URL
    assert (!tab.web_view.search_text ("about:", true, false, false));
#endif

    tab.settings.tabhome = "about:blank";
    tab.set_uri ("about:new");
    do { loop.iteration (true); } while (tab.load_status != Midori.LoadStatus.FINISHED);
    // FIXME: Katze.assert_str_equal ("about:new", tab.uri, tab.settings.tabhome);
#if !HAVE_WEBKIT2
    // Check that this is the real page, not white page with a URL
    assert (!tab.web_view.search_text ("about:", true, false, false));
#endif

    tab.settings.tabhome = "about:search";
    tab.settings.location_entry_search = "http://.invalid/";
    tab.set_uri ("about:new");
    do { loop.iteration (true); } while (tab.load_status != Midori.LoadStatus.FINISHED);
    // FIXME: Katze.assert_str_equal ("about:new", tab.uri, tab.settings.location_entry_search);
#if !HAVE_WEBKIT2
    // Check that this is the real page, not white page with a URL
    assert (!tab.web_view.search_text ("about:", true, false, false));
#endif

    tab.settings.tabhome = "about:home";
    tab.settings.homepage = "http://.invalid/";
    tab.set_uri ("about:new");
    do { loop.iteration (true); } while (tab.load_status != Midori.LoadStatus.FINISHED);
    // FIXME: Katze.assert_str_equal ("about:new", tab.uri, tab.settings.homepage);
#if !HAVE_WEBKIT2
    // Check that this is the real page, not white page with a URL
    assert (!tab.web_view.search_text ("about:", true, false, false));
#endif
}

void tab_http () {
    Midori.Test.grab_max_timeout ();

    Midori.Test.log_set_fatal_handler_for_icons ();
    var browser = new Midori.Browser ();
    var settings = new Midori.WebSettings ();
    browser.set ("settings", settings);
    var tab = new Midori.View.with_title ();
    tab.settings = new Midori.WebSettings ();
    tab.settings.set ("enable-plugins", false);
    browser.add_tab (tab);
    var loop = MainContext.default ();

#if HAVE_LIBSOUP_2_48_0
    var test_server = new Soup.Server ("server-header", null, null);
    try {
        test_server.listen_local (0, Soup.ServerListenOptions.IPV4_ONLY);
    }
    catch (Error error) {
        GLib.error (error.message);
    }
    var port = test_server.get_uris ().data.port;
#else
    var test_address = new Soup.Address ("127.0.0.1", Soup.ADDRESS_ANY_PORT);
    test_address.resolve_sync (null);
    var test_server = new Soup.Server ("interface", test_address, null);
    test_server.run_async ();
    var port = test_server.get_port ();
#endif
    string test_url = "http://127.0.0.1:%u".printf (port);
    test_server.add_handler ("/", (server, msg, path, query, client)=>{
        msg.set_status_full (200, "OK");
        msg.response_body.append_take ("<body></body>".data);
        });

    var item = tab.get_proxy_item ();
    item.set_meta_integer ("delay", Midori.Delay.UNDELAYED);
    tab.set_uri (test_url);
    do { loop.iteration (true); } while (tab.load_status != Midori.LoadStatus.FINISHED);
    assert (tab.can_view_source ());
    assert (!tab.special);
    assert (tab.can_save ());

    tab.set_uri (test_url);
    do { loop.iteration (true); } while (tab.load_status != Midori.LoadStatus.FINISHED);
    assert (!tab.is_blank ());
    assert (tab.can_view_source ());
    assert (!tab.special);
    assert (tab.can_save ());

    Midori.Test.release_max_timeout ();
}

void tab_movement () {
    Midori.Test.log_set_fatal_handler_for_icons ();
    var browser = new Midori.Browser ();
    var settings = new Midori.WebSettings ();
    browser.set ("settings", settings);
    var tab = new Midori.View.with_title ();
    tab.settings = new Midori.WebSettings ();
    for (var i = 0 ; i < 7; i++)
        browser.add_uri ("about:blank");

    browser.activate_action ("TabMoveForward");
    browser.activate_action ("TabMoveBackward");
    browser.activate_action ("TabMoveFirst");
    browser.activate_action ("TabMoveLast");
    browser.activate_action ("TabDuplicate");
    browser.activate_action ("TabCloseOther");
}

void tab_download_dialog () {
    Midori.Test.log_set_fatal_handler_for_icons ();
    var loop = MainContext.default ();
    var browser = new Gtk.Window (Gtk.WindowType.TOPLEVEL);
    var tab = new Midori.View.with_title ();
    browser.add (tab);
    /* An embedded plugin shouldn't cause a download dialog */
    Midori.Test.set_dialog_response (Gtk.ResponseType.DELETE_EVENT);
    bool did_request_download = false;
    tab.download_requested.connect ((download) => {
        did_request_download = true;
        return true;
        });
    /* png2swf -z -j 1 -o data/midori.swf ./icons/16x16/midori.png */
    tab.set_html ("<embed src=\"res:///midori.swf\">");
    do { loop.iteration (true); } while (tab.load_status != Midori.LoadStatus.FINISHED);
    assert (!did_request_download);
}

void tab_scroll () {
    /* ensure that no scrolls occur due to error iframes */
    var markup = "<style>p{height: 90%}</style><p></p><iframe src=\"http://.invalid/\" height=\"90%\"/>";
    Midori.Test.grab_max_timeout ();

    Midori.Test.idle_timeouts ();
    Midori.Test.log_set_fatal_handler_for_icons ();
    var browser = new Midori.Browser ();
    var settings = new Midori.WebSettings ();
    browser.set ("settings", settings);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    for (var i = 0 ; i < 7; i++) {
        var tab = browser.add_uri ("data:text/html;charset=utf-8;base64," + 
                                       GLib.Base64.encode (markup.data)) as Midori.Tab;
        #if HAVE_GTK3
        var vadj = (tab.web_view as Gtk.Scrollable).get_vadjustment ();
        #else
        var vadj = (tab.web_view.get_parent () as Gtk.ScrolledWindow).get_vadjustment ();
        #endif
        vadj.value_changed.connect ((vadj) => {
            assert(vadj.get_value () == vadj.get_lower ());
        });
        do { loop.iteration (true); } while (tab.progress != 0.0);
        browser.close_tab (tab as Midori.View);
    }

    Midori.Test.release_max_timeout ();
}

void main (string[] args) {
    Test.init (ref args);
    Midori.App.setup (ref args, null);
    Midori.Paths.init (Midori.RuntimeMode.NORMAL, null);
#if !HAVE_WEBKIT2
    WebKit.get_default_session ().set_data<bool> ("midori-session-initialized", true);
#endif
    Test.add_func ("/tab/load-title", tab_load_title);
    Test.add_func ("/tab/display-title", tab_display_title);
    Test.add_func ("/tab/ellipsize", tab_display_ellipsize);
    Test.add_func ("/tab/special", tab_special);
    Test.add_func ("/tab/alias", tab_alias);
    Test.add_func ("/tab/http", tab_http);
    Test.add_func ("/tab/movement", tab_movement);
    Test.add_func ("/tab/download", tab_download_dialog);
    Test.add_func ("/tab/scroll", tab_scroll);
    Test.run ();
}

