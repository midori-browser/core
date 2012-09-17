/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class HSTS : GLib.Object, Soup.SessionFeature {
        public class Directive {
            public Soup.Date? expires = null;
            public bool sub_domains = false;

            public Directive (bool include_sub_domains) {
                expires = new Soup.Date.from_now (int.MAX);
                sub_domains = include_sub_domains;
            }

            public Directive.from_header (string header) {
                var param_list = Soup.header_parse_param_list (header);
                if (param_list == null)
                    return;

                string? max_age = param_list.lookup ("max-age");
                if (max_age != null)
                    expires = new Soup.Date.from_now (max_age.to_int ());
                // if (param_list.lookup_extended ("includeSubDomains", null, null))
                if ("includeSubDomains" in header)
                    sub_domains = true;
                Soup.header_free_param_list (param_list);
            }

            public bool is_valid () {
                return expires != null && !expires.is_past ();
            }
        }

        File file;
        HashTable<string, Directive> whitelist;
        bool debug = false;

        public HSTS (owned string filename) {
            whitelist = new HashTable<string, Directive> (str_hash, str_equal);
            read_cache (File.new_for_path (Paths.get_preset_filename (null, "hsts")));
            file = File.new_for_path (filename);
            read_cache (file);
            if (strcmp (Environment.get_variable ("MIDORI_DEBUG"), "hsts") == 0)
                debug = true;
        }

        async void read_cache (File file) {
            try {
                var stream = new DataInputStream (yield file.read_async ());
                do {
                    string? line = yield stream.read_line_async ();
                    if (line == null)
                        break;
                    string[] parts = line.split (" ", 2);
                    if (parts[0] == null || parts[1] == null)
                        break;
                    var directive = new Directive.from_header (parts[1]);
                    if (directive.is_valid ())
                        append_to_whitelist (parts[0], directive);
                } while (true);
            }
            catch (Error error) { }
        }

        /* No sub-features */
        public bool add_feature (Type type) { return false; }
        public bool remove_feature (Type type) { return false; }
        public bool has_feature (Type type) { return false; }

        public void attach (Soup.Session session) { session.request_queued.connect (queued); }
        public void detach (Soup.Session session) { /* FIXME disconnect */ }

        /* Never called but required by the interface */
        public void request_started (Soup.Session session, Soup.Message msg, Soup.Socket socket) { }
        public void request_queued (Soup.Session session, Soup.Message message) { }
        public void request_unqueued (Soup.Session session, Soup.Message msg) { }

        bool should_secure_host (string host) {
            Directive? directive = whitelist.lookup (host);
            if (directive == null)
                directive = whitelist.lookup ("*." + host);
            return directive != null && directive.is_valid ();
        }

        void queued (Soup.Session session, Soup.Message message) {
            if (should_secure_host (message.uri.host)) {
                message.uri.set_scheme ("https");
                session.requeue_message (message);
                if (debug)
                    stdout.printf ("HSTS: Enforce %s\n", message.uri.host);
            }
            else if (message.uri.scheme == "http")
                message.finished.connect (strict_transport_security_handled);
        }

        void append_to_whitelist (string host, Directive directive) {
            whitelist.insert (host, directive);
            if (directive.sub_domains)
                whitelist.insert ("*." + host, directive);
        }

        async void append_to_cache (string host, string header) {
            if (Midori.Paths.is_readonly ())
                return;

            try {
                var stream = file.append_to/* FIXME _async*/ (FileCreateFlags.NONE);
                yield stream.write_async ((host + " " + header + "\n").data);
                yield stream.flush_async ();
            }
            catch (Error error) {
                critical ("Failed to update %s: %s", file.get_path (), error.message);
            }
        }

        void strict_transport_security_handled (Soup.Message message) {
            if (message == null || message.uri == null)
                return;

            unowned string? hsts = message.response_headers.get_one ("Strict-Transport-Security");
            if (hsts == null)
                return;

            var directive = new Directive.from_header (hsts);
            if (directive.is_valid ()) {
                append_to_whitelist (message.uri.host, directive);
                append_to_cache (message.uri.host, hsts);
            }
            if (debug)
                stdout.printf ("HSTS: '%s' sets '%s' valid? %s\n",
                    message.uri.host, hsts, directive.is_valid ().to_string ());
        }

    }
}
