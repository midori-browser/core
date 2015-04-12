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
        static string? fork_uri = null;

        public static string? parse_hostname (string? uri, out string path) {
            path = null;
            if (uri == null)
                return uri;
            unowned string? hostname = uri.chr (-1, '/');
            if (hostname == null || hostname[1] != '/'
             || hostname.chr (-1, ' ') != null)
                return null;
            hostname = hostname.offset (2);
            if ((path = hostname.chr (-1, '/')) != null)
                return hostname.split ("/")[0];
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
        public static string get_base_domain (string uri) {
#if HAVE_LIBSOUP_2_40_0
            try {
                string ascii = to_ascii (uri);
                return Soup.tld_get_base_domain (ascii);
            } catch (Error error) {
                /* This is fine, we fallback to hostname */
            }
#endif
            return parse_hostname (uri, null);
        }

        public static string unescape (string uri_str) {
            /* We cannot use g_uri_unescape_string, because it returns NULL if it
               encounters the sequence '%00', whereas the goal of this function is
               to unescape all escape sequences except %00, %0A, %0D, %20, and %25 */
            size_t len = uri_str.length;
            uint8[] uri = uri_str.data;
            var escaped = new StringBuilder();
            for (var i=0; i < len; i++)
            {
                uint8 c = uri[i];
                if (c == '%')
                {
                    /* only unescape if there are enough chars for a valid escape sequence */
                    if (i + 2 < len)
                    {
                        var x1 = ((char)uri[i+1]).xdigit_value();
                        var x2 = ((char)uri[i+2]).xdigit_value();
                        var x = (x1<<4) + x2;
                        /* if the escape is valid and the character should be unescaped */
                        if (x1 >= 0 && x2 >= 0 && x != '\0' && x != '\n' && x != '\r' && x != ' ' && x != '%')
                        {
                            /* consume the encoded characters */
                            c = (uint8)x;
                            i += 2;
                        }
                    }
                }
                escaped.append_c((char)c);
            }
            return escaped.str;
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
                string unescaped = unescape (uri).replace(" ", "%20");
                if (!unescaped.validate ())
                    return uri;
                string path;
                string? hostname = parse_hostname (unescaped, out path);
                if (hostname != null) {
                    string decoded = hostname_to_unicode (hostname);
                    if (decoded != null)
                        return "http://" + decoded + path;
                }
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

            checksum = fragment != null ? fragment.offset (delimiter.length) : null;
            label = display;
            return type;
        }

        /*
          Protects against recursive invokations of Midori with the same URI.
          Consider a tel:// URI opened via Tab.open_uri, being handed off to GIO,
          which in turns calls exo-open, which in turn can't open tel:// and falls
          back to the browser ie. Midori.
          So: code opening URIs calls this function with %true, #Midori.App passes %false.

          Since: 0.5.8
         */
        public static bool recursive_fork_protection (string uri, bool set_uri) {
            if (set_uri)
                fork_uri = uri;
            return fork_uri != uri;
        }

        /**
         * Returns a Glib.Icon for the given @uri.
         *
         * Since: 0.5.8
         **/
        public static async GLib.Icon? get_icon (string uri, Cancellable? cancellable=null) throws Error {
#if HAVE_WEBKIT2
            var database = WebKit.WebContext.get_default ().get_favicon_database ();
            var surface = yield database.get_favicon (uri, cancellable);
            var image = (Cairo.ImageSurface)surface;
            var pixbuf = Gdk.pixbuf_get_from_surface (image, 0, 0, image.get_width (), image.get_height ());
#else
            var database = WebKit.get_favicon_database ();
            // We must not pass a Cancellable due to a crasher bug
            var pixbuf = yield database.get_favicon_pixbuf (uri, 0, 0, null);
#endif
            return pixbuf as GLib.Icon;
        }

        /**
         * Returns a Glib.Icon for the given @uri or falls back to @fallback.
         *
         * Since: 0.5.8
         **/
        public static async GLib.Icon? get_icon_fallback (string uri, GLib.Icon? fallback=null, Cancellable? cancellable=null) {
        try {
                return yield get_icon (uri, cancellable);
            } catch (Error error) {
                debug ("Icon failed to load: %s", error.message);
                return fallback;
            }
        }

        /**
         * A Glib.Icon subclass that loads the icon for a given URI.
         * In the case of an error @fallback will be used.
         *
         * Since: 0.5.8
         **/
        public class Icon : InitiallyUnowned, GLib.Icon, LoadableIcon {
            public string uri { get; private set; }
            public GLib.Icon? fallback { get; private set; }
            InputStream? stream = null;
            public Icon (string website_uri, GLib.Icon? fallback=null) {
                uri = website_uri;
                /* TODO: Use fallback */
                this.fallback = fallback;
            }
            public bool equal (GLib.Icon? other) {
                return other is Icon && (other as Icon).uri == uri;
            }
            public uint hash () {
                return uri.hash ();
            }
            public InputStream load (int size, out string? type = null, Cancellable? cancellable = null) throws Error {
                /* Implementation notes:
                   GTK+ up to GTK+ 3.10 loads any GLib.Icon synchronously
                   Favicons may be cached but usually trigger loading here
                   Only one async code path in favour of consistent results
                 */
                if (stream != null) {
                    type = "image/png";
                    return stream;
                }
                load_async.begin (size, cancellable, (obj, res)=>{
                    try {
                        stream = load_async.end (res);
                    }
                    catch (Error error) {
                       debug ("Icon failed to load: %s", error.message);
                    }
                });
                throw new FileError.EXIST ("Triggered load - no data yet");
            }

            public async InputStream load_async (int size, Cancellable? cancellable = null, out string? type = null) throws Error {
                type = "image/png";
                if (stream != null)
                    return stream;
                var icon = yield get_icon (uri, cancellable);
                if (icon != null && icon is Gdk.Pixbuf) {
                    var pixbuf = icon as Gdk.Pixbuf;
                    // TODO: scale it to "size" here
                    uint8[] buffer;
                    pixbuf.save_to_buffer (out buffer, "png");
                    stream = new MemoryInputStream.from_data (buffer, null);
                }
                else
                    throw new FileError.EXIST ("No icon available");
                return stream;
            }
        }
    }
}
