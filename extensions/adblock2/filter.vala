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
    public abstract class Filter : GLib.Object {
        Options optslist;

        public abstract void insert (string sig, Regex regex);
        public abstract Regex? lookup (string sig);
        public abstract uint size ();
        public abstract bool match (string request_uri, string page_uri) throws Error;

        protected Filter (Options options) {
            optslist = options;
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
