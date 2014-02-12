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
    enum Directive {
        ALLOW,
        BLOCK
    }

    public class Extension : Midori.Extension {
        HashTable<string, Directive?> cache;
        internal Pattern pattern;
        Keys keys;
        Options optslist;

#if HAVE_WEBKIT2
        public Extension (WebKit.WebExtension web_extension) {
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
        public Extension () {
            GLib.Object (name: _("Advertisement blocker"),
                         description: _("Block advertisements according to a filter list"),
                         version: "2.0",
                         authors: "Christian Dywan <christian@twotoasts.de>");
            // TODO: install_string_list ("filters", null);
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
            WebKit.WebResource resource, WebKit.NetworkRequest request, WebKit.NetworkResponse? response) {

            if (request_handled (web_view.uri, request.uri))
                request.set_uri ("about:blank");
        }
#endif

        internal void init () {
            debug ("Adblock2");
            reload_rules ();
        }

        void reload_rules () {
            cache = new HashTable<string, Directive?> (str_hash, str_equal);
            optslist = new Options ();
            pattern = new Pattern (optslist);
            keys = new Keys (optslist);

#if HAVE_WEBKIT2
            string config_dir = GLib.Path.build_filename (GLib.Environment.get_user_config_dir (), "midori", "extensions", "libadblock.so"); // FIXME
#else
            string? config_dir = get_config_dir ();
#endif
            string filename = GLib.Path.build_filename (config_dir, "config"); // use midori vapi
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
            } catch (FileError.NOENT exist_error) {
                /* It's no error if no config file exists */
            } catch (GLib.Error settings_error) {
                stderr.printf ("Error reading settings from %s: %s\n", filename, settings_error.message);
            }
        }

        internal void parse_line (string? line) throws Error {
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
            if (line.has_prefix ("|")) {
                /* TODO: handle options and domains excludes */
                if (line.contains("$"))
                    return;

                if (line.has_prefix ("||"))
                    add_url_pattern ("", "fulluri", line.offset (2));
                else
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
                RegexCompileFlags.CASELESS, RegexMatchFlags.NOTEMPTY))
                return;

            string format_patt = fixup_regex (prefix, patt);
            debug ("got: %s opts %s", format_patt, opts);
            compile_regexp (format_patt, opts);
            /* return format_patt */
        }

        bool compile_regexp (string? patt, string opts) throws Error {
            if (patt == null)
                return false;
            try {
                var regex = new Regex (patt, RegexCompileFlags.OPTIMIZE, RegexMatchFlags.NOTEMPTY);
                /* is pattern is already a regex? */
                if (Regex.match_simple ("^/.*[\\^\\$\\*].*/$", patt, RegexCompileFlags.UNGREEDY, RegexMatchFlags.NOTEMPTY)) {
                    debug ("patt: %s", patt);
                    pattern.insert (patt, regex);
                    optslist.insert (patt, opts);
                    return false;
                } else { /* nope, no regex */
                    int pos = 0, len;
                    int signature_size = 8;
                    string sig;
                    len = patt.length;

                    /* chop up pattern into substrings for faster matching */
                    for (pos = len - signature_size; pos>=0; pos--)
                    {
                        sig = patt.offset (pos).ndup (signature_size);
                        /* we don't have a * nor \\, does not look like regex, save chunk as "key" */
                        if (!Regex.match_simple ("[\\*]", sig, RegexCompileFlags.UNGREEDY, RegexMatchFlags.NOTEMPTY) && keys.lookup (sig) == null) {
                            keys.insert (sig, regex);
                            optslist.insert (sig, opts);
                        } else {
                            /* starts with * or \\ - save as regex */
                            if ((sig.has_prefix ("*") || sig.has_prefix("\\")) && pattern.lookup (sig) == null) {
                                pattern.insert (sig, regex);
                                optslist.insert (sig, opts);
                            }
                        }
                    }
                }
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
                directive = Directive.ALLOW;
                try {
                    if (keys.match (request_uri, page_uri)
                     || pattern.match (request_uri, page_uri))
                        directive = Directive.BLOCK;
                } catch (Error error) {
                    warning ("Adblock match error: %s", error.message);
                }
                cache.insert (request_uri, directive);
            }

            return directive == Directive.BLOCK;
        }
    }

    static void debug (string format, ...) {
        bool debug_match = "adblock:match" in (Environment.get_variable ("MIDORI_DEBUG") ?? "");
        if (!debug_match)
            return;

        var args = va_list ();
        stdout.vprintf (format + "\n", args);
    }

    internal static string? fixup_regex (string prefix, string? src) {
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
}

#if HAVE_WEBKIT2
Adblock.Extension? filter;
public static void webkit_web_extension_initialize (WebKit.WebExtension web_extension) {
    filter = new Adblock.Extension (web_extension);
}
#else
public Midori.Extension extension_init () {
    return new Adblock.Extension ();
}
#endif

// FIXME: Allow test to build with WK2
#if !HAVE_WEBKIT2
struct TestCaseLine {
    public string line;
    public string fixed;
    public bool added;
}

const TestCaseLine[] lines = {
    { null, null, false },
    { "!", "!", false },
    { "@@", "@@", false },
    { "##", "##", false },
    { "[", "\\[", false },
    { "+advert/", "advert/", false },
    { "*foo", "foo", false },
    // TODO:
};

void test_adblock_parse () {
    var filter = new Adblock.Extension ();
    filter.init ();
    foreach (var line in lines) {
        try {
            uint i = filter.pattern.size ();
            Katze.assert_str_equal (line.line, Adblock.fixup_regex ("", line.line), line.fixed);
            filter.parse_line (line.line);
            // Added a pattern?
            if (line.added)
                assert (filter.pattern.size () == i + 1);
        } catch (Error error) {
            GLib.error ("Line '%s' didn't parse: %s", line.line, error.message);
        }
    }
}

void test_adblock_pattern () {
}

void test_subscription_update () {
}

public void extension_test () {
    Test.add_func ("/extensions/adblock2/parse", test_adblock_parse);
    Test.add_func ("/extensions/adblock2/pattern", test_adblock_pattern);
    Test.add_func ("/extensions/adblock2/update", test_subscription_update);
}
#endif

