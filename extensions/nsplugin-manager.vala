/*
   Copyright (C) 2012 André Stösel <andre@stoesel.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

namespace NSPlugins {
    private int active_plugins = 0;

    private class Extension : Midori.Extension {
        protected WebKit.WebPlugin plugin;

        void activated (Midori.App app) {
            active_plugins += 1;
            this.plugin.set_enabled (true);
            app.settings.enable_plugins = active_plugins > 0;
        }

        void deactivated () {
            Midori.App app = this.get_app ();
            active_plugins -= 1;
            this.plugin.set_enabled (false);
            app.settings.enable_plugins = active_plugins > 0;
        }

        internal Extension (WebKit.WebPlugin plugin) {
            string desc = plugin.get_description ();
            try {
                var regex = new Regex ("<a.+href.+>(.+)</a>");
                desc = regex.replace (desc, -1, 0, "<u>\\1</u>");
                desc = desc.replace ("<br>", "\n");
            }
            catch (Error error) { }
            GLib.Object (stock_id: Midori.Stock.PLUGINS,
                         name: plugin.get_name (),
                         description: desc,
                         use_markup: true,
                         key: GLib.Path.get_basename (plugin.get_path ()),
                         version: "(%s)".printf ("Netscape plugins"),
                         authors: "");

            this.plugin = plugin;
            this.plugin.set_enabled (false);

            this.activate.connect (activated);
            this.deactivate.connect (deactivated);
        }
    }
}

public Katze.Array? extension_init () {
    if (!Midori.WebSettings.has_plugin_support ())
        return null;

    var extensions = new Katze.Array( typeof (Midori.Extension));
    WebKit.WebPluginDatabase pdb = WebKit.get_web_plugin_database ();
    SList<WebKit.WebPlugin> plugins = pdb.get_plugins ();

    foreach (WebKit.WebPlugin plugin in plugins) {
        if (Midori.WebSettings.skip_plugin (plugin.get_path ()))
            continue;
        extensions.add_item (new NSPlugins.Extension (plugin));
    }
    return extensions;
}

