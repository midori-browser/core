/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    [GtkTemplate (ui = "/ui/network-check.ui")]
    public class NetworkCheck : Gtk.ActionBar {
        [GtkChild]
        Gtk.Button login;

        construct {
            login.clicked.connect (login_clicked);

            var monitor = NetworkMonitor.get_default ();
            visible = monitor.connectivity == NetworkConnectivity.PORTAL;
            monitor.notify["connectivity"].connect ((pspec) => {
                visible = monitor.connectivity == NetworkConnectivity.PORTAL;
            });

        }

        void login_clicked () {
            var browser = ((Browser)get_toplevel ());
            browser.add (new Tab (null, browser.web_context, "http://example.com"));
        }
    }
}
