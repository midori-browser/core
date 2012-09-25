/*
 Copyright (C) 2007-2012 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009 Jean-François Guchens <zcx000@gmail.com>
 Copyright (C) 2011 Peter Hatina <phatina@redhat.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public enum NewView {
        TAB,
        BACKGROUND,
        WINDOW,
    }
    /* Since: 0.1.2 */

    public enum Security {
        NONE, /* The connection is neither encrypted nor verified. */
        UNKNOWN, /* The security is unknown, due to lack of validation. */
        TRUSTED /* The security is validated and trusted. */
    }
    /* Since: 0.2.5 */

    [CCode (cprefix = "MIDORI_LOAD_")]
    public enum LoadStatus {
        FINISHED, /* The current website is fully loaded. */
        COMMITTED, /* Data is being loaded and rendered. */
        PROVISIONAL /* A new URI was scheduled. */
    }

    public class Tab : Gtk.VBox {
        private string current_uri = "about:blank";
        public string uri { get {
            return current_uri;
        }
        protected set {
            current_uri = Midori.URI.format_for_display (value);
        }
        }

        /* Since: 0.4.8 */
        public string mime_type { get; protected set; default = "text/plain"; }
        /* Since: 0.1.2 */
        public Security security { get; protected set; default = Security.NONE; }
        public LoadStatus load_status { get; protected set; default = LoadStatus.FINISHED; }
        public string? statusbar_text { get; protected set; default = null; }

        private double current_progress = 0.0;
        public double progress { get {
            return current_progress;
        }
        protected set {
            /* When we are finished, we don't want to *see* progress anymore */
            if (load_status == LoadStatus.FINISHED)
                current_progress = 0.0;
            /* Full progress but not finished: presumably all loaded */
            else if (value == 1.0)
                current_progress = 0.0;
            /* When loading we want to see at minimum 10% progress */
            else
                current_progress = value.clamp (0.1, 1.0);
        }
        }

        public bool is_blank () {
            return URI.is_blank (uri);
        }

        public bool can_view_source () {
            if (is_blank ())
                return false;
            string content_type = ContentType.from_mime_type (mime_type);
#if HAVE_WIN32
            /* On Win32 text/plain maps to ".txt" but is_a expects "text" */
            string text_type = "text";
#else
            string text_type = ContentType.from_mime_type ("text/plain");
#endif
            return ContentType.is_a (content_type, text_type);
        }

        public static string get_display_title (string? title, string uri) {
            /* Render filename as title of patches */
            if (title == null && (uri.has_suffix (".diff") || uri.has_suffix (".patch")))
                return File.new_for_uri (uri).get_basename ();

            /* Work-around libSoup not setting a proper directory title */
            if (title == null || (title == "OMG!" && uri.has_prefix ("file://")))
                return uri;

#if !HAVE_WIN32
            /* If left-to-right text is combined with right-to-left text the default
               behaviour of Pango can result in awkwardly aligned text. For example
               "‪بستيان نوصر (hadess) | An era comes to an end - Midori" becomes
               "hadess) | An era comes to an end - Midori) بستيان نوصر". So to prevent
               this we insert an LRE character before the title which indicates that
               we want left-to-right but retains the direction of right-to-left text. */
            if (!title.has_prefix ("‪"))
                return "‪" + title;
#endif
            return title;
        }

        public static Pango.EllipsizeMode get_display_ellipsize (string title, string uri) {
            if (title == uri)
                return Pango.EllipsizeMode.START;

            if (title.has_suffix (".diff") || title.has_suffix (".patch"))
                return Pango.EllipsizeMode.START;

            string[] parts = title.split (" ");
            if (uri.has_suffix (parts[parts.length - 1].down ()))
                return Pango.EllipsizeMode.START;

            return Pango.EllipsizeMode.END;
        }
    }
}
