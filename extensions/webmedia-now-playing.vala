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
        DbusService dbus_service { get; set; }
        internal Manager () {
            GLib.Object (name: _("Now-playing extension"),
                         description: _("Share 'youtube, vimeo, dailymotion' that you are playing in Midori using org.midori.mediaHerald"),
                         version: "0.1" + Midori.VERSION_SUFFIX,
                         authors: "James Axl <axlrose112@gmail.com>");
            activate.connect (this.activated);
            deactivate.connect (this.deactivated);
        }

        void youtube_validation (string title, string uri) {
            try {
                    var youtube = new Regex("""(http|https)://www.youtube.com\/watch\?v=[&=_\-A-Za-z0-9.]+""");
                    var vimeo = new Regex("""(http|https)://vimeo.com/[_\-A-Za-z0-9]+""");
                    var dailymotion = new Regex("""(http|https)://www.dailymotion.com/video/[_\-A-Za-z0-9]+""");
                    if (youtube.match(uri)) {
                        dbus_service.video_title = title;
                        dbus_service.video_uri = uri;
                    } else if (vimeo.match(uri)) {
                        dbus_service.video_title = title;
                        dbus_service.video_uri = uri;
                    } else if (dailymotion.match(uri)) {
                        dbus_service.video_title = title;
                        dbus_service.video_uri = uri;
                    } else {
                        dbus_service.video_title = "";
                        dbus_service.video_uri = "";
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
            dbus_service = new DbusService();
            foreach (var browser in app.get_browsers ())
                browser_added (browser);
            app.add_browser.connect (browser_added);
            dbus_service.register_service();
        }

        void deactivated () {
            var app = get_app ();
            app.add_browser.disconnect (browser_added);
            dbus_service.unregister_service();
        }
    }

    [DBus (name = "org.midori.mediaHerald")]
    public class DbusService : Object {
        uint service { get; set; }
        uint own_name_id { get; set; }
        DBusConnection dbus_service_connection { get; set; }
        public string video_title { get; set; }
        public string video_uri { get; set; }

        public DbusService() {
            video_title = "";
            video_uri = "";
        }

        public void register_service() {
            own_name_id = Bus.own_name (BusType.SESSION, "org.midori.mediaHerald", BusNameOwnerFlags.NONE,
                  on_bus_aquired,
                  () => {},
                  () => stderr.printf ("Could not acquire name\n"));
        }

        public void unregister_service() {
            Bus.unown_name(own_name_id);
            dbus_service_connection.unregister_object (service);
        }

        void on_bus_aquired (DBusConnection connection) {
            try {
                dbus_service_connection = connection;
                service = connection.register_object ("/org/midori/mediaHerald", this);
            } catch (IOError e) {
                stderr.printf ("Could not register service\n");
            }
        }
    }
}

public Midori.Extension extension_init () {
    return new Sandcat.Manager ();
}
