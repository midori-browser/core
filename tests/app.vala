/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

void app_normal () {
    Midori.Test.grab_max_timeout ();

    Midori.Test.idle_timeouts ();
    Midori.Test.log_set_fatal_handler_for_icons ();
    Midori.Paths.Test.reset_runtime_mode ();
    var app = Midori.normal_app_new (null, "test-normal", false, null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    for (var i = 0 ; i < 7; i++) {
        var tab = app.browser.get_nth_tab (app.browser.add_uri ("about:blank"));
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

void app_custom_config () {
    Midori.Test.log_set_fatal_handler_for_icons ();
    Midori.Paths.Test.reset_runtime_mode ();
    var app = Midori.normal_app_new ("/tmp/mylittlepony",
        "test-custom-config-normal", false, null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    Midori.normal_app_on_quit (app);
}
void app_private () {
    Midori.Test.log_set_fatal_handler_for_icons ();
    Midori.Paths.Test.reset_runtime_mode ();
    Midori.private_app_new (null, null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
}

void app_web () {
    Midori.Paths.Test.reset_runtime_mode ();
    Midori.web_app_new (null, null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
}

void app_extensions () {
    Midori.Test.grab_max_timeout ();

    Midori.Test.idle_timeouts ();
    Midori.Test.log_set_fatal_handler_for_icons ();
    Midori.Paths.Test.reset_runtime_mode ();
    var app = Midori.normal_app_new (null, "test-extensions-normal", false, null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    /* No extensions loaded */
    assert (app.extensions.get_length () == 0);
    Midori.Extension.load_from_folder (app, null, false);
    /* All extensions loaded, inactive */
    assert (app.extensions.get_length () > 0);
    foreach (var item in app.extensions.get_items ())
        assert (!(item as Midori.Extension).is_active ());

    for (var i = 0 ; i < 7; i++) {
        var tab = app.browser.get_nth_tab (app.browser.add_uri ("about:blank"));
        app.browser.close_tab (tab);
        do { loop.iteration (true); } while (loop.pending ());
    }

    Midori.Test.release_max_timeout ();
}

void main (string[] args) {
    Test.init (ref args);
    Midori.App.setup (ref args, null);
    Test.add_func ("/app/normal", app_normal);
    Test.add_func ("/app/custom-config", app_custom_config);
    Test.add_func ("/app/private", app_private);
    Test.add_func ("/app/web", app_web);
    Test.add_func ("/app/extensions", app_extensions);
    Test.run ();
}

