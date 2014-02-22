/* Copyright (C) 2012 André Stösel <andre@stoesel.de>
   This file is licensed under the terms of the expat license, see the file EXPAT. */

[CCode (cprefix = "Katze", lower_case_cprefix = "katze_")]
namespace Katze {
    static void assert_str_equal (string input, string result, string? expected);
    static unowned Gtk.Widget property_proxy (void* object, string property, string? hint);
    [CCode (cheader_filename = "katze/katze.h", cprefix = "KATZE_MENU_POSITION_")]
    enum MenuPos {
        CURSOR,
        LEFT,
        RIGHT
    }
    static void widget_popup (Gtk.Widget? widget, Gtk.Menu menu, Gdk.EventButton? event, MenuPos pos);

    [CCode (cheader_filename = "katze/katze.h")]
    public class Array : Katze.Item {
        public Array (GLib.Type type);
        public signal void add_item (GLib.Object item);
        public signal void remove_item (GLib.Object item);
        public uint get_length ();
        public GLib.List<unowned Item> get_items ();
        public bool is_empty ();
    }

    [CCode (cheader_filename = "katze/katze.h")]
    public class Item : GLib.Object {
        public Item ();
        public string? uri { get; set; }
        public string? name { get; set; }
        public string? text { get; set; }

        public bool get_meta_boolean (string key);
        public int64 get_meta_integer (string key);
        public void set_meta_integer (string key, int64 value);
        public unowned string? get_meta_string (string key);
        public void set_meta_string (string key, string value);
    }

    [CCode (cheader_filename = "katze/katze.h")]
    public class Preferences : Gtk.Dialog {
        public unowned Gtk.Box add_category (string label, string icon);
        public void add_group (string? label);
        public void add_widget (Gtk.Widget widget, string type);
    }
}

