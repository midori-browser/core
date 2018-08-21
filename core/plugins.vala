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
            string source_path = Path.build_path (Path.DIR_SEPARATOR_S,
                Environment.get_user_data_dir (), Environment.get_prgname (), "extensions");
            debug ("Loading plugins from %s", source_path);
            add_search_path (source_path, null);
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
        public void plug (Object object) {
            var extensions = new Peas.ExtensionSet (this, typeof (Peas.Activatable), "object", object, null);
            extensions.extension_added.connect ((info, extension) => { ((Peas.Activatable)extension).activate (); });
            extensions.extension_removed.connect ((info, extension) => { ((Peas.Activatable)extension).deactivate (); });
            extensions.foreach ((extensions, info, extension) => { extensions.extension_added (info, extension); });
            object.set_data<Object> ("midori-plug", extensions);
        }
    }
}
