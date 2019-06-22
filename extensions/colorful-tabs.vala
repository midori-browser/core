/*
 Copyright (C) 2009-2018 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2010 Samuel Creshal <creshal@arcor.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace ColorfulTabs {
    public class Tint : Peas.ExtensionBase, Midori.TabActivatable {
        public Midori.Viewable tab { owned get; set; }

        public void activate () {
            tab.notify["display-uri"].connect (apply_tint);
            apply_tint ();
            deactivate.connect (() => {
                tab.notify["display-uri"].disconnect (apply_tint);
                tab.color = null;
            });
        }

        void apply_tint () {
            if ("://" in tab.display_uri) {
                Gdk.Color color;
                // Hash the hostname without the protocol or path
                string hostname = tab.display_uri.chr (-1, '/').offset (2).split ("/")[0];
                string hash = Checksum.compute_for_string (ChecksumType.MD5, hostname, 1);
                Gdk.Color.parse ("#" + hash.substring (0, 6), out color);
                // Adjust background brightness
                uint16 grey = 137 * 255;
                uint16 adjustment = 78 * 255;
                uint16 blue = 39 * 255;
                uint16 extra = 19 * 255;
                if (color.red < grey && color.green < grey && color.blue < grey) {
                    color.red += adjustment;
                    color.green += adjustment;
                    color.blue += adjustment;
                }
                color.red = color.red < blue ? extra : color.red - extra;
                color.blue = color.blue < blue ? extra : color.blue - extra;
                color.green = color.green < blue ? extra : color.green - extra;
                tab.color = color.to_string ();
            } else {
                tab.color = null;
            }
        }
    }
}

[ModuleInit]
public void peas_register_types(TypeModule module) {
    ((Peas.ObjectModule)module).register_extension_type (
        typeof (Midori.TabActivatable), typeof (ColorfulTabs.Tint));
}
