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
        public Pattern (Options options) {
            base (options);
        }

        public override Directive? match (string request_uri, string page_uri) throws Error {
            foreach (unowned string patt in rules.get_keys ())
                if (check_rule (rules.lookup (patt), patt, request_uri, page_uri))
                    return Directive.BLOCK;
            return null;
        }
    }
}
