/* Copyright (C) 2012 André Stösel <andre@stoesel.de>
   This file is licensed under the terms of the expat license, see the file EXPAT. */

[CCode (cprefix = "Katze", lower_case_cprefix = "katze_")]
namespace Katze {
    public class Array : GLib.Object {
        public Array (GLib.Type type);
        public void add_item (GLib.Object item);
    }
}

