/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    [GtkTemplate (ui = "/ui/clear-private-data.ui")]
    class ClearPrivateData : Gtk.Dialog {
        [GtkChild]
        Gtk.ComboBoxText timerange;
        [GtkChild]
        Gtk.CheckButton history;
        [GtkChild]
        Gtk.CheckButton websitedata;
        [GtkChild]
        Gtk.CheckButton cache;
        public ClearPrivateData (Gtk.Window parent) {
           Object (transient_for: parent,
                   // Adding this property via GtkBuilder doesn't work
                   // "for technical reasons, this property is declared as an integer"
                   use_header_bar: 1);
        }

        public override void show () {
            populate_data.begin ();
            try {
                var database = HistoryDatabase.get_default ();
                ulong handler = 0;
                handler = database.items_changed.connect ((position, added, removed) => {
                    history.sensitive = database.get_n_items () > 0;
                    SignalHandler.disconnect (database, handler);
                });
                history.sensitive = database.get_n_items () > 0;
            } catch (DatabaseError error) {
                debug ("Failed to check history: %s", error.message);
            }
            base.show ();
        }

        async void populate_data (Cancellable? cancellable=null) {
            var manager = WebKit.WebContext.get_default ().website_data_manager;
            try {
                var data = yield manager.fetch (WebKit.WebsiteDataTypes.ALL, cancellable);
                foreach (var website in data) {
                    if (((website.get_types () & WebKit.WebsiteDataTypes.COOKIES) != 0) ||
                        ((website.get_types () & WebKit.WebsiteDataTypes.LOCAL_STORAGE) != 0) ||
                        ((website.get_types () & WebKit.WebsiteDataTypes.WEBSQL_DATABASES) != 0) ||
                        ((website.get_types () & WebKit.WebsiteDataTypes.INDEXEDDB_DATABASES) != 0)) {
                        websitedata.sensitive = true;
                    } else if ((website.get_types () & WebKit.WebsiteDataTypes.DISK_CACHE) != 0) {
                        cache.sensitive = true;
                    }
                }
            } catch (Error error) {
                debug ("Failed to fetch data: %s", error.message);
            }
        }

        public override void response (int response_id) {
            if (response_id == Gtk.ResponseType.OK) {
                var timespan = timerange.active_id == "last-hour"
                  ? TimeSpan.HOUR : TimeSpan.DAY;
                WebKit.WebsiteDataTypes types = 0;
                if (websitedata.active) {
                    types |= WebKit.WebsiteDataTypes.COOKIES;
                    types |= WebKit.WebsiteDataTypes.LOCAL_STORAGE;
                    types |= WebKit.WebsiteDataTypes.WEBSQL_DATABASES;
                    types |= WebKit.WebsiteDataTypes.INDEXEDDB_DATABASES;
                }
                if (types != 0) {
                    var manager = WebKit.WebContext.get_default ().website_data_manager;
                    manager.clear.begin (types, timespan, null);
                }
                if (history.active) {
                    // Note: TimeSpan is defined in microseconds
                    int64 age = new DateTime.now_local ().to_unix () - timespan / 1000000;
                    try {
                        HistoryDatabase.get_default ().clear.begin (age);
                    } catch (DatabaseError error) {
                        debug ("Failed to clear history: %s", error.message);
                    }
                }
            }
            close ();
        }
    }
}
