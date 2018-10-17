/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class Statusbar : Gtk.Statusbar {
        internal bool has_children = false;

        string? _label = null;
        public string? label { get { return _label; } set {
            _label = value ?? "";
            visible = has_children || label != "";
            push (1, _label);
        } }


        construct {
            // Persistent statusbar mode with child widgets
            add.connect ((widget) => { has_children = true; show (); });
            ((Gtk.Box)this).remove.connect ((widget) => {
                has_children = get_children ().length () > 0;
                visible = !has_children && label != null && label != "";
            });
        }
    }
}
