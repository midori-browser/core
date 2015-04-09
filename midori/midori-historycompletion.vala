/*
 Copyright (C) 2012-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class HistoryCompletion : Completion {
        public HistoryDatabase? database = null;

        public HistoryCompletion () {
            GLib.Object (description: _("Bookmarks and History"));
        }

        public override void prepare (GLib.Object app) {
            try {
                database = new HistoryDatabase (app);
            }
            catch (Error error) {
                warning (error.message);
            }
        }

        public override bool can_complete (string text) {
            return database != null;
        }

        public override bool can_action (string action) {
            return false;
        }

        public override async List<Suggestion>? complete (string text, string? action, Cancellable cancellable) {
            return_val_if_fail (database != null, null);

            List<HistoryItem> items = yield database.list_by_count_with_bookmarks (text, max_items, cancellable);
            if (items == null)
                return null;

            var suggestions = new List<Suggestion> ();
            foreach (var item in items) {
                if (item is Midori.HistoryWebsite) {
                    var website = item as Midori.HistoryWebsite;
                    suggestions.append (new Suggestion (website.uri, website.title,
                        true, null, yield Midori.URI.get_icon_fallback (website.uri, null, cancellable), this.position));
                }
                else if (item is Midori.HistorySearch) {
                    var search = item as Midori.HistorySearch;
                    suggestions.append (new Suggestion (search.uri, search.title + "\n" + search.uri,
                        false, "gray", yield Midori.URI.get_icon_fallback (search.uri, null, cancellable), this.position));
                }
                else
                    warn_if_reached ();
            }

            if (cancellable.is_cancelled ())
                return null;

            return suggestions;
        }
    }
}
