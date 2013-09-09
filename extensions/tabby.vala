/*
   Copyright (C) 2013 André Stösel <andre@stoesel.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

namespace Tabby {
    /* function called from Manager object */
    public interface IStorage : GLib.Object {
        public abstract Katze.Array get_sessions ();
        public abstract Base.Session get_new_session ();
        public abstract void restore_last_sessions ();
        public abstract void import_session (Katze.Array tabs);
    }

    public interface ISession : GLib.Object {
        public abstract Katze.Array get_tabs ();
        public abstract void add_item (Katze.Item item);
        public abstract void attach (Midori.Browser browser);
        public abstract void restore (Midori.Browser browser);
        public abstract void close ();
    }

    namespace Base {
        /* each base class should connect to all necessary signals and provide an abstract function to handle them */

        public abstract class Storage : GLib.Object, IStorage {
            public Midori.App app { get; construct; }

            public abstract Katze.Array get_sessions ();
            public abstract Base.Session get_new_session ();
            public void restore_last_sessions () {
                Katze.Array sessions = this.get_sessions ();
                if (sessions.is_empty ()) {
                    sessions.add_item (this.get_new_session ());
                }

                GLib.List<unowned Katze.Item> items = sessions.get_items ();
                foreach (Katze.Item item in items) {
                    Session session = item as Session;
                    Midori.Browser browser = this.app.create_browser ();

                    /* FixMe: tabby-session should be set in .restore and .attch */
                    browser.set_data<Base.Session> ("tabby-session", session as Base.Session);

                    app.add_browser (browser);
                    browser.show ();

                    session.restore (browser);
                }
            }

            public virtual void import_session (Katze.Array tabs) {
                Session session = this.get_new_session ();
                GLib.List<unowned Katze.Item> items = tabs.get_items ();
                foreach (Katze.Item item in items) {
                    session.add_item (item);
                }
            }
        }

        public abstract class Session : GLib.Object, ISession {
            public abstract void add_item (Katze.Item item);
            public abstract void uri_changed (Midori.View view, string uri);
            public abstract void tab_added (Midori.Browser browser, Midori.View view);
            public abstract void tab_removed (Midori.Browser browser, Midori.View view);
            public abstract void close ();
            public abstract Katze.Array get_tabs ();

            public void attach (Midori.Browser browser) {
                browser.add_tab.connect (this.tab_added);
                browser.add_tab.connect (this.helper_uri_changed);
                browser.remove_tab.connect (this.tab_removed);

                foreach (Midori.View view in browser.get_tabs ()) {
                    this.tab_added (browser, view);
                    this.helper_uri_changed (browser, view);
                }
            }

            public void restore (Midori.Browser browser) {
                Katze.Array tabs = this.get_tabs ();

                if(tabs.is_empty ()) {
                    Katze.Item item = new Katze.Item ();
                    item.uri = "about:home";
                    tabs.add_item (item);
                }

                browser.add_tab.connect (this.tab_added);
                browser.add_tab.connect (this.helper_uri_changed);
                browser.remove_tab.connect (this.tab_removed);

                GLib.List<unowned Katze.Item> items = tabs.get_items ();
                unowned GLib.List<unowned Katze.Item> u_items = items;

                bool delay = false;

                GLib.Idle.add (() => {
                    /* Note: we need to use `items` for something to maintain a valid reference */
                    if (items.length () > 0) {
                        for (int i = 0; i < 3; i++) {
                            if (u_items == null)
                                return false;

                            Katze.Item t_item = u_items.data<Katze.Item>;

                            if (delay)
                                t_item.set_meta_integer ("delay", Midori.Delay.DELAYED);
                            else
                                delay = true;

                            browser.add_item (t_item);

                            u_items = u_items.next;
                        }
                    }
                    return u_items != null;
                });
            }

            private void helper_uri_changed (Midori.Browser browser, Midori.View view) {
                /* FixMe: skip first event while restoring the session */
                view.web_view.notify["uri"].connect ( () => {
                    this.uri_changed (view, view.web_view.uri);
                });
            }
        }
    }

    namespace Local {
        private class Session : Base.Session {
            public static int open_sessions = 0;
            public int64 id { get; private set; }
            private unowned Sqlite.Database db;

            public override void add_item (Katze.Item item) {
                GLib.DateTime time = new DateTime.now_local ();
                string sqlcmd = "INSERT INTO `tabs` (`crdate`, `tstamp`, `session_id`, `uri`, `title`) VALUES (:tstamp, :tstamp, :session_id, :uri, :title);";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to update database: %s"), db.errmsg);
                stmt.bind_int64 (stmt.bind_parameter_index (":tstamp"), time.to_unix ());
                stmt.bind_int64 (stmt.bind_parameter_index (":session_id"), this.id);
                stmt.bind_text (stmt.bind_parameter_index (":uri"), item.uri);
                stmt.bind_text (stmt.bind_parameter_index (":title"), item.name);
                if (stmt.step () != Sqlite.DONE)
                    critical (_("Failed to update database: %s"), db.errmsg);
                else {
                    int64 tab_id = this.db.last_insert_rowid ();
                    item.set_meta_integer ("tabby-id", tab_id);
                }
            }

            protected override void uri_changed (Midori.View view, string uri) {
                unowned Katze.Item item = view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                string sqlcmd = "UPDATE `tabs` SET uri = :uri, title = :title WHERE session_id = :session_id AND id = :tab_id;";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to update database: %s"), db.errmsg ());
                stmt.bind_text (stmt.bind_parameter_index (":uri"), uri);
                stmt.bind_text (stmt.bind_parameter_index (":title"), view.get_display_title ());
                stmt.bind_int64 (stmt.bind_parameter_index (":session_id"), this.id);
                stmt.bind_int64 (stmt.bind_parameter_index (":tab_id"), tab_id);
                if (stmt.step () != Sqlite.DONE)
                    critical (_("Failed to update database: %s"), db.errmsg ());
            }

            protected override void tab_added (Midori.Browser browser, Midori.View view) {
                unowned Katze.Item item = view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                if (tab_id < 1) {
                    this.add_item (item);
                }
           }

            protected override void tab_removed (Midori.Browser browser, Midori.View view) {
                unowned Katze.Item item = view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                /* FixMe: mark as deleted */
                string sqlcmd = "DELETE FROM `tabs` WHERE session_id = :session_id AND id = :tab_id;";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to update database: %s"), db.errmsg ());
                stmt.bind_int64 (stmt.bind_parameter_index (":session_id"), this.id);
                stmt.bind_int64 (stmt.bind_parameter_index (":tab_id"), tab_id);
                if (stmt.step () != Sqlite.DONE)
                    critical (_("Failed to update database: %s"), db.errmsg ());
            }

            public override void close() {
                if (Session.open_sessions == 1)
                    return;

                GLib.DateTime time = new DateTime.now_local ();
                string sqlcmd = "UPDATE `sessions` SET closed = 1, tstamp = :tstamp WHERE id = :session_id;";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to update database: %s"), db.errmsg ());

                stmt.bind_int64 (stmt.bind_parameter_index (":session_id"), this.id);
                stmt.bind_int64 (stmt.bind_parameter_index (":tstamp"), time.to_unix ());
                if (stmt.step () != Sqlite.DONE)
                    critical (_("Failed to update database: %s"), db.errmsg ());
            }

            public override Katze.Array get_tabs() {
                Katze.Array tabs = new Katze.Array (typeof (Katze.Item));

                string sqlcmd = "SELECT id, uri, title FROM tabs WHERE session_id = :session_id";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to select from database: %s"), db.errmsg ());
                stmt.bind_int64 (stmt.bind_parameter_index (":session_id"), this.id);
                int result = stmt.step ();
                if (!(result == Sqlite.DONE || result == Sqlite.ROW)) {
                    critical (_("Failed to select from database: %s"), db.errmsg ());
                    return tabs;
                }

                while (result == Sqlite.ROW) {
                    Katze.Item item = new Katze.Item ();
                    int64 id = stmt.column_int64 (0);
                    string uri = stmt.column_text (1);
                    string title = stmt.column_text (2);
                    item.uri = uri;
                    item.name = title;
                    item.set_meta_integer ("tabby-id", id);
                    tabs.add_item (item);
                    result = stmt.step ();
                 }

                 return tabs;
            }

            internal Session (Sqlite.Database db) {
                this.db = db;

                GLib.DateTime time = new DateTime.now_local ();

                string sqlcmd = "INSERT INTO `sessions` (`tstamp`) VALUES (:tstamp);";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to update database: %s"), db.errmsg);
                stmt.bind_int64 (stmt.bind_parameter_index (":tstamp"), time.to_unix ());
                if (stmt.step () != Sqlite.DONE)
                    critical (_("Failed to update database: %s"), db.errmsg);
                else
                    this.id = this.db.last_insert_rowid ();
            }

            internal Session.with_id (Sqlite.Database db, int64 id) {
                this.db = db;
                this.id = id;

                GLib.DateTime time = new DateTime.now_local ();
                string sqlcmd = "UPDATE `sessions` SET closed = 0, tstamp = :tstamp WHERE id = :session_id;";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to update database: %s"), db.errmsg);

                stmt.bind_int64 (stmt.bind_parameter_index (":session_id"), this.id);
                stmt.bind_int64 (stmt.bind_parameter_index (":tstamp"), time.to_unix ());
                if (stmt.step () != Sqlite.DONE)
                    critical (_("Failed to update database: %s"), db.errmsg);
            }

            construct {
                Session.open_sessions++;
            }

            ~Session () {
                Session.open_sessions--;
            }

        }

        private class Storage : Base.Storage {
            protected Sqlite.Database db;

            public override Katze.Array get_sessions () {
                Katze.Array sessions = new Katze.Array (typeof (Session));

                string sqlcmd = "SELECT id FROM sessions WHERE closed = 0;";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to select from database: %s"), db.errmsg);
                int result = stmt.step ();
                if (!(result == Sqlite.DONE || result == Sqlite.ROW)) {
                    critical (_("Failed to select from database: %s"), db.errmsg);
                    return sessions;
                }

                while (result == Sqlite.ROW) {
                    int64 id = stmt.column_int64 (0);
                    sessions.add_item (new Session.with_id (this.db, id));
                    result = stmt.step ();
                 }
 
                if (sessions.is_empty ()) {
                    sessions.add_item (new Session (this.db));
                }

                return sessions;
            }

            public override void import_session (Katze.Array tabs) {
                this.db.exec ("BEGIN;");
                base.import_session(tabs);
                this.db.exec("COMMIT;");
            }

            public override Base.Session get_new_session () {
                return new Session (this.db) as Base.Session;
            }

            internal Storage (Midori.App app) {
                GLib.Object (app: app);

                string db_path = Midori.Paths.get_config_filename_for_writing ("tabby.db");

                /* FixMe: why does GLib.FileUtils.test(db_path, GLib.FileTest.EXISTS); randomly work or not? */

                if (Sqlite.Database.open_v2 (db_path, out this.db) != Sqlite.OK)
                    critical (_("Failed to open stored session: %s"), db.errmsg);

                string filename = Midori.Paths.get_res_filename ("tabby/Create.sql");
                string schema;
                try {
                        bool success = FileUtils.get_contents (filename, out schema, null);
                        if (!success || schema == null)
                            critical (_("Failed to open database schema file: %s"), filename);
                        if (success && schema != null)
                            if (this.db.exec (schema) != Sqlite.OK)
                                critical (_("Failed to execute database schema: %s"), filename);
                            else {
                                string config_file = Midori.Paths.get_config_filename_for_reading ("session.xbel");
                                try {
                                    Katze.Array old_session = new Katze.Array (typeof (Katze.Item));
                                    Midori.array_from_file (old_session, config_file, "xbel-tiny");
                                    this.import_session (old_session);
                                } catch (GLib.FileError file_error) {
                                    /* no old session.xbel -> could be a new profile -> ignore it */
                                } catch (GLib.Error error) {
                                    critical (_("Failed to import legacy session: %s"), error.message);
                                }
                            }
                } catch (GLib.FileError schema_error) {
                    critical (_("Failed to open database schema file: %s"), schema_error.message);
                }
            }
        }
    }

    private class Manager : Midori.Extension {
        private Base.Storage storage;
        private bool load_session () {
            this.storage.restore_last_sessions ();
            return false;
        }

        private void browser_added (Midori.Browser browser) {
            Base.Session session = browser.get_data<Base.Session> ("tabby-session");
            if (session == null) {
                session = this.storage.get_new_session () as Base.Session;
                browser.set_data<Base.Session> ("tabby-session", session);
                session.attach (browser);
            }
        }

        private void browser_removed (Midori.Browser browser) {
            Base.Session session = browser.get_data<Base.Session> ("tabby-session");
            if (session == null) {
                GLib.warning ("missing session");
            } else {
                session.close ();
            }
        }

        private void activated (Midori.App app) {
            /* FixMe: provide an option to replace Local.Storage with IStorage based Objects */
            this.storage = new Local.Storage (this.get_app ()) as Base.Storage;

            app.add_browser.connect (browser_added);
            app.remove_browser.connect (browser_removed);

            GLib.Idle.add (this.load_session);
        }

        internal Manager () {
            GLib.Object (name: _("Tabby"),
                         description: _("Tab and session management."),
                         version: "0.1",
                         authors: "André Stösel <andre@stoesel.de>");

            activate.connect (this.activated);
        }
    }
}

public Midori.Extension extension_init () {
    return new Tabby.Manager ();
}
