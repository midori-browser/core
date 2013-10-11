/*
 Copyright (C) 2013 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class HistoryWebsite : HistoryItem {
        public string uri { get; set; }
        public HistoryWebsite (string uri, string? title, int64 date) {
            GLib.Object (uri: uri,
                         title: title,
                         date: date);
        }
    }

    public class HistorySearch : HistoryItem {
        public string uri { get; set; }
        public string keywords { get; set; }
        public HistorySearch (string uri, string keywords, int64 date) {
            GLib.Object (uri: uri,
                         keywords: keywords,
                         title: _("Search for %s").printf (keywords),
                         date: date);
        }
    }

    public class HistoryItem : GLib.Object {
        public string? title { get; set; }
        public int64 date { get; set; }
    }

    public class HistoryDatabase : Midori.Database {
        public HistoryDatabase (GLib.Object? app) throws DatabaseError {
			Object (path: "history.db");
            init ();
            Midori.BookmarksDatabase bookmarks_database = new Midori.BookmarksDatabase ();
            exec ("ATTACH DATABASE '%s' AS bookmarks".printf (bookmarks_database.path));

            try {
                exec ("SELECT day FROM history LIMIT 1");
            } catch (Error error) {
                exec_script ("Day");
            }
        }

        public async List<HistoryItem>? query (string sqlcmd, string? filter, int day, int max_items, Cancellable cancellable) {
            return_val_if_fail (db != null, null);

            Sqlite.Statement stmt;
            int result;

            result = db.prepare_v2 (sqlcmd, -1, out stmt, null);
            if (result != Sqlite.OK) {
                critical (_("Failed to select from history: %s"), db.errmsg ());
                return null;
            }

            if (":filter" in sqlcmd) {
                string real_filter = "%" + filter.replace (" ", "%") + "%";
                stmt.bind_text (stmt.bind_parameter_index (":filter"), real_filter);
            }
            if (":day" in sqlcmd)
                stmt.bind_int64 (stmt.bind_parameter_index (":day"), day);
            if (":limit" in sqlcmd)
                stmt.bind_int64 (stmt.bind_parameter_index (":limit"), max_items);

            result = stmt.step ();
            if (!(result == Sqlite.DONE || result == Sqlite.ROW)) {
                critical (_("Failed to select from history: %s"), db.errmsg ());
                return null;
            }

            var items = new List<HistoryItem> ();
            while (result == Sqlite.ROW) {
                int64 type = stmt.column_int64 (0);
                int64 date = stmt.column_int64 (1);
                switch (type) {
                    case 1:
                        string uri = stmt.column_text (2);
                        string title = stmt.column_text (3);
                        items.append (new HistoryWebsite (uri, title, date));
                        break;
                    case 2:
                        string uri = stmt.column_text (2);
                        string title = stmt.column_text (3);
                        items.append (new HistorySearch (uri, title, date));
                        break;
                    default:
                        warn_if_reached ();
                        break;
                }

                uint src = Idle.add (query.callback);
                yield;
                Source.remove (src);

                if (cancellable.is_cancelled ())
                    return null;

                result = stmt.step ();
            }

            if (cancellable.is_cancelled ())
                return null;
            return items;
        }

        public async List<HistoryItem>? list_by_count_with_bookmarks (string? filter, int max_items, Cancellable cancellable) {
            unowned string sqlcmd = """
                SELECT type, date, uri, title FROM (
                SELECT 1 AS type, date, uri, title, count() AS ct FROM history
                WHERE uri LIKE :filter OR title LIKE :filter GROUP BY uri
                UNION ALL
                SELECT 2 AS type, day AS date, replace(uri, '%s', keywords) AS uri,
                       keywords AS title, count() AS ct FROM search
                WHERE uri LIKE :filter OR title LIKE :filter GROUP BY uri
                UNION ALL
                SELECT 1 AS type, last_visit AS date, uri, title, 50 AS ct FROM bookmarks
                WHERE title LIKE :filter OR uri LIKE :filter AND uri !='' AND uri NOT LIKE 'javascript:%'
                ) GROUP BY uri ORDER BY ct DESC LIMIT :limit
                """;
            return yield query (sqlcmd, filter, 0, max_items, cancellable);
        }
   }
}
