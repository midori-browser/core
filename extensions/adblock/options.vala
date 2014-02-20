/*
 Copyright (C) 2014 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Adblock {
    public class Options : GLib.Object {
        HashTable<string, string?> optslist;

        public Options () {
            clear ();
        }

        public void insert (string sig, string? opts) {
            optslist.insert (sig, opts);
        }

        public string? lookup (string sig) {
            return optslist.lookup (sig);
        }

        public void clear () {
            optslist = new HashTable<string, string?> (str_hash, str_equal);
        }
    }
}
