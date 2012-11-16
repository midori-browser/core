/*
   Copyright (C) 2012 André Stösel <andre@stoesel.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

public class Midori.ExtensionsColumn : Gtk.TreeViewColumn {
    public signal void row_clicked (Gtk.TreeView view, Gtk.TreePath path);
}
