/*
   Copyright (C) 2012-2013 André Stösel <andre@stoesel.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

namespace DelayedLoad {
    private class Manager : Midori.Extension {
        private void tab_changed (Midori.View? old_view, Midori.View? new_view) {
            if (new_view != null) {
                unowned Katze.Item item = new_view.get_proxy_item ();

                int64 delay = item.get_meta_integer ("delay");
                if (delay == Midori.Delay.PENDING_UNDELAY && new_view.progress < 1.0) {
                    new_view.reload (true);
                }
            }
        }

        private void browser_added (Midori.Browser browser) {
            browser.switch_tab.connect_after (this.tab_changed);
        }

        private void browser_removed (Midori.Browser browser) {
            browser.switch_tab.disconnect (this.tab_changed);
        }

        public void activated (Midori.App app) {
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
                         version: "0.2",
                         authors: "André Stösel <andre@stoesel.de>");

            activate.connect (this.activated);
            deactivate.connect (this.deactivated);
        }
    }
}

public Midori.Extension extension_init () {
    return new DelayedLoad.Manager ();
}

