/*
 Copyright (C) 2009-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Katze {
    public class SeparatorAction : Gtk.Action {
        Gtk.MenuItem? menuitem = null;
        Gtk.ToolItem? toolitem = null;

        public override unowned Gtk.Widget create_menu_item () {
            menuitem = new Gtk.SeparatorMenuItem ();
            return menuitem;
        }

        public override unowned Gtk.Widget create_tool_item () {
            toolitem = new Gtk.SeparatorToolItem ();
            return toolitem;
        }
    }
}

