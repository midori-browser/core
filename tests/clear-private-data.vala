
/*
 Copyright (C) 2019 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

class ClearPrivateDataTest {
    public static void test_show () {
        var app = new Midori.App ();
        var args = Environment.get_current_dir ().split (" "); unowned string[] _args = args;
        int status = 0; unowned int _status = status;
        app.local_command_line (ref _args, out _status);
        try {
            app.register ();
        } catch (Error e) {
            error (e.message);
        }
        var browser = new Midori.Browser (app);
        var dialog = new Midori.ClearPrivateData (browser);
        dialog.show ();
        dialog.close ();
    }
}

void main (string[] args) {
    Gtk.test_init (ref args);
    Test.add_func ("/clear-private-data/test_show", ClearPrivateDataTest.test_show);
    Test.run ();
}

