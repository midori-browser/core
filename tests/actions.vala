/*
 Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

void actions_view_page () {
    var browser = new Midori.Browser ();
    var view = new Midori.View.with_title (null, new Midori.WebSettings ());
    browser.add_tab (view);
    browser.show ();
    view.set_html ("<body>The earth is <em>flat</em> for a fact.</body>");
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (view.load_status != Midori.LoadStatus.FINISHED);

    var hit_test_result = Object.new (typeof (WebKit.HitTestResult), "context", WebKit.HitTestResultContext.DOCUMENT) as WebKit.HitTestResult;
    var menu = view.get_page_context_action (hit_test_result);
    assert (menu != null);
    assert (menu.name == "PageContextMenu");
    assert (menu.get_by_name ("Back") != null);

#if !HAVE_WEBKIT2
    var hit_test_result2 = Object.new (typeof (WebKit.HitTestResult), "context", WebKit.HitTestResultContext.EDITABLE) as WebKit.HitTestResult;
    var menu2 = view.get_page_context_action (hit_test_result2);
    assert (menu2 != null);
    assert (menu2.name == "PageContextMenu");
    var copy = menu2.get_by_name ("Copy");
    assert (copy != null);
    assert (!copy.sensitive);
    assert (view.web_view.search_text ("flat", true, false, false));
    menu = view.get_page_context_action (hit_test_result);
    copy = menu.get_by_name ("Copy");
    assert (copy.sensitive);
#endif

    /* Reload contents to clear selection */
    view.set_html ("<body>The earth is <em>flat</em> for a fact.</body>");
    do { loop.iteration (true); } while (view.load_status != Midori.LoadStatus.FINISHED);

#if !HAVE_WEBKIT2
    hit_test_result = Object.new (typeof (WebKit.HitTestResult), "context", WebKit.HitTestResultContext.SELECTION) as WebKit.HitTestResult;
    menu = view.get_page_context_action (hit_test_result);
    copy = menu.get_by_name ("Copy");
    assert (!copy.sensitive);
    assert (view.web_view.search_text ("flat", true, false, false));
    menu = view.get_page_context_action (hit_test_result);
    copy = menu.get_by_name ("Copy");
    assert (copy.sensitive);
#endif
}

void main (string[] args) {
    Test.init (ref args);
    Midori.App.setup (ref args, null);
    Midori.Paths.init (Midori.RuntimeMode.NORMAL, null);
#if !HAVE_WEBKIT2
    WebKit.get_default_session ().set_data<bool> ("midori-session-initialized", true);
#endif
    Test.add_func ("/actions/view/page", actions_view_page);
    Test.run ();
}

