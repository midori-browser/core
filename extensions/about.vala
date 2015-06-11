/*
   Copyright (C) 2013 André Stösel <andre@stoesel.de>
   Copyright (C) 2014 Christian Dywan <christian@twotoasts.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

namespace About {
    private abstract class Page : GLib.Object {
        public abstract string uri { get; set; }
        public abstract void get_contents (Midori.View view, string uri);
        protected void load_html (Midori.View view, string content, string uri) {
            #if HAVE_WEBKIT2 
                view.web_view.load_html (content, uri);
            #else
                view.web_view.load_html_string (content, uri);
            #endif
        }
    }

    private class Widgets : Page {
        public override string uri { get; set; default = "about:widgets"; }
        public override void get_contents (Midori.View view, string uri) {
            string[] widgets = {
                "<input value=\"demo\"%s>",
                "<p><input type=\"password\" value=\"demo\"%s></p>",
                "<p><input type=\"checkbox\" value=\"demo\"%s> demo</p>",
                "<p><input type=\"radio\" value=\"demo\"%s> demo</p>",
                "<p><select%s><option>foo bar</option><option selected>spam eggs</option></select></p>",
                "<p><select%s size=\"3\"><option>foo bar</option><option selected>spam eggs</option></select></p>",
                "<p><input type=\"file\"%s></p>",
                "<p><input type=\"file\" multiple%s></p>",
                "<input type=\"button\" value=\"demo\"%s>",
                "<p><input type=\"email\" value=\"user@localhost.com\"%s></p>",
                "<input type=\"url\" value=\"http://www.example.com\"%s>",
                "<input type=\"tel\" value=\"+1 234 567 890\" pattern=\"^[0+][1-9 /-]*$\"%s>",
                "<input type=\"number\" min=1 max=9 step=1 value=\"4\"%s>",
                "<input type=\"range\" min=1 max=9 step=1 value=\"4\"%s>",
                "<input type=\"date\" min=1990-01-01 max=2010-01-01%s>",
                "<input type=\"search\" placeholder=\"demo\"%s>",
                "<textarea%s>Lorem ipsum doloret sit amet…</textarea>",
                "<input type=\"color\" value=\"#d1eeb9\"%s>",
                "<progress min=1 max=9 value=4 %s></progress>",
                "<keygen type=\"rsa\" challenge=\"235ldahlae983dadfar\"%s>",
                "<p><input type=\"reset\"%s></p>",
                "<input type=\"submit\"%s>"
            };

            string content = """<html>
                <head>
                    <style>
                        .fallback::-webkit-slider-thumb,
                        .fallback, .fallback::-webkit-file-upload-button {
                            -webkit-appearance: none !important;
                        }
                        .column {
                            display:inline-block; vertical-align:top;
                            width:25%;
                            margin-right:1%;
                        }
                    </style>
                    <title>%s</title>
                </head>
                <body>
                    <h1>%s</h1>
                    <div class="column">%s</div>
                    <div class="column">%s</div>
                    <div class="column">%s</div>
                    <p><a href="http://example.com" target="wp" onclick="javascript:window.open('http://example.com','wp','width=320, height=240, toolbar=false'); return false;">Popup window</a></p>
                </body>
            </html>""";

            string[] widget_options = {"", " disabled", " class=\"fallback\""};
            string[] widgets_formated = {"", "", ""};

            for (int i = 0; i < widget_options.length && i < widgets_formated.length; i++) {
                for (int j = 0; j < widgets.length; j++) {
                    widgets_formated[i] = widgets_formated[i] + widgets[j].printf (widget_options[i]);
                }
            }

            this.load_html (view, content.printf (uri, uri, widgets_formated[0], widgets_formated[1], widgets_formated[2]), uri);
        }
    }

    private class Version : Page {
        public override string uri { get; set; }
        private GLib.HashTable<string, Page> about_pages;

        public Version (string alias, HashTable<string, Page> about_pages) {
            this.uri = alias;
            this.about_pages = about_pages;
        }

        private string list_about_uris () {
            string links = "";
            foreach (unowned string uri in about_pages.get_keys ())
                links = links + "<a href=\"%s\">%s</a> &nbsp;".printf (uri, uri);
            return "<p>%s</p>".printf (links);
        }

        public override void get_contents (Midori.View view, string uri) {
            string contents = """<html>
                <head><title>about:version</title></head>
                <body>
                    <h1>a<span style="position: absolute; left: -1000px; top: -1000px">lias a=b; echo Copy carefully #</span>bout:version</h1>
                    <p>%s</p>
                    <img src="res://logo-shade.png" style="position: absolute; right: 15px; bottom: 15px; z-index: -9;">
                    <table>
                        <tr><td>Command line %s</td></tr>
                        %s
                        <tr><td>Platform %s %s %s</td></tr>
                        <tr><td>Identification %s</td></tr>
                        %s
                    </table>
                    <table>
                        %s
                    </table>
                    %s
                </body>
            </html>""";

            GLib.StringBuilder versions = new GLib.StringBuilder ();
            Midori.View.list_versions (versions, true);

            string ident; 
            unowned string architecture;
            unowned string platform;
            unowned string sys_name = Midori.WebSettings.get_system_name (out architecture, out platform);
            view.settings.get ("user-agent", out ident);

            GLib.StringBuilder video_formats = new GLib.StringBuilder ();
            view.list_video_formats (video_formats, true);

            GLib.StringBuilder ns_plugins = new GLib.StringBuilder ();
            view.list_plugins (ns_plugins, true);

            /* TODO: list active extensions */

            this.load_html (view, contents.printf (
                _("Version numbers in brackets show the version used at runtime."),
                Midori.Paths.get_command_line_str (true),
                versions.str,
                platform, sys_name, architecture != null ? architecture : "",
                ident,
                video_formats.str,
                ns_plugins.str,
                this.list_about_uris ()
            ), uri);
        }
    }

    private class Private : Page {
        public override string uri { get; set; default = "about:private"; }
        public override void get_contents (Midori.View view, string uri) {
            this.load_html (view, """<html dir="ltr">
                <head>
                    <title>%s</title>
                    <link rel="stylesheet" type="text/css" href="res://about.css">
                </head>
                <body>
                    <img id="logo" src="res://logo-shade.png" />
                    <div id="main" style="background-image: url(stock://dialog/gtk-dialog-info);">
                    <div id="text">
                    <h1>%s</h1>
                    <p class="message">%s</p><ul class=" suggestions"><li>%s</li><li>%s</li><li>%s</li></ul>
                    <p class="message">%s</p><ul class=" suggestions"><li>%s</li><li>%s</li><li>%s</li><li>%s</li></ul>
                    </div><br style="clear: both"></div>
                </body>
            </html>""".printf (
                _("Private Browsing"), _("Private Browsing"),
                _("Midori doesn't store any personal data:"),
                _("No history or web cookies are being saved."),
                _("Extensions are disabled."),
                _("HTML5 storage, local database and application caches are disabled."),
                _("Midori prevents websites from tracking the user:"),
                _("Referrer URLs are stripped down to the hostname."),
                _("DNS prefetching is disabled."),
                _("The language and timezone are not revealed to websites."),
                _("Flash and other Netscape plugins cannot be listed by websites.")
            ), uri);
        }
    }

    private class Paths : Page {
        public override string uri { get; set; default = "about:paths"; }
        public override void get_contents (Midori.View view, string uri) {
            string res_dir = Midori.Paths.get_res_filename ("about.css");
            string lib_dir = Midori.Paths.get_lib_path (PACKAGE_NAME);
            this.load_html (view, """<html>
                <body>
                    <h1>%s</h1>
                    <p>config: <code>%s</code></p>
                    <p>res: <code>%s</code></p>
                    <p>data: <code>%s/%s</code></p>
                    <p>lib: <code>%s</code></p>
                    <p>cache: <code>%s</code></p>
                    <p>tmp: <code>%s</code></p>
                </body>
            </html>""".printf (
                uri, Midori.Paths.get_config_dir_for_reading (), res_dir,
                Midori.Paths.get_user_data_dir_for_reading (), PACKAGE_NAME,
                lib_dir, Midori.Paths.get_cache_dir_for_reading (), Midori.Paths.get_tmp_dir ()
            ), uri);
        }
    }

    private class Dial : Page {
        public override string uri { get; set; default = "about:dial"; }
        public override void get_contents (Midori.View view, string uri) {
            var browser = Midori.Browser.get_for_widget (view);
            Midori.SpeedDial dial;
            browser.get ("speed-dial", out dial);
            if (dial == null)
                return;
            try {
                this.load_html (view, dial.get_html (), uri);
            } catch (Error error) {
                this.load_html (view, error.message, uri);
            }
        }
    }

    private class Geolocation : Page {
        public override string uri { get; set; default = "about:geolocation"; }
        public override void get_contents (Midori.View view, string uri) {
            this.load_html (view, """<html>
                <body>
                    <a href="http://dev.w3.org/geo/api/spec-source.html" id="method"></a>
                    <span id="locationInfo"><noscript>No Geolocation without Javascript</noscript></span>
                    <script>
                        function displayLocation (position) {
                            var geouri = 'geo:' + position.coords.latitude + ',' + position.coords.longitude + ',' + position.coords.altitude + ',u=' + position.coords.accuracy;
                            document.getElementById('locationInfo').innerHTML = '<a href="' + geouri + '">' + geouri + '</a><br><code>'
                                + ' timestamp: ' + position.timestamp
                                + ' latitude: ' + position.coords.latitude
                                + ' longitude: ' + position.coords.longitude
                                + ' altitude: ' + position.coords.altitude + '<br>'
                                + ' accuracy: ' + position.coords.accuracy
                                + ' altitudeAccuracy: ' + position.coords.altitudeAccuracy
                                + ' heading: ' + position.coords.heading
                                + ' speed: ' + position.coords.speed
                                + '</code>';
                            }
                            function handleError (error) {
                                var errorMessage = '<b>' + ['Unknown error', 'Permission denied', 'Position failed', 'Timed out'][error.code] + '</b>';
                                if (error.code == 3) document.getElementById('locationInfo').innerHTML += (' ' + errorMessage);
                                else document.getElementById('locationInfo').innerHTML = errorMessage;
                            }
                            if (navigator.geolocation) {
                                var options = { enableHighAccuracy: true, timeout: 60000, maximumAge: "Infinite" };
                                if (navigator.geolocation.watchPosition) {
                                    document.getElementById('method').innerHTML = '<code>geolocation.watchPosition</code>:';
                                    navigator.geolocation.watchPosition(displayLocation, handleError, options);
                                } else {
                                    document.getElementById('method').innerHTML = '<code>geolocation.getCurrentPosition</code>:';
                                    navigator.geolocation.getCurrentPosition(displayLocation, handleError);
                                }
                            } else
                                document.getElementById('locationInfo').innerHTML = 'Geolocation unavailable';
                    </script>
                </body>
            </html>""", uri);
        }
    }

    private class Redirects : Page {
        public override string uri { get; set; }
        private string property;
        public Redirects (string alias, string property) {
            this.uri = alias;
            this.property = property;
        }
        public override void get_contents (Midori.View view, string uri) {
            string new_uri = uri;
            view.settings.get (property, out new_uri);
            if (uri == "about:search")
                new_uri = Midori.URI.for_search (new_uri, "");
            view.set_uri (new_uri);
        }
    }

    private class Manager : Midori.Extension {
        private GLib.HashTable<string, Page>? about_pages;

        private void register (Page page) {
            this.about_pages.insert (page.uri, page);
        }

        private bool about_content (Midori.View view, string uri) {
            unowned Page? page = this.about_pages.get (uri);
            if (page != null) {
                page.get_contents (view, uri);
                return true;
            }

            return false;
        }

        private void tab_added (Midori.Browser browser, Midori.View view) {
            view.about_content.connect (this.about_content);
        }

        private void tab_removed (Midori.Browser browser, Midori.View view) {
            view.about_content.disconnect (this.about_content);
        }

        private void browser_added (Midori.Browser browser) {
            foreach (Midori.View tab in browser.get_tabs ()) {
                this.tab_added (browser, tab);
            }
            browser.add_tab.connect (this.tab_added);
            browser.remove_tab.connect (this.tab_removed);
        }

        private void browser_removed (Midori.Browser browser) {
            foreach (Midori.View tab in browser.get_tabs ()) {
                this.tab_removed (browser, tab);
            }
            browser.add_tab.disconnect (this.tab_added);
            browser.remove_tab.disconnect (this.tab_removed);
        }

        public void activated (Midori.App app) {
            this.about_pages = new GLib.HashTable<string, Page> (GLib.str_hash, GLib.str_equal);
            register (new Widgets ());
            register (new Version ("about:", about_pages));
            register (new Version ("about:version", about_pages));
            register (new Private ());
            register (new Paths ());
            register (new Geolocation ());
            register (new Redirects ("about:new", "tabhome"));
            register (new Redirects ("about:home", "homepage"));
            register (new Redirects ("about:search", "location-entry-search"));
            register (new Dial ());

            foreach (Midori.Browser browser in app.get_browsers ()) {
                this.browser_added (browser);
            }
            app.add_browser.connect (this.browser_added);
        }

        public void deactivated () {
            Midori.App app = this.get_app ();
            foreach (Midori.Browser browser in app.get_browsers ()) {
                this.browser_removed (browser);
            }
            app.add_browser.disconnect (this.browser_added);

            this.about_pages = null;
        }

        internal Manager () {
            GLib.Object (name: "About pages",
                         description: "Internal about: handler",
                         version: "0.1",
                         authors: "André Stösel <andre@stoesel.de>");

            this.activate.connect (this.activated);
            this.deactivate.connect (this.deactivated);
        }
    }
}

public Midori.Extension extension_init () {
    return new About.Manager ();
}

