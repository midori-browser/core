/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    [CCode (cname = "g_hostname_is_ip_address")]
    extern bool hostname_is_ip_address (string hostname);

    public class HSTS : GLib.Object, Soup.SessionFeature {
        public class Directive {
            public Soup.Date? expires = null;
            public bool sub_domains = false;

            public Directive (string max_age, bool include_sub_domains) {
                expires = new Soup.Date.from_string (max_age);
                sub_domains = include_sub_domains;
            }

            public Directive.from_header (string header) {
                var param_list = Soup.header_parse_param_list (header);
                if (param_list == null)
                    return;

                string? max_age = param_list.lookup ("max-age");
                if (max_age == null)
                    return;

                if (max_age != null) {
                    int val = max_age.to_int ();
                    if (val != 0)
                        expires = new Soup.Date.from_now (val);
                }

                if ("includeSubDomains" in header)
                    sub_domains = true;

                Soup.header_free_param_list (param_list);
            }

            public bool is_expired () {
                return expires.is_past ();
            }

            public bool is_valid () {
                // The max-age parameter is *required*
                return expires != null;
            }
        }

        HashTable<string, Directive> whitelist;
        bool debug = false;

        public HSTS () {
            whitelist = new HashTable<string, Directive> (str_hash, str_equal);
            read_cache.begin (File.new_for_path (Paths.get_preset_filename (null, "hsts")));
            read_cache.begin (File.new_for_path (Paths.get_config_filename_for_reading ("hsts")));
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

                    // hostname ' ' expiration-date ' ' allow-subdomains <y|n>
                    string[] parts = line.split (" ", 3);
                    if (parts[0] == null || parts[1] == null || parts[2] == null)
                        continue;

                    var host = parts[0]._strip ();
                    var expire = parts[1]._strip ();
                    var allow_subdomains = bool.parse (parts[2]._strip ());

                    var directive = new Directive (expire, allow_subdomains);
                    if (directive.is_valid () && !directive.is_expired ()) {
                        if (debug)
                            stdout.printf ("HSTS: loading rule for %s\n", host);
                        whitelist_append (host, directive);
                    }
                } while (true);
            }
            catch (Error error) { }
        }

        void queued (Soup.Session session, Soup.Message message) {
            /* Only trust the HSTS headers sent over a secure connection */
            if (message.uri.scheme == "https") {
                message.finished.connect (strict_transport_security_handled);
            }
            else if (whitelist_lookup (message.uri.host)) {
                message.uri.set_scheme ("https");
                session.requeue_message (message);
                if (debug)
                    stdout.printf ("HSTS: Enforce %s\n", message.uri.host);
            }
        }

        bool whitelist_lookup (string host) {
            Directive? directive = null;
            bool is_subdomain = false;

            if (hostname_is_ip_address (host))
                return false;

            // try an exact match first
            directive = whitelist.lookup (host);

            // no luck, try walking the domain tree
            if (directive == null) {
                int offset = 0;
                for (offset = host.index_of_char ('.', offset) + 1;
                     offset > 0;
                     offset = host.index_of_char ('.', offset) + 1) {
                    string component = host.substring(offset);

                    directive = whitelist.lookup (component);
                    if (directive != null) {
                        is_subdomain = true;
                        break;
                    }
                }
            }

            return directive != null && 
                   !directive.is_expired () &&
                   (is_subdomain? directive.sub_domains: true);
        }

        void whitelist_append (string host, Directive directive) {
            whitelist.insert (host, directive);
        }

        void whitelist_remove (string host) {
            whitelist.remove(host);
        }

        async void whitelist_serialize () {
            if (Midori.Paths.is_readonly ())
                return;

            string filename = Paths.get_config_filename_for_writing ("hsts");
            try {
                var file = File.new_for_path (filename);
                var stream = file.replace (null, false, FileCreateFlags.NONE);

                foreach (string host in whitelist.get_keys ()) {
                    var directive = whitelist.lookup (host);

                    // Don't serialize the expired directives
                    if (directive.is_expired ())
                        continue;

                    yield stream.write_async (("%s %s %s\n".printf (host,
                                                                    directive.expires.to_string (Soup.DateFormat.ISO8601_COMPACT),
                                                                    directive.sub_domains.to_string ())).data);
                }
                yield stream.flush_async ();
            }
            catch (Error error) {
                critical ("Failed to update %s: %s", filename, error.message);
            }
        }

        void strict_transport_security_handled (Soup.Message message) {
            if (message == null || message.uri == null)
                return;

            unowned string? hsts = message.response_headers.get_one ("Strict-Transport-Security");
            if (hsts == null)
                return;

            var directive = new Directive.from_header (hsts);

            if (debug)
                stdout.printf ("HSTS: '%s' sets '%s' valid? %s\n",
                    message.uri.host, hsts, directive.is_valid ().to_string ());

            if (directive.is_valid ())
                whitelist_append (message.uri.host, directive);
            else
                whitelist_remove (message.uri.host);


            whitelist_serialize.begin ();
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
    }
}
