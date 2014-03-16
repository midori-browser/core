/*
 Copyright (C) 2014 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Adblock {
    public class Element : Feature {
        public HashTable<string, string> blockcssprivate;
        bool debug_element;

        public Element () {
            base ();
            debug_element = "adblock:element" in (Environment.get_variable ("MIDORI_DEBUG") ?? "");
        }

        public override void clear () {
            blockcssprivate = new HashTable<string, string> (str_hash, str_equal);
        }

        public string? lookup (string domain) {
            return blockcssprivate.lookup (domain);
        }

        public void insert (string domain, string value) {
            if (debug_element)
                stdout.printf ("Element to be blocked %s => %s\n", domain, value);
            blockcssprivate.insert (domain, value);
        }
    }
}
