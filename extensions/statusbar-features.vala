/*
 Copyright (C) 2008-2018 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace StatusbarFeatures {
    public class Frontend : Object, Midori.BrowserActivatable {
        public Midori.Browser browser { owned get; set; }

        public void add_zoom () {
            var zoom = new Gtk.ComboBoxText.with_entry ();
            var entry = zoom.get_child () as Gtk.Entry;
            zoom.append_text ("50%");
            zoom.append_text ("80%");
            zoom.append_text ("100%");
            zoom.append_text ("120%");
            zoom.append_text ("150%");
            zoom.append_text ("200%");
            entry.set_width_chars (6);
            entry.set_text ((100 * browser.tab.zoom_level).to_string () + "%");
            zoom.show ();
            zoom.changed.connect(() => {
                if (zoom.get_active_text() == "50%") {
                    browser.tab.zoom_level = 0.5;
                } else if (zoom.get_active_text() == "80%") {
                    browser.tab.zoom_level = 0.8;
                } else if (zoom.get_active_text() == "100%") {
                    browser.tab.zoom_level = 1.0;
                } else if (zoom.get_active_text() == "120%") {
                    browser.tab.zoom_level = 1.2;
                } else if (zoom.get_active_text() == "150%") {
                    browser.tab.zoom_level = 1.5;
                } else if (zoom.get_active_text() == "200%") {
                    browser.tab.zoom_level = 2.0;
                }
                if (entry.has_focus == false) {
                    browser.tab.grab_focus ();
                }
            });

            entry.activate.connect(() => {
                if  (double.parse(entry.get_text ()) >= 1) {
                    browser.tab.zoom_level = double.parse(zoom.get_active_text ()) / 100;
                }
                entry.set_text ((100 * browser.tab.zoom_level).to_string () + "%");
                browser.tab.grab_focus ();
            });

             deactivate.connect (() => {
                zoom.destroy ();
            });
            browser.statusbar.add (zoom);
        }
        void add_toggle (string item, string? icon_name=null, string? tooltip=null) {
            var button = new Gtk.ToggleButton ();
            if (icon_name != null) {
                button.add (new Gtk.Image.from_icon_name (icon_name, Gtk.IconSize.BUTTON));
            } else {
                button.label = item;
            }
            button.tooltip_text = tooltip;
            var settings = Midori.CoreSettings.get_default ();
            if (settings.get_class ().find_property (item) != null) {
                settings.bind_property (item, button, "active", BindingFlags.SYNC_CREATE | BindingFlags.BIDIRECTIONAL);
            } else {
                button.sensitive = false;
            }
            button.show_all ();
            deactivate.connect (() => {
                button.destroy ();
            });
            browser.statusbar.add (button);
        }

        public void activate () {
            string items = "auto-load-images;enable-javascript;enable-plugins";
            foreach (string item in items.split (";")) {
                if (item == "enable-javascript") {
                    add_toggle (item, "text-x-script", _("Enable scripts"));
                } else if (item == "auto-load-images") {
                    add_toggle (item, "image-x-generic", _("Load images automatically"));
                } else if (item == "enable-plugins") {
                    add_toggle (item, "libpeas-plugin", _("Enable Netscape plugins"));
                } else {
                    add_toggle (item);
                }
            }
            add_zoom ();
        }
    }
}

[ModuleInit]
public void peas_register_types(TypeModule module) {
    ((Peas.ObjectModule)module).register_extension_type (
        typeof (Midori.BrowserActivatable), typeof (StatusbarFeatures.Frontend));
}
