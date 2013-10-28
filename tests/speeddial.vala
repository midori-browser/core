/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/
static string? tmp_folder = null;
string get_test_file (string contents) {
    if (tmp_folder == null)
        tmp_folder = Midori.Paths.make_tmp_dir ("speeddialXXXXXX");
    string checksum = Checksum.compute_for_string (ChecksumType.MD5, contents);
    string file = Path.build_path (Path.DIR_SEPARATOR_S, tmp_folder, checksum);
    try {
        FileUtils.set_contents (file, contents, -1);
    } catch (Error file_error) {
        GLib.error (file_error.message);
    }
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

    try {
        dial_data.save_message ("speed_dial-save-rename 1 Lorem");
        Katze.assert_str_equal (data, dial_data.keyfile.get_string ("Dial 1", "title"), "Lorem");
        dial_data.save_message ("speed_dial-save-rename 1 Lorem Ipsum Dolomit");
        Katze.assert_str_equal (data, dial_data.keyfile.get_string ("Dial 1", "title"), "Lorem Ipsum Dolomit");
        dial_data.save_message ("speed_dial-save-delete 1");
        Katze.assert_str_equal (data, dial_data.get_next_free_slot (), "Dial 1");
    } catch (Error message_error) {
        GLib.error (message_error.message);
    }

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
    try {
        Katze.assert_str_equal (data, dial_data.get_next_free_slot (), "Dial 1");
        dial_data.save_message ("speed_dial-save-swap 2 4");
        Katze.assert_str_equal (data, dial_data.keyfile.get_string ("Dial 2", "title"), "IT-News");
    } catch (Error message_error) {
        GLib.error (message_error.message);
    }
}

void main (string[] args) {
    Test.init (ref args);
    Midori.App.setup (ref args, null);
    Midori.Paths.init (Midori.RuntimeMode.NORMAL, null);
    Test.add_func ("/speeddial/load", speeddial_load);
    Test.run ();
}

