/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

bool check_sensible_window_size (Gtk.Window window, Midori.WebSettings settings) {
    Gdk.Rectangle monitor;
    window.screen.get_monitor_geometry (0, out monitor);
    return settings.last_window_width + 1 >= monitor.width / 1.7
        && settings.last_window_height + 1 >= monitor.height / 1.7;
}

void app_normal () {
    Midori.Test.grab_max_timeout ();

    Midori.Test.idle_timeouts ();
    Midori.Test.log_set_fatal_handler_for_icons ();
    Midori.Paths.Test.reset_runtime_mode ();
    Midori.App.set_instance_is_running (false);
    var app = Midori.normal_app_new (null, "test-normal", false, null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    for (var i = 0 ; i < 7; i++) {
        var tab = app.browser.add_uri ("about:blank") as Midori.View;
        app.browser.close_tab (tab);
        do { loop.iteration (true); } while (loop.pending ());
    }
    Midori.normal_app_on_quit (app);
    /* FIXME
    for (var i = 0 ; i < 7; i++) {
        app.settings.maximum_cache_size++;
        do { loop.iteration (true); } while (loop.pending ());
    }
    */

    Midori.Test.release_max_timeout ();
}

void app_normal_custom_config () {
    Midori.Test.log_set_fatal_handler_for_icons ();
    Midori.Paths.Test.reset_runtime_mode ();
    Midori.App.set_instance_is_running (false);
    var app = Midori.normal_app_new (Midori.Paths.make_tmp_dir ("custom-configXXXXXX"),
        "test-custom-config-normal", false, null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    assert (check_sensible_window_size (app.browser, app.settings));
    Midori.normal_app_on_quit (app);
}

void app_private () {
    Midori.Test.log_set_fatal_handler_for_icons ();
    Midori.Paths.Test.reset_runtime_mode ();
    Midori.App.set_instance_is_running (false);
    var browser = Midori.private_app_new (null, null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    assert (check_sensible_window_size (browser, browser.settings));
}

void app_web () {
    Midori.Paths.Test.reset_runtime_mode ();
    Midori.App.set_instance_is_running (false);
    var browser = Midori.web_app_new (null, null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    assert (check_sensible_window_size (browser, browser.settings));
}

void app_web_custom_config () {
    Midori.Paths.Test.reset_runtime_mode ();
    Midori.App.set_instance_is_running (false);
    var browser = Midori.web_app_new (Midori.Paths.make_tmp_dir ("custom-configXXXXXX"), null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    assert (check_sensible_window_size (browser, browser.settings));
}


void app_extensions_load () {
    Midori.Test.grab_max_timeout ();

    Midori.Test.idle_timeouts ();
    Midori.Test.log_set_fatal_handler_for_icons ();
    Midori.Paths.Test.reset_runtime_mode ();
    Midori.App.set_instance_is_running (false);
    var app = Midori.normal_app_new (null, "test-extensions-normal", false, null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    /* No extensions loaded */
    assert (app.extensions.get_length () == 0);
    Midori.Extension.load_from_folder (app, null, false);
    /* All extensions loaded, inactive */
    assert (app.extensions.get_length () > 0);

    /* Number of expected extensions matches */
    /* FIXME Counting .so/dll doesn't see multiple extensions in one binary
    Dir dir;
    try {
        dir = Dir.open (Midori.Paths.get_lib_path (PACKAGE_NAME), 0);
    }
    catch (Error error) {
        GLib.error (error.message);
    }
    uint count = 0;
    string? name;
    while ((name = dir.read_name ()) != null) {
        if (name.has_suffix (GLib.Module.SUFFIX))
            count++;
    }
    assert (app.extensions.get_length () == count); */

    foreach (var item in app.extensions.get_items ())
        assert (!(item as Midori.Extension).is_active ());

    for (var i = 0 ; i < 7; i++) {
        var tab = app.browser.add_uri ("about:blank") as Midori.View;
        app.browser.close_tab (tab);
    }
    do { loop.iteration (true); } while (loop.pending ());

    /*
    Midori.Test.release_max_timeout ();
}

void app_extensions_activate () {
    Midori.Test.grab_max_timeout ();

    Midori.Test.idle_timeouts ();
    Midori.Test.log_set_fatal_handler_for_icons ();
    Midori.Paths.Test.reset_runtime_mode ();
    Midori.App.set_instance_is_running (false);
    var app = Midori.normal_app_new (null, "test-extensions-normal", false, null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    Midori.Extension.load_from_folder (app, null, false);

    assert (app.extensions.get_length () > 0);
    */

    foreach (var item in app.extensions.get_items ()) {
        stdout.printf ("- %s\n", (item as Midori.Extension).name);
        (item as Midori.Extension).activate (app);
    }
    do { loop.iteration (true); } while (loop.pending ());

    for (var i = 0 ; i < 7; i++) {
        var tab = app.browser.add_uri ("about:blank") as Midori.View;
        app.browser.close_tab (tab);
    }
    do { loop.iteration (true); } while (loop.pending ());

    foreach (var item in app.extensions.get_items ())
        (item as Midori.Extension).deactivate ();
    do { loop.iteration (true); } while (loop.pending ());

    for (var i = 0 ; i < 7; i++) {
        var tab = app.browser.add_uri ("about:blank") as Midori.View;
        app.browser.close_tab (tab);
        do { loop.iteration (true); } while (loop.pending ());
    }

    Midori.Test.release_max_timeout ();
}


void main (string[] args) {
    Test.init (ref args);
    Midori.App.setup (ref args, null);
    Test.add_func ("/app/normal", app_normal);
    Test.add_func ("/app/normal-custom-config", app_normal_custom_config);
    Test.add_func ("/app/private", app_private);
    Test.add_func ("/app/web", app_web);
    Test.add_func ("/app/web-custom-config", app_web_custom_config);
    Test.add_func ("/app/extensions-load", app_extensions_load);
    /* Test.add_func ("/app/extensions-activate", app_extensions_activate); */
    Test.run ();
}

