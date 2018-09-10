/*
 Copyright (C) 2013-2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Tabby {
    class SessionDatabase : Midori.Database {
        int64 id { get; set; }

        static SessionDatabase? _default = null;

        public static SessionDatabase get_default () throws Midori.DatabaseError {
            if (_default == null) {
                _default = new SessionDatabase ();
            }
            return _default;
        }

        SessionDatabase () throws Midori.DatabaseError {
            Object (path: "tabby.db", table: "tabs");
            init ();
        }

        public async override List<Midori.DatabaseItem>? query (string? filter=null, int64 max_items=15, Cancellable? cancellable=null) throws Midori.DatabaseError {
            string where = filter != null ? "WHERE uri LIKE :filter OR title LIKE :filter" : "";
            string sqlcmd = """
                SELECT id, uri, title, tstamp, sorting FROM %s
                %s
                ORDER BY tstamp DESC LIMIT :limit
                """.printf (table, where);
            Midori.DatabaseStatement statement;
            try {
                statement = prepare (sqlcmd,
                    ":limit", typeof (int64), max_items);
                if (filter != null) {
                    string real_filter = "%" + filter.replace (" ", "%") + "%";
                    statement.bind (":filter", typeof (string), real_filter);
                }
            } catch (Midori.DatabaseError error) {
                throw new Midori.DatabaseError.EXECUTE ("Failed to select from %s: %s".printf (table, error.message));
            }

            var items = new List<Midori.DatabaseItem> ();
            try {
                while (statement.step ()) {
                    string uri = statement.get_string ("uri");
                    string title = statement.get_string ("title");
                    int64 date = statement.get_int64 ("tstamp");
                    var item = new Midori.DatabaseItem (uri, title, date);
                    item.database = this;
                    item.id = statement.get_int64 ("id");
                    item.set_data<double?> ("sorting", statement.get_double ("sorting"));
                    items.append (item);

                    uint src = Idle.add (query.callback);
                    yield;
                    Source.remove (src);

                    if (cancellable != null && cancellable.is_cancelled ())
                        return null;
                }
            } catch (Midori.DatabaseError error) {
                throw new Midori.DatabaseError.EXECUTE ("Failed to select from %s: %s".printf (table, error.message));
            }

            if (cancellable != null && cancellable.is_cancelled ())
                return null;
            return items;
        }

        public async override bool insert (Midori.DatabaseItem item) throws Midori.DatabaseError {
            item.database = this;

            string sqlcmd = """
                INSERT INTO %s (crdate, tstamp, session_id, uri, title, sorting)
                VALUES (:crdate, :tstamp, :session_id, :uri, :title, :sorting)
                """.printf (table);

            var statement = prepare (sqlcmd,
                ":crdate", typeof (int64), new DateTime.now_local ().to_unix (),
                ":tstamp", typeof (int64), item.date,
                ":session_id", typeof (int64), id,
                ":uri", typeof (string), item.uri,
                ":title", typeof (string), item.title,
                ":sorting", typeof (double), item.get_data<double?> ("sorting") ?? 1);
            if (statement.exec ()) {
                item.id = statement.row_id ();
                /* XXX: if (_items != null) {
                    _items.append (item);
                    items_changed (_items.index (item), 0, 1);
                } */
                return true;
            }
            return false;
        }

        public async override bool update (Midori.DatabaseItem item) throws Midori.DatabaseError {
            string sqlcmd = """
                UPDATE %s SET uri=:uri, title=:title, tstamp=:tstamp WHERE rowid=:id
                """.printf (table);
            try {
                var statement = prepare (sqlcmd,
                    ":id", typeof (int64), item.id,
                    ":uri", typeof (string), item.uri,
                    ":title", typeof (string), item.title,
                    ":tstamp", typeof (int64), item.date);
                if (statement.exec ()) {
                    /* XXX: if (_items != null) {
                        items_changed (_items.index (item), 0, 0);
                    } */
                    return true;
                }
            } catch (Error error) {
                critical ("Failed to update %s: %s", table, error.message);
            }
            return false;
        }

        // XXX: save/ restore tab trash
        // XXX: don't add restored tabs to history

        public async void restore_session (Midori.Browser browser) throws Midori.DatabaseError {
            /* XXX:
            string sqlcmd = "SELECT id, uri, title, sorting FROM tabs WHERE session_id = :session_id ORDER BY tstamp DESC";
            string sqlcmd = "SELECT MAX(sorting) FROM tabs WHERE session_id = :session_id";
            string sqlcmd = """
                SELECT id, closed FROM sessions WHERE closed = 0
                UNION
                SELECT * FROM (SELECT id, closed FROM sessions WHERE closed = 1 ORDER BY tstamp DESC LIMIT 1)
                ORDER BY closed;
            """;
            */
            // XXX: id = 1234;
            foreach (var item in yield query ()) {
                var tab = new Midori.Tab (browser.tab, browser.web_context,
                                          item.uri, item.title);
                tab.notify["uri"].connect ((pspec) => { update.begin (item); });
                tab.notify["title"].connect ((pspec) => { item.title = tab.title; });
                // XXX: add new tabs
                // XXX: add new windows
                // XXX: drop closed tabs
                browser.add (tab);
            }
        }
    }

    public class BrowserSession : Peas.ExtensionBase, Midori.BrowserActivatable {
        public Midori.Browser browser { owned get; set; }

        public void activate () {
            activate_async.begin ();
        }

        async void activate_async () {
            try {
                yield SessionDatabase.get_default ().restore_session (browser);
            } catch (Midori.DatabaseError error) {
                critical ("Failed to restore session: %s", error.message);
            }
        }
    }
}

[ModuleInit]
public void peas_register_types(TypeModule module) {
    ((Peas.ObjectModule)module).register_extension_type (
        typeof (Midori.BrowserActivatable), typeof (Tabby.BrowserSession));

}
