/*
 Copyright (C) 2011 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace GLib {
    extern static string hostname_to_unicode (string hostname);
    extern static string hostname_to_ascii (string hostname);
}

namespace Midori {
    public class URI : Object {
        public static string? parse_hostname (string? uri, out string path) {
            /* path may be null. */
            if (&path != null)
                path = null;
            if (uri == null)
                return uri;
            unowned string? hostname = uri.chr (-1, '/');
            if (hostname == null || hostname[1] != '/'
             || hostname.chr (-1, ' ') != null)
                return null;
            hostname = hostname.offset (2);
            if (&path != null) {
                if ((path = hostname.chr (-1, '/')) != null)
                    return hostname.split ("/")[0];
            }
            return hostname;
        }
        /* Deprecated: 0.4.3 */
        public static string parse (string uri, out string path) {
            return parse_hostname (uri, out path) ?? uri;
        }
        public static string to_ascii (string uri) {
            /* Convert hostname to ASCII. */
            string? proto = null;
            if (uri.chr (-1, '/') != null && uri.chr (-1, ':') != null)
                proto = uri.split ("://")[0];
            string? path = null;
            string? hostname = parse_hostname (uri, out path) ?? uri;
            string encoded = hostname_to_ascii (hostname);
            if (encoded != null) {
                return (proto ?? "")
                     + (proto != null ? "://" : "")
                     + encoded + path;
            }
            return uri;
        }
        public static string unescape (string uri) {
            /* Unescape, pass through + and %20 */
            if (uri.chr (-1, '%') != null || uri.chr (-1, ' ') != null) {
                /* Preserve %20 for pasting URLs into other windows */
                string? unescaped = GLib.Uri.unescape_string (uri, "+");
                if (unescaped == null)
                    return uri;
                return unescaped.replace (" ", "%20").replace ("\n", "%0A");
            }
            return uri;
        }

        /* Strip http(s), file and www. for tab titles or completion */
        public static string strip_prefix_for_display (string uri) {
            if (is_http (uri) || uri.has_prefix ("file://")) {
                string stripped_uri = uri.split ("://")[1];
                if (is_http (uri) && stripped_uri.has_prefix ("www."))
                    return stripped_uri.substring (4, -1);
                return stripped_uri;
            }
            return uri;
        }

        public static string format_for_display (string? uri) {
            /* Percent-decode and decode puniycode for user display */
            if (uri != null && uri.has_prefix ("http://")) {
                string unescaped = unescape (uri);
                if (unescaped == null)
                    return uri;
                else if (!unescaped.validate ())
                    return uri;
                string path;
                string hostname = parse_hostname (unescaped, out path);
                string decoded = hostname_to_unicode (hostname);
                if (decoded != null)
                    return "http://" + decoded + path;
                return unescaped;
            }
            return uri;
        }
        public static string for_search (string? uri, string keywords) {
            /* Take a search engine URI and insert specified keywords.
               Keywords are percent-encoded. If the uri contains a %s
               the keywords are inserted there, otherwise appended. */
            if (uri == null)
                return keywords;
            string escaped = GLib.Uri.escape_string (keywords, ":/", true);
            /* Allow DuckDuckGo to distinguish Midori and in turn share revenue */
            if (uri == "https://duckduckgo.com/?q=%s")
                return "https://duckduckgo.com/?q=%s&t=midori".printf (escaped);
            if (uri.str ("%s") != null)
                return uri.printf (escaped);
            return uri + escaped;
        }
        public static bool is_blank (string? uri) {
            return !(uri != null && uri != "" && !uri.has_prefix ("about:"));
        }
        public static bool is_http (string? uri) {
            return uri != null
             && (uri.has_prefix ("http://") || uri.has_prefix ("https://"));
        }
        public static bool is_resource (string? uri) {
            return uri != null
              && (is_http (uri)
               || (uri.has_prefix ("data:") && uri.chr (-1, ';') != null));
        }
        public static bool is_location (string? uri) {
            /* file:// is not considered a location for security reasons */
            return uri != null
             && ((uri.str ("://") != null && uri.chr (-1, ' ') == null)
              || is_http (uri)
              || uri.has_prefix ("about:")
              || (uri.has_prefix ("data:") && uri.chr (-1, ';') != null)
              || (uri.has_prefix ("geo:") && uri.chr (-1, ',') != null)
              || uri.has_prefix ("javascript:"));
        }

        public static bool is_ip_address (string? uri) {
            /* Quick check for IPv4 or IPv6, no validation.
               FIXME: Schemes are not handled
               hostname_is_ip_address () is not used because
               we'd have to separate the path from the URI first. */
            if (uri == null)
                return false;
            /* Skip leading user/ password */
            if (uri.chr (-1, '@') != null)
                return is_ip_address (uri.split ("@")[1]);
            /* IPv4 */
            if (uri[0] != '0' && uri[0].isdigit () && (uri.chr (4, '.') != null))
                return true;
            /* IPv6 */
            if (uri[0].isalnum () && uri[1].isalnum ()
             && uri[2].isalnum () && uri[3].isalnum () && uri[4] == ':'
             && (uri[5] == ':' || uri[5].isalnum ()))
                return true;
            return false;
        }
        public static bool is_valid (string? uri) {
            return uri != null
             && uri.chr (-1, ' ') == null
             && (URI.is_location (uri) || uri.chr (-1, '.') != null);
        }

        public static string? get_folder (string uri) {
            /* Base the start folder on the current view's uri if it is local */
            try {
                string? filename = Filename.from_uri (uri);
                if (filename != null) {
                    string? dirname = Path.get_dirname (filename);
                    if (dirname != null && FileUtils.test (dirname, FileTest.IS_DIR))
                        return dirname;
                }
            }
            catch (Error error) { }
            return null;
        }

        public static GLib.ChecksumType get_fingerprint (string uri,
            out string checksum, out string label) {

            /* http://foo.bar/baz/spam.eggs#!algo!123456 */
            unowned string display = null;
            GLib.ChecksumType type = (GLib.ChecksumType)int.MAX;

            unowned string delimiter = "#!md5!";
            unowned string? fragment = uri.str (delimiter);
            if (fragment != null) {
                display = _("MD5-Checksum:");
                type = GLib.ChecksumType.MD5;
            }

            delimiter = "#!sha1!";
            fragment = uri.str (delimiter);
            if (fragment != null) {
                display = _("SHA1-Checksum:");
                type = GLib.ChecksumType.SHA1;
            }

            /* No SHA256: no known usage and no need for strong encryption */

            if (&checksum != null)
                checksum = fragment != null
                    ? fragment.offset (delimiter.length) : null;
            if (&label != null)
                label = display;
            return type;
        }
    }
}
