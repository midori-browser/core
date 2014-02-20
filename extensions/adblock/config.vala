/*
 Copyright (C) 2014 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Adblock {
    public class Config : GLib.Object {
        List<Subscription> subscriptions;
        string? path;
        KeyFile keyfile;
        Subscription? custom;

        public Config (string? path) {
            subscriptions = new GLib.List<Subscription> ();

            this.path = path;
            if (path == null)
                return;

            string custom_list = GLib.Path.build_filename (path, "custom.list");
            try {
                custom = new Subscription (Filename.to_uri (custom_list, null));
                subscriptions.append (custom);
            } catch (Error error) {
                custom = null;
                warning ("Failed to add custom list %s: %s", custom_list, error.message);
            }

            string filename = GLib.Path.build_filename (path, "config");
            keyfile = new GLib.KeyFile ();
            try {
                keyfile.load_from_file (filename, GLib.KeyFileFlags.NONE);
                string[] filters = keyfile.get_string_list ("settings", "filters");
                foreach (string filter in filters) {
                    bool active = false;
                    string uri = filter;
                    if (filter.has_prefix ("http-"))
                        uri = "http:" + filter.substring (6);
                    else if (filter.has_prefix ("file-"))
                        uri = "file:" + filter.substring (6);
                    else if (filter.has_prefix ("https-"))
                        uri = "https:" + filter.substring (7);
                    else
                        active = true;
                    Subscription sub = new Subscription (uri);
                    sub.active = active;
                    sub.add_feature (new Updater ());
                    sub.notify["active"].connect (active_changed);
                    subscriptions.append (sub);
                }
            } catch (FileError.NOENT exist_error) {
                /* It's no error if no config file exists */
            } catch (GLib.Error settings_error) {
                warning ("Error reading settings from %s: %s\n", filename, settings_error.message);
            }

            size = subscriptions.length ();
        }

        void active_changed (Object subscription, ParamSpec pspec) {
            var filters = new StringBuilder ();
            foreach (var sub in subscriptions) {
                if (sub == custom)
                    continue;
                if (sub.uri.has_prefix ("http:") && !sub.active)
                    filters.append ("http-" + sub.uri.substring (4));
                else if (sub.uri.has_prefix ("file:") && !sub.active)
                    filters.append ("file-" + sub.uri.substring (4));
                else if (sub.uri.has_prefix ("https:") && !sub.active)
                    filters.append ("https-" + sub.uri.substring (5));
                else
                    filters.append (sub.uri);
                filters.append_c (';');
            }

            string[] list = (filters.str.slice (0, -1)).split (";");
            keyfile.set_string_list ("settings", "filters", list);
            try {
                string filename = GLib.Path.build_filename (path, "config");
                FileUtils.set_contents (filename, keyfile.to_data ());
            } catch (Error error) {
                warning ("Failed to save settings: %s", error.message);
            }
        }

        public void add_custom_rule (string rule) {
            try {
                var file = File.new_for_uri (custom.uri);
                file.append_to (FileCreateFlags.NONE).write (("%s\n".printf (rule)).data);
            } catch (Error error) {
                warning ("Failed to add custom rule: %s", error.message);
            }
        }

        /* foreach support */
        public new Subscription? get (uint index) {
            return subscriptions.nth_data (index);
        }
        public uint size { get; private set; }
    }
}
