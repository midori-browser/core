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
        return null;
    }
}

void completion_autocompleter () {
    var app = new Midori.App ();
    var autocompleter = new Midori.Autocompleter (app);
    assert (!autocompleter.can_complete (""));
    var completion = new TestCompletion ();
    autocompleter.add (completion);
    completion.test_can_complete = false;
    assert (!autocompleter.can_complete (""));
    completion.test_can_complete = true;
    assert (autocompleter.can_complete (""));
}

struct TestCaseCompletion {
    public string prefix;
    public string text;
    public int expected_count;
}

const TestCaseCompletion[] completions = {
    { "history", "example", 1 }
};

async void complete_spec (Midori.Completion completion, TestCaseCompletion spec) {
    assert (completion.can_complete (spec.text));
    var cancellable = new Cancellable ();
    var suggestions = yield completion.complete (spec.text, null, cancellable);
    if (spec.expected_count != suggestions.length ())
        error ("%u expected for %s/ %s but got %u",
            spec.expected_count, spec.prefix, spec.text, suggestions.length ());
}

void completion_history () {
    /* TODO: mock history database
    var completion = new Midori.HistoryCompletion ();
    var app = new Midori.App ();
    var history = new Katze.Array (typeof (Katze.Item));
    app.set ("history", history);
    Sqlite.Database db;
    Sqlite.Database.open_v2 (":memory:", out db);
    history.set_data<unowned Sqlite.Database?> ("db", db);
    completion.prepare (app);
    foreach (var spec in completions)
        complete_spec (completion, spec); */
}

void main (string[] args) {
    Test.init (ref args);
    Test.add_func ("/completion/autocompleter", completion_autocompleter);
    Test.add_func ("/completion/history", completion_history);
    Test.run ();
}

