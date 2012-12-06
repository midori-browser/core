/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class HistoryCompletion : Completion {
        unowned Sqlite.Database db;

        public HistoryCompletion () {
            GLib.Object (description: _("History"));
        }

        public override void prepare (GLib.Object app) {
            GLib.Object history;
            app.get ("history", out history);
            return_if_fail (history != null);
            db = history.get_data<Sqlite.Database?> ("db");
            return_if_fail (db != null);
        }

        public override bool can_complete (string text) {
            return db != null;
        }

        public override bool can_action (string action) {
            return false;
        }

        public override async List<Suggestion>? complete (string text, string? action, Cancellable cancellable) {
            return_val_if_fail (db != null, null);

            Sqlite.Statement stmt;
            unowned string sqlcmd = """
                SELECT type, uri, title FROM (
                SELECT 1 AS type, uri, title, count() AS ct FROM history
                WHERE uri LIKE ?1 OR title LIKE ?1 GROUP BY uri
                UNION ALL
                SELECT 2 AS type, replace(uri, '%s', keywords) AS uri,
                       keywords AS title, count() AS ct FROM search
                WHERE uri LIKE ?1 OR title LIKE ?1 GROUP BY uri
                UNION ALL
                SELECT 1 AS type, uri, title, 50 AS ct FROM bookmarks
                WHERE title LIKE ?1 OR uri LIKE ?1 AND uri !=''
                ) GROUP BY uri ORDER BY ct DESC LIMIT ?2
                """;
            if (db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK) {
                critical (_("Failed to initialize history: %s"), db.errmsg ());
                return null;
            }

            string query = "%" + text.replace (" ", "%") + "%";
            stmt.bind_text (1, query);
            stmt.bind_int64 (2, max_items);

            int result = stmt.step ();
            if (result != Sqlite.ROW) {
                if (result == Sqlite.ERROR)
                    critical (_("Failed to select from history: %s"), db.errmsg ());
                return null;
            }

            var suggestions = new List<Suggestion> ();
            while (result == Sqlite.ROW) {
                int64 type = stmt.column_int64 (0);
                unowned string uri = stmt.column_text (1);
                unowned string title = stmt.column_text (2);
                Gdk.Pixbuf? icon = Midori.Paths.get_icon (uri, null);
                Suggestion suggestion;

                switch (type) {
                    case 1: /* history_view */
                        suggestion = new Suggestion (uri, title, false, null, icon);
                        suggestions.append (suggestion);
                        break;
                    case 2: /* search_view */
                        string desc = _("Search for %s").printf (title) + "\n" + uri;
                        /* FIXME: Theming? Win32? */
                        string background = "gray";
                        suggestion = new Suggestion (uri, desc, false, background, icon);
                        suggestions.append (suggestion);
                        break;
                    default:
                        warn_if_reached ();
                        break;
                }

                uint src = Idle.add (complete.callback);
                yield;
                Source.remove (src);

                if (cancellable.is_cancelled ())
                    return null;

                result = stmt.step ();
            }

            if (cancellable.is_cancelled ())
                return null;

            return suggestions;
        }
    }
}
