/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

Midori.Plugins? plugins;
public void webkit_web_extension_initialize_with_user_data (WebKit.WebExtension extension, Variant user_data) {
    plugins = Midori.Plugins.get_default (user_data.get_string ());
    extension.page_created.connect ((page) => {
        var extensions = plugins.plug<Peas.Activatable> ("object", page);
        extensions.extension_added.connect ((info, extension) => ((Peas.Activatable)extension).activate ());
        extensions.extension_removed.connect ((info, extension) => { ((Peas.Activatable)extension).deactivate (); });
        extensions.foreach ((extensions, info, extension) => { extensions.extension_added (info, extension); });
    });
}
