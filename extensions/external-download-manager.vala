/*
   Copyright (C) 2012 André Stösel <andre@stoesel.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

using Gtk;
using WebKit;
using Midori;

namespace EDM {
    [DBus (name = "net.launchpad.steadyflow.App")]
    interface SteadyflowInterface : GLib.Object {
        public abstract void AddFile (string url) throws IOError;
    }

    private class Manager : Midori.Extension {
        public bool download_requested (Midori.View view,
            WebKit.Download download, Midori.Browser browser) {
            if (download.get_data<void*> ("save-as-download") == null
             && download.get_data<void*> ("open-download") == null) {
                try {
                    SteadyflowInterface db = Bus.get_proxy_sync (
                        BusType.SESSION,
                        "net.launchpad.steadyflow.App",
                        "/net/launchpad/steadyflow/app");
                    db.AddFile (download.get_uri ());
                    return true;
                } catch (Error e) {
                    stderr.printf("Error: %s\n", e.message);
                }
            }
            return false;
        }

        public void tab_added (Midori.Browser browser, Midori.View view) {
            view.download_requested.connect (download_requested);
        }

        public void tab_removed (Midori.Browser browser, Midori.View view) {
            view.download_requested.disconnect(download_requested);
        }

        public void browser_added (Midori.Browser browser) {
            foreach (var tab in browser.get_tabs ())
                tab_added (browser, tab);
            browser.add_tab.connect (tab_added);
            browser.remove_tab.connect (tab_removed);
        }

        public void browser_removed (Midori.Browser browser) {
            foreach (var tab in browser.get_tabs ())
                tab_removed (browser, tab);
            browser.add_tab.disconnect (tab_added);
            browser.remove_tab.disconnect (tab_removed);
        }

        public void activated (Midori.App app) {
            foreach (var browser in app.get_browsers ())
                browser_added (browser);
            app.add_browser.connect (browser_added);
        }

        public void deactivated () {
            var app = get_app ();
            foreach (var browser in app.get_browsers ())
                browser_removed (browser);
            app.add_browser.disconnect (browser_added);
        }

        internal Manager () {
            GLib.Object (name: _("External Download Manager"),
                         description: _("Download files with SteadyFlow"),
                         version: "0.1" + Midori.VERSION_SUFFIX,
                         authors: "André Stösel <andre@stoesel.de>");

            this.activate.connect (activated);
            this.deactivate.connect (deactivated);
        }
    }
}

public Midori.Extension extension_init () {
    return new EDM.Manager ();
}

