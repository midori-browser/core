/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    [GtkTemplate (ui = "/ui/navigationbar.ui")]
    public class Navigationbar : Gtk.ActionBar {
        [GtkChild]
        public Gtk.Box actionbox;
        [GtkChild]
        public Gtk.Button go_back;
        [GtkChild]
        public Gtk.Button go_forward;
        [GtkChild]
        public Gtk.Button reload;
        [GtkChild]
        public Gtk.Button stop_loading;
        [GtkChild]
        public Gtk.Button homepage;
        [GtkChild]
        public Urlbar urlbar;
        [GtkChild]
        public Gtk.MenuButton menubutton;
        [GtkChild]
        public Gtk.Button restore;

        construct {
            var settings = CoreSettings.get_default ();
            homepage.visible = settings.homepage_in_toolbar;
            settings.bind_property ("homepage-in-toolbar", homepage, "visible");
        }
    }
}
