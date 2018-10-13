/*
 Copyright (C) 2018 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class CoreSettings : Settings {
        static CoreSettings? _default = null;

        public static CoreSettings get_default () {
            if (_default == null) {
                string filename = Path.build_filename (Environment.get_user_config_dir (),
                    Environment.get_prgname (), "config");
                _default = new CoreSettings (filename);
            }
            return _default;
        }

        CoreSettings (string filename) {
            Object (filename: filename);
        }

        internal bool get_plugin_enabled (string plugin) {
            return get_boolean ("extensions", "lib%s.so".printf (plugin));
        }

        internal void set_plugin_enabled (string plugin, bool enabled) {
            set_boolean ("extensions", "lib%s.so".printf (plugin), enabled);
        }

        public bool enable_spell_checking { get {
            return get_boolean ("settings", "enable-spell-checking", true);
        } set {
            set_boolean ("settings", "enable-spell-checking", value, true);
        } }
        public bool enable_javascript { get {
            return get_boolean ("settings", "enable-javascript", true);
        } set {
            set_boolean ("settings", "enable-javascript", value, true);
        } }

        public bool close_buttons_on_tabs { get {
            return get_boolean ("settings", "close-buttons-on-tabs", true);
        } set {
            set_boolean ("settings", "close-buttons-on-tabs", value, true);
        } }
    }

    public class Settings : Object {
        KeyFile? keyfile = new KeyFile ();

        public string filename { get; construct set; }

        construct {
            load ();
        }

        void load () {
            try {
                keyfile.load_from_file (filename, KeyFileFlags.NONE);
            } catch (FileError.NOENT error) {
                /* It's no error if no config file exists */
            } catch (Error error) {
                critical ("Failed to load settings from %s: %s", filename, error.message);
            }
        }

        void save () {
            try {
                keyfile.save_to_file (filename);
            } catch (Error error) {
                critical ("Failed to save settings to %s: %s", filename, error.message);
            }
        }

        public void set_boolean (string group, string key, bool value, bool default=false) {
            if (value != get_boolean (group, key, default)) {
                if (value != default) {
                    keyfile.set_boolean (group, key, value);
                } else {
                    try {
                        keyfile.remove_key (group, key);
                    } catch (KeyFileError error) {
                        warn_if_reached ();
                    }
                }
                save ();
            }
        }

        public bool get_boolean (string group, string key, bool default=false) {
            try {
                return keyfile.get_boolean (group, key);
            } catch (KeyFileError.KEY_NOT_FOUND error) {
                /* It's no error if a key is missing */
            } catch (KeyFileError.GROUP_NOT_FOUND error) {
                /* It's no error if a group is missing */
            } catch (KeyFileError error) {
                warn_if_reached ();
            }
            return default;
        }
    }
}
