/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

/*
struct FormSpec {
    public string html;
    public Pango.EllipsizeMode ellipsize;
    public string? uri;
    public string? title;
}

const FormSpec[] forms = {
    { "<form></form>", Pango.EllipsizeMode.END, null, null },
    { "<form><input></form>", Pango.EllipsizeMode.END, null, null }
};
*/

void searchaction_form () {
    /*
    foreach (var form in forms) {
        var view = new Midori.View.with_title ();
        view.get_web_view ().load_html_string (form.html, "");
        Katze.Item? result = Midori.SearchAction.get_engine_for_form (
            view.get_web_view (), form.ellipsize);
        Katze.assert_str_equal (form.html,
            result != null ? result.uri : null, form.uri);
        Katze.assert_str_equal (form.html,
            result != null ? result.name : null, form.title);
    }
    */
}

struct TokenSpec {
    public string uri;
    public string expected_token;
}

const TokenSpec[] tokens = {
    { "https://bugs.launchpad.net/midori/+bugs", "lnch" },
    { "https://duckduckgo.com/search", "dckd" },
    { "http://en.wikipedia.org/w/index.php?title=Special:Search&profile=default&search=%s&fulltext=Search&", "wkpd" },
    { "about:blank", "" },
    { "", "" },
    { "file:///", "" },
    { "file:///form.html", "" }
};

void searchaction_token () {
    foreach(var token in tokens)
    {
        Katze.assert_str_equal (token.uri,
            Midori.SearchAction.token_for_uri(token.uri), token.expected_token);
    }
}

void main (string[] args) {
    Test.init (ref args);
    Midori.App.setup (ref args, null);
    Test.add_func ("/searchaction/form", searchaction_form);
    Test.add_func ("/searchaction/token", searchaction_token);
    Test.run ();
}

