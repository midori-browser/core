/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Adblock {
    public class Settings : Midori.Settings {
        static Settings? _default = null;
        public string default_filters = "https://easylist.to/easylist/easylist.txt&title=EasyList;https://easylist.to/easylist/easyprivacy.txt&title=EasyPrivacy";
        List<Subscription> subscriptions;

        public static Settings get_default () {
            if (_default == null) {
                string filename = Path.build_filename (Environment.get_user_config_dir (),
                    Config.PROJECT_NAME, "extensions", "libadblock.so", "config");
                _default = new Settings (filename);
            }
            return _default;
        }

        Settings (string filename) {
            Object (filename: filename);
            string[] filters = get_string ("settings", "filters", default_filters).split (";");
            foreach (unowned string filter in filters) {
                if (filter == "") {
                    continue;
                }

                string uri = filter;
                if (filter.has_prefix ("http-/")) {
                    uri = "http:" + filter.substring (5);
                } else if (filter.has_prefix ("file-/")) {
                    uri = "file:" + filter.substring (5);
                } else if (filter.has_prefix ("http-:")) {
                    uri = "https" + filter.substring (5);
                }
                add (new Subscription (uri, filter == uri));
            }
            // Always add the default filters in case they were removed
            foreach (unowned string uri in default_filters.split (";")) {
                add (new Subscription (uri));
            }
        }

        public bool enabled { get {
            return !get_boolean ("settings", "disabled", false);
        } set {
            set_boolean ("settings", "disabled", !value, false);
        } }

        /* foreach support */
        public new unowned Subscription? get (uint index) {
            return subscriptions.nth_data (index);
        }
        public uint size { get; private set; }

        public bool contains (Subscription subscription) {
            foreach (unowned Subscription sub in subscriptions) {
                if (sub.file.get_path () == subscription.file.get_path ())
                    return true;
            }
            return false;
        }

        public void add (Subscription sub) {
            if (contains (sub)) {
                return;
            }

            sub.notify["active"].connect (active_changed);
            subscriptions.append (sub);
            size++;
        }

        void active_changed () {
            var filters = new StringBuilder ();
            foreach (unowned Subscription sub in subscriptions) {
                if (sub.uri.has_prefix ("http:") && !sub.active) {
                    filters.append ("http-" + sub.uri.substring (4));
                } else if (sub.uri.has_prefix ("file:") && !sub.active) {
                    filters.append ("file-" + sub.uri.substring (5));
                } else if (sub.uri.has_prefix ("https:") && !sub.active) {
                    filters.append ("http-" + sub.uri.substring (5));
                } else {
                    filters.append (sub.uri);
                }
                filters.append_c (';');
            }

            if (filters.str.has_suffix (";"))
                filters.truncate (filters.len - 1);
            set_string ("settings", "filters", filters.str);
        }

        public void remove (Subscription sub) {
            subscriptions.remove (sub);
            size--;
            sub.notify["active"].disconnect (active_changed);
            active_changed ();
        }
    }
}
