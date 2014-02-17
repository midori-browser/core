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

        public Config (string? path) {
            subscriptions = new GLib.List<Subscription> ();

            this.path = path;
            if (path == null)
                return;

            string custom_list = GLib.Path.build_filename (path, "custom.list");
            try {
                subscriptions.append (new Subscription (Filename.to_uri (custom_list, null)));
            } catch (Error error) {
                warning ("Failed to add custom list %s: %s", custom_list, error.message);
            }

            string filename = GLib.Path.build_filename (path, "config");
            keyfile = new GLib.KeyFile ();
            try {
                keyfile.load_from_file (filename, GLib.KeyFileFlags.NONE);
                string[] filters = keyfile.get_string_list ("settings", "filters");
                foreach (string filter in filters) {
                    Subscription sub = new Subscription (filter);
                    sub.add_feature (new Updater ());
                    subscriptions.append (sub);
                }
            } catch (FileError.NOENT exist_error) {
                /* It's no error if no config file exists */
            } catch (GLib.Error settings_error) {
                stderr.printf ("Error reading settings from %s: %s\n", filename, settings_error.message);
            }

            size = subscriptions.length ();
        }

        /* foreach support */
        public new Subscription? get (uint index) {
            return subscriptions.nth_data (index);
        }
        public uint size { get; private set; }
    }
}
