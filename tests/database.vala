/*
 Copyright (C) 2015-2018 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

class DatabaseTest {
    public static void test_bind () {
        try {
            var database = new Midori.Database ();
            database.exec ("CREATE TABLE cats (cat text, favorite text)");
            database.exec ("INSERT INTO cats (cat, favorite) VALUES ('Henry', 'pillow')");
            var statement = database.prepare ("SELECT cat FROM cats WHERE favorite = :toy");
            statement.bind ("toy", typeof (string), "pillow");
            // Missing : should throw an error
            assert_not_reached ();
        } catch (Error error) {
            var expected = new Midori.DatabaseError.TYPE ("");
            assert_true (error.domain == expected.domain);
            assert_true (error.code == expected.code);
        }
    }

    public static void test_insert_delete () {
        var loop = new MainLoop ();
        test_insert_delete_async.begin ((obj, res) => {
            test_insert_delete_async.end (res);
            loop.quit ();
        });
        loop.run ();
    }

    public static async void test_insert_delete_async () {
        try {
            var database = new Midori.Database ();
            var item = new Midori.DatabaseItem ("http://example.com", "Example", 0);
            yield database.insert (item);
            assert_true (item.database == database);
            assert_true (item in database);
            var items = yield database.query ();
            assert_true (items.data.uri == item.uri);
            yield item.delete ();
            assert_true (!(item in database));
        } catch (Error error) {
            critical (error.message);
        }
    }
}

void main (string[] args) {
    Test.init (ref args);
    Test.add_func ("/database/bind", DatabaseTest.test_bind);
    Test.add_func ("/database/insert_delete", DatabaseTest.test_insert_delete);
    Test.run ();
}

