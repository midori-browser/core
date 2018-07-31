/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    [GtkTemplate (ui = "/ui/suggestion-row.ui")]
    public class SuggestionRow : Gtk.ListBoxRow {
        public DatabaseItem item { get; protected set; }
        string? escaped_uri = null;
        string? escaped_title = null;
        public string? location { get; set; }
        public Regex? regex { get; set; }
        public string? key { get; set; }
        [GtkChild]
        Gtk.Box box;
        [GtkChild]
        Favicon icon;
        [GtkChild]
        Gtk.Label title;
        [GtkChild]
        Gtk.Label uri;
        public SuggestionRow (DatabaseItem item) {
            Object (item: item);
            if (item is SuggestionItem) {
                box.set_child_packing (title, true, true, 0, Gtk.PackType.END);
                notify["location"].connect ((pspec) => {
                    if (location != null) {
                        item.uri = location;
                        icon.icon_name = "go-jump-symbolic";
                        uri.label = Markup.escape_text (location);
                        title.label = "";
                    }
                });
                notify["key"].connect ((pspec) => {
                    if (key != null) {
                        var suggestion = (SuggestionItem)item;
                        item.uri = suggestion.search.printf (Uri.escape_string (key, ":/", true));
                        icon.icon_name = "edit-find-symbolic";
                        uri.label = Markup.escape_text (key);
                        title.label = item.title;
                    }
                });
            // Double-check type for the sake of plugins
            } else if (item is DatabaseItem) {
                icon.uri = item.uri;
                escaped_title = item.title != null ? Markup.escape_text (item.title) : "";
                title.label = escaped_title;
                escaped_uri = Markup.escape_text (strip_uri_prefix (item.uri));
                uri.label = escaped_uri;
                notify["regex"].connect ((pspec) => {
                    if (regex != null) {
                        try {
                            var highlight = "<b>\\1</b>";
                            uri.label = regex.replace (escaped_uri, -1, 0, highlight);
                            title.label = regex.replace (escaped_title, -1, 0, highlight);
                        } catch (RegexError error) {
                            debug ("Failed to apply regex: %s", error.message);
                        }
                    }
                });
            }
        }

        string? strip_uri_prefix (string uri) {
            bool is_http = uri.has_prefix ("http://") || uri.has_prefix ("https://");
            if (is_http || uri.has_prefix ("file://")) {
                string stripped_uri = uri.split ("://")[1];
                if (is_http && stripped_uri.has_prefix ("www."))
                    return stripped_uri.substring (4, -1);
                return stripped_uri;
            }
            return uri;
        }
    }
}
