/*
 Copyright (C) 2015 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

class DatabaseTest : Midori.Test.Job {
    public static void test () { new DatabaseTest ().run_sync (); }
    public override async void run (Cancellable cancellable) throws GLib.Error {
        var database = new Midori.Database ();
        database.exec ("CREATE TABLE cats (cat text, favorite text)");
        database.exec ("INSERT INTO cats (cat, favorite) VALUES ('Henry', 'pillow')");
        var statement = database.prepare ("SELECT cat FROM cats WHERE favorite = :toy");
        /* Missing : should throw an error */
        try {
            statement.bind ("toy", typeof (string), "pillow");
        } catch (Midori.DatabaseError error) {
            Katze.assert_str_equal (statement.query, error.message,
                "No such parameter 'toy' in statement: " + statement.query);
        }
        statement.bind (":toy", typeof (string), "pillow");
    }
}

void main (string[] args) {
    Midori.Test.init (ref args);
    Midori.App.setup (ref args, null);
    Midori.Paths.init (Midori.RuntimeMode.NORMAL, null);
    Test.add_func ("/database/bind", DatabaseTest.test);
    Test.run ();
}

