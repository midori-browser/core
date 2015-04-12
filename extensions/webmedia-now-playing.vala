/*
 Copyright (C) 2014 James Axl <axlrose112@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace WebMedia {

    private class Manager : Midori.Extension {
        DbusService dbus_service { get; set; }
        WebMediaNotify web_media_notify { get; set; }
        string web_media_uri { get; set; }
        string web_media_title { get; set; }
        internal Manager () {
            GLib.Object (name: _("Webmedia now-playing"),
                         description: _("Share 'youtube, vimeo, dailymotion, coub and zippcast' that you are playing in Midori using org.midori.mediaHerald"),
                         version: "0.1" + Midori.VERSION_SUFFIX,
                         authors: "James Axl <axlrose112@gmail.com>");
            activate.connect (this.activated);
            deactivate.connect (this.deactivated);
        }

        void youtube_validation (Object object,  ParamSpec pspec) {
            var browser = object as Midori.Browser;
            if(browser.uri == browser.title || browser.uri.contains(browser.title)) return;
            if (web_media_uri == browser.uri) return;
            if (web_media_title == browser.title) return;
            web_media_uri = browser.uri;
            web_media_title = browser.title;
            try { 
                    var youtube = new Regex("""(http|https)://www.youtube.com/watch\?v=[&=_\-A-Za-z0-9.\|]+""");
                    var vimeo = new Regex("""(http|https)://vimeo.com/[0-9]+""");
                    var dailymotion = new Regex("""(http|https)://www.dailymotion.com/video/[_\-A-Za-z0-9]+""");
                    var coub = new Regex("""(http|https)://coub.com/view/[&=_\-A-Za-z0-9.\|]+""");
                    var zippcast = new Regex("""(http|https)://www.zippcast.com/video/[&=_\-A-Za-z0-9.\|]+"""); 
                    
                    string website = null;
                    if (web_media_uri.contains("youtube") || web_media_uri.contains("vimeo") || 
						web_media_uri.contains ("dailymotion") || web_media_uri.contains ("coub") || 
						web_media_uri.contains ("zippcast") ) {
                        if (youtube.match(web_media_uri))
                            website = "Youtube";
                        else if (vimeo.match(web_media_uri))
                            website = "Vimeo";
                        else if (dailymotion.match(web_media_uri))
                            website = "Dailymotion";
                        else if (coub.match(web_media_uri))
                            website = "Coub";
                        else if (zippcast.match(web_media_uri))
                            website = "ZippCast";   

                        if (website != null) {
                            dbus_service.video_title = web_media_title;
                            web_media_notify.notify_media = website;
                            web_media_notify.notify_video_title = web_media_title;
                            dbus_service.video_uri = web_media_uri;
                            web_media_notify.show_notify();
                        }
                    }
            } catch(RegexError e) {
                    warning ("%s", e.message);
            }
        }

        void browser_added (Midori.Browser browser) {
            browser.notify["title"].connect (youtube_validation);
        }

        void activated (Midori.App app) {
            dbus_service = new DbusService();
            web_media_notify = new WebMediaNotify(app);
            foreach (var browser in app.get_browsers ())
                browser_added (browser);
            app.add_browser.connect (browser_added);
            dbus_service.register_service();
        }

        void deactivated () {
            var app = get_app ();
            app.add_browser.disconnect (browser_added);
            dbus_service.unregister_service();
            foreach (var browser in app.get_browsers ())
                browser.notify["title"].disconnect (youtube_validation);
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
            dbus_empty();
        }
        
        public void dbus_empty() {
            video_title = null;
            video_uri = null;
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

    public class WebMediaNotify : Object {
        public Midori.App app { get; set; }
        public string notify_video_title { get; set; }
        public string notify_media { get; set; }

        public WebMediaNotify (Midori.App app) {
            Object (app: app);
        }

        public void show_notify () {
            app.send_notification ("Midori is playing in " + notify_media, notify_video_title);
        }
    }
}

public Midori.Extension extension_init () {
    return new WebMedia.Manager ();
}
