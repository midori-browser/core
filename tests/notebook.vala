/*
 Copyright (C) 2014 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

void notebook_menu () {
    var notebook = new Midori.Notebook ();
    /* FIXME: we should just use a Tab but currently it has no title property */
    var tab = new Midori.View.with_title ("a_fairy_tale", new Midori.WebSettings ());
    /*var tab = new Midori.View.with_title (null, new Midori.WebSettings ());
    view.set_html ("<title>a_fairy_tale</title><body>The earth is <em>flat</em> for a fact.</body>");
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (view.load_status != Midori.LoadStatus.FINISHED);*/
    notebook.insert (tab, 0);
    var menu = notebook.get_context_action ();
    var tab_menu = menu.get_by_name ("Tab0");
    assert (tab_menu != null);
    /* The underscores should be escaped */
    Katze.assert_str_equal ("Tab0", tab_menu.label, "‪a__fairy__tale");
}

void main (string[] args) {
    Test.init (ref args);
    Midori.App.setup (ref args, null);
    Test.add_func ("/notebook/menu", notebook_menu);
    Test.run ();
}
