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
        static SessionDatabase? _default = null;
        // Note: Using string instead of int64 because it's a hashable type
        HashTable<string, Midori.Browser> browsers;

        public static SessionDatabase get_default () throws Midori.DatabaseError {
            if (_default == null) {
                _default = new SessionDatabase ();
            }
            return _default;
        }

        SessionDatabase () throws Midori.DatabaseError {
            Object (path: "tabby.db", table: "tabs");
            init ();
            browsers = new HashTable<string, Midori.Browser> (str_hash, str_equal);
        }

        async List<int64?> get_sessions () throws Midori.DatabaseError {
            string sqlcmd = """
                SELECT id, closed FROM sessions WHERE closed = 0
                UNION
                SELECT * FROM (SELECT id, closed FROM sessions WHERE closed = 1 ORDER BY tstamp DESC LIMIT 1)
                ORDER BY closed;
            """;
            var sessions = new List<int64?> ();
            var statement = prepare (sqlcmd);
            while (statement.step ()) {
                int64 id = statement.get_int64 ("id");
                int64 closed = statement.get_int64 ("closed");
                if (closed == 0 || sessions.length () == 0) {
                    sessions.append (id);
                }
            }
            return sessions;
        }

        async List<Midori.DatabaseItem>? get_items (int64 session_id, string? filter=null, int64 max_items=15, Cancellable? cancellable=null) throws Midori.DatabaseError {
            string where = filter != null ? "AND (uri LIKE :filter OR title LIKE :filter)" : "";
            string sqlcmd = """
                SELECT id, uri, title, tstamp, pinned FROM %s
                WHERE session_id = :session_id %s
                ORDER BY tstamp DESC LIMIT :limit
                """.printf (table, where);
            var statement = prepare (sqlcmd,
                ":session_id", typeof (int64), session_id,
                ":limit", typeof (int64), max_items);
            if (filter != null) {
                string real_filter = "%" + filter.replace (" ", "%") + "%";
                statement.bind (":filter", typeof (string), real_filter);
            }

            var items = new List<Midori.DatabaseItem> ();
            while (statement.step ()) {
                string uri = statement.get_string ("uri");
                string title = statement.get_string ("title");
                int64 date = statement.get_int64 ("tstamp");
                var item = new Midori.DatabaseItem (uri, title, date);
                item.database = this;
                item.id = statement.get_int64 ("id");
                item.set_data<int64> ("session_id", session_id);
                item.set_data<int64> ("pinned", statement.get_int64 ("pinned"));
                items.append (item);

                uint src = Idle.add (get_items.callback);
                yield;
                Source.remove (src);

                if (cancellable != null && cancellable.is_cancelled ())
                    return null;
            }

            if (cancellable != null && cancellable.is_cancelled ())
                return null;
            return items;
        }

        public async override List<Midori.DatabaseItem>? query (string? filter=null, int64 max_items=15, Cancellable? cancellable=null) throws Midori.DatabaseError {
            var items = new List<Midori.DatabaseItem> ();
            foreach (int64 session_id in yield get_sessions ()) {
                foreach (var item in yield get_items (session_id, filter, max_items, cancellable)) {
                    items.append (item);
                }
            }

            if (cancellable != null && cancellable.is_cancelled ())
                return null;
            return items;
        }

        public async override bool insert (Midori.DatabaseItem item) throws Midori.DatabaseError {
            item.database = this;

            string sqlcmd = """
                INSERT INTO %s (crdate, tstamp, session_id, uri, title)
                VALUES (:crdate, :tstamp, :session_id, :uri, :title)
                """.printf (table);

            var statement = prepare (sqlcmd,
                ":crdate", typeof (int64), item.date,
                ":tstamp", typeof (int64), item.date,
                ":session_id", typeof (int64), item.get_data<int64> ("session_id"),
                ":uri", typeof (string), item.uri,
                ":title", typeof (string), item.title);
            if (statement.exec ()) {
                item.id = statement.row_id ();
                return true;
            }
            return false;
        }

        public async override bool update (Midori.DatabaseItem item) throws Midori.DatabaseError {
            string sqlcmd = """
                UPDATE %s SET uri = :uri, title = :title, tstamp = :tstamp WHERE id = :id
                """.printf (table);
            try {
                var statement = prepare (sqlcmd,
                    ":id", typeof (int64), item.id,
                    ":uri", typeof (string), item.uri,
                    ":title", typeof (string), item.title,
                    ":tstamp", typeof (int64), new DateTime.now_local ().to_unix ());
                if (statement.exec ()) {
                    return true;
                }
            } catch (Midori.DatabaseError error) {
                critical ("Failed to update %s: %s", table, error.message);
            }
            return false;
        }

        public async override bool delete (Midori.DatabaseItem item) throws Midori.DatabaseError {
            string sqlcmd = """
                DELETE FROM %s WHERE id = :id
                """.printf (table);
            var statement = prepare (sqlcmd,
                ":id", typeof (int64), item.id);
            if (statement.exec ()) {
                return true;
            }
            return false;
        }

        int64 insert_session () {
            string sqlcmd = """
                INSERT INTO sessions (tstamp) VALUES (:tstamp)
                """;
            try {
                var statement = prepare (sqlcmd,
                    ":tstamp", typeof (int64), new DateTime.now_local ().to_unix ());
                statement.exec ();
                debug ("Added session: %s", statement.row_id ().to_string ());
                return statement.row_id ();
            } catch (Midori.DatabaseError error) {
                critical ("Failed to add session: %s", error.message);
            }
            return -1;
         }

        void update_session (int64 id, bool closed) {
            string sqlcmd = """
                UPDATE sessions SET closed=:closed, tstamp=:tstamp WHERE id = :id
                """;
            try {
                var statement = prepare (sqlcmd,
                    ":id", typeof (int64), id,
                    ":tstamp", typeof (int64), new DateTime.now_local ().to_unix (),
                    ":closed", typeof (int64), closed ? 1 : 0);
                statement.exec ();
            } catch (Midori.DatabaseError error) {
                critical ("Failed to update session: %s", error.message);
            }
        }

        async void update_tab (Midori.DatabaseItem item) throws Midori.DatabaseError {
            string sqlcmd = """
                UPDATE %s SET pinned=:pinned WHERE rowid = :id
                """.printf (table);
            prepare (sqlcmd,
                ":id", typeof (int64), item.id,
                ":pinned", typeof (int64), item.get_data<int64> ("pinned")).exec ();
        }

        public async override bool clear (TimeSpan timespan) throws Midori.DatabaseError {
            // Note: TimeSpan is defined in microseconds
            int64 maximum_age = new DateTime.now_local ().to_unix () - timespan / 1000000;

            string sqlcmd = """
                DELETE FROM %s WHERE tstamp >= :maximum_age;
                DELETE FROM sessions WHERE tstamp >= :maximum_age;
                """.printf (table);
            var statement = prepare (sqlcmd,
                ":maximum_age", typeof (int64), maximum_age);
            return statement.exec ();
        }

        public async bool restore_windows (Midori.Browser default_browser) throws Midori.DatabaseError {
            bool restored = false;

            // Restore existing session(s) that weren't closed, or the last closed one
            foreach (var item in yield query (null, int64.MAX - 1)) {
                Midori.Browser browser;
                int64 id = item.get_data<int64> ("session_id");
                if (!restored) {
                    browser = default_browser;
                    restored = true;
                    connect_browser (browser, id);
                    foreach (var widget in browser.tabs.get_children ()) {
                        yield tab_added (widget as Midori.Tab, id);
                    }
                } else {
                    var app = (Midori.App)default_browser.get_application ();
                    browser = browser_for_session (app, id);
                }
                var tab = new Midori.Tab (null, browser.web_context,
                                          item.uri, item.title);
                tab.pinned = item.get_data<bool> ("pinned");
                connect_tab (tab, item);
                browser.add (tab);
            }
            return restored;
        }

        Midori.Browser browser_for_session (Midori.App app, int64 id) {
            var browser = browsers.lookup (id.to_string ());
            if (browser == null) {
                debug ("Restoring session %s", id.to_string ());
                browser = new Midori.Browser (app);
                browser.show ();
                connect_browser (browser, id);
            }
            return browser;
        }

        public void connect_browser (Midori.Browser browser, int64 id=-1) {
            if (id < 0) {
                id = insert_session ();
            } else {
                update_session (id, false);
            }

            browsers.insert (id.to_string (), browser);
            browser.set_data<bool> ("tabby_connected", true);
            foreach (var widget in browser.tabs.get_children ()) {
                tab_added.begin (widget as Midori.Tab, id);
            }
            browser.tabs.add.connect ((widget) => { tab_added.begin (widget as Midori.Tab, id); });
            browser.delete_event.connect ((event) => {
                debug ("Closing session %s", id.to_string ());
                update_session (id, true);
                return false;
            });
        }

        void connect_tab (Midori.Tab tab, Midori.DatabaseItem item) {
            debug ("Connecting %s to session %s", item.uri, item.get_data<int64> ("session_id").to_string ());
            tab.set_data<Midori.DatabaseItem?> ("tabby-item", item);
            tab.notify["uri"].connect ((pspec) => { item.uri = tab.uri; update.begin (item); });
            tab.notify["title"].connect ((pspec) => { item.title = tab.title; });
            tab.notify["pinned"].connect ((pspec) => { item.set_data<bool> ("pinned", tab.pinned); update_tab.begin (item); });
            tab.close.connect (() => { tab_removed (tab); });
        }

        bool tab_is_connected (Midori.Tab tab) {
            return tab.get_data<Midori.DatabaseItem?> ("tabby-item") != null;
        }

        async void tab_added (Midori.Tab tab, int64 id) {
            if (tab_is_connected (tab)) {
                return;
            }
            var item = new Midori.DatabaseItem (tab.display_uri, tab.display_title,
                                                new DateTime.now_local ().to_unix ());
            item.set_data<int64> ("session_id", id);
            try {
                yield insert (item);
                connect_tab (tab, item);
            } catch (Midori.DatabaseError error) {
                critical ("Failed add tab to session database: %s", error.message);
            }
        }

        void tab_removed (Midori.Tab tab) {
            var item = tab.get_data<Midori.DatabaseItem?> ("tabby-item");
            debug ("Trashing tab %s:%s", item.get_data<int64> ("session_id").to_string (), tab.display_uri);
            item.delete.begin ();
        }
    }

    public class Session : Peas.ExtensionBase, Midori.BrowserActivatable {
        public Midori.Browser browser { owned get; set; }

        static bool session_restored = false;

        public void activate () {
            // Don't track locked (app) or private windows
            if (browser.is_locked || browser.web_context.is_ephemeral ()) {
                return;
            }
            // Skip windows already in the session
            if (browser.get_data<bool> ("tabby_connected")) {
                return;
            }

            browser.default_tab.connect (restore_or_connect);
            try {
                var session = SessionDatabase.get_default ();
                if (session_restored) {
                    session.connect_browser (browser);
                    browser.activate_action ("tab-new", null);
                } else {
                    session_restored = true;
                    restore_session.begin (session);
                }
            } catch (Midori.DatabaseError error) {
                critical ("Failed to restore session: %s", error.message);
            }
        }

        bool restore_or_connect () {
            try {
                var session = SessionDatabase.get_default ();
                var settings = Midori.CoreSettings.get_default ();
                if (settings.load_on_startup == Midori.StartupType.SPEED_DIAL) {
                    session.connect_browser (browser);
                } else if (settings.load_on_startup == Midori.StartupType.HOMEPAGE) {
                    session.connect_browser (browser);
                    browser.activate_action ("homepage", null);
                    return true;
                } else {
                    return true;
                }
            } catch (Midori.DatabaseError error) {
                critical ("Failed to restore session: %s", error.message);
            }
            return false;
        }

        async void restore_session (SessionDatabase session) {
            try {
                bool restored = yield session.restore_windows (browser);
                if (!restored) {
                    browser.add (new Midori.Tab (null, browser.web_context));
                    session.connect_browser (browser);
                }
            } catch (Midori.DatabaseError error) {
                critical ("Failed to restore session: %s", error.message);
            }
        }
    }

    public class Preferences : Object, Midori.PreferencesActivatable {
        public Midori.Preferences preferences { owned get; set; }

        public void activate () {
            var settings = Midori.CoreSettings.get_default ();
            var box = new Midori.LabelWidget (_("Startup"));
            var combo = new Gtk.ComboBoxText ();
            combo.append ("0", _("Show Speed Dial"));
            combo.append ("1", _("Show Homepage"));
            combo.append ("2", _("Show last open tabs"));
            settings.bind_property ("load-on-startup", combo, "active", BindingFlags.SYNC_CREATE | BindingFlags.BIDIRECTIONAL);
            var button = new Midori.LabelWidget (_("When Midori starts:"), combo);
            box.add (button);
            box.show_all ();
            preferences.add (_("Browsing"), box);
            deactivate.connect (() => {
                box.destroy ();
            });
        }
    }

    public class ClearSession : Peas.ExtensionBase, Midori.ClearPrivateDataActivatable {
        public Gtk.Box box { owned get; set; }

        Gtk.CheckButton button;

        public void activate () {
            button = new Gtk.CheckButton.with_mnemonic (_("Last open _tabs"));
            button.show ();
            box.add (button);
        }

        public async void clear (TimeSpan timespan) {
            if (!button.active) {
                return;
            }

            try {
                yield SessionDatabase.get_default ().clear (timespan);
            } catch (Midori.DatabaseError error) {
                critical ("Failed to clear session: %s", error.message);
            }
        }
    }
}

[ModuleInit]
public void peas_register_types(TypeModule module) {
    ((Peas.ObjectModule)module).register_extension_type (
        typeof (Midori.BrowserActivatable), typeof (Tabby.Session));
    ((Peas.ObjectModule)module).register_extension_type (
        typeof (Midori.PreferencesActivatable), typeof (Tabby.Preferences));
    ((Peas.ObjectModule)module).register_extension_type (
        typeof (Midori.ClearPrivateDataActivatable), typeof (Tabby.ClearSession));

}
