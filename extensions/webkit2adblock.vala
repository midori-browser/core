/*
 Copyright (C) 2009-2013 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Adblock {
    enum Directive {
        ALLOW,
        BLOCK
    }

    public class Filter : Midori.Extension {
        bool debug_match;
        HashTable<string, Directive?> cache;
        HashTable<string, Regex?> pattern;
        HashTable<string, Regex?> keys;
        HashTable<string, string?> optslist;
        List<Regex> blacklist;

#if HAVE_WEBKIT2
        public Filter (WebKit.WebExtension web_extension) {
            init ();
            web_extension.page_created.connect (page_created);
        }

        void page_created (WebKit.WebPage web_page) {
            web_page.send_request.connect (send_request);
        }

        bool send_request (WebKit.WebPage web_page, WebKit.URIRequest request, WebKit.URIResponse? redirected_response) {
            return request_handled (web_page.uri, request.uri);
        }
#else
        public Filter () {
            GLib.Object (name: _("Advertisement blocker"),
                         description: _("Block advertisements according to a filter list"),
                         version: "0.6",
                         authors: "Christian Dywan <christian@twotoasts.de>");
            install_string_list ("filters", null);
            activate.connect (extension_activated);
        }

        void extension_activated (Midori.App app) {
            init ();
            foreach (var browser in app.get_browsers ())
                browser_added (browser);
            app.add_browser.connect (browser_added);
        }

        void browser_added (Midori.Browser browser) {
            foreach (var tab in browser.get_tabs ())
                tab_added (tab);
            browser.add_tab.connect (tab_added);
        }

        void tab_added (Midori.View view) {
            view.web_view.resource_request_starting.connect (resource_requested);
        }

        void resource_requested (WebKit.WebView web_view, WebKit.WebFrame frame,
            WebKit.WebResource resource, WebKit.NetworkRequest request, WebKit.NetworkResponse response) {

            if (request_handled (web_view.uri, request.uri))
                request.set_uri ("about:blank");
        }
#endif

        void init () {
            debug_match = "adblock:match" in (Environment.get_variable ("MIDORI_DEBUG") ?? "");
            stdout.printf ("WebKit2Adblock%s\n", debug_match ? " debug" : "");
            reload_rules ();
        }

        void reload_rules () {
            cache = new HashTable<string, Directive?> (str_hash, str_equal);
            pattern = new HashTable<string, Regex?> (str_hash, str_equal);
            keys = new HashTable<string, Regex?> (str_hash, str_equal);
            optslist = new HashTable<string, string?> (str_hash, str_equal);
            blacklist = new List<Regex> ();

            // FIXME
            string filename = GLib.Path.build_filename (GLib.Environment.get_home_dir (), ".config", "midori", "extensions", "libadblock.so", "config"); // use midori vapi
            var keyfile = new GLib.KeyFile ();
            try {
                keyfile.load_from_file (filename, GLib.KeyFileFlags.NONE);
                string[] filters = keyfile.get_string_list ("settings", "filters");
                foreach (string filter in filters) {
                    try {
                        string filter_filename = GLib.Path.build_filename (GLib.Environment.get_home_dir (), ".cache", "midori", "adblock", Checksum.compute_for_string (ChecksumType.MD5, filter, -1)); // FIXME
                        stdout.printf ("Parsing %s (%s)\n", filter, filter_filename);
                        var filter_file = File.new_for_path (filter_filename);
                        var stream = new DataInputStream (filter_file.read ());
                        string? line;
                        while ((line = stream.read_line (null)) != null)
                            parse_line (line.chomp ());
                    }
                    catch (GLib.Error io_error) {
                        stdout.printf ("Error reading file for %s: %s\n", filter, io_error.message);
                    }
                }
            }
            catch (GLib.Error settings_error) {
                stdout.printf ("Error reading settings: %s\n", settings_error.message);
            }
        }

        void parse_line (string line) {
            /* Empty or comment */
            if (!(line != null && line[0] != ' ' && line[0] != '!' && line[0] != '\0'))
                return;
            /* TODO: Whitelisting */
            if (line.has_prefix ("@@"))
                return;
            /* TODO: [include] [exclude] */
            if (line[0] == '[')
                return;

            /* CSS block hider */
            if (line.has_prefix ("##")) {
                frame_add (line);
                return;
            }
            if (line[0] == '#')
                return;

            /* Per domain CSS hider rule */
            if ("##" in line) {
                frame_add_private (line, "##");
                return;
            }
            if ("#" in line) {
                frame_add_private (line, "#");
                return;
            }

            /* URL blocker rule */
            if (line.has_prefix ("||")) {
                add_url_pattern ("", "fulluri", line.offset (2));
                return /* add_url_pattern */;
            }
            if (line[0] == '|') {
                add_url_pattern ("^", "fulluri", line.offset (1));
                return /* add_url_pattern */;
            }
            add_url_pattern ("", "uri", line);
            return /* add_url_pattern */;
        }

        void frame_add (string line) {
            /* TODO */
        }

        void frame_add_private (string line, string sep) {
            /* TODO */
        }

        void add_url_pattern (string prefix, string type, string line) {
            string[]? data = line.split ("$");
            if (data == null || data[0] == null)
                return;

            string patt, opts;
            if (data[1] != null && data[2] != null) {
                patt = data[0] + data[1];
                opts = type + "," + data[2];
            }
            else if (data[1] != null) {
                patt = data[0];
                opts = type + "," + data[1];
            }
            else {
                patt = data[0];
                opts = type;
            }

            if (Regex.match_simple ("subdocument", opts,
                RegexCompileFlags.CASELESS, RegexMatchFlags.NOTEMPTY))
                return;

            string format_patt = fixup_regex (prefix, patt);
            if (debug_match)
                debug ("got: %s opts %s", format_patt, opts);
            compile_regexp (format_patt, opts);
            /* return format_patt */
        }

        bool compile_regexp (string? patt, string opts) {
            if (patt == null)
                return false;
            try {
                var regex = new Regex (patt, RegexCompileFlags.OPTIMIZE, RegexMatchFlags.NOTEMPTY);
                if (Regex.match_simple ("^/.*[\\^\\$\\*].*/$", patt, RegexCompileFlags.UNGREEDY, RegexMatchFlags.NOTEMPTY)) {
                    if (debug_match)
                        debug ("patt: %s", patt);
                    /* Pattern is a regexp chars */
                    pattern.insert (patt, regex);
                    optslist.insert (patt, opts);
                }
                /* TODO */
                return false;
            }
            catch (Error error) {
                warning ("Adblock compile regexp: %s", error.message);
                return true;
            }
        }

        bool request_handled (string page_uri, string request_uri) {
            /* Always allow the main page */
            if (request_uri == page_uri)
                return false;

            Directive? directive = cache.lookup (request_uri);
            if (directive == null) {
                if (matched_by_key (request_uri, page_uri)
                 || matched_by_pattern (request_uri, page_uri)) {
                    directive = Directive.BLOCK;
                }
                else
                    directive = Directive.ALLOW;
                cache.insert (request_uri, directive);
            }

            return directive == Directive.BLOCK;
        }

        string? fixup_regex (string prefix, string? src) {
            if (src == null)
                return null;

            var fixed = new StringBuilder ();
            fixed.append(prefix);

            uint i = 0;
            if (src[0] == '*')
                i++;
            uint l = src.length;
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
                        fixed.append_printf ("\\%c", c); break;
                    default:
                        fixed.append_c (c); break;
                }
                i++;
            }
            return fixed.str;
        }

        bool matched_by_key (string request_uri, string page_uri) {
            string? uri = fixup_regex ("", request_uri);
            if (uri == null)
                return false;

            uint signature_size = 8;
            uint pos, l = uri.length;
            for (pos = l - signature_size; pos >= 0; pos--) {
                string signature = uri.offset (pos).ndup (signature_size);
                var regex = keys.lookup (signature);
                if (regex == null || blacklist.find (regex) != null)
                    continue;

                if (check_rule (regex, uri, request_uri, page_uri))
                    return true;
                blacklist.prepend (regex);
            }

            return false;
        }

        bool check_rule (Regex regex, string pattern, string request_uri, string page_uri) {
            stdout.printf ("check rule: patt %s req_uri %s, page uri %s\n", pattern, request_uri, page_uri);
            if (regex.match_full (request_uri))
                return false;

            var opts = optslist.lookup (pattern);
            if (opts != null && Regex.match_simple (",third-party", opts,
                RegexCompileFlags.CASELESS, RegexMatchFlags.NOTEMPTY))
                if (page_uri != null && regex.match_full (page_uri))
                    return false;
            if (debug_match)
                debug ("blocked by pattern regexp=%s -- %s", regex.get_pattern (), request_uri);
            return true;
        }

        bool matched_by_pattern (string request_uri, string page_uri) {
            foreach (var patt in pattern.get_keys ())
                if (check_rule (pattern.lookup (patt), patt, request_uri, page_uri))
                    return true;
            return false;
        }
    }
}

#if HAVE_WEBKIT2
Adblock.Filter? filter;
public static void webkit_web_extension_initialize (WebKit.WebExtension web_extension) {
    filter = new Adblock.Filter (web_extension);
}
#else
public Midori.Extension extension_init () {
    return new Adblock.Filter ();
}
#endif

