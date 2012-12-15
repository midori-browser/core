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
    public string mime_type;
    public string? expected_filename;
    public string? expected_extension;
}

const TestCase[] filenames = {
    { "file:///tmp/midori-user/tumblr123.jpg", "image/jpg", "tumblr123.jpg", ".jpg" },
    { "https://green.cat/8019B6/a.b/500.jpg", "image/jpg", "500.jpg", ".jpg" },
    { "http://example.com/file.png", "image/png", "file.png", ".png" },
    { "http://svn.sf.net/doc/doxy_to_dev.xsl.m4?rev=253", "application/xslt+xml", "doxy_to_dev.xsl.m4", ".m4" }
};

static void download_suggestion () {
    foreach (var filename in filenames) {
        string? result = Midori.Download.get_filename_suggestion_for_uri (
            filename.mime_type, filename.data);
        Katze.assert_str_equal (filename.data, result, filename.expected_filename);
    }
}

static void download_extension () {
    foreach (var filename in filenames) {
        string? result = Midori.Download.get_extension_for_uri (filename.data);
        Katze.assert_str_equal (filename.data, result, filename.expected_extension);
    }
}

static void download_unique () {
    string folder = Midori.Paths.make_tmp_dir ("cacheXXXXXX");
    string filename = Path.build_path (Path.DIR_SEPARATOR_S, folder, "foo.png");
    string org_filename = Path.build_path (Path.DIR_SEPARATOR_S, folder, "foo.png");
    string unique = Midori.Download.get_unique_filename (org_filename);
    Katze.assert_str_equal (folder, unique, filename);
    try {
        FileUtils.set_contents (filename, "12345");
        unique = Midori.Download.get_unique_filename (org_filename);
        filename = Path.build_path (Path.DIR_SEPARATOR_S, folder, "foo-0.png");
        Katze.assert_str_equal (folder, unique, filename);
        FileUtils.set_contents (filename, "12345");
        unique = Midori.Download.get_unique_filename (org_filename);
        filename = Path.build_path (Path.DIR_SEPARATOR_S, folder, "foo-1.png");
        Katze.assert_str_equal (folder, unique, filename);

        for (var i = 0; i < 10; i++) {
            filename = Path.build_path (Path.DIR_SEPARATOR_S, folder, "foo-%d.png".printf (i));
            FileUtils.set_contents (filename, "12345");
        }
    }
    catch (Error error) {
        GLib.error (error.message);
    }
    unique = Midori.Download.get_unique_filename (org_filename);
    filename = Path.build_path (Path.DIR_SEPARATOR_S, folder, "foo-10.png");
    Katze.assert_str_equal (folder, unique, filename);
    DirUtils.remove (folder);
}

void download_properties () {
    var download = new WebKit.Download (new WebKit.NetworkRequest ("file:///klaatu/barada/nikto.ogg"));
    assert (Midori.Download.get_type (download) == 0);
    Midori.Download.set_type (download, Midori.DownloadType.OPEN);
    assert (Midori.Download.get_type (download) == Midori.DownloadType.OPEN);
    assert (Midori.Download.get_progress (download) == 0.0);

    if (Environment.get_variable ("MIDORI_TEST_UNDEFINED") != "1") return;

    try {
        string filename;
        FileUtils.close (FileUtils.open_tmp ("XXXXXX", out filename));
        download.destination_uri = Filename.to_uri (filename, null);
        string tee = Midori.Download.get_tooltip (download);
        download.start ();
        assert (Midori.Download.get_progress (download) == 0.0);
        var loop = MainContext.default ();
        do { loop.iteration (true); } while (loop.pending ());
        string tee2 = Midori.Download.get_tooltip (download);
        assert (tee2.contains (tee));
        do { loop.iteration (true); } while (!Midori.Download.is_finished (download));
        assert (download.status == WebKit.DownloadStatus.ERROR);
    }
    catch (Error error) {
        GLib.error (error.message);
    }
}

void main (string[] args) {
    Test.init (ref args);
    Midori.App.setup (ref args, null);
    Midori.Paths.init (Midori.RuntimeMode.NORMAL, null);
    Test.add_func ("/download/suggestion", download_suggestion);
    Test.add_func ("/download/extension", download_extension);
    Test.add_func ("/download/unique", download_unique);
    Test.add_func ("/download/properties", download_properties);
    Test.run ();
}

