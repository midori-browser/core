/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public interface BrowserActivatable : Object {
        public abstract Browser browser { owned get; set; }
        public abstract void activate ();
    }

    [GtkTemplate (ui = "/ui/browser.ui")]
    public class Browser : Gtk.ApplicationWindow {
        public WebKit.WebContext web_context { get; construct set; }
        public bool is_loading { get { return tab != null && tab.is_loading; } }
        public Tab? tab { get; protected set; }
        public ListStore trash { get; protected set; }
        public bool is_fullscreen { get; protected set; default = false; }

        const ActionEntry[] actions = {
            { "tab-new", tab_new_activated },
            { "tab-close", tab_close_activated },
            { "close", close_activated },
            { "tab-reopen", tab_reopen_activated },
            { "goto", goto_activated },
            { "go-back", go_back_activated },
            { "go-forward", go_forward_activated },
            { "tab-reload", tab_reload_activated },
            { "tab-stop-loading", tab_stop_loading_activated },
            { "tab-previous", tab_previous_activated },
            { "tab-next", tab_next_activated },
            { "fullscreen", fullscreen_activated },
            { "show-downloads", show_downloads_activated },
            { "find", find_activated },
            { "view-source", view_source_activated },
            { "print", print_activated },
            { "show-inspector", show_inspector_activated },
            { "clear-private-data", clear_private_data_activated },
            { "about", about_activated },
        };
        [GtkChild]
        Gtk.HeaderBar panelbar;
        [GtkChild]
        Gtk.Stack panel;
        [GtkChild]
        Gtk.ToggleButton panel_toggle;
        [GtkChild]
        Gtk.HeaderBar tabbar;
        [GtkChild]
        DownloadButton downloads;
        [GtkChild]
        Gtk.MenuButton app_menu;
        [GtkChild]
        Navigationbar navigationbar;
        [GtkChild]
        Gtk.Stack tabs;
        [GtkChild]
        Gtk.Overlay overlay;
        [GtkChild]
        Statusbar statusbar;
        [GtkChild]
        Gtk.SearchBar search;
        [GtkChild]
        Gtk.SearchEntry search_entry;

        List<Binding> bindings;
        uint focus_timeout = 0;

        construct {
            overlay.add_events (Gdk.EventMask.ENTER_NOTIFY_MASK);
            overlay.enter_notify_event.connect ((event) => {
                if (is_fullscreen && !tab.pinned) {
                    navigationbar.show ();
                    navigationbar.urlbar.grab_focus ();
                }
                statusbar.hide ();
                statusbar.halign = statusbar.halign == Gtk.Align.START ? Gtk.Align.END : Gtk.Align.START;
                statusbar.show ();
                return false;
            });
            navigationbar.urlbar.focus_out_event.connect ((event) => {
                if (is_fullscreen) {
                    navigationbar.hide ();
                }
                return false;
            });

            add_action_entries (actions, this);

            notify["application"].connect ((pspec) => {
                application.set_accels_for_action ("win.panel", { "F9" });
                application.set_accels_for_action ("win.tab-new", { "<Primary>t" });
                application.set_accels_for_action ("win.tab-close", { "<Primary>w" });
                application.set_accels_for_action ("win.close", { "<Primary><Shift>w" });
                application.set_accels_for_action ("win.tab-reopen", { "<Primary><Shift>t" });
                application.set_accels_for_action ("win.fullscreen", { "F11" });
                application.set_accels_for_action ("win.show-downloads", { "<Primary><Shift>j" });
                application.set_accels_for_action ("win.find", { "<Primary>f", "slash" });
                application.set_accels_for_action ("win.view-source", { "<Primary>u", "<Primary><Alt>u" });
                application.set_accels_for_action ("win.print", { "<Primary>p" });
                application.set_accels_for_action ("win.show-inspector", { "<Primary><Shift>i" });
                application.set_accels_for_action ("win.goto", { "<Primary>l", "F7" });
                application.set_accels_for_action ("win.go-back", { "<Alt>Left", "BackSpace" });
                application.set_accels_for_action ("win.go-forward", { "<Alt>Right", "<Shift>BackSpace" });
                application.set_accels_for_action ("win.tab-reload", { "<Primary>r", "F5" });
                application.set_accels_for_action ("win.tab-stop-loading", { "Escape" });
                application.set_accels_for_action ("win.tab-previous", { "<Primary><Shift>Tab" });
                application.set_accels_for_action ("win.tab-next", { "<Primary>Tab" });
                application.set_accels_for_action ("win.clear-private-data", { "<Primary><Shift>Delete" });

                for (var i = 0; i < 10; i++) {
                    application.set_accels_for_action ("win.tab-by-index(%d)".printf(i),
                        { "<Alt>%d".printf (i < 9 ? i + 1 : 0) });
                }
                application.set_accels_for_action ("win.tab-zoom(0.1)", { "<Primary>plus", "<Primary>equal" });
                application.set_accels_for_action ("win.tab-zoom(-0.1)", { "<Primary>minus" });
                application.set_accels_for_action ("win.tab-zoom(1.0)", { "<Primary>0" });

                app_menu.menu_model = application.get_menu_by_id ("app-menu");
                navigationbar.menubutton.menu_model = application.get_menu_by_id ("page-menu");

                application.bind_busy_property (this, "is-loading");
            });

            // Action for switching tabs via Alt+number
            var action = new SimpleAction ("tab-by-index", VariantType.INT32);
            action.activate.connect (tab_by_index_activated);
            add_action (action);
            // Action for zooming
            action = new SimpleAction ("tab-zoom", VariantType.DOUBLE);
            action.activate.connect (tab_zoom_activated);
            add_action (action);
            // Action for panel toggling
            action = new SimpleAction.stateful ("panel", null, false);
            action.change_state.connect (panel_activated);
            add_action (action);

            trash = new ListStore (typeof (DatabaseItem));

            navigationbar.urlbar.notify["uri"].connect ((pspec) => {
                string uri = navigationbar.urlbar.uri;
                if (uri.has_prefix ("javascript:")) {
                    tab.run_javascript.begin (uri.substring (11, -1), null);
                } else if (uri != tab.display_uri) {
                    tab.load_uri (uri);
                }
            });
            tabs.notify["visible-child"].connect (() => {
                if (bindings != null) {
                    foreach (var binding in bindings) {
                        binding.unbind ();
                    }
                }
                tab = (Tab)tabs.visible_child;
                if (tab != null) {
                    navigationbar.go_back.sensitive = tab.can_go_back;
                    navigationbar.go_forward.sensitive = tab.can_go_forward;
                    navigationbar.reload.visible = !tab.is_loading;
                    navigationbar.stop_loading.visible = tab.is_loading;
                    navigationbar.urlbar.progress_fraction = tab.progress;
                    title = tab.display_title;
                    navigationbar.urlbar.secure = tab.secure;
                    statusbar.label = tab.link_uri;
                    navigationbar.urlbar.uri = tab.display_uri;
                    navigationbar.visible = !tab.pinned;
                    bindings.append (tab.bind_property ("can-go-back", navigationbar.go_back, "sensitive"));
                    bindings.append (tab.bind_property ("can-go-forward", navigationbar.go_forward, "sensitive"));
                    bindings.append (tab.bind_property ("is-loading", navigationbar.reload, "visible", BindingFlags.INVERT_BOOLEAN));
                    bindings.append (tab.bind_property ("is-loading", navigationbar.stop_loading, "visible"));
                    bindings.append (tab.bind_property ("progress", navigationbar.urlbar, "progress-fraction"));
                    bindings.append (tab.bind_property ("display-title", this, "title"));
                    bindings.append (tab.bind_property ("secure", navigationbar.urlbar, "secure"));
                    bindings.append (tab.bind_property ("link-uri", statusbar, "label"));
                    bindings.append (tab.bind_property ("display-uri", navigationbar.urlbar, "uri"));
                    bindings.append (tab.bind_property ("pinned", navigationbar, "visible", BindingFlags.INVERT_BOOLEAN));
                    if (focus_timeout > 0) {
                        Source.remove (focus_timeout);
                        focus_timeout = 0;
                    }
                    focus_timeout = Timeout.add (500, () => {
                        tab.grab_focus ();
                        goto_activated ();
                        return Source.REMOVE;
                    }, Priority.LOW);
                } else {
                    var previous_tab = tabs.get_children ().nth_data (0);
                    if (previous_tab == null)
                        close ();
                    else
                        tab = (Tab)previous_tab;
                }
            });

            search_entry.activate.connect (find_text);
            search_entry.search_changed.connect (find_text);
            search_entry.next_match.connect (find_text);
            search_entry.previous_match.connect (() => {
                find_text_backwards (true);
            });

            var provider = new Gtk.CssProvider ();
            provider.load_from_resource ("/data/gtk3.css");
            Gtk.StyleContext.add_provider_for_screen (get_screen (), provider,
                Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION);

            // Make headerbar (titlebar) the topmost bar if CSD is disabled
            if (Environment.get_variable ("GTK_CSD") == "0") {
                var titlebar = get_titlebar ();
                titlebar.ref ();
                set_titlebar (null);
                panelbar.show_close_button = false;
                tabbar.show_close_button = false;
                var box = (get_child () as Gtk.Box);
                box.add (titlebar);
                box.reorder_child (titlebar, 0);
                bind_property ("is-fullscreen", titlebar, "visible", BindingFlags.INVERT_BOOLEAN);
                titlebar.unref ();
                titlebar.get_style_context ().remove_class ("titlebar");
            } else {
                Gtk.Settings.get_default ().notify["gtk-decoration-layout"].connect ((pspec) => {
                    update_decoration_layout ();
                });
                update_decoration_layout ();
            }

            // Reveal panel toggle after panels are added
            panel.add.connect ((widget) => { panel_toggle.show (); });

            var extensions = Plugins.get_default ().plug<BrowserActivatable> ("browser", this);
            extensions.extension_added.connect ((info, extension) => ((BrowserActivatable)extension).activate ());
            extensions.foreach ((extensions, info, extension) => { extensions.extension_added (info, extension); });
        }

        void update_decoration_layout () {
            // With panels are visible the window decoration is split in two
            if (panel.visible) {
                string[] layout = Gtk.Settings.get_default ().gtk_decoration_layout.split (":", 2);
                panelbar.decoration_layout = layout[0];
                tabbar.decoration_layout = ":" + layout[1];
            } else {
                tabbar.decoration_layout = null;
            }
        }

        public Browser (App app) {
            Object (application: app,
                    web_context: WebKit.WebContext.get_default ());
        }

        public Browser.incognito (App app) {
            Object (application: app,
                    web_context: app.ephemeral_context ());

            remove_action ("clear-private-data");
            get_style_context ().add_class ("incognito");
        }

        public override bool key_press_event (Gdk.EventKey event) {
            // Give key handling in widgets precedence over actions
            // eg. Backspace in textfields should delete rather than go back
            if (propagate_key_event (event)) {
                return true;
            }
            if (base.key_press_event (event)) {
                // Popdown completion if a key binding was fired
                navigationbar.urlbar.popdown ();
                return true;
            }
            return false;
        }

        void panel_activated (SimpleAction action, Variant? state) {
            if (panel_toggle.visible) {
                action.set_state (state);
                panel.visible = state.get_boolean ();
                update_decoration_layout ();
            }
        }

        void tab_new_activated () {
            var tab = new Tab (tab, web_context);
            add (tab);
            tabs.visible_child = tab;
        }

        void tab_close_activated () {
            tab.try_close ();
        }

        void close_activated () {
            close ();
        }

        void tab_reopen_activated () {
            uint index = trash.get_n_items ();
            if (index > 0) {
                var item = trash.get_object (index - 1) as DatabaseItem;
                add (new Tab (tab, web_context, item.uri, item.title));
                trash.remove (index - 1);
            }
        }

        void goto_activated () {
            if (!tab.pinned) {
                navigationbar.show ();
                navigationbar.urlbar.grab_focus ();
            }
        }

        void go_back_activated () {
            tab.go_back ();
        }

        void go_forward_activated () {
            tab.go_forward ();
        }

        void tab_reload_activated () {
            tab.reload ();
        }

        void tab_stop_loading_activated () {
            tab.stop_loading ();
        }

        void tab_previous_activated () {
            int index = tabs.get_children ().index (tab);
            var previous = tabs.get_children ().nth_data (index - 1);
            if (previous != null)
                tabs.visible_child = (Tab)previous;
        }

        void tab_next_activated () {
            int index = tabs.get_children ().index (tab);
            var next = tabs.get_children ().nth_data (index + 1);
            if (next != null)
                tabs.visible_child = (Tab)next;
        }

        void tab_zoom_activated (Action action, Variant? parameter) {
            double zoom_level = parameter.get_double ();
            tab.zoom_level = zoom_level == 1.0 ? 1.0 : (tab.zoom_level + zoom_level);
        }

        void tab_by_index_activated (Action action, Variant? parameter) {
            var nth_tab = tabs.get_children ().nth_data (parameter.get_int32 ());
            if (nth_tab != null) {
                tabs.visible_child = nth_tab;
            }
        }

        void fullscreen_activated () {
            is_fullscreen = !is_fullscreen;
            navigationbar.restore.visible = is_fullscreen;
            navigationbar.menubutton.visible = !is_fullscreen;
            if (is_fullscreen) {
                fullscreen ();
                navigationbar.hide ();
                panel.hide ();
            } else {
                unfullscreen ();
                navigationbar.visible = !tab.pinned;
                panel.visible = lookup_action ("panel").state.get_boolean ();
            }
        }

        void show_downloads_activated () {
            downloads.show_downloads ();
        }

        void find_activated () {
            search.show ();
            search.search_mode_enabled = true;
            search_entry.grab_focus ();
        }

        void find_text () {
            find_text_backwards (false);
        }

        void find_text_backwards (bool backwards) {
            uint options = WebKit.FindOptions.WRAP_AROUND;
            // Smart case: case sensitive if starting with an uppercase character
            if (search_entry.text[0].islower ()) {
                options |= WebKit.FindOptions.CASE_INSENSITIVE;
            }
            if (backwards) {
                options |= WebKit.FindOptions.BACKWARDS;
            }
            tab.get_find_controller ().search (search_entry.text, options, int.MAX);
        }

        void view_source_activated () {
            view_source.begin (tab);
        }

        async void view_source (Tab tab) {
            string uri = tab.display_uri;
            try {
                var file = File.new_for_uri (uri);
                if (!uri.has_prefix ("file:///")) {
                    FileIOStream stream;
                    file = File.new_tmp ("sourceXXXXXX", out stream);
                    var data = yield tab.get_main_resource ().get_data (null);
                    yield stream.output_stream.write_async (data);
                    yield stream.close_async ();
                }
                var files = new List<File> ();
                files.append (file);
                var info = AppInfo.get_default_for_type ("text/plain", false);
                info.launch (files, get_display ().get_app_launch_context ());
            } catch (Error error) {
                critical ("Failed to open %s in editor: %s", uri, error.message);
            }
        }

        void print_activated () {
            tab.print (new WebKit.PrintOperation (tab));
        }

        void show_inspector_activated () {
            tab.get_inspector ().show ();
        }

        public new void add (Tab tab) {
            tab.create.connect ((action) => {
                var new_tab = new Tab (tab, web_context);
                new_tab.hide ();
                new_tab.ready_to_show.connect (() => {
                    new_tab.show ();
                    add (new_tab);
                });
                return new_tab;
            });
            tab.enter_fullscreen.connect (() => {
                navigationbar.hide ();
                return false;
            });
            tab.leave_fullscreen.connect (() => {
                navigationbar.visible = !tab.pinned;
                return false;
            });
            tab.close.connect (() => {
                trash.append (tab.item);
            });
            // Support Gtk.StackSwitcher
            tab.notify["display-title"].connect ((pspec) => {
                tabs.child_set (tab, "title", tab.display_title);
            });
            tabs.add_titled (tab, tab.id, tab.display_title);
        }

        void clear_private_data_activated () {
            new ClearPrivateData (this).show ();
        }

        void about_activated () {
            new About (this).show ();
        }
    }
}
