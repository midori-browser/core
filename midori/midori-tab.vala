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
        public WebKit.WebView web_view { get; private set; }

        #if HAVE_GRANITE_CLUTTER
        public Granite.Widgets.NavigationBox navigation_box { get; private set; }
        #endif

        private string current_uri = "about:blank";
        public string uri { get {
            return current_uri;
        }
        protected set {
            current_uri = Midori.URI.format_for_display (value);
        }
        }

        /* Special is an error, blank or delayed page */
        public bool special { get; protected set; default = false; }
        /* Since: 0.4.8 */
        public string mime_type { get; protected set; default = "text/plain"; }
        /* Since: 0.1.2 */
        public Security security { get; protected set; default = Security.NONE; }
        public LoadStatus load_status { get; protected set; default = LoadStatus.FINISHED; }
        public string? statusbar_text { get; protected set; default = null; }
        /* Since: 0.5.0 */
        public Gdk.Color? fg_color { get; protected set; default = null; }
        public Gdk.Color? bg_color { get; protected set; default = null; }

        /* Special pages don't convey progress */
        private double current_progress = 0.0;
        public double progress { get {
            return special ? 0.0 : current_progress;
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

        public signal void console_message (string message, int line, string source_id);
        public signal void attach_inspector (WebKit.WebView inspector_view);
        /* Emitted when an open inspector that was previously
           attached to the window is now detached again.
           Since: 0.3.4
         */
        public signal void detach_inspector (WebKit.WebView inspector_view);
        /* Allow the browser to provide the find bar */
        public signal void search_text (bool found, string typing);

        public bool is_blank () {
            return URI.is_blank (uri);
        }

        construct {
            #if HAVE_GTK3
            orientation = Gtk.Orientation.VERTICAL;
            #endif

            #if HAVE_GRANITE_CLUTTER
            navigation_box = new Granite.Widgets.NavigationBox ();
            #endif

            web_view = new WebKit.WebView ();
            /* Load something to avoid a bug where WebKit might not set a main frame */
            web_view.load_uri ("");
        }

        public void inject_stylesheet (string stylesheet) {
#if !HAVE_WEBKIT2
            #if HAVE_DOM
            var dom = web_view.get_dom_document ();
            try {
                var style = dom.create_element ("style");
                style.set_attribute ("type", "text/css");
                style.append_child (dom.create_text_node (stylesheet));
                return_if_fail (dom.head != null);
                dom.head.append_child (style);
            }
            catch (Error error) {
                critical (_("Failed to inject stylesheet: %s"), error.message);
            }
            #else
            web_view.execute_script ("""
                (function () {
                var style = document.createElement ('style');
                style.setAttribute ('type', 'text/css');
                style.appendChild (document.createTextNode ('%s'));
                var head = document.getElementsByTagName ('head')[0];
                if (head) head.appendChild (style);
                else document.documentElement.insertBefore
                (style, document.documentElement.firstChild);
                }) ();
                """.printf (stylesheet));
            #endif
#endif
        }

#if HAVE_WEBKIT2
        /* Since: 0.5.1 */
        public bool view_source { get {
            return web_view.view_mode == WebKit.ViewMode.SOURCE;
        }
        set {
            web_view.view_mode = value ? WebKit.ViewMode.SOURCE : WebKit.ViewMode.WEB;
        }
        }
#else
        /* Since: 0.5.1 */
        public bool view_source { get {
            return web_view.get_view_source_mode ();
        }
        set {
            web_view.set_view_source_mode (value);
        }
        }
#endif

        public bool can_view_source () {
            if (is_blank () || special || view_source)
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
                return Midori.URI.strip_prefix_for_display (uri);

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
            if (parts[0] != null && uri.has_suffix (parts[parts.length - 1].down ()))
                return Pango.EllipsizeMode.START;

            return Pango.EllipsizeMode.END;
        }

        /* Since: 0.4.3 */
        public bool can_save () {
            if (is_blank () || special)
                return false;
            if (view_source)
                return false;
#if !HAVE_WEBKIT2
            if (web_view.get_main_frame ().get_data_source ().get_data () == null)
                return false;
#endif
            return true;
        }

        public void stop_loading () {
            web_view.stop_loading ();
        }

        public bool can_go_forward () {
            return web_view.can_go_forward ();
        }

        public void go_forward () {
            #if HAVE_GRANITE_CLUTTER
            navigation_box.forward ();
            #endif
            web_view.go_forward ();
        }

        public void unmark_text_matches () {
#if !HAVE_WEBKIT2
            web_view.unmark_text_matches ();
#endif
        }

        public bool find (string text, bool case_sensitive, bool forward) {
#if HAVE_WEBKIT2
            var controller = web_view.get_find_controller ();
            uint options = WebKit.FindOptions.WRAP_AROUND;
            if (!case_sensitive)
                options += WebKit.FindOptions.CASE_INSENSITIVE;
            if (!forward)
                options += WebKit.FindOptions.BACKWARDS;
            controller.search (text, options, 0);
            // FIXME: mark matches, count matches, not found
            return true;
#else
            bool found = false;
            found = web_view.search_text (text, case_sensitive, forward, true);
            web_view.mark_text_matches (text, case_sensitive, 0);
            web_view.set_highlight_text_matches (true);
            return found;
#endif
        }
    }
}
