/*
 Copyright (C) 2018-2019 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public interface ClearPrivateDataActivatable : Object {
        public abstract Gtk.Box box { owned get; set; }
        public abstract void activate ();
        public async abstract void clear (TimeSpan timespan);
    }

    [GtkTemplate (ui = "/ui/clear-private-data.ui")]
    public class ClearPrivateData : Gtk.Dialog {
        [GtkChild]
        Gtk.ComboBoxText timerange;
        [GtkChild]
        Gtk.CheckButton history;
        [GtkChild]
        Gtk.CheckButton websitedata;
        [GtkChild]
        Gtk.CheckButton cache;

        Cancellable? show_cancellable = null;
        Peas.ExtensionSet extensions;

        public ClearPrivateData (Gtk.Window parent) {
           Object (transient_for: parent);
        }

        construct {
            if (get_settings ().gtk_dialogs_use_header) {
                title = null;
                // "for technical reasons, this property is declared as an integer"
                use_header_bar = 1;
            }
        }

        public override void show () {
            show_cancellable = new Cancellable ();

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

            extensions = Plugins.get_default ().plug<ClearPrivateDataActivatable> ("box", history.parent);
            extensions.extension_added.connect ((info, extension) => { ((ClearPrivateDataActivatable)extension).activate (); });
            extensions.foreach ((extensions, info, extension) => { extensions.extension_added (info, extension); });

            base.show ();
        }

        public override void response (int response_id) {
            show_cancellable.cancel ();
            response_async.begin (response_id);
        }

        async void response_async (int response_id) {
            if (response_id == Gtk.ResponseType.OK) {
                // The ID is the number of days as a string; 0 means everything
                var timespan = timerange.active_id.to_int () * TimeSpan.DAY;
                WebKit.WebsiteDataTypes types = 0;
                if (websitedata.active) {
                    types |= WebKit.WebsiteDataTypes.COOKIES;
                    types |= WebKit.WebsiteDataTypes.LOCAL_STORAGE;
                    types |= WebKit.WebsiteDataTypes.WEBSQL_DATABASES;
                    types |= WebKit.WebsiteDataTypes.INDEXEDDB_DATABASES;
                }
                if (cache.active) {
                    types |= WebKit.WebsiteDataTypes.DISK_CACHE;
                }
                if (types != 0) {
                    var manager = ((Browser)get_ancestor (typeof (Browser))).web_context.website_data_manager;
                    try {
                        yield manager.clear (types, timespan, null);
                    } catch (Error error) {
                        critical ("Failed to clear website data: %s", error.message);
                    }
                }
                if (history.active) {
                    try {
                        yield HistoryDatabase.get_default ().clear (timespan);
                    } catch (DatabaseError error) {
                        critical ("Failed to clear history: %s", error.message);
                    }
                }

                var active_extensions = new List<Peas.Extension> ();
                extensions.foreach ((extensions, info, extension) => { active_extensions.append (extension); });
                foreach (var extension in active_extensions) {
                    yield ((ClearPrivateDataActivatable)extension).clear (timespan);
                }
            }
            close ();
        }
    }
}
