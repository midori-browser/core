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

namespace Katze {
    extern static string assert_str_equal (string input, string result, string expected);
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
}

void main (string[] args) {
    string temporary_cache = DirUtils.make_tmp ("cacheXXXXXX");
    Environment.set_variable ("XDG_CACHE_HOME", temporary_cache, true);
    Test.init (ref args);
    Test.add_func ("/speeddial/load", speeddial_load);
    Test.run ();
    DirUtils.remove (temporary_cache);
}

