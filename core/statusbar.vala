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
        string? _label = null;
        public string? label { get { return _label; } set {
            if (value != null && value != "") {
                _label = value;
                show ();
            } else {
                _label = "";
                hide ();
            }
            push (1, _label);
        } }
    }
}
