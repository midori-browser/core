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
    public abstract class Feature : GLib.Object {
        public abstract bool header (string key, string value);
    }

    public class Subscription : GLib.Object {
        public string? path;
        public string uri { get; set; default = null; }
        public bool active { get; set; default = true; }
        List<Feature> features;
        public Pattern pattern;
        public Keys keys;
        public Options optslist;
        public Whitelist whitelist;
        WebKit.Download? download;

        public Subscription (string uri) {
            this.uri = uri;
            active = uri[4] != '-' && uri[5] != '-';
            clear ();
        }

        public void add_feature (Feature feature) {
            features.append (feature);
        }

        public void clear () {
            this.optslist = new Options ();
            this.pattern = new Pattern (optslist);
            this.keys = new Keys (optslist);
            this.whitelist = new Whitelist (optslist);
        }

        internal void parse_line (string? line) throws Error {
            if (line.has_prefix ("@@")) {
                if (line.contains("$") && line.contains ("domain"))
                    return;
                if (line.has_prefix ("@@||"))
                    add_url_pattern ("^", "whitelist", line.offset (4));
                else if (line.has_prefix ("@@|"))
                    add_url_pattern ("^", "whitelist", line.offset (3));
                else
                    add_url_pattern ("", "whitelist", line.offset (2));
                return;
            }
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
                if (Regex.match_simple ("^/.*[\\^\\$\\*].*/$", patt,
                    RegexCompileFlags.UNGREEDY, RegexMatchFlags.NOTEMPTY)
                 || opts != null && opts.contains ("whitelist")) {
                    debug ("patt: %s", patt);
                    if (opts.contains ("whitelist"))
                        this.whitelist.insert (patt, regex);
                    else
                        this.pattern.insert (patt, regex);
                    this.optslist.insert (patt, opts);
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
                            this.keys.insert (sig, regex);
                            this.optslist.insert (sig, opts);
                        } else {
                            /* starts with * or \\ - save as regex */
                            if ((sig.has_prefix ("*") || sig.has_prefix("\\")) && this.pattern.lookup (sig) == null) {
                                this.pattern.insert (sig, regex);
                                this.optslist.insert (sig, opts);
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

        public void parse_header (string header) throws Error {
            /* Headers come in two forms
               ! Foo: Bar
               ! Some freeform text
             */
            string key = header;
            string value = "";
            if (header.contains (":")) {
                string[] parts = header.split (":", 2);
                if (parts[0] != null) {
                    key = parts[0].substring (2, -1);
                    value = parts[1].substring (1, -1);
                }
            }
            debug ("Header '%s' says '%s'", key, value);
            foreach (var feature in features) {
                if (feature.header (key, value))
                    break;
            }
        }

        void download_status (ParamSpec pspec) {
            if (download.get_status () != WebKit.DownloadStatus.FINISHED)
                return;

            download = null;
            try {
                parse ();
            } catch (Error error) {
                warning ("Error parsing %s: %s", uri, error.message);
            }
        }

        public void parse () throws Error
        {
            if (!active)
                return;

            debug ("Parsing %s (%s)", uri, path);

            clear ();

            if (uri.has_prefix ("file://"))
                path = Filename.from_uri (uri);
            else {
                string cache_dir = GLib.Path.build_filename (GLib.Environment.get_home_dir (), ".cache", "midori", "adblock");
                Midori.Paths.mkdir_with_parents (cache_dir);
                string filename = Checksum.compute_for_string (ChecksumType.MD5, this.uri, -1);
                path = GLib.Path.build_filename (cache_dir, filename);
            }

            File filter_file = File.new_for_path (path);
            DataInputStream stream;
            try  {
                stream = new DataInputStream (filter_file.read ());
            } catch (IOError.NOT_FOUND exist_error) {
#if HAVE_WEBKIT2
                /* TODO */
#else
                if (download != null)
                    return;

                download = new WebKit.Download (new WebKit.NetworkRequest (uri));
                download.destination_uri = Filename.to_uri (path, null);
                download.notify["status"].connect (download_status);
                debug ("Fetching %s to %s now", uri, download.destination_uri);
                download.start ();
#endif
                return;
            }

            string? line;
            while ((line = stream.read_line (null)) != null) {
                if (line == null)
                    continue;
                string chomped = line.chomp ();
                if (chomped == "")
                    continue;
                if (line[0] == '!')
                    parse_header (chomped);
                else
                    parse_line (chomped);
            }
        }

        public Directive? get_directive (string request_uri, string page_uri) {
            try {
                if (this.whitelist.match (request_uri, page_uri))
                    return Directive.ALLOW;

                if (this.keys.match (request_uri, page_uri))
                    return Directive.BLOCK;

                if (this.pattern.match (request_uri, page_uri))
                    return Directive.BLOCK;
            } catch (Error error) {
                warning ("Adblock match error: %s\n", error.message);
            }
            return null;
        }
    }
}
