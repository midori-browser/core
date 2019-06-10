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
        internal Tab tab { get; protected set; }
        public string? uri { get; set; }
        public string? title { get; set; }
        bool _show_close;
        public bool show_close { get { return _show_close; } set {
            _show_close = value;
            update_visibility ();
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
        Gtk.Image audio;
        [GtkChild]
        Gtk.Button close;

        public Tally (Viewable viewable) {
            var tab = viewable as Tab;
            return_if_fail (tab != null);
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
            tab.notify["color"].connect (apply_color);
            apply_color ();
            tab.notify["is-loading"].connect ((pspec) => {
                favicon.visible = !tab.is_loading;
                spinner.visible = !favicon.visible;
            });
            tab.bind_property ("is-playing-audio", audio, "visible", BindingFlags.SYNC_CREATE);

            // Pinned tab style: icon only
            tab.notify["pinned"].connect ((pspec) => {
                update_visibility ();
            });
            CoreSettings.get_default ().notify["close-buttons-on-tabs"].connect ((pspec) => {
                update_visibility ();
            });

            update_close_position ();
            Gtk.Settings.get_default ().notify["gtk-decoration-layout"].connect ((pspec) => {
                update_close_position ();
            });
        }

        void apply_color () {
            Gdk.Color? background_color = null;
            Gdk.Color? foreground_color = null;
            if (tab.color != null) {
                Gdk.Color.parse (tab.color, out background_color);
                // Ensure high contrast by enforcing black/ white foreground based on Y(UV)
                float brightness = 0.299f * (float)background_color.red / 255f
                                 + 0.587f * (float)background_color.green / 255f
                                 + 0.114f * (float)background_color.blue / 255f;
                Gdk.Color.parse (brightness < 128 ? "white" : "black", out foreground_color);
            }
            modify_fg (Gtk.StateType.NORMAL, foreground_color);
            modify_fg (Gtk.StateType.ACTIVE, foreground_color);
            modify_bg (Gtk.StateType.NORMAL, background_color);
            modify_bg (Gtk.StateType.ACTIVE, background_color);
        }

        void update_close_position () {
            string layout = Gtk.Settings.get_default ().gtk_decoration_layout;
            var box = (Gtk.Box)close.parent;
            if (layout.index_of ("c") < layout.index_of (":")) {
                box.reorder_child (close, 0);
                box.reorder_child (favicon, -1);
                box.reorder_child (spinner, -1);
            } else {
                box.reorder_child (close, -1);
                box.reorder_child (favicon, 0);
                box.reorder_child (spinner, 0);
            }
        }

        void update_visibility () {
            caption.visible = !(tab.pinned && _show_close);
            close.visible = !tab.pinned && CoreSettings.get_default ().close_buttons_on_tabs;
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
            action = new SimpleAction ("duplicate", null);
            action.activate.connect (() => {
                var browser = (Browser)tab.get_ancestor (typeof (Browser));
                browser.add (new Tab (null, tab.web_context, uri));
            });
            group.add_action (action);
            action = new SimpleAction ("close-other", null);
            action.activate.connect (() => {
                var browser = (Browser)tab.get_ancestor (typeof (Browser));
                foreach (var widget in browser.tabs.get_children ()) {
                    if (widget != tab) {
                        ((Tab)widget).try_close ();
                    }
                }
            });
            group.add_action (action);
            action = new SimpleAction ("close-tab", null);
            action.activate.connect (() => {
                tab.try_close ();
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
