/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

struct TestCase {
    public string data;
    public string? expected;
}

const TestCase[] filenames = {
    { "/tmp/midori-user/tumblr123.jpg", ".jpg" },
    { "https://green.cat/8019B6/a.b/500.jpg", ".jpg" },
    { "http://example.com/file.png", ".png" }
};

static void download_extension () {
    foreach (var filename in filenames) {
        string? result = Midori.Download.get_extension_for_uri (filename.data);
        Katze.assert_str_equal (filename.data, result, filename.expected);
    }
}

static void download_unique () {
    string folder = DirUtils.make_tmp ("cacheXXXXXX");
    string filename = Path.build_path (Path.DIR_SEPARATOR_S, folder, "foo.png");
    string unique = Midori.Download.get_unique_filename (filename);
    Katze.assert_str_equal (folder, unique, filename);
    FileUtils.set_contents (filename, "12345");
    unique = Midori.Download.get_unique_filename (filename);
    filename = Path.build_path (Path.DIR_SEPARATOR_S, folder, "foo-0.png");
    Katze.assert_str_equal (folder, unique, filename);
    FileUtils.set_contents (filename, "12345");
    unique = Midori.Download.get_unique_filename (filename);
    filename = Path.build_path (Path.DIR_SEPARATOR_S, folder, "foo-1.png");
    Katze.assert_str_equal (folder, unique, filename);

    filename = Path.build_path (Path.DIR_SEPARATOR_S, folder, "foo-9.png");
    FileUtils.set_contents (filename, "12345");
    unique = Midori.Download.get_unique_filename (filename);
    filename = Path.build_path (Path.DIR_SEPARATOR_S, folder, "foo-10.png");
    Katze.assert_str_equal (folder, unique, filename);
    DirUtils.remove (folder);
}

void main (string[] args) {
    Test.init (ref args);
    Test.add_func ("/download/extension", download_extension);
    Test.add_func ("/download/unique", download_unique);
    Test.run ();
}

