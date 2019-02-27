/*
 Copyright (C) 2009-2018 Christian Dywan <christian@twotoats.de>
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Adblock {
    public enum Directive {
        ALLOW,
        BLOCK
    }

    public static string? fixup_regex (string prefix, string? src) {
        if (src == null) {
            return null;
        }

        var fixed = new StringBuilder ();
        fixed.append (prefix);

        uint i = 0, l = src.length;
        if (src[0] == '*') {
            i++;
        }
        while (i < l) {
            char c = src[i];
            switch (c) {
                case '*':
                    fixed.append (".*"); break;
                case '|':
                case '^':
                case '+':
                    break;
                case '?':
                case '[':
                case ']':
                case '.':
                case '(':
                case ')':
                    fixed.append_printf ("\\%c", c);
                    break;
                default:
                    fixed.append_c (c);
                    break;
            }
            i++;
        }
        return fixed.str;
    }

    public abstract class Feature : Object {
        public virtual Directive? match (string request_uri, string page_uri) throws Error {
            return null;
        }
    }

    public class Subscription : Object {
        public string uri { get; protected construct set; }
        string? _title = null;
        public string? title { get {
            if (_title == null) {
                ensure_headers ();
                if (_title == null) {
                    // Fallback to title from the URI
                    string[] parts = Soup.URI.decode (uri).split ("&");
                    foreach (string part in parts) {
                        if (part.has_prefix ("title=")) {
                            _title = part.substring (6, -1);
                            break;
                        }
                    }
                    if (_title == null) {
                        _title = uri.substring (uri.index_of ("://") + 3, -1);
                    }
                }
            }
            return _title;
        } }
        public bool active { get; set; default = true; }

        HashTable<string, Directive?>? cache = null;
        List<Feature> features;
        Options optslist;
        Whitelist whitelist;
        Keys keys;
        Pattern pattern;
        public File file { get; protected set; }

        public Subscription (string uri, bool active=false) {
            Object (uri: uri, active: active);
        }

        construct {
            // Consider the URI without an optional query
            string base_uri = uri.split ("&")[0];
            if (uri.has_prefix ("file://")) {
                file = File.new_for_uri (base_uri);
            } else {
                string cache_dir = Path.build_filename (Environment.get_user_cache_dir (), Config.PROJECT_NAME, "adblock");
                string filename = Checksum.compute_for_string (ChecksumType.MD5, base_uri, -1);
                file = File.new_for_path (Path.build_filename (cache_dir, filename));
            }
        }

        public void add_feature (Feature feature) {
            features.append (feature);
            size++;
        }

        /* foreach support */
        public new unowned Feature? get (uint index) {
            return features.nth_data (index);
        }
        public uint size { get; private set; }

        void clear () {
            cache = new HashTable<string, Directive?> (str_hash, str_equal);
            optslist = new Options ();
            whitelist = new Whitelist (optslist);
            add_feature (whitelist);
            keys = new Keys (optslist);
            add_feature (keys);
            pattern = new Pattern (optslist);
            add_feature (pattern);
        }

        /*
         * Parse headers only.
         */
        public void ensure_headers () {
            if (!ensure_downloaded (true)) {
                return;
            }

            queue_parse.begin (true);
        }

        /*
         * Attempt to parse filters unless inactive or already cached.
         */
        public bool ensure_parsed () {
            if (!active || cache != null) {
                return active;
            }

            // Skip if this hasn't been downloaded (yet)
            if (!file.query_exists ()) {
                return false;
            }

            queue_parse.begin ();
            return true;
        }

        bool ensure_downloaded (bool headers_only) {
            if (file.query_exists ()) {
                return true;
            }

            try {
                file.get_parent ().make_directory_with_parents ();
            } catch (Error error) {
                // It's no error if the folder already exists
            }

            var download = WebKit.WebContext.get_default ().download_uri (uri.split ("&")[0]);
            download.allow_overwrite = true;
            download.set_destination (file.get_uri ());
            download.finished.connect (() => {
                queue_parse.begin (true);
            });
            return false;
        }

        async void queue_parse (bool headers_only=false) {
            try {
                yield parse (headers_only);
            } catch (Error error) {
                critical ("Failed to parse %s%s: %s", headers_only ? "headers for " : "", uri, error.message);
            }
        }

        /*
         * Parse either headers or filters depending on the flag.
         */
        async void parse (bool headers_only=false) throws Error {
            debug ("Parsing %s, caching %s", uri, file.get_path ());
            clear ();
            var stream = new DataInputStream (file.read ());
            string? line;
            while ((line = stream.read_line (null)) != null) {
                if (line == null) {
                    continue;
                }
                string chomped = line.chomp ();
                if (chomped == "") {
                    continue;
                }
                if (line[0] == '!' && headers_only) {
                    parse_header (chomped);
                } else if (!headers_only) {
                    parse_line (chomped);
                }
            }
        }

        void parse_header (string header) {
            /* Headers come in two forms
               ! Foo: Bar
               ! Some freeform text
             */
            string key = header;
            string value = "";
            if (header.contains (":")) {
                string[] parts = header.split (":", 2);
                if (parts[0] != null && parts[0] != ""
                 && parts[1] != null && parts[1] != "") {
                    key = parts[0].substring (2, -1);
                    value = parts[1].substring (1, -1);
                }
            }
            debug ("Header '%s' says '%s'", key, value);
            if (key == "Title") {
                _title = value;
            }
        }

        void parse_line (string line) throws Error {
            if (line.has_prefix ("@@")) {
                if (line.contains("$") && line.contains ("domain"))
                    return;
                if (line.has_prefix ("@@||")) {
                    add_url_pattern ("^", "whitelist", line.offset (4));
                } else if (line.has_prefix ("@@|")) {
                    add_url_pattern ("^", "whitelist", line.offset (3));
                } else {
                    add_url_pattern ("", "whitelist", line.offset (2));
                }
                return;
            }
            if (line[0] == '[') {
                return;
            }

            // CSS block hiding
            if (line.has_prefix ("##")) {
                // Not implemented here so just skip
                return;
            }
            if (line[0] == '#')
                return;

            if ("#@#" in line)
                return;

            // Per domain CSS hiding rule
            if ("##" in line || "#" in line) {
                // Not implemented here so just skip
                return;
            }

            /* URL blocker rule */
            if (line.has_prefix ("|")) {
                if (line.contains("$"))
                    return;

                if (line.has_prefix ("||")) {
                    add_url_pattern ("", "fulluri", line.offset (2));
                } else {
                    add_url_pattern ("^", "fulluri", line.offset (1));
                }
                return;
            }

            add_url_pattern ("", "uri", line);
            return;
        }

        void add_url_pattern (string prefix, string type, string line) throws Error {
            string[]? data = line.split ("$", 2);
            if (data == null || data[0] == null)
                return;

            string patt, opts;
            patt = data[0];
            opts = type;

            if (data[1] != null)
                opts = type + "," + data[1];

            if (Regex.match_simple ("subdocument", opts,
                RegexCompileFlags.CASELESS, RegexMatchFlags.NOTEMPTY)) {
                return;
            }

            string format_patt = fixup_regex (prefix, patt);
            debug ("got: %s opts %s", format_patt, opts);
            compile_regexp (format_patt, opts);
        }

        void compile_regexp (string? patt, string opts) throws Error {
            if (patt == null) {
                return;
            }

            var regex = new Regex (patt, RegexCompileFlags.OPTIMIZE, RegexMatchFlags.NOTEMPTY);
            // is pattern is already a regular expression?
            if (Regex.match_simple ("^/.*[\\^\\$\\*].*/$", patt,
                RegexCompileFlags.UNGREEDY, RegexMatchFlags.NOTEMPTY)
             || (opts != null && opts.contains ("whitelist"))) {
                debug ("patt: %s", patt);
                if (opts.contains ("whitelist")) {
                    whitelist.insert (patt, regex);
                } else {
                    pattern.insert (patt, regex);
                }
                optslist.insert (patt, opts);
            } else {
                int pos = 0, len;
                int signature_size = 8;
                string sig;
                len = patt.length;

                // Chop up pattern into substrings for faster matching
                for (pos = len - signature_size; pos >= 0; pos--) {
                    sig = patt.offset (pos).ndup (signature_size);
                    // No * nor \\, doesn't look like regex, save chunk as "key"
                    if (!Regex.match_simple ("[\\*]", sig, RegexCompileFlags.UNGREEDY, RegexMatchFlags.NOTEMPTY) && keys.lookup (sig) == null) {
                        keys.insert (sig, regex);
                        optslist.insert (sig, opts);
                    } else {
                        // Starts with * or \\: save as regex
                        if ((sig.has_prefix ("*") || sig.has_prefix("\\")) && pattern.lookup (sig) == null) {
                            pattern.insert (sig, regex);
                            optslist.insert (sig, opts);
                        }
                    }
                }
            }
        }

        public Directive? get_directive (string request_uri, string page_uri) {
            if (!ensure_parsed ()) {
                return null;
            }

            Directive? directive = cache.lookup (request_uri);
            if (directive != null) {
                debug ("%s for %s (%s)", directive.to_string (), request_uri, page_uri);
                return directive;
            }

            try {
                //The uri is either Allowed(whitelist), Blocked(pattern), or neither
                directive = whitelist.match (request_uri, page_uri);
                if (directive == null) {
                    directive = pattern.match (request_uri, page_uri);
                }
            } catch (Error error) {
                critical ("Error matching %s %s: %s", request_uri, uri, error.message);
            }

            if (directive != null)
                cache.insert (request_uri, directive);

            return directive;
        }
    }
}
