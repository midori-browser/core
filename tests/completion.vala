/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

class TestCompletion : Midori.Completion {
    public bool test_can_complete { get; set; }
    public uint test_suggestions { get; set; }

    public TestCompletion () {
    }

    public override void prepare (GLib.Object app) {
    }

    public override bool can_complete (string text) {
        return test_can_complete;
    }

    public override bool can_action (string action) {
        return false;
    }

    public override async List<Midori.Suggestion>? complete (string text, string? action, Cancellable cancellable) {
        var suggestions = new List<Midori.Suggestion> ();
        if (test_suggestions == 0)
            return null;
        if (test_suggestions >= 1)
            suggestions.append (new Midori.Suggestion ("about:first", "First"));
        if (test_suggestions >= 2)
            suggestions.append (new Midori.Suggestion ("about:second", "Second"));
        if (test_suggestions >= 3)
            suggestions.append (new Midori.Suggestion ("about:third", "Third"));
        if (cancellable.is_cancelled ())
            return null;
        return suggestions;
    }
}

class CompletionAutocompleter : Midori.Test.Job {
    public static void test () { new CompletionAutocompleter ().run_sync (); }
    public override async void run (Cancellable cancellable) throws GLib.Error {
        var app = new Midori.App ();
        var autocompleter = new Midori.Autocompleter (app);
        assert (!autocompleter.can_complete (""));
        var completion = new TestCompletion ();
        autocompleter.add (completion);
        completion.test_can_complete = false;
        assert (!autocompleter.can_complete (""));
        completion.test_can_complete = true;
        assert (autocompleter.can_complete (""));

        completion.test_suggestions = 0;
        yield autocompleter.complete ("");
        assert (autocompleter.model.iter_n_children (null) == 0);

        completion.test_suggestions = 1;
        yield autocompleter.complete ("");
        assert (autocompleter.model.iter_n_children (null) == 1);

        /* Order */
        completion.test_suggestions = 2;
        yield autocompleter.complete ("");
        assert (autocompleter.model.iter_n_children (null) == 2);
        Gtk.TreeIter iter_first;
        autocompleter.model.get_iter_first (out iter_first);
        string title;
        autocompleter.model.get (iter_first, Midori.Autocompleter.Columns.MARKUP, out title);
        if (title != "First")
            error ("Expected %s but got %s", "First", title);

        /* Cancellation */
        yield autocompleter.complete ("");
        completion.test_suggestions = 3;
        yield autocompleter.complete ("");
        int n = autocompleter.model.iter_n_children (null);
        if (n != 3)
            error ("Expected %d but got %d", 3, n);
    }
}

class CompletionHistory : Midori.Test.Job {
    public static void test () { new CompletionHistory ().run_sync (); }
    public override async void run (Cancellable cancellable) throws GLib.Error {
        var bookmarks_database = new Midori.BookmarksDatabase ();
        assert (bookmarks_database.db != null);

        Midori.HistoryDatabase history = new Midori.HistoryDatabase (null);
        assert (history.db != null);
        history.clear (0);

        history.insert ("http://example.com", "Ejemplo", 0, 0);
        var results = yield history.list_by_count_with_bookmarks ("example", 1, cancellable);
        assert (results.length () == 1);
        var first = results.nth_data (0);
        assert (first.title == "Ejemplo");
        results = yield history.list_by_count_with_bookmarks ("ejemplo", 1, cancellable);
        assert (results.length () == 1);
        first = results.nth_data (0);
        assert (first.title == "Ejemplo");
    }
}

struct TestCaseRender {
    public string keys;
    public string uri;
    public string title;
    public string expected_uri;
    public string expected_title;
}

