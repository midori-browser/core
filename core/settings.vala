/*
 Copyright (C) 2018 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    [CCode (cprefix = "MIDORI_STARTUP_")]
    public enum StartupType {
        SPEED_DIAL,
        HOMEPAGE,
        LAST_OPEN_PAGES,
        DELAYED_PAGES
    }

    [CCode (cprefix = "MIDORI_PROXY_")]
    public enum ProxyType {
        AUTOMATIC,
        HTTP,
        NONE,
    }

    public class CoreSettings : Settings {
        static CoreSettings? _default = null;

        public static CoreSettings get_default () {
            if (_default == null) {
                string filename = Path.build_filename (Environment.get_user_config_dir (),
                    Config.PROJECT_NAME, "config");
                _default = new CoreSettings (filename);
            }
            return _default;
        }

        CoreSettings (string filename) {
            Object (filename: filename);
        }

        public bool get_plugin_enabled (string plugin) {
            return get_boolean ("extensions", "lib%s.so".printf (plugin));
        }

        public void set_plugin_enabled (string plugin, bool enabled) {
            set_boolean ("extensions", "lib%s.so".printf (plugin), enabled);
        }

        // Note: Speed Dial is saved as Blank Page for compatibility reasons
        public StartupType load_on_startup { get {
            var startup = get_string ("settings", "load-on-startup", "MIDORI_STARTUP_LAST_OPEN_PAGES");
            if (startup.has_suffix ("BLANK_PAGE")) {
                return StartupType.SPEED_DIAL;
            } else if (startup.has_suffix ("HOMEPAGE")) {
                return StartupType.HOMEPAGE;
            } else {
                return StartupType.LAST_OPEN_PAGES;
            }
        } set {
            var startup = value == StartupType.SPEED_DIAL ? "MIDORI_STARTUP_BLANK_PAGE" : value.to_string ();
            set_string ("settings", "load-on-startup", startup, "MIDORI_STARTUP_LAST_OPEN_PAGES");
        } }

        public bool enable_spell_checking { get {
            return get_boolean ("settings", "enable-spell-checking", true);
        } set {
            set_boolean ("settings", "enable-spell-checking", value, true);
        } }
        public bool auto_load_images { get {
            return get_boolean ("settings", "auto-load-images", true);
        } set {
            set_boolean ("settings", "auto-load-images", value, true);
        } }
        public bool enable_javascript { get {
            return get_boolean ("settings", "enable-javascript", true);
        } set {
            set_boolean ("settings", "enable-javascript", value, true);
        } }
        public bool enable_plugins { get {
            return get_boolean ("settings", "enable-plugins", true);
        } set {
            set_boolean ("settings", "enable-plugins", value, true);
        } }
        public bool enable_caret_browsing { get {
            return get_boolean ("settings", "enable-caret-browsing", false);
        } set {
            set_boolean ("settings", "enable-caret-browsing", value, false);
        } }

        public bool close_buttons_on_tabs { get {
            return get_boolean ("settings", "close-buttons-on-tabs", true);
        } set {
            set_boolean ("settings", "close-buttons-on-tabs", value, true);
        } }

        string default_toolbar = "TabNew,Back,ReloadStop,Location,BookmarkAdd,CompactMenu";
        internal string toolbar_items { owned get {
            return get_string ("settings", "toolbar-items", default_toolbar);
        } set {
            set_string ("settings", "toolbar-items", value.replace (",,", ","), default_toolbar);
        } }

        public string uri_for_search (string? keywords=null, string? search=null) {
            string uri = search ?? location_entry_search;
            /* Take a search engine URI and insert specified keywords.
               Keywords are percent-encoded. If the URI contains a %s
               the keywords are inserted there, otherwise appended. */
            string escaped = keywords != null ? Uri.escape_string (keywords, ":/", true) : "";
            // Allow DuckDuckGo to distinguish Midori and in turn share revenue
            if (uri == "https://duckduckgo.com/?q=%s") {
                return "https://duckduckgo.com/?q=%s&t=midori".printf (escaped);
            } else if (uri.str ("%s") != null) {
                return uri.printf (escaped);
            }
            return uri + escaped;
        }

        string default_search = "https://duckduckgo.com/?q=%s";
        public string location_entry_search { owned get {
            return get_string ("settings", "location-entry-search", default_search);
        } set {
            set_string ("settings", "location-entry-search", value, default_search);
        } }

        string default_homepage = "about:search";
        public string homepage { owned get {
            return get_string ("settings", "homepage", default_homepage);
        } set {
            // Fallback to search if hompepage isn't a proper URL (eg. about:search or empty)
            set_string ("settings", "homepage", ("://" in value || "." in value) ? value : default_homepage, default_homepage);
        } }

        internal bool homepage_in_toolbar { get {
            return "Homepage" in toolbar_items;
        } set {
            if (value && !toolbar_items.contains ("Homepage")) {
                toolbar_items = toolbar_items.replace ("Location", "Homepage,Location");
            } else if (!value && toolbar_items.contains ("Homepage")) {
                toolbar_items = toolbar_items.replace ("Homepage", "");
            }
        } }

        public ProxyType proxy_type { get {
            var proxy = get_string ("settings", "proxy-type", "MIDORI_PROXY_AUTOMATIC");
            if (proxy.has_suffix ("AUTOMATIC")) {
                return ProxyType.AUTOMATIC;
            } else if (proxy.has_suffix ("HTTP")) {
                return ProxyType.HTTP;
            } else {
                return ProxyType.NONE;
            }
        } set {
            set_string ("settings", "proxy-type", value.to_string (), "MIDORI_PROXY_AUTOMATIC");
        } }
        public string http_proxy { owned get {
            return get_string ("settings", "http-proxy", "");
        } set {
            set_string ("settings", "http-proxy", value);
        } }
        public int http_proxy_port { get {
            return get_string ("settings", "http-proxy-port", "8080").to_int ();
        } set {
            set_string ("settings", "http-proxy-port", value.to_string (), "8080");
        } }

        public bool first_party_cookies_only { get {
            return get_boolean ("settings", "first-party-cookies-only", true);
        } set {
            set_boolean ("settings", "first-party-cookies-only", value, true);
        } }
        public int maximum_history_age { get {
            return get_string ("settings", "maximum-history-age", "30").to_int ();
        } set {
            set_string ("settings", "maximum-history-age", value.to_string (), "30");
        } }
    }

    public class Settings : Object {
        KeyFile? keyfile = new KeyFile ();
        FileMonitor monitor;

        public string filename { get; construct set; }

        construct {
            load ();
        }

        void load () {
            try {
                keyfile.load_from_file (filename, KeyFileFlags.NONE);
                monitor = File.new_for_path (filename).monitor (0, null);
                monitor.changed.connect ((file, other, event) => {
                    switch (event) {
                        case GLib.FileMonitorEvent.CHANGES_DONE_HINT:
                            debug ("Reloading settings from %s", filename);
                            break;
                    }
                });
            } catch (FileError.NOENT error) {
                /* It's no error if no config file exists */
            } catch (Error error) {
                critical ("Failed to load settings from %s: %s", filename, error.message);
            }
        }

        void save () {
            try {
                File.new_for_path (filename).get_parent ().make_directory_with_parents ();
            } catch (Error error) {
                // It's no error if the folder already exists
            }
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

        public void set_string (string group, string key, string value, string? default=null) {
            if (value != get_string (group, key, default)) {
                if (value != default) {
                    keyfile.set_string (group, key, value);
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
        public string? get_string (string group, string key, string? default=null) {
            try {
                return keyfile.get_string (group, key);
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
