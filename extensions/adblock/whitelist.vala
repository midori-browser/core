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
        public Whitelist (Options options) {
            base (options);
        }

        public override Directive? match (string request_uri, string page_uri) throws Error {
            foreach (unowned string white in rules.get_keys ()) {
                var regex = rules.lookup (white);
                if (!regex.match_full (request_uri))
                    return null;
                if (Regex.match_simple (regex.get_pattern (), request_uri, RegexCompileFlags.UNGREEDY, RegexMatchFlags.NOTEMPTY))
                    return Directive.ALLOW;
            }
            return null;
        }
    }
}
