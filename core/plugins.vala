/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class Extension : Object {
        internal static List<Extension> extensions = null;

        public string id { get; set; }
        public string name { get; set; }
        public string description { get; set; }
        public Icon icon { get; set; default = new ThemedIcon.with_default_fallbacks ("libpeas-plugin-symbolic"); }
        public bool available { get; set; default = false; }
        public bool active { get {
            return Midori.CoreSettings.get_default ().get_plugin_enabled (id);
        } set {
            Midori.CoreSettings.get_default ().set_plugin_enabled (id, value);
        } }

        construct {
            extensions.append (this);
        }

        internal Extension (string id, string name, string description, Icon icon) {
            Object (id: id, name: name, description: description, icon: icon);
        }
    }

    public class Plugins : Peas.Engine, Loggable {
        public string builtin_path { get; construct set; }

        static Plugins? _default = null;

        internal static new Plugins get_default (string? builtin_path=null) {
            if (_default == null) {
                _default = new Plugins (builtin_path);
            }
            return _default;
        }

        Plugins (string builtin_path) {
            Object (builtin_path: builtin_path);
        }

        construct {
            enable_loader ("python");
            // Plugins installed by the user
            string user_path = Path.build_path (Path.DIR_SEPARATOR_S,
                Environment.get_user_data_dir (), Config.PROJECT_NAME, "extensions");
            debug ("Loading plugins from %s", user_path);
            add_search_path (user_path, null);
            debug ("Loading plugins from %s", builtin_path);
            add_search_path (builtin_path, user_path);

            var settings = CoreSettings.get_default ();
            foreach (var plugin in get_plugin_list ()) {
                debug ("Found plugin %s", plugin.get_name ());
                if (!plugin.is_builtin ()) {
                    var extension = new Extension (
                        "lib%s.so".printf (plugin.get_module_name ()), plugin.get_name (),
                        plugin.get_description (), new ThemedIcon.with_default_fallbacks (plugin.get_icon_name ()));
                    try {
                        extension.available = plugin.is_available ();
                        extension.notify["active"].connect (() => {
                            if (extension.active) {
                                try_load_plugin (plugin);
                            }
                            else {
                                try_unload_plugin (plugin);
                            }
                        });
                    } catch (Error error) {
                        critical ("Failed to prepare plugin %s", plugin.get_module_name ());
                    }
                }
                if (plugin.is_builtin ()
                 || settings.get_plugin_enabled ("lib%s.so".printf (plugin.get_module_name ()))) {
                    if (!try_load_plugin (plugin)) {
                        critical ("Failed to load plugin %s", plugin.get_module_name ());
                    }
                }
            }
        }

        /*
         * Plug the instance of the given object to make it extensible via Peas.
         */
        public Peas.ExtensionSet plug<T> (string name, Object object) {
            var extensions = new Peas.ExtensionSet (this, typeof (T), name, object, null);
            object.set_data<Object> ("midori-plug", extensions);
            return extensions;
        }
    }
}