const TestCaseRender[] renders = {
    { "debian", "planet.debian.org", "Planet Debian", "planet.<b>debian</b>.org", "Planet <b>Debian</b>" },
    { "p debian o", "planet.debian.org", "Planet Debian", "<b>p</b>lanet.<b>debian</b>.<b>o</b>rg", "Planet Debian" },
    { "pla deb o", "planet.debian.org", "Planet Debian", "<b>pla</b>net.<b>deb</b>ian.<b>o</b>rg", "Planet Debian" },
    { "ebi", "planet.debian.org", "Planet Debian", "planet.d<b>ebi</b>an.org", "Planet D<b>ebi</b>an" },
    { "an ebi", "planet.debian.org", "Planet Debian", "pl<b>an</b>et.d<b>ebi</b>an.org", "Pl<b>an</b>et D<b>ebi</b>an" }
};

void completion_location_action () {
    foreach (var spec in renders) {
        string uri = Midori.LocationAction.render_uri (spec.keys.split (" ", 0), spec.uri);
        string title = Midori.LocationAction.render_title (spec.keys.split (" ", 0), spec.title);
        if (uri != spec.expected_uri || title != spec.expected_title)
            error ("\nExpected: %s/ %s\nInput   : %s/ %s/ %s\nResult  : %s/ %s",
                   spec.expected_uri, spec.expected_title,
                   spec.keys, spec.uri, spec.title, uri, title);
    }
}

class HistoryMarkup : Midori.Test.Job {
    public static void test () { new HistoryMarkup ().run_sync (); }
    public override async void run (Cancellable cancellable) throws GLib.Error {
        var app = new Midori.App ();
        var autocompleter = new Midori.Autocompleter (app);
        assert (!autocompleter.can_complete (""));

        var histcomp = new Midori.HistoryCompletion ();
        assert (!histcomp.can_complete (""));

        //this calls histcomp.prepare (app):
        autocompleter.add (histcomp);

        //any time the history completion has a db, its can_complete method returns true
        //assert (!histcomp.can_complete (""));

        //remove entries from previous tests
        histcomp.database.clear (0);

        histcomp.database.insert ("https://duckduckgo.com/?q=%3E&ia=about", "> (Clojure) - DuckDuckGo", 0, 0);
        yield autocompleter.complete ("");
        assert (autocompleter.model.iter_n_children (null) == 2);

        histcomp.database.insert ("https://duckduckgo.com/", "DuckDuckGo", 0, 0);
        yield autocompleter.complete ("");
        assert (autocompleter.model.iter_n_children (null) == 3);

        histcomp.database.insert ("http://stackoverflow.com/questions/5068951/what-do-lt-and-gt-stand-for",
            "html - What do &lt; and &gt; stand for? - Stack Overflow", 0, 0);
        yield autocompleter.complete ("");
        assert (autocompleter.model.iter_n_children (null) == 4);

        Gtk.TreeIter iter;
        string title, expected;

        expected = "DuckDuckGo";
        autocompleter.model.iter_nth_child (out iter, null, 2);
        autocompleter.model.get (iter, Midori.Autocompleter.Columns.MARKUP, out title);
        if (title != expected)
            error ("Expected %s but got %s", expected, title);

        expected = "> (Clojure) - DuckDuckGo";
        autocompleter.model.iter_nth_child (out iter, null, 3);
        autocompleter.model.get (iter, Midori.Autocompleter.Columns.MARKUP, out title);
        if (title != expected)
            error ("Expected %s but got %s", expected, title);

        expected = "html - What do &lt; and &gt; stand for? - Stack Overflow";
        autocompleter.model.iter_nth_child (out iter, null, 1);
        autocompleter.model.get (iter, Midori.Autocompleter.Columns.MARKUP, out title);
        if (title != expected)
            error ("Expected %s but got %s", expected, title);
    }
}

void main (string[] args) {
    Midori.Test.init (ref args);
    Midori.App.setup (ref args, null);
    Midori.Paths.init (Midori.RuntimeMode.NORMAL, null);
    Test.add_func ("/completion/autocompleter", CompletionAutocompleter.test);
    Test.add_func ("/completion/history", CompletionHistory.test);
    Test.add_func ("/completion/location-action", completion_location_action);
    Test.add_func ("/completion/historymarkup", HistoryMarkup.test);
    Test.run ();
}

