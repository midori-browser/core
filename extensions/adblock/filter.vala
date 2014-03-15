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
    public abstract class Filter : Feature {
        Options optslist;
        protected HashTable<string, Regex?> rules;

        public virtual void insert (string sig, Regex regex) {
            rules.insert (sig, regex);
        }

        public virtual Regex? lookup (string sig) {
            return rules.lookup (sig);
        }

        public virtual uint size () {
            return rules.size ();
        }

        protected Filter (Options options) {
            optslist = options;
            clear ();
        }

        public override void clear () {
            rules = new HashTable<string, Regex> (str_hash, str_equal);
        }

        protected bool check_rule (Regex regex, string pattern, string request_uri, string page_uri) throws Error {
            if (!regex.match_full (request_uri))
                return false;

            var opts = optslist.lookup (pattern);
            if (opts != null && Regex.match_simple (",third-party", opts,
                RegexCompileFlags.CASELESS, RegexMatchFlags.NOTEMPTY))
                if (page_uri != null && regex.match_full (page_uri))
                    return false;
            debug ("blocked by pattern regexp=%s -- %s", regex.get_pattern (), request_uri);
            return true;
        }
    }
}
