/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class Plugins : Peas.Engine {
        public Plugins (WebKit.WebExtension extension, Variant user_data) {
            enable_loader ("python");
            // Plugins installed by the user
            string user_path = Path.build_path (Path.DIR_SEPARATOR_S,
                Environment.get_user_data_dir (), Config.PROJECT_NAME, "extensions");
            debug ("Loading plugins from %s", user_path);
            add_search_path (user_path, null);
            // Built-ins from the build folder or system-wide
            string builtin_path = user_data.get_string ();
            add_search_path (builtin_path, user_path);
            foreach (var plugin in get_plugin_list ()) {
                debug ("Found plugin %s", plugin.get_name ());
                if (!try_load_plugin (plugin)) {
                    critical ("Failed to load plugin %s", plugin.get_module_name ());
                }
            }
        }

        public void plug (Object object) {
            var extensions = new Peas.ExtensionSet (this, typeof (Peas.Activatable), "object", object, null);
            extensions.extension_added.connect ((info, extension) => { ((Peas.Activatable)extension).activate (); });
            extensions.extension_removed.connect ((info, extension) => { ((Peas.Activatable)extension).deactivate (); });
            extensions.foreach ((extensions, info, extension) => { extensions.extension_added (info, extension); });
            object.set_data<Object> ("midori-plug", extensions);
        }
    }
}

Midori.Plugins? plugins;
public void webkit_web_extension_initialize_with_user_data (WebKit.WebExtension extension, Variant user_data) {
    plugins = new Midori.Plugins (extension, user_data);
    extension.page_created.connect ((page) => {
        plugins.plug (page);
    });
}
