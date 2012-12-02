/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

void app_normal () {
    Midori.Paths.Test.reset_runtime_mode ();
    var app = Midori.normal_app_new (null, false, false, null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    Midori.normal_app_on_quit (app);
}

void app_custom_config () {
    /*
    Test.log_set_fatal_handler ((domain, log_levels, message)=> {
        return !message.contains("Error loading theme icon");
        });

    Midori.Paths.Test.reset_runtime_mode ();
    var app = Midori.normal_app_new ("/tmp/mylittlepony", false, false, null, null, null, -1, null);
    var loop = MainContext.default ();
    do { loop.iteration (true); } while (loop.pending ());
    Midori.normal_app_on_quit (app);
    */
}
void app_private () {
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

void main (string[] args) {
    Test.init (ref args);
    Midori.App.setup (ref args, null);
    Test.add_func ("/app/normal", app_normal);
    Test.add_func ("/app/custom-config", app_custom_config);
    Test.add_func ("/app/private", app_private);
    Test.add_func ("/app/web", app_web);
    Test.run ();
}

