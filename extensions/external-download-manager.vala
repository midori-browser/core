/*
   Copyright (C) 2012 André Stösel <andre@stoesel.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

using Gtk;
using Soup;
using Katze;
using Midori;
using WebKit;

namespace EDM {
    [DBus (name = "net.launchpad.steadyflow.App")]
    interface SteadyflowInterface : GLib.Object {
        public abstract void AddFile (string url) throws IOError;
    }

    private class DownloadRequest : GLib.Object {
        public string uri;
        public string auth;
        public string referer;
        public string? cookie_header;
    }

    internal Manager manager;

    private class Manager : GLib.Object {
        private CookieJar cookie_jar;
        private GLib.PtrArray download_managers =  new GLib.PtrArray ();

        public bool download_requested (Midori.View view, WebKit.Download download) {
            if (download.get_data<void*> ("save-as-download") == null
             && download.get_data<void*> ("open-download") == null
             && download.get_data<void*> ("cancel-download") == null) {
                var dlReq = new DownloadRequest ();
                dlReq.uri = download.get_uri ();

                var request = download.get_network_request ();
                var message = request.get_message ();
                weak MessageHeaders headers = message.request_headers;

                dlReq.auth = headers.get ("Authorization");
                dlReq.referer = headers.get ("Referer");
                dlReq.cookie_header = this.cookie_jar.get_cookies (new Soup.URI (dlReq.uri), true);

                for (var i = 0 ; i < download_managers.len; i++) {
                    var dm = download_managers.index (i) as ExternalDownloadManager;
                    if (dm.download (dlReq))
                        return true;
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

        public void activated (Midori.Extension extension, Midori.App app) {
            this.download_managers.add (extension);
            if (this.download_managers.len == 1) {
                foreach (var browser in app.get_browsers ())
                    browser_added (browser);
                app.add_browser.connect (browser_added);
            }
        }

        public void deactivated (Midori.Extension extension) {
            this.download_managers.remove (extension);
            if (this.download_managers.len == 0) {
                var app = extension.get_app ();
                foreach (var browser in app.get_browsers ())
                    browser_removed (browser);
                app.add_browser.disconnect (browser_added);
            }
        }

        construct {
            var session = WebKit.get_default_session ();
            this.cookie_jar = session.get_feature (typeof (CookieJar)) as CookieJar;
        }
    }

    private abstract class ExternalDownloadManager : Midori.Extension {
        public void activated (Midori.App app) {
            manager.activated (this, app);
        }

        public void deactivated () {
            manager.deactivated (this);
        }

        public void handle_exception (GLib.Error error) {
            string ext_name;
            this.get ("name",out ext_name);
            var dialog = new MessageDialog (null, DialogFlags.MODAL,
                MessageType.ERROR, ButtonsType.CLOSE,
                _("An error occurred when attempting to download a file with the following plugin:\n" +
                  "%s\n\n" +
                  "Error:\n%s\n\n" +
                  "Carry on without this plugin."
                  ),
                ext_name, error.message);
            dialog.response.connect ((a) => { dialog.destroy (); });
            dialog.run ();
        }

        public abstract bool download (DownloadRequest dlReq);
    }

    private class Aria2 : ExternalDownloadManager {
        public override bool download (DownloadRequest dlReq) {
            var url = value_array_new ();
            value_array_insert (url, 0, typeof (string), dlReq.uri);

            GLib.HashTable<string, GLib.Value?> options = value_hash_new ();
            var referer = new GLib.Value (typeof (string));
            referer.set_string (dlReq.referer);
            options.insert ("referer", referer);

            var headers = value_array_new ();
            if (dlReq.cookie_header != null) {
                value_array_insert (headers, 0, typeof (string), "Cookie: %s".printf(dlReq.cookie_header));
            }

            if (headers.n_values > 0)
               options.insert ("header", headers);

            var message = XMLRPC.request_new ("http://127.0.0.1:6800/rpc",
                "aria2.addUri",
                typeof (ValueArray), url,
                typeof(HashTable), options);
            var session = new SessionSync ();
            session.send_message (message);

            try {
                Value v;
                XMLRPC.parse_method_response ((string) message.response_body.flatten ().data, -1, out v);
                return true;
            } catch (Error e) {
                this.handle_exception (e);
            }

            return false;
        }

        internal Aria2 () {
            GLib.Object (name: _("External Download Manager - Aria2"),
                         description: _("Download files with Aria2"),
                         version: "0.1" + Midori.VERSION_SUFFIX,
                         authors: "André Stösel <andre@stoesel.de>",
                         key: "aria2");

            this.activate.connect (activated);
            this.deactivate.connect (deactivated);
        }
    }

    private class SteadyFlow : ExternalDownloadManager {
        public override bool download (DownloadRequest dlReq) {
            try {
                SteadyflowInterface dm = Bus.get_proxy_sync (
                    BusType.SESSION,
                    "net.launchpad.steadyflow.App",
                    "/net/launchpad/steadyflow/app");
                dm.AddFile (dlReq.uri);
                return true;
            } catch (Error e) {
                this.handle_exception (e);
            }
            return false;
        }

        internal SteadyFlow () {
            GLib.Object (name: _("External Download Manager - SteadyFlow"),
                         description: _("Download files with SteadyFlow"),
                         version: "0.1" + Midori.VERSION_SUFFIX,
                         authors: "André Stösel <andre@stoesel.de>",
                         key: "steadyflow");

            this.activate.connect (activated);
            this.deactivate.connect (deactivated);
        }
    }
}

public Katze.Array extension_init () {
    EDM.manager = new EDM.Manager();

    var extensions = new Katze.Array( typeof (Midori.Extension));
    extensions.add_item (new EDM.Aria2 ());
    extensions.add_item (new EDM.SteadyFlow ());
    return extensions;
}

