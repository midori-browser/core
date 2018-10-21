/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
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
                if (plugin.is_builtin ()
                 || settings.get_plugin_enabled (plugin.get_module_name ())) {
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
