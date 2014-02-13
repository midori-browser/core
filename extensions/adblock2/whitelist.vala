/*
 Copyright (C) 2014 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2014 Paweł Forysiuk <tuxator@o2.pl>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Adblock {
    public class Whitelist : Filter {
        HashTable<string, Regex?> white;

        public Whitelist (Options options) {
            base (options);
            white = new HashTable<string, Regex> (str_hash, str_equal);
        }

        public override void insert (string sig, Regex regex) {
            white.insert (sig, regex);
        }

        public override Regex? lookup (string sig) {
            return white.lookup (sig);
        }

        public override uint size () {
            return white.size ();
        }

        public override bool match (string request_uri, string page_uri) throws Error {
            foreach (var wht in white.get_keys ()) {
                var regex = white.lookup (wht);
                if (!regex.match_full (request_uri))
                    return false;
                if (Regex.match_simple (regex.get_pattern (), request_uri, RegexCompileFlags.UNGREEDY, RegexMatchFlags.NOTEMPTY))
                    return true;
            }
            return false;
        }
    }
}
