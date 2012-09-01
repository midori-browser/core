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
                string? max_age = param_list.lookup ("max-age");
                if (max_age != null)
                    expires = new Soup.Date.from_now (max_age.to_int ());
                if (param_list.lookup_extended ("includeSubDomains", null, null))
                    sub_domains = true;
                Soup.header_free_param_list (param_list);
            }

            public bool is_valid () {
                return expires != null && !expires.is_past ();
            }
        }

        public string? filename { get; set; default = null; }
        HashTable<string, Directive> whitelist;
        bool debug = false;

        public HSTS (owned string new_filename) {
            filename = new_filename;
            whitelist = new HashTable<string, Directive> (str_hash, str_equal);
            if (strcmp (Environment.get_variable ("MIDORI_DEBUG"), "hsts") == 0)
                debug = true;
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

        void queued (Soup.Session session, Soup.Message message) {
            Directive? directive = whitelist.lookup (message.uri.host);
            if (directive != null && directive.is_valid ()) {
                message.uri.set_scheme ("https");
                session.requeue_message (message);
                if (debug)
                    stdout.printf ("HTPS: Enforce %s\n", message.uri.to_string (false));
            }
            else if (message.uri.scheme == "http")
                message.finished.connect (strict_transport_security_handled);
        }

        void strict_transport_security_handled (Soup.Message message) {
            if (message == null || message.uri == null)
                return;

            unowned string? hsts = message.response_headers.get_one ("Strict-Transport-Security");
            if (hsts == null)
                return;

            var directive = new Directive.from_header (hsts);
            if (directive.is_valid ())
                whitelist.insert (message.uri.host, directive);
            if (debug)
                stdout.printf ("HTPS: '%s' sets '%s' valid? %s\n",
                    message.uri.to_string (false), hsts, directive.is_valid ().to_string ());
        }

    }
}
