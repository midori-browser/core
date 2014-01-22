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
    var app = Midori.normal_app_new (null, "test-normal", false, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (app.browser == null);
    for (var i = 0 ; i < 3; i++) {
        var tab = app.browser.add_uri ("about:blank") as Midori.View;
        app.browser.close_tab (tab);
        do { loop.iteration (true); } while (loop.pending ());
    }
    Midori.normal_app_on_quit (app);
    Midori.Test.release_max_timeout ();
}

void app_normal_custom_config () {
    Midori.Test.grab_max_timeout ();
    Midori.Test.idle_timeouts ();
    Midori.Test.log_set_fatal_handler_for_icons ();
    Midori.Paths.Test.reset_runtime_mode ();
    Midori.App.set_instance_is_running (false);
    string? config_dir = null;
    try {
        config_dir = DirUtils.make_tmp ("custom-configXXXXXX");
    } catch (Error error) {
        GLib.error (error.message);
    }
    var app = Midori.normal_app_new (config_dir,
        "test-custom-config-normal", false, null, null, -1, null);
    Midori.normal_app_on_quit (app);
    Midori.Test.release_max_timeout ();
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
    var browser = Midori.web_app_new ("https://mail.google.com", null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    assert (check_sensible_window_size (browser, browser.settings));
}

void app_web_custom_config () {
    Midori.Paths.Test.reset_runtime_mode ();
    Midori.App.set_instance_is_running (false);
    var browser = Midori.web_app_new ("https://mail.google.com", null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    assert (check_sensible_window_size (browser, browser.settings));
}

void main (string[] args) {
    Test.init (ref args);
    Midori.App.setup (ref args, null);
    Test.add_func ("/app/normal", app_normal);
    Test.add_func ("/app/normal-custom-config", app_normal_custom_config);
    Test.add_func ("/app/private", app_private);
    Test.add_func ("/app/web", app_web);
    Test.add_func ("/app/web-custom-config", app_web_custom_config);
    Test.run ();
}

