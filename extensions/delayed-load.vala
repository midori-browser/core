/*
   Copyright (C) 2012 André Stösel <andre@stoesel.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

using Katze;
using Midori;

namespace DelayedLoad {
    private class Manager : Midori.Extension {
        private void tab_changed (GLib.Object window, GLib.ParamSpec pspec) {
            Midori.Browser browser = window as Midori.Browser;
            Midori.View? view = browser.tab as Midori.View;

            Katze.Item item = view.get_proxy_item ();
            item.ref();

            int64 delay = item.get_meta_integer ("delay");
            if (delay == -2 && view.can_reload ()) {
                view.reload (true);
            }
        }

        private bool reload_first_tab () {
            Midori.App app = get_app ();
            Midori.Browser? browser = app.browser;
            Midori.View? view = browser.tab as Midori.View;

            Katze.Item item = view.get_proxy_item ();
            item.ref();

            int64 delay = item.get_meta_integer ("delay");
            if (delay != 1) {
                unowned WebKit.WebView web_view = view.get_web_view ();
                WebKit.LoadStatus load_status = web_view.load_status;
                if (load_status == WebKit.LoadStatus.FINISHED) {
                    if (view.can_reload ())
                        view.reload (false);
                    return false;
                }
            }

            return true;
        }

        private void browser_added (Midori.Browser browser) {
            browser.notify["tab"].connect (this.tab_changed);
        }

        private void browser_removed (Midori.Browser browser) {
            browser.notify["tab"].disconnect (this.tab_changed);
        }

        public void activated (Midori.App app) {
            /* FIXME: override behavior without changing the preference */
            app.settings.load_on_startup = MidoriStartup.DELAYED_PAGES;

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

            activate.connect (this.activated);
            deactivate.connect (this.deactivated);
        }
    }
}

public Midori.Extension extension_init () {
    return new DelayedLoad.Manager ();
}

