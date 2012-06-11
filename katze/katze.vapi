/* Copyright (C) 2012 André Stösel <andre@stoesel.de>
   This file is licensed under the terms of the expat license, see the file EXPAT. */

[CCode (cprefix = "Katze", lower_case_cprefix = "katze_")]
namespace Katze {
    public class Array : GLib.Object {
        public Array (GLib.Type type);
        public void add_item (GLib.Object item);
    }

    public class Item : GLib.Object {
        public bool get_meta_boolean (string key);
        public int64 get_meta_integer (string key);
        public void set_meta_integer (string key, int64 value);
    }
}

