/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class Favicon : Gtk.Image {
        public new Cairo.Surface? surface { set {
            var image = (Cairo.ImageSurface)value;
            var pixbuf = image != null ? Gdk.pixbuf_get_from_surface (image, 0, 0, image.get_width (), image.get_height ()) : null;
            int _icon_size = icon_size;
            gicon = pixbuf != null ? (Icon)scale_to_icon_size (pixbuf) : null;
            icon_size = _icon_size;
        } }

        string? _uri = null;
        public string? uri { get { return _uri; } set {
            // Reset icon without losing the size
            int _icon_size = icon_size;
            gicon = null;
            icon_size = _icon_size;

            _uri = value;
            load_icon.begin ();
        } }

        WebKit.FaviconDatabase? database;
        async void load_icon (Cancellable? cancellable = null) {
            if (database == null) {
                database = WebKit.WebContext.get_default ().get_favicon_database ();
                database.favicon_changed.connect ((page_uri, favicon_uri) => {
                    if (page_uri == uri) {
                        load_icon.begin ();
                    }
                });
            }

            try {
                surface = yield database.get_favicon (uri, cancellable);
            } catch (Error error) {
                debug ("Icon failed to load: %s", error.message);
            }
        }

        Gdk.Pixbuf scale_to_icon_size (Gdk.Pixbuf pixbuf) {
            int icon_width = 16, icon_height = 16;
            Gtk.icon_size_lookup ((Gtk.IconSize)icon_size, out icon_width, out icon_height);
            // Take scale factor into account
            icon_width *= scale_factor;
            icon_height *= scale_factor;
            return pixbuf.scale_simple (icon_width, icon_height, Gdk.InterpType.BILINEAR);
        }
    }
}
