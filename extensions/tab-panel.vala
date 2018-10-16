/*
 Copyright (C) 2008-2018 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace TabPanel {
    public class Frontend : Object, Midori.BrowserActivatable {
        public Midori.Browser browser { owned get; set; }

        public void activate () {
            var switcher = new Midori.Switcher ();
            switcher.stack = browser.tabs;
            switcher.orientation = Gtk.Orientation.VERTICAL;
            switcher.show ();
            browser.panel.add_titled (switcher, "tab-panel", _("Tab Panel"));
            browser.panel.child_set (switcher, "icon-name", "view-list-symbolic");
            deactivate.connect (() => {
                switcher.destroy ();
            });
        }
    }
}

[ModuleInit]
public void peas_register_types(TypeModule module) {
    ((Peas.ObjectModule)module).register_extension_type (
        typeof (Midori.BrowserActivatable), typeof (TabPanel.Frontend));
}
