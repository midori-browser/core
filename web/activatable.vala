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
        page.document_loaded.connect (() => {
            try {
                // cf. http://ogp.me
                // Note: Some websites incorrectly use "name" instead of "property"
                var image = page.get_dom_document ().query_selector ("meta[property=\"og:image\"],meta[name=\"og:image\"]");
                var uri = image != null ? image.get_attribute ("content") : null;
                if (uri == null) {
                    // Fallback to high res apple-touch-icon or "shortcut icon"
                    image = page.get_dom_document ().query_selector ("link[sizes=\"any\"],link[sizes=\"152x152\"],link[sizes=\"144x144\"]");
                    uri = image != null ? image.get_attribute ("href") : null;
                }
                if (uri != null && uri != "") {
                    // Relative URL
                    if (!("://" in uri)) {
                        var soup_uri = new Soup.URI (page.uri);
                        soup_uri.set_path ("/" + uri);
                        soup_uri.set_query (null);
                        uri = soup_uri.to_string (false);
                    }
                    debug ("Found thumbnail for %s: %s", page.uri, uri);
                    var history = Midori.HistoryDatabase.get_default ();
                    history.prepare ("UPDATE %s SET image = :image WHERE uri = :uri".printf (history.table),
                                     ":image", typeof (string), uri,
                                     ":uri", typeof (string), page.uri).exec ();
                }
            } catch (Error error) {
                debug ("Failed to locate thumbnail for %s: %s", page.uri, error.message);
            }
        });
        var extensions = plugins.plug<Peas.Activatable> ("object", page);
        extensions.extension_added.connect ((info, extension) => ((Peas.Activatable)extension).activate ());
        extensions.extension_removed.connect ((info, extension) => { ((Peas.Activatable)extension).deactivate (); });
        extensions.foreach ((extensions, info, extension) => { extensions.extension_added (info, extension); });
    });
}
