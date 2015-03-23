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
            attach (bookmarks_database.path, "bookmarks");

            try {
                exec ("SELECT day FROM history LIMIT 1");
            } catch (Error error) {
                exec_script ("Day");
            }
        }

        public async List<HistoryItem>? query (string sqlcmd, string? filter, int64 day, int64 max_items, Cancellable cancellable) {
            return_val_if_fail (db != null, null);

            Midori.DatabaseStatement statement;

            try {
                string real_filter = "%" + filter.replace (" ", "%") + "%";
                statement = prepare (sqlcmd,
                    ":filter", typeof (string), real_filter,
                    ":limit", typeof (int64), max_items);
            } catch (Error error) {
                critical (_("Failed to select from history: %s"), error.message);
                return null;
            }

            var items = new List<HistoryItem> ();
            try {
                while (statement.step ()) {
                    int64 type = statement.get_int64 ("type");
                    int64 date = statement.get_int64 ("date");
                    switch (type) {
                        case 1:
                            string uri = statement.get_string ("uri");
                            string title = statement.get_string ("title");
                            items.append (new HistoryWebsite (uri, title, date));
                            break;
                        case 2:
                            string uri = statement.get_string ("uri");
                            string title = statement.get_string ("title");
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
                }
            } catch (Error error) {
                critical (_("Failed to select from history: %s"), error.message);
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

        public bool insert (string uri, string title, int64 date, int64 day) throws DatabaseError {
            unowned string sqlcmd = "INSERT INTO history (uri, title, date, day) VALUES (:uri, :title, :date, :day)";
            var statement = prepare (sqlcmd,
                ":uri", typeof (string), uri,
                ":title", typeof (string), title,
                ":date", typeof (int64), date,
                ":day", typeof (int64), day);
            return statement.exec ();
        }

        public bool clear (int64 maximum_age=0) throws DatabaseError {
            unowned string sqlcmd = """
                DELETE FROM history WHERE
                (julianday(date('now')) - julianday(date(date,'unixepoch')))
                >= :maximum_age;
                DELETE FROM search WHERE
                (julianday(date('now')) - julianday(date(date,'unixepoch')))
                >= :maximum_age;
                """;
            var statement = prepare (sqlcmd,
                ":maximum_age", typeof (int64), maximum_age);
            return statement.exec ();
        }
   }
}
