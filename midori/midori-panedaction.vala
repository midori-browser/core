/*
 Copyright (C) 2011 Peter Hatina <phatina@redhat.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class PanedAction : Gtk.Action {
        Gtk.HPaned? hpaned = null;
        Gtk.ToolItem? toolitem = null;
        Child child1 = new Child();
        Child child2 = new Child();

        private struct Child {
            protected Gtk.Widget widget;
            string name;
            bool resize;
            bool shrink;
        }

        public override unowned Gtk.Widget create_tool_item () {
            Gtk.Alignment alignment = new Gtk.Alignment (0.0f, 0.5f, 1.0f, 0.1f);
            hpaned = new Gtk.HPaned ();
            toolitem = new Gtk.ToolItem ();
            toolitem.set_expand (true);
            toolitem.add (alignment);
            alignment.add (hpaned);

            hpaned.pack1 (child1.widget, child1.resize, child1.shrink);
            hpaned.pack2 (child2.widget, child2.resize, child2.shrink);
            toolitem.show_all ();
            return toolitem;
        }

        public void set_child1 (Gtk.Widget widget, string name, bool resize, bool shrink) {
            child1.widget = widget;
            child1.name = name;
            child1.resize = resize;
            child1.shrink = shrink;
        }

        public void set_child2 (Gtk.Widget widget, string name, bool resize, bool shrink) {
            child2.widget = widget;
            child2.name = name;
            child2.resize = resize;
            child2.shrink = shrink;
        }

        public Gtk.Widget? get_child1 () {
            return child1.widget;
        }

        public Gtk.Widget? get_child2 () {
            return child2.widget;
        }

        public Gtk.Widget? get_child_by_name (string name) {
            if (name == child1.name)
                return child1.widget;
            else if (name == child2.name)
                return child2.widget;
            return null;
        }

        public string get_child1_name () {
            return child1.name;
        }

        public string get_child2_name () {
            return child2.name;
        }
    }
}

