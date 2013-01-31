/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/
namespace Katze {
    extern static unowned List<GLib.Object> array_peek_items (GLib.Object array);
}

namespace Midori {
    public class SearchCompletion : Completion {
        GLib.Object search_engines;

        public SearchCompletion () {
            GLib.Object (description: _("Search with…"));
        }

        public override void prepare (GLib.Object app) {
            app.get ("search-engines", out search_engines);
        }

        public override bool can_complete (string text) {
            return search_engines != null;
        }

        public override bool can_action (string action) {
            return action == "about:search";
        }

        public override async List<Suggestion>? complete (string text, string? action, Cancellable cancellable) {
            return_val_if_fail (search_engines != null, null);

            unowned List<GLib.Object> items = Katze.array_peek_items (search_engines);
            var suggestions = new List<Suggestion> ();
            uint n = 0;
            foreach (var item in items) {
                string icon, uri, title, desc;
                item.get ("icon", out icon);
                item.get ("uri", out uri);
                item.get ("name", out title);
                item.get ("text", out desc);
                string search_uri = URI.for_search (uri, text);
                string search_title = _("Search with %s").printf (title);
                Gdk.Pixbuf? pixbuf = Midori.Paths.get_icon (icon, null);
                if (pixbuf == null)
                    pixbuf = Midori.Paths.get_icon (uri, null);
                string search_desc = search_title + "\n" + desc ?? uri;
                /* FIXME: Theming? Win32? */
                string background = "gray";
                var suggestion = new Suggestion (search_uri, search_desc, false, background, pixbuf);
                suggestions.append (suggestion);

                n++;
                if (n == 3 && action == null) {
                    suggestion = new Suggestion ("about:search", _("Search with…"), false, background);
                    suggestion.action = true;
                    suggestions.append (suggestion);
                    break;
                }

                uint src = Idle.add (complete.callback);
                yield;
                Source.remove (src);
            }

            if (cancellable.is_cancelled ())
                return null;

            return suggestions;
        }
    }
}
