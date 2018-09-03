/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    class Plugins : Peas.Engine, Loggable {
        static Plugins? _default = null;

        public static new Plugins get_default () {
            if (_default == null) {
                _default = new Plugins ();
            }
            return _default;
        }

        Plugins () {
            enable_loader ("python");
            // Plugins installed by the user
            string user_path = Path.build_path (Path.DIR_SEPARATOR_S,
                Environment.get_user_data_dir (), Environment.get_prgname (), "extensions");
            add_search_path (user_path, null);
            debug ("Loading plugins from %s", user_path);

            var exec_path = ((App)Application.get_default ()).exec_path;
            // Try and load plugins from build folder
            var build_path = exec_path.get_parent ().get_child ("extensions");
            if (build_path.query_exists (null)) {
                debug ("Loading plugins from %s", build_path.get_path ());
                add_search_path (build_path.get_path (), user_path);
            }
            // System-wide plugins
            var system_path = exec_path.get_parent ().get_parent ().get_child ("lib").get_child (Environment.get_prgname ());
            if (system_path.query_exists (null)) {
                debug ("Loading plugins from %s", system_path.get_path ());
                add_search_path (system_path.get_path (), user_path);
            }
            foreach (var plugin in get_plugin_list ()) {
                debug ("Found plugin %s", plugin.get_name ());
                if (!try_load_plugin (plugin)) {
                    critical ("Failed to load plugin %s", plugin.get_module_name ());
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
