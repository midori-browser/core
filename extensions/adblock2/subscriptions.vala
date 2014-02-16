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
    public class Subscription : GLib.Object {
        public string? path;
        public string uri { get; set; default = null; }
        public bool active { get; set; default = true; }
        public Pattern pattern;
        public Keys keys;
        public Options optslist;
        public Whitelist whitelist;
        WebKit.Download? download;
        string expires_meta { get; set; default = null; }
        string last_mod_meta { get; set; default = null; }
        public int64 update_tstamp { get; set; default = 0; }
        public int64 last_mod_tstamp { get; set; default = 0; }
        public int64 last_check_tstamp { get; set; default = 0; }

        public Subscription (string uri) {
            this.uri = uri;
            active = uri[4] != '-' && uri[5] != '-';
            clear ();
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
            if (!header.contains (":"))
                return;
            string[] parts = header.split (":", 2);
            if (parts[0] == null)
                return;
            string key = parts[0].substring (2, -1);
            string value = parts[1].substring (1, -1);
            if (key.contains ("xpires"))
                this.expires_meta = value;
            if (key.has_prefix ("Last mod") || key.has_prefix ("Updated"))
                this.last_mod_meta = value;
            debug ("Header '%s' says '%s'", key, value);
        }

        int get_month_from_string (string? month) {
            if (month == null)
                return 0;

            string[] months = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
            for (int i = 0; i<= months.length; i++)
            {
                if (month.has_prefix (months[i]))
                    return i+1;
            }
            return 0;
        }

        public bool needs_updating () {
            DateTime now = new DateTime.now_local ();
            DateTime expire_date = null;
            DateTime last_mod_date = null;
            string? last_mod = this.last_mod_meta;
            string? expires = this.expires_meta;

            /* We have "last modification" metadata */
            if (last_mod != null) {
                int h = 0, min = 0, d, m, y;
                /* Date in a form of: 20.08.2012 12:34 */
                if (last_mod.contains (".") || last_mod.contains("-")) {
                    string[] parts = last_mod.split (" ", 2);
                    string[] date_parts;
                    string split_char = " ";

                    /* contains time part ? */
                    if (parts[1] != "" && parts[1].contains (":")) {
                        string[] time_parts = parts[1].split (":", 2);
                        h = int.parse(time_parts[0]);
                        min = int.parse(time_parts[1]);
                    }

                    /* check if dot or dash was used as a delimiter */
                    if (parts[0].contains ("."))
                        split_char = ".";
                    else if (parts[0].contains ("-"))
                        split_char = "-";

                    date_parts = parts[0].split (split_char, 3);
                    m = int.parse(date_parts[1]);
                    if (date_parts[2].length == 4) {
                        y = int.parse(date_parts[2]);
                        d = int.parse(date_parts[0]);
                    } else {
                        y = int.parse(date_parts[0]);
                        d = int.parse(date_parts[2]);
                    }
                } else { /* Date in a form of: 20 Mar 2012 12:34 */
                    string[] parts = last_mod.split (" ", 4);
                    /* contains time part ? */
                    if (parts[3] != null && parts[3].contains (":")) {
                        string[] time_parts = parts[3].split (":", 2);
                        h = int.parse(time_parts[0]);
                        min = int.parse(time_parts[1]);
                    }

                    m = get_month_from_string (parts[1]);
                    if (parts[2].length == 4) {
                        y = int.parse(parts[2]);
                        d = int.parse(parts[0]);
                    } else {
                        y = int.parse(parts[0]);
                        d = int.parse(parts[2]);
                    }
                }

                last_mod_date = new DateTime.local (y, m, d, h, min, 0.0);
            }

            if (last_mod_date == null)
                last_mod_date = now;

            expire_date = last_mod_date;

            /* We have "expires" metadata */

            /* Data in form: Expires: 5 days (update frequency) */
            if (expires.has_prefix ("Expires:")) {
                string[] parts = expires.split (" ", 4);
                expire_date.add_days (int.parse (parts[1]));
            } else if (expires.contains ("expires after")) { /*  This list expires after 14 days|hours */
                string[] parts = expires.split (" ", 7);
                if (parts[5] == "hours")
                    expire_date.add_hours (int.parse (parts[4]));
                else if (parts[5] == "days")
                    expire_date.add_days (int.parse (parts[4]));
            } else { /* No expire metadata found, assume x days */
                /* XXX: user could control this? */
                int days_to_expire = 7;
                expire_date.add_days (days_to_expire);
            }

            /* TODO: save "last modification timestamp" to keyfile */
            this.last_mod_tstamp = last_mod_date.to_unix ();
            this.last_check_tstamp = now.to_unix ();
            this.update_tstamp = expire_date.to_unix ();

            /* Check if we are past expire date */
            if (now.compare (expire_date) != -1)
                return false;

            return true;
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
