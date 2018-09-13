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
            caption.visible = !(tab.pinned && _show_close);
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

        SimpleActionGroup? group = null;

        [GtkChild]
        Gtk.Label caption;
        [GtkChild]
        Gtk.Spinner spinner;
        [GtkChild]
        Favicon favicon;
        [GtkChild]
        Gtk.Button close;

        public Tally (Tab tab) {
            Object (tab: tab,
                    uri: tab.display_uri,
                    title: tab.display_title,
                    tooltip_text: tab.display_title,
                    visible: tab.visible);
            tab.bind_property ("display-uri", this, "uri");
            title = tab.display_title;
            tab.bind_property ("display-title", this, "title");
            bind_property ("title", this, "tooltip-text");
            tab.bind_property ("visible", this, "visible");
            close.clicked.connect (() => { tab.try_close (); });
            tab.notify["is-loading"].connect ((pspec) => {
                favicon.visible = !tab.is_loading;
                spinner.visible = !favicon.visible;
            });

            // Pinned tab style: icon only
            caption.visible = !(tab.pinned && _show_close);
            tab.notify["pinned"].connect ((pspec) => {
                caption.visible = !(tab.pinned && _show_close);
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

            group = new SimpleActionGroup ();
            var action = new SimpleAction ("pin", null);
            action.activate.connect (() => {
                tab.pinned = true;
            });
            group.add_action (action);
            action = new SimpleAction ("unpin", null);
            action.activate.connect (() => {
                tab.pinned = false;
            });
            group.add_action (action);
            insert_action_group ("tally", group);
        }

        protected override bool button_press_event (Gdk.EventButton event) {
            // No context menu for a single tab
            if (!show_close) {
                return false;
            }

            switch (event.button) {
                case Gdk.BUTTON_SECONDARY:
                    ((SimpleAction)group.lookup_action ("pin")).set_enabled (!tab.pinned);
                    ((SimpleAction)group.lookup_action ("unpin")).set_enabled (tab.pinned);
                    var app = (App)Application.get_default ();
                    var menu = new Gtk.Popover.from_model (this, app.get_menu_by_id ("tally-menu"));
                    menu.show ();
                    break;
            }
            return true;
        }

        protected override bool button_release_event (Gdk.EventButton event) {
            switch (event.button) {
                case Gdk.BUTTON_PRIMARY:
                    clicked ();
                    break;
                case Gdk.BUTTON_MIDDLE:
                    tab.try_close ();
                    break;
            }
            return true;
        }
    }
}
