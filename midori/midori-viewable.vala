/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public interface Viewable {
        public abstract unowned string get_stock_id ();
        public abstract unowned string get_label ();
        public abstract Gtk.Widget get_toolbar ();
        /* Emitted when an Option menu is displayed, for instance
         * when the user clicks the Options button in the panel.
         * Deprecated: 0.2.3 */
        public signal void populate_option_menu (Gtk.Menu menu);
    }
}

