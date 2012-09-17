/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/
string get_test_file (string contents) {
    string file;
    int fd = FileUtils.open_tmp ("speeddialXXXXXX", out file);
    FileUtils.set_contents (file, contents, -1);
    FileUtils.close (fd);
    return file;
}

static void speeddial_load () {
    string data = get_test_file ("""
            [Dial 1]
            uri=http://example.com
            title=Example
            [settings]
            columns=3
            rows=3
        """);

    string json = get_test_file ("""
            '{"shortcuts":[{"id":"s1","href":"http://example.com","title":"Example","img":"a2F0emU="}]}'
        """);

    var dial_data = new Midori.SpeedDial (data, "");
    var dial_json = new Midori.SpeedDial ("", json);
    FileUtils.remove (data);
    FileUtils.remove (json);

    Katze.assert_str_equal (json, dial_data.keyfile.to_data (), dial_json.keyfile.to_data ());
    Katze.assert_str_equal (json, dial_data.get_next_free_slot (), "Dial 2");
    Katze.assert_str_equal (json, dial_json.get_next_free_slot (), "Dial 2");

    try {
        dial_data.save_message ("SpeedDial");
        assert_not_reached ();
    }
    catch (Error error) { /* Error expected: pass */ }
    try {
        dial_data.save_message ("speed_dial-save-rename ");
        assert_not_reached ();
    }
    catch (Error error) { /* Error expected: pass */ }
    try {
        dial_data.save_message ("speed_dial-save-foo 1");
        assert_not_reached ();
    }
    catch (Error error) { /* Error expected: pass */ }

    dial_data.save_message ("speed_dial-save-rename 1 Lorem");
    Katze.assert_str_equal (data, dial_data.keyfile.get_string ("Dial 1", "title"), "Lorem");
    dial_data.save_message ("speed_dial-save-delete 1");
    Katze.assert_str_equal (data, dial_data.get_next_free_slot (), "Dial 1");

    data = get_test_file ("""
            [settings]
            columns=3
            rows=3

            [Dial 2]
            uri=http://green.cat
            title=Green cat is green

            [Dial 4]
            uri=http://heise.de
            title=IT-News
        """);
    dial_data = new Midori.SpeedDial (data, "");
    FileUtils.remove (data);
    Katze.assert_str_equal (data, dial_data.get_next_free_slot (), "Dial 1");
    dial_data.save_message ("speed_dial-save-swap 2 4");
    Katze.assert_str_equal (data, dial_data.keyfile.get_string ("Dial 2", "title"), "IT-News");
}

void main (string[] args) {
    string temporary_cache = DirUtils.make_tmp ("cacheXXXXXX");
    Environment.set_variable ("XDG_CACHE_HOME", temporary_cache, true);
    Test.init (ref args);
    Midori.Paths.init (Midori.RuntimeMode.PRIVATE, null);
    Test.add_func ("/speeddial/load", speeddial_load);
    Test.run ();
    DirUtils.remove (temporary_cache);
}

