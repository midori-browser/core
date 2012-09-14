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
            Midori.DownloadType download_type = download.get_data<Midori.DownloadType> ("midori-download-type");

            if (download_type == Midori.DownloadType.SAVE) {
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

    private class CommandLinePreferences : Dialog {
        protected Entry input;
        protected CommandLine commandline;

        public CommandLinePreferences(CommandLine cl) {
            this.commandline = cl;

            string ext_name;
            this.get ("name",out ext_name);

            this.title = _("Preferences for %s").printf (ext_name);
            if (this.get_class ().find_property ("has-separator") != null)
                this.set ("has-separator", false);
            this.border_width = 5;
            this.set_modal (true);
            this.set_default_size (400, 100);
            this.create_widgets ();

            this.response.connect (response_cb);
        }

        private void response_cb (Dialog source, int response_id) {
            switch (response_id) {
                case ResponseType.APPLY:
                    this.commandline.set_string ("commandline", this.input.get_text ());
                    this.destroy ();
                    break;
                case ResponseType.CANCEL:
                    this.destroy ();
                    break;
            }
        }

        private void create_widgets () {
            Label text = new Label ("%s:".printf (_("Command")));
            this.input = new Entry ();
            this.input.set_text (this.commandline.get_string ("commandline"));


#if HAVE_GTK3
            Gtk.Box vbox = get_content_area () as Gtk.Box;
            vbox.pack_start (text, false, false, 0);
            vbox.pack_start (this.input, false, true, 0);
#else
            this.vbox.pack_start (text, false, false, 0);
            this.vbox.pack_start (this.input, false, true, 0);
#endif

            this.add_button (Gtk.STOCK_CANCEL, ResponseType.CANCEL);
            this.add_button (Gtk.STOCK_APPLY, ResponseType.APPLY);

            this.show_all ();
        }
    }

    private class CommandLine : ExternalDownloadManager {
        private void show_preferences () {
            CommandLinePreferences dialog = new CommandLinePreferences (this);
            dialog.show ();
        }

        public override bool download (DownloadRequest dlReq) {
            try {
                string cmd = this.get_string ("commandline");
                cmd = cmd.replace("{REFERER}", GLib.Shell.quote (dlReq.referer));
                if (dlReq.cookie_header != null) {
                    cmd = cmd.replace("{COOKIES}", GLib.Shell.quote ("Cookie: " + dlReq.cookie_header));
                } else {
                    cmd = cmd.replace("{COOKIES}", "\'\'");
                }
                cmd = cmd.replace("{URL}", GLib.Shell.quote (dlReq.uri));
                GLib.Process.spawn_command_line_async (cmd);
                return true;
            } catch (Error e) {
                this.handle_exception (e);
            }
            return false;
        }

        internal CommandLine () {
            GLib.Object (name: _("External Download Manager - CommandLine"),
                         description: _("Download files with a specified command"),
                         version: "0.1" + Midori.VERSION_SUFFIX,
                         authors: "André Stösel <andre@stoesel.de>",
                         key: "commandline");

            this.install_string ("commandline", "wget --no-check-certificate --referer={REFERER} --header={COOKIES} {URL}");

            this.activate.connect (activated);
            this.deactivate.connect (deactivated);
            this.open_preferences.connect (show_preferences);
        }
    }
}

public Katze.Array extension_init () {
    EDM.manager = new EDM.Manager();

    var extensions = new Katze.Array( typeof (Midori.Extension));
    extensions.add_item (new EDM.Aria2 ());
    extensions.add_item (new EDM.SteadyFlow ());
    extensions.add_item (new EDM.CommandLine ());
    return extensions;
}

