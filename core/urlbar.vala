/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    [GtkTemplate (ui = "/ui/urlbar.ui")]
    public class Urlbar : Gtk.Entry {
        public string? key { get; protected set; }
        public Regex? regex { get; protected set; }
        public string? location { get; protected set; }
        string _uri;
        public string uri { get {
            return _uri;
        } set {
            _uri = value;
            location = value;
            // Treat about:blank specially
            text = blank ? "" : value;
            set_position (-1);
            update_icon ();
        } }
        bool _secure = false;
        public bool secure { get { return _secure; } set {
            _secure = value;
            update_icon ();
        } }
        bool blank { get { return uri == "about:blank" || uri == "internal:speed-dial"; } }

        [GtkChild]
        Gtk.Popover? suggestions;
        [GtkChild]
        Gtk.ListBox listbox;
        Gtk.ListBoxRow? selected_row { get; protected set; default = null; }
        [GtkChild]
        Gtk.Popover security;
        [GtkChild]
        Gtk.Box security_box;
        [GtkChild]
        Gtk.Label security_status;
        Gcr.CertificateWidget? details = null;

        construct {
            size_allocate.connect (resize);
            suggestions.hide.connect (() => {
                Gtk.grab_remove (suggestions);
            });

            listbox.row_selected.connect ((row) => {
                selected_row = row;
            });
            listbox.row_activated.connect ((row) => {
                popdown ();
                var suggestion = (SuggestionRow)row;
                uri = suggestion.item.uri;
            });

            insert_text.connect_after ((new_text, new_position, ref position) => {
                update_key (text);
            });
            delete_text.connect_after ((start_pos, end_pos) => {
                update_key (text);
            });
            icon_press.connect (icon_pressed);
        }

        public Gtk.Widget create_row (Object item) {
            var suggestion = new SuggestionRow ((DatabaseItem)item);
            suggestion.key = key;
            bind_property ("key", suggestion, "key");
            suggestion.regex = regex;
            bind_property ("regex", suggestion, "regex");
            suggestion.location = location;
            bind_property ("location", suggestion, "location");
            return suggestion;
        }

        protected override bool key_press_event (Gdk.EventKey event) {
            // Listbox
            if (suggestions.visible) {
                bool has_shift = (event.state & Gdk.ModifierType.SHIFT_MASK) != 0;
                switch (event.keyval) {
                    case Gdk.Key.Tab:
                    case Gdk.Key.ISO_Left_Tab:
                    case Gdk.Key.Down:
                    case Gdk.Key.KP_Down:
                        listbox.move_cursor (Gtk.MovementStep.DISPLAY_LINES,
                            has_shift ? -1 : 1);
                        return true;
                    case Gdk.Key.Up:
                    case Gdk.Key.KP_Up:
                        listbox.move_cursor (Gtk.MovementStep.DISPLAY_LINES, -1);
                        return true;
                    case Gdk.Key.Left:
                    case Gdk.Key.KP_Left:
                    case Gdk.Key.Right:
                    case Gdk.Key.KP_Right:
                        popdown ();
                        return base.key_press_event (event);
                    case Gdk.Key.Delete:
                    case Gdk.Key.KP_Delete:
                        var suggestion_row = (SuggestionRow)selected_row;
                        if (suggestion_row != null && suggestion_row.get_index() != 0 && !suggestion_row.item.database.readonly) {
                            listbox.move_cursor (Gtk.MovementStep.DISPLAY_LINES, -1);
                            suggestion_row.item.delete.begin ();
                        } else {                            
                            if (suggestion_row.get_index() == 0){
                                return base.key_press_event (event);
                            }
                        }
                        return true;
                    case Gdk.Key.Escape:
                        popdown ();
                        return true;
                }
                // The list receives events first, then the popover
                if (!listbox.key_press_event (event)) {
                    if (suggestions.key_press_event (event)) {
                        return true;
                    }
                }
            }
            // Entry
            switch (event.keyval) {
                case Gdk.Key.Tab:
                case Gdk.Key.ISO_Left_Tab:
                    return false;
                case Gdk.Key.ISO_Enter:
                case Gdk.Key.KP_Enter:
                case Gdk.Key.Return:
                    bool has_ctrl = (event.state & Gdk.ModifierType.CONTROL_MASK) != 0;
                    if (has_ctrl) {
                        popdown ();
                        bool has_shift = (event.state & Gdk.ModifierType.SHIFT_MASK) != 0;
                        if (has_shift) {
                            Application.get_default ().activate_action ("win-new", text);
                        } else {
                            var browser = ((Browser)get_toplevel ());
                            browser.add (new Tab (null, browser.web_context, text));
                        }
                    } else {
                        listbox.activate_cursor_row ();
                    }
                    return true;
                case Gdk.Key.Down:
                case Gdk.Key.KP_Down:
                    complete ();
                    return true;
                case Gdk.Key.Escape:
                    text = blank ? "" : uri;
                    set_position (-1);
                    // Propagate to allow Escape to stop loading
                    return false;
            }

            // No completion on control characters
            unichar character = Gdk.keyval_to_unicode (event.keyval);
            if (character != 0 && event.is_modifier == 0) {
                complete ();
            }
            return base.key_press_event (event);
        }

        void update_key (string text) {
            location = magic_uri (text);
            try {
                key = text;
                regex = new Regex ("(%s)".printf (Regex.escape_string (key)),
                                   RegexCompileFlags.CASELESS);
            } catch (RegexError error) {
                regex = null;
                debug ("Failed to create regex: %s", error.message);
            }
        }

        string? magic_uri (string text) {
            if (" " in text) {
                return null;
            } else if (Path.is_absolute (text)) {
                try {
                    return Filename.to_uri (text);
                } catch (ConvertError error ) {
                    debug ("Failed to convert URI to filename: %s", error.message);
                    return text;
                }
            } else if (FileUtils.test (text, FileTest.EXISTS | FileTest.IS_REGULAR)) {
                return File.new_for_commandline_arg (text).get_uri ();
            }else if (is_external (text)) {
                return text;
            } else if (text.has_prefix ("geo:")) {
                // Parse URI geo:48.202778,16.368472;crs=wgs84;u=40 as location
                return text;
            } else if (is_location (text)) {
                return text;
            } else if (is_ip_address (text)) {
                return "http://" + text;
            } else if (text.has_prefix ("localhost") || "." in text) {
                return "http://" + text;
            } else if (text == "") {
                return "about:blank";
            }
            // This seems to be keywords for a search
            return null;
        }

        bool is_location (string uri) {
            /* file:// is not considered a location for security reasons */
            return uri.has_prefix ("about:")
              || uri.has_prefix ("http://")
              || uri.has_prefix ("https://")
              || (uri.has_prefix ("data:") && (";" in uri))
              || uri.has_prefix ("javascript:");
        }

        bool is_external (string uri) {
            if (uri.has_prefix ("file://")) {
                return true;
            }
            var scheme = Uri.parse_scheme (uri);
            return scheme != null
                && AppInfo.get_default_for_uri_scheme (scheme) != null;
        }

        bool is_ip_address (string uri) {
            /* Quick check for IPv4 or IPv6, no validation.
               hostname_is_ip_address () is not used because
               we'd have to separate the path from the URI first. */
            /* Skip leading user/ password */
            if ("@" in uri)
                return is_ip_address (uri.split ("@")[1]);
            /* IPv4 */
            if (uri[0] != '0' && uri[0].isdigit () && "." in uri)
                return true;
            /* IPv6 */
            if (uri[0].isalnum () && uri[1].isalnum ()
             && uri[2].isalnum () && uri[3].isalnum () && uri[4] == ':'
             && (uri[5] == ':' || uri[5].isalnum ()))
                return true;
            return false;
        }
 
        protected override bool focus_out_event (Gdk.EventFocus event) {
            popdown ();
            return base.focus_out_event (event);
        }

        public void popdown () {
            // Note: Guard against popover being destroyed before popdown
            if (suggestions != null) {
                suggestions.hide ();
            }
        }

        void resize (Gtk.Allocation allocation) {
            // 2/3 of window size
            int width;
            ((Gtk.Window)(get_toplevel ())).get_size (out width, null);
            width = (int)(width / 1.5);
            listbox.set_size_request (width, -1);
        }

        void complete () {
            if (!suggestions.visible) {
                suggestions.set_default_widget (this);
                suggestions.relative_to = this;
                var completion = new Completion (((Browser)get_toplevel ()).tab.web_context.is_ephemeral ());
                bind_property ("key", completion, "key");
                listbox.bind_model (completion, create_row);
            }
            suggestions.show ();
            Gtk.grab_add (suggestions);
            suggestions.grab_focus ();
        }

        void update_icon () {
            if (blank) {
                primary_icon_name = null;
            } else {
                primary_icon_name = secure ? "channel-secure-symbolic" : "channel-insecure-symbolic";
            }
            primary_icon_activatable = !blank;
        }

        void icon_pressed (Gtk.EntryIconPosition position, Gdk.Event event) {
            var tls = ((Browser)get_toplevel ()).tab.tls;
            var certificate = tls != null ? new Gcr.SimpleCertificate (tls.certificate.data) : null;
            if (details == null) {
                var icon_area = get_icon_area (position);
                security.relative_to = this;
                security.pointing_to = icon_area;
                // Clicking expander causes an assertion:
                // Gtk.Widget.is_ancestor: is Gtk.Widget
                // Override popdown here to avoid that problem.
                security.button_press_event.connect ((event) => {
                    var widget = Gtk.get_event_widget (event);
                    if (widget != this && !widget.is_ancestor (security)) {
                        security.hide ();
                    }
                    return true;
                });

                // Insert widget here because Gtk.Builder won't recognize the type
                details = new Gcr.CertificateWidget (null);
                security_box.add (details);
            }
            details.visible = tls != null;
            details.certificate = certificate;
            security_status.visible = !secure;
            security.show ();
        }
    }

}
