/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    [GtkTemplate (ui = "/ui/tally.ui")]
    public class Tally : Gtk.EventBox {
        public Tab tab { get; protected set; }
        public string? uri { get; set; }
        public string? title { get; set; }
        bool _show_close;
        public bool show_close { get { return _show_close; } set {
            _show_close = value;
            close.visible = _show_close && !tab.pinned;
        } }

        public signal void clicked ();
        // Implement toggled state of Gtk.ToggleButton
        bool _active = false;
        public bool active { get { return _active; } set {
            _active = value;
            if (_active) {
                set_state_flags (Gtk.StateFlags.CHECKED, false);
            } else {
                unset_state_flags (Gtk.StateFlags.CHECKED);
            }
        } }

        [GtkChild]
        Gtk.Label caption;
        [GtkChild]
        Gtk.Spinner spinner;
        [GtkChild]
        Favicon favicon;
        [GtkChild]
        Gtk.Button close;

        public Tally (Tab tab) {
            Object (tab: tab, uri: tab.display_uri, title: tab.display_title, visible: tab.visible);
            tab.bind_property ("display-uri", this, "uri");
            tab.bind_property ("display-title", this, "title");
            tab.bind_property ("visible", this, "visible");
            close.clicked.connect (() => { tab.try_close (); });
            tab.notify["is-loading"].connect ((pspec) => {
                favicon.visible = !tab.is_loading;
                spinner.visible = !favicon.visible;
            });

            // Pinned tab style: icon only
            caption.visible = !tab.pinned;
            tab.notify["pinned"].connect ((pspec) => {
                caption.visible = !tab.pinned;
                close.visible = _show_close && !tab.pinned;
            });
        }

        construct {
            bind_property ("uri", favicon, "uri");
            bind_property ("title", caption, "label");
            add_events (Gdk.EventMask.ENTER_NOTIFY_MASK);
            add_events (Gdk.EventMask.LEAVE_NOTIFY_MASK);
            enter_notify_event.connect ((event) => {
                set_state_flags (Gtk.StateFlags.PRELIGHT, false);
            });
            leave_notify_event.connect ((event) => {
                unset_state_flags (Gtk.StateFlags.PRELIGHT);
            });
        }

        protected override bool button_release_event (Gdk.EventButton event) {
            switch (event.button) {
                case 1:
                    clicked ();
                    break;
                case 3:
                    tab.try_close ();
                    break;
            }
            return true;
        }
    }
}
