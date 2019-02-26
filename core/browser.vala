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
        public signal void deactivate ();
    }

    [GtkTemplate (ui = "/ui/browser.ui")]
    public class Browser : Gtk.ApplicationWindow {
        public WebKit.WebContext web_context { get; construct set; }
        public bool is_loading { get; protected set; default = false; }
        public string? uri { get; protected set; }
        public Tab? tab { get; protected set; }
        public ListStore trash { get; protected set; }
        public bool is_fullscreen { get; protected set; default = false; }
        public bool is_locked { get; construct set; default = false; }
        internal bool is_small { get; protected set; default = false; }

        const ActionEntry[] actions = {
            { "tab-close", tab_close_activated },
            { "close", close_activated },
            { "tab-reopen", tab_reopen_activated },
            { "goto", goto_activated },
            { "tab-previous", tab_previous_activated },
            { "tab-next", tab_next_activated },
            { "find", find_activated },
            { "view-source", view_source_activated },
            { "print", print_activated },
            { "caret-browsing", caret_browsing_activated },
            { "show-inspector", show_inspector_activated },
            { "clear-private-data", clear_private_data_activated },
            { "preferences", preferences_activated },
            { "about", about_activated },
        };
        [GtkChild]
        Gtk.HeaderBar panelbar;
        [GtkChild]
        Gtk.Stack panel;
        [GtkChild]
        Gtk.HeaderBar tabbar;
        [GtkChild]
        Gtk.Box actionbox;
        [GtkChild]
        Gtk.ToggleButton panel_toggle;
        [GtkChild]
        DownloadButton downloads;
        [GtkChild]
        Gtk.Button tab_new;
        [GtkChild]
        Gtk.Button toggle_fullscreen;
        [GtkChild]
        Gtk.MenuButton app_menu;
        [GtkChild]
        Navigationbar navigationbar;
        [GtkChild]
        public Gtk.Stack tabs;
        [GtkChild]
        public Gtk.Overlay overlay;
        [GtkChild]
        public Statusbar statusbar;
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
                if (!statusbar.has_children) {
                    statusbar.hide ();
                    // Flip overlay to evade the mouse pointer
                    statusbar.halign = statusbar.halign == Gtk.Align.START ? Gtk.Align.END : Gtk.Align.START;
                    statusbar.show ();
                }
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
                application.set_accels_for_action ("win.close", { "<Primary><Shift>w", "<Primary>F4" });
                application.set_accels_for_action ("win.tab-reopen", { "<Primary><Shift>t" });
                application.set_accels_for_action ("win.fullscreen", { "F11" });
                application.set_accels_for_action ("win.show-downloads", { "<Primary><Shift>j" });
                application.set_accels_for_action ("win.find", { "<Primary>f", "slash" });
                application.set_accels_for_action ("win.view-source", { "<Primary>u", "<Primary><Alt>u" });
                application.set_accels_for_action ("win.print", { "<Primary>p" });
                application.set_accels_for_action ("win.caret-browsing", { "F7" });
                application.set_accels_for_action ("win.show-inspector", { "<Primary><Shift>i" });
                application.set_accels_for_action ("win.goto", { "<Primary>l", "F7" });
                application.set_accels_for_action ("win.go-back", { "<Alt>Left", "BackSpace", "Back" });
                application.set_accels_for_action ("win.go-forward", { "<Alt>Right", "<Shift>BackSpace" });
                application.set_accels_for_action ("win.tab-reload", { "<Primary>r", "F5" });
                application.set_accels_for_action ("win.tab-stop-loading", { "Escape" });
                application.set_accels_for_action ("win.homepage", { "<Alt>Home", "HomePage" });
                application.set_accels_for_action ("win.tab-previous", { "<Primary><Shift>Tab" });
                application.set_accels_for_action ("win.tab-next", { "<Primary>Tab" });
                application.set_accels_for_action ("win.clear-private-data", { "<Primary><Shift>Delete" });
                application.set_accels_for_action ("win.preferences", { "<Primary><Alt>p" });
                application.set_accels_for_action ("win.show-help-overlay", { "<Primary>F1", "<Shift>question" });

                for (var i = 0; i < 10; i++) {
                    application.set_accels_for_action ("win.tab-by-index(%d)".printf(i),
                        { "<Alt>%d".printf (i < 9 ? i + 1 : 0), "<Primary>%d".printf (i < 9 ? i + 1 : 0) });
                }
                application.set_accels_for_action ("win.tab-zoom(0.1)", { "<Primary>plus", "<Primary>equal" });
                application.set_accels_for_action ("win.tab-zoom(-0.1)", { "<Primary>minus" });
                application.set_accels_for_action ("win.tab-zoom(1.0)", { "<Primary>0" });

                app_menu.menu_model = application.get_menu_by_id ("app-menu");
                navigationbar.menubutton.menu_model = application.get_menu_by_id ("page-menu");
                notify["is-small"].connect (() => {
                    var app_menu_model = new Menu ();
                    app_menu_model.prepend_section (null, application.get_menu_by_id ("app-menu"));
                    var page_menu_model = new Menu ();
                    page_menu_model.prepend_section (null, application.get_menu_by_id ("page-menu"));
                    if (is_small) {
                        app_menu_model.prepend_section (null, application.get_menu_by_id ("app-menu-small"));
                        page_menu_model.prepend_section (null, application.get_menu_by_id ("page-menu-small"));
                        // Anchor downloads popover to app menu if the button is hidden
                        downloads.popover.relative_to = app_menu;
                    } else {
                        downloads.popover.relative_to = downloads;
                    }
                    app_menu.menu_model = app_menu_model;
                    navigationbar.menubutton.menu_model = page_menu_model;
                });

                application.bind_busy_property (this, "is-loading");

                // Plug only after the app is connected and everything is setup
                var extensions = Plugins.get_default ().plug<BrowserActivatable> ("browser", this);
                extensions.extension_added.connect ((info, extension) => ((BrowserActivatable)extension).activate ());
                extensions.extension_removed.connect ((info, extension) => ((BrowserActivatable)extension).deactivate ());
                extensions.foreach ((extensions, info, extension) => { extensions.extension_added (info, extension); });
            });

            // Action for switching tabs via Alt+number
            var action = new SimpleAction ("tab-by-index", VariantType.INT32);
            action.activate.connect (tab_by_index_activated);
            add_action (action);
            // Action for zooming
            action = new SimpleAction ("tab-zoom", VariantType.DOUBLE);
            action.activate.connect (tab_zoom_activated);
            add_action (action);
            // Browser actions
            action = new SimpleAction ("tab-new", null);
            action.activate.connect (tab_new_activated);
            add_action (action);
            action.set_enabled (!is_locked);
            var show_downloads = new SimpleAction ("show-downloads", null);
            show_downloads.activate.connect (show_downloads_activated);
            add_action (show_downloads);
            show_downloads.set_enabled (false);
            downloads.notify["visible"].connect (() => {
                show_downloads.set_enabled (downloads.visible);
            });
            var fullscreen = new SimpleAction ("fullscreen", null);
            fullscreen.activate.connect (fullscreen_activated);
            add_action (fullscreen);
            navigationbar.notify["visible"].connect (() => {
                fullscreen.set_enabled (navigationbar.visible);
            });
            // Action for panel toggling
            action = new SimpleAction.stateful ("panel", null, false);
            action.set_enabled (false);
            action.change_state.connect (panel_activated);
            add_action (action);
            // Reveal panel toggle after panels are added
            panel.add.connect ((widget) => {
                panel_toggle.show ();
                action.set_enabled (true);
            });
            // Page actions
            var go_back = new SimpleAction ("go-back", null);
            go_back.activate.connect (go_back_activated);
            add_action (go_back);
            var go_forward = new SimpleAction ("go-forward", null);
            go_back.activate.connect (go_forward_activated);
            add_action (go_forward);
            notify["uri"].connect (() => {
                go_back.set_enabled (tab.can_go_back);
                go_forward.set_enabled (tab.can_go_forward);
            });
            var reload = new SimpleAction ("tab-reload", null);
            reload.activate.connect (tab_reload_activated);
            add_action (reload);
            var stop = new SimpleAction ("tab-stop-loading", null);
            stop.activate.connect (tab_stop_loading_activated);
            add_action (stop);
            notify["is-loading"].connect (() => {
                reload.set_enabled (!is_loading);
                stop.set_enabled (is_loading);
            });
            action = new SimpleAction ("homepage", null);
            action.activate.connect (homepage_activated);
            add_action (action);
            var settings = CoreSettings.get_default ();
            action.set_enabled (settings.homepage_in_toolbar);
            settings.notify["homepage-in-toolbar"].connect (() => {
                action.set_enabled (settings.homepage_in_toolbar);
            });

            trash = new ListStore (typeof (DatabaseItem));

            bind_property ("is-locked", tab_new, "visible", BindingFlags.INVERT_BOOLEAN);
            bind_property ("is-small", actionbox, "visible", BindingFlags.SYNC_CREATE | BindingFlags.INVERT_BOOLEAN);
            bind_property ("is-small", navigationbar.actionbox, "visible", BindingFlags.SYNC_CREATE | BindingFlags.INVERT_BOOLEAN);
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
                    navigationbar.urlbar.progress_fraction = tab.progress;
                    uri = tab.display_uri;
                    title = tab.display_title;
                    navigationbar.urlbar.secure = tab.secure;
                    statusbar.label = tab.link_uri;
                    navigationbar.urlbar.uri = tab.display_uri;
                    toggle_fullscreen.visible = !tab.pinned;
                    navigationbar.visible = !tab.pinned;
                    bindings.append (tab.bind_property ("is-loading", navigationbar.reload, "visible", BindingFlags.SYNC_CREATE | BindingFlags.INVERT_BOOLEAN));
                    bindings.append (tab.bind_property ("is-loading", navigationbar.stop_loading, "visible", BindingFlags.SYNC_CREATE));
                    bindings.append (tab.bind_property ("is-loading", this, "is-loading", BindingFlags.SYNC_CREATE));
                    bindings.append (tab.bind_property ("progress", navigationbar.urlbar, "progress-fraction"));
                    bindings.append (tab.bind_property ("display-title", this, "title"));
                    bindings.append (tab.bind_property ("display-uri", this, "uri"));
                    bindings.append (tab.bind_property ("secure", navigationbar.urlbar, "secure"));
                    bindings.append (tab.bind_property ("link-uri", statusbar, "label"));
                    bindings.append (tab.bind_property ("display-uri", navigationbar.urlbar, "uri"));
                    bindings.append (tab.bind_property ("pinned", toggle_fullscreen, "visible", BindingFlags.INVERT_BOOLEAN));
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
            get_settings ().gtk_dialogs_use_header = Environment.get_variable ("GTK_CSD") != "0";
            if (!get_settings ().gtk_dialogs_use_header) {
                var titlebar = get_titlebar ();
                titlebar.ref ();
                set_titlebar (null);
                panelbar.show_close_button = false;
                tabbar.show_close_button = false;
                bind_property ("is-locked", tabbar, "visible", BindingFlags.INVERT_BOOLEAN);
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

            downloads.web_context = web_context;
            if (web_context.is_ephemeral ()) {
                get_style_context ().add_class ("incognito");
            }

            if (settings.last_window_width > 0 && settings.last_window_height > 0) {
                default_width = settings.last_window_width;
                default_height = settings.last_window_height;
            }
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

        public override bool configure_event (Gdk.EventConfigure event) {
            bool result = base.configure_event (event);

            int width;
            get_size (out width, null);
            is_small = width < 500;

            if (!(get_style_context ().has_class ("tiled") || is_maximized || is_fullscreen)) {
                int height;
                get_size (null, out height);
                var settings = CoreSettings.get_default ();
                settings.last_window_width = width;
                settings.last_window_height = height;
            }

            return result;
        }

        /*
         * Add a button to be displayed in a toolbar.
         */
        public void add_button (Gtk.Button button) {
            navigationbar.pack_end (button);
        }

        /*
         * Requests a default tab to be added to an otherwise empty window.
         *
         * Connect, adding one or more windows, and return true to override.
         */
        public signal bool default_tab ();

        public Browser (App app, bool is_locked=false) {
            Object (application: app,
                    is_locked: is_locked,
                    web_context: WebKit.WebContext.get_default ());
        }

        public Browser.incognito (App app) {
            Object (application: app,
                    web_context: app.ephemeral_context ());
        }

        public override bool key_press_event (Gdk.EventKey event) {
            // No keyboard shortcuts in locked state
            if (is_locked) {
                return propagate_key_event (event);
            }
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
            var tab = new Tab (null, web_context);
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
                add (new Tab (null, web_context, item.uri, item.title));
                trash.remove (index - 1);
            }
        }

        void goto_activated () {
            if (!tab.pinned) {
                navigationbar.show ();
                if (navigationbar.urlbar.blank) {
                    navigationbar.urlbar.grab_focus ();
                }
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

        void homepage_activated () {
            var settings = CoreSettings.get_default ();
            string homepage = settings.homepage;
            string uri;
            if ("://" in homepage) {
                uri = homepage;
            } else if ("." in homepage) {
                // Prepend http:// if hompepage has no scheme
                uri = "http://" + homepage;
            } else {
                // Fallback to search if URI is about:search or anything else
                uri = settings.uri_for_search ();
            }
            if (tab == null) {
                add (new Tab (null, web_context, uri));
            } else {
                tab.load_uri (uri);
            }
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

        void caret_browsing_activated () {
            var settings = CoreSettings.get_default ();
            if (!settings.enable_caret_browsing) {
                var dialog = new Gtk.Dialog.with_buttons (
                    get_settings ().gtk_dialogs_use_header ? null : _("Toggle text cursor navigation"),
                    this,
                    get_settings ().gtk_dialogs_use_header ? Gtk.DialogFlags.USE_HEADER_BAR : 0,
                    Gtk.Stock.CANCEL, Gtk.ResponseType.CANCEL,
                    _("_Enable Caret Browsing"), Gtk.ResponseType.ACCEPT);
                var label = new Gtk.Label (_("Pressing F7 toggles Caret Browsing. When active, a text cursor appears in all websites."));
                label.wrap = true;
                label.max_width_chars = 33;
                label.margin = 8;
                label.show ();
                dialog.get_content_area ().add (label);
                dialog.set_default_response (Gtk.ResponseType.ACCEPT);
                if (dialog.run () == Gtk.ResponseType.ACCEPT) {
                    settings.enable_caret_browsing = true;
                }
                dialog.close ();
            } else {
                settings.enable_caret_browsing = false;
            }
        }

        void show_inspector_activated () {
            tab.get_inspector ().show ();
        }

        public new void add (Tab tab) {
            tab.popover.relative_to = navigationbar.urlbar;
            if (is_locked) {
                tab.decide_policy.connect ((decision, type) => {
                    switch (type) {
                        case WebKit.PolicyDecisionType.NAVIGATION_ACTION:
                            // No user-initiated new tabs
                            decision.use ();
                            return true;
                        case WebKit.PolicyDecisionType.NEW_WINDOW_ACTION:
                            // External links open in the default browser
                            var action = ((WebKit.NavigationPolicyDecision)decision).navigation_action;
                            string uri = action.get_request ().uri;
                            try {
                                Gtk.show_uri (get_screen (), uri, Gtk.get_current_event_time ());
                            } catch (Error error) {
                                critical ("Failed to open %s: %s", uri, error.message);
                            }
                            decision.ignore ();
                            return true;
                        default:
                            return false;
                    }
                });
            }
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
                // Don't add internal or blank pages to trash
                if (tab.item.uri.has_prefix ("internal:") || tab.item.uri.has_prefix ("about:")) {
                    return;
                }
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

        void preferences_activated () {
            new Preferences (this).show ();
        }

        void about_activated () {
            var about = new About (this);
            about.run ();
            about.close ();
        }
    }
}
