/*
   Copyright (C) 2012 André Stösel <andre@stoesel.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

using Gtk;
using Katze;
using Midori;

namespace DelayedLoad {
    private class PreferencesDialog : Dialog {
        protected Manager dl_manager;
        protected Scale slider;

        public PreferencesDialog (Manager manager) {
            this.dl_manager = manager;

            this.title = _("Preferences for %s").printf ( _("Delayed load"));
            if (this.get_class ().find_property ("has-separator") != null)
                this.set ("has-separator", false);
            this.border_width = 5;
            this.set_modal (true);
            this.set_default_size (350, 100);
            this.create_widgets ();

            this.response.connect (response_cb);
        }

        private void response_cb (Dialog source, int response_id) {
            switch (response_id) {
                case ResponseType.APPLY:
                    this.dl_manager.set_integer ("delay", (int) (this.slider.get_value () * 1000));
                    this.dl_manager.preferences_changed ();
                    this.destroy ();
                    break;
                case ResponseType.CANCEL:
                    this.destroy ();
                    break;
            }
        }

        private void create_widgets () {
            Label text = new Label ("%s:".printf (_("Delay in seconds until loading the page")));
#if HAVE_GTK3
            this.slider = new Scale.with_range (Orientation.HORIZONTAL, 0, 15, 0.1);
#else
            this.slider = new HScale.with_range (0, 15, 0.1);
#endif

            int delay = this.dl_manager.get_integer ("delay");
            if (delay > 0)
                this.slider.set_value ((float)delay / 1000);

#if HAVE_GTK3
            Gtk.Box vbox = get_content_area () as Gtk.Box;
            vbox.pack_start (text, false, false, 0);
            vbox.pack_start (this.slider, false, true, 0);
#else
            this.vbox.pack_start (text, false, false, 0);
            this.vbox.pack_start (this.slider, false, true, 0);
#endif

            this.add_button (Gtk.STOCK_CANCEL, ResponseType.CANCEL);
            this.add_button (Gtk.STOCK_APPLY, ResponseType.APPLY);

            this.show_all ();
        }
    }

    private class TabShaker : GLib.Object {
        public unowned Midori.Browser browser;
        public GLib.PtrArray tasks;

        public bool reload_tab () {
            if (tasks.len == 1) {
                Midori.View? view = browser.tab as Midori.View;
                Midori.View scheduled_view = tasks.index (0) as Midori.View;
                if (scheduled_view == view) {
                    Katze.Item item = view.get_proxy_item ();
                    item.ref();

                    int64 delay = item.get_meta_integer ("delay");
                    if (delay == -2) {
                        view.reload (true);
                    }
                }
            }
            tasks.remove_index (0);
            return false;
        }

        public TabShaker (Midori.Browser browser) {
            this.browser = browser;
        }

        construct {
            this.tasks = new GLib.PtrArray ();
        }
    }

    private class Manager : Midori.Extension {
        private int timeout = 0;
        private bool timeout_handler = false;
        private HashTable<Midori.Browser, TabShaker> tasks;

        public signal void preferences_changed ();

        private void preferences_changed_cb () {
            this.timeout = get_integer ("delay");
        }

        private void show_preferences () {
            PreferencesDialog dialog = new PreferencesDialog (this);
            dialog.show ();
        }

        private void schedule_reload (Midori.Browser browser, Midori.View view) {
            if (this.timeout == 0)
                view.reload (true);
            else {
                unowned TabShaker shaker = tasks.get (browser);
                if (shaker != null) {
                    shaker.tasks.add (view);
                    Timeout.add (this.timeout, shaker.reload_tab);
                }
            }
        }

        private void tab_changed (Midori.View? old_view, Midori.View? new_view) {
            if (new_view != null) {
                Midori.App app = get_app ();
                Midori.Browser browser = app.browser;

                Katze.Item item = new_view.get_proxy_item ();
                item.ref();

                int64 delay = item.get_meta_integer ("delay");
                if (delay == -2 && new_view.progress < 1.0) {
                    this.schedule_reload (browser, new_view);
                }
            }
        }

        private bool reload_first_tab () {
            Midori.App app = get_app ();
            Midori.Browser? browser = app.browser;
            Midori.View? view = browser.tab as Midori.View;

            if (view != null) {
                Katze.Item item = view.get_proxy_item ();
                item.ref();

                int64 delay = item.get_meta_integer ("delay");
                if (delay != 1) {
                    unowned WebKit.WebView web_view = view.get_web_view ();
                    WebKit.LoadStatus load_status = web_view.load_status;
                    if (load_status == WebKit.LoadStatus.FINISHED) {
                        if (this.timeout != 0)
                            this.tasks.set (browser, new TabShaker (browser));

                        if (view.progress < 1.0)
                            this.schedule_reload (browser, view);

                        return false;
                    }
                }
            }

            return true;
        }

        private void browser_added (Midori.Browser browser) {
            browser.switch_tab.connect_after (this.tab_changed);
        }

        private void browser_removed (Midori.Browser browser) {
            browser.switch_tab.disconnect (this.tab_changed);
        }

        public void activated (Midori.App app) {
            /* FIXME: override behavior without changing the preference */
            app.settings.load_on_startup = MidoriStartup.DELAYED_PAGES;

            this.preferences_changed ();

            Midori.Browser? focused_browser = app.browser;
            if (focused_browser == null)
                Timeout.add (50, this.reload_first_tab);

            foreach (Midori.Browser browser in app.get_browsers ()) {
                browser_added (browser);
            }
            app.add_browser.connect (browser_added);
        }

        public void deactivated () {
            Midori.App app = get_app ();
            foreach (Midori.Browser browser in app.get_browsers ()) {
                browser_removed (browser);
            }
            app.add_browser.disconnect (browser_added);
        }

        internal Manager () {
            GLib.Object (name: _("Delayed load"),
                         description: _("Delay page load until you actually use the tab."),
                         version: "0.1",
                         authors: "André Stösel <andre@stoesel.de>");

            install_integer ("delay", 0);

            activate.connect (this.activated);
            deactivate.connect (this.deactivated);
            open_preferences.connect (show_preferences);
            preferences_changed.connect (preferences_changed_cb);

            this.tasks = new HashTable<Midori.Browser, TabShaker> (GLib.direct_hash, GLib.direct_equal);
        }
    }
}

public Midori.Extension extension_init () {
    return new DelayedLoad.Manager ();
}

