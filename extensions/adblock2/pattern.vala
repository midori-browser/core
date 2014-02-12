/*
 Copyright (C) 2009-2014 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Adblock {
    public class Pattern : Filter {
        HashTable<string, Regex?> pattern;

        public Pattern (Options options) {
            base (options);
            pattern = new HashTable<string, Regex> (str_hash, str_equal);
        }

        public override void insert (string sig, Regex regex) {
            pattern.insert (sig, regex);
        }

        public override Regex? lookup (string sig) {
            return pattern.lookup (sig);
        }

        public override uint size () {
            return pattern.size ();
        }

        public override bool match (string request_uri, string page_uri) throws Error {
            foreach (var patt in pattern.get_keys ())
                if (check_rule (pattern.lookup (patt), patt, request_uri, page_uri))
                    return true;
            return false;
        }
    }
}
