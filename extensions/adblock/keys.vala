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
    public class Keys : Filter {
        List<Regex> blacklist;

        public Keys (Options options) {
            base (options);
        }

        public override void clear () {
            base.clear ();
            blacklist = new List<Regex> ();
        }

        public override Directive? match (string request_uri, string page_uri) throws Error {
            string? uri = fixup_regex ("", request_uri);
            if (uri == null)
                return null;

            int signature_size = 8;
            int pos, l = uri.length;
            for (pos = l - signature_size; pos >= 0; pos--) {
                string signature = uri.offset (pos).ndup (signature_size);
                var regex = rules.lookup (signature);
                if (regex == null || blacklist.find (regex) != null)
                    continue;

                if (check_rule (regex, uri, request_uri, page_uri))
                    return Directive.BLOCK;
                blacklist.prepend (regex);
            }

            return null;
        }
    }
}
