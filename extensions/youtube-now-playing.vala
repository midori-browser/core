/*
 Copyright (C) 2014 James Axl <axlrose112@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/
namespace Sandcat {
    private class Manager : Midori.Extension {
    DemoServer dbus_service { get; set; }
    uint service { get; set; }
    DBusConnection connection { get; set; }
        internal Manager () {
            GLib.Object (name: _("Youtube now-playing extension"),
                         description: _("Share the video that you are playing in Midori using DBUS org.web.midori"),
                         version: "0.1" + Midori.VERSION_SUFFIX,
                         authors: "James Axl <axlrose112@gmail.com>");
            activate.connect (this.activated);
            deactivate.connect (this.deactivated);
        }

        void on_bus_aquired (DBusConnection conn) {
            try {
                connection = conn;
                service = connection.register_object ("/org/web/midori", dbus_service);
            } catch (IOError e) {
                stderr.printf ("Could not register service\n");
            }
        }
        
        void unregister () {
                connection.unregister_object (service);
        }

        void youtube_validation (string title, string uri) {
            try {
                    var reg = new Regex("""(http|https)://www.youtube.com\/watch\?v=_*-*\w+""");
                    if (reg.match(uri)) {
                        dbus_service.youtube_title = title;
                        dbus_service.youtube_uri = uri;
                    } else {
                        dbus_service.youtube_title = "";
                        dbus_service.youtube_uri= "";
                    }
                } catch(RegexError e) {
                    warning ("%s", e.message);
                }
        }

        void browser_added (Midori.Browser browser) {
            browser.notify["title"].connect (() => {
                youtube_validation(browser.title, browser.uri);
            });
        }

        void activated (Midori.App app) {
            dbus_service = new DemoServer();
            foreach (var browser in app.get_browsers ())
                browser_added (browser);
            app.add_browser.connect (browser_added);

            Bus.own_name (BusType.SESSION, "org.web.midori", BusNameOwnerFlags.NONE,
                  on_bus_aquired,
                  () => {},
                  () => stderr.printf ("Could not acquire name\n"));
            
        }

        void deactivated () {
            var app = get_app ();
            app.add_browser.disconnect (browser_added);
            unregister();
        }
    }

    [DBus (name = "org.web.midori")]
    public class DemoServer : Object {
        public string youtube_title { get; set; }
        public string youtube_uri { get; set; }
        
        public DemoServer() {
			youtube_title = "";
			youtube_uri = "";
		}
    }
}

public Midori.Extension extension_init () {
    return new Sandcat.Manager ();
}
