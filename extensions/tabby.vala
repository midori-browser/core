/*
   Copyright (C) 2013 André Stösel <andre@stoesel.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

namespace Tabby {
    int IDLE_RESTORE_COUNT = 13;
    /* FixMe: don't use a global object */
    Midori.App? APP;

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
        public abstract void remove ();
        public abstract void close ();
    }

    public enum SessionState {
        OPEN,
        CLOSED,
        RESTORING
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
                double i = 0;
                foreach (Katze.Item item in items) {
                    item.set_meta_string ("sorting", i.to_string());
                    i += 1024;
                    session.add_item (item);
                }
            }
        }

        public abstract class Session : GLib.Object, ISession {
            protected GLib.SList<double?> tab_sorting;

            public Midori.Browser browser { get; protected set; }
            public SessionState state { get; protected set; default = SessionState.CLOSED; }

            public abstract void add_item (Katze.Item item);
            public abstract void uri_changed (Midori.View view, string uri);
            public abstract void data_changed (Midori.View view);
            public abstract void tab_added (Midori.Browser browser, Midori.View view);
            public abstract void tab_removed (Midori.Browser browser, Midori.View view);
            public abstract void tab_switched (Midori.View? old_view, Midori.View? new_view);
            public abstract void tab_reordered (Gtk.Widget tab, uint pos);

            public abstract void remove ();

            public abstract Katze.Array get_tabs ();
            public abstract double? get_max_sorting ();

            public void attach (Midori.Browser browser) {
                this.browser = browser;

                browser.add_tab.connect_after (this.tab_added);
                browser.add_tab.connect (this.helper_data_changed);

                browser.remove_tab.connect (this.tab_removed);
                browser.switch_tab.connect (this.tab_switched);
                browser.delete_event.connect_after(this.delete_event);
                browser.notebook.page_reordered.connect_after (this.tab_reordered);

                this.state = SessionState.OPEN;

                foreach (Midori.View view in browser.get_tabs ()) {
                    this.tab_added (browser, view);
                    this.helper_data_changed (browser, view);
                }
            }

            public void restore (Midori.Browser browser) {
                this.browser = browser;

                Katze.Array tabs = this.get_tabs ();
                unowned Katze.Array? open_uris = browser.get_data ("tabby-open-uris");

                if(tabs.is_empty () && open_uris == null) {
                    Katze.Item item = new Katze.Item ();
                    item.uri = "about:home";
                    tabs.add_item (item);
                }

                browser.add_tab.connect_after (this.tab_added);
                browser.add_tab.connect (this.helper_data_changed);

                browser.remove_tab.connect (this.tab_removed);
                browser.switch_tab.connect (this.tab_switched);
                browser.delete_event.connect_after(this.delete_event);
                browser.notebook.page_reordered.connect_after (this.tab_reordered);

                GLib.List<unowned Katze.Item> items = new GLib.List<unowned Katze.Item> ();
                if (open_uris != null) {
                    items.concat (open_uris.get_items ());
                }
                items.concat (tabs.get_items ());
                unowned GLib.List<unowned Katze.Item> u_items = items;

                bool delay = false;

                this.state = SessionState.RESTORING;

                GLib.Idle.add (() => {
                    /* Note: we need to use `items` for something to maintain a valid reference */
                    GLib.PtrArray new_tabs = new GLib.PtrArray ();
                    if (items.length () > 0) {
                        for (int i = 0; i < IDLE_RESTORE_COUNT; i++) {
                            if (u_items == null) {
                                this.helper_reorder_tabs (new_tabs);
                                this.state = SessionState.OPEN;
                                return false;
                            }

                            Katze.Item t_item = u_items.data<Katze.Item>;

                            t_item.set_meta_integer ("append", 1);

                            if (delay)
                                t_item.set_meta_integer ("delay", Midori.Delay.DELAYED);
                            else
                                delay = true;

                            unowned Gtk.Widget tab = browser.add_item (t_item);
                            new_tabs.add (tab);

                            u_items = u_items.next;
                        }
                        this.helper_reorder_tabs (new_tabs);
                    }
                    if (u_items == null) {
                        this.state = SessionState.OPEN;
                        return false;
                    }
                    return true;
                });
            }

            public virtual void close () {
                this.browser.add_tab.disconnect (this.tab_added);
                this.browser.add_tab.disconnect (this.helper_data_changed);
                this.browser.remove_tab.disconnect (this.tab_removed);
                this.browser.switch_tab.disconnect (this.tab_switched);
                this.browser.delete_event.disconnect (this.delete_event);
                this.browser.notebook.page_reordered.disconnect (this.tab_reordered);
            }

#if HAVE_GTK3
            protected bool delete_event (Gtk.Widget widget, Gdk.EventAny event) {
#else
            protected bool delete_event (Gtk.Widget widget, Gdk.Event event) {
#endif

                this.close();
                return false;

            }

            protected double? get_tab_sorting (Midori.View view) {
                int this_pos = this.browser.notebook.page_num (view);
                Midori.View prev_view = this.browser.notebook.get_nth_page (this_pos - 1) as Midori.View;
                Midori.View next_view = this.browser.notebook.get_nth_page (this_pos + 1) as Midori.View;

                string? prev_meta_sorting = null;
                string? next_meta_sorting = null;
                double? prev_sorting, next_sorting, this_sorting;

                if (prev_view != null) {
                    unowned Katze.Item prev_item = prev_view.get_proxy_item ();
                    prev_meta_sorting = prev_item.get_meta_string ("sorting");
                }

                if (prev_meta_sorting == null)
                    if (this.state == SessionState.RESTORING)
                        prev_sorting = this.get_max_sorting ();
                    else
                        prev_sorting = double.parse ("0");
                else
                    prev_sorting = double.parse (prev_meta_sorting);

                if (next_view != null) {
                    unowned Katze.Item next_item = next_view.get_proxy_item ();
                    next_meta_sorting = next_item.get_meta_string ("sorting");
                }

                if (next_meta_sorting == null)
                    next_sorting = prev_sorting + 2048;
                else
                    next_sorting = double.parse (next_meta_sorting);

                this_sorting = prev_sorting + (next_sorting - prev_sorting) / 2;

                return this_sorting;
            }

            private void load_status (GLib.Object _view, ParamSpec pspec) {
                Midori.View view = (Midori.View)_view;

                if (view.load_status == Midori.LoadStatus.PROVISIONAL) {
                    unowned Katze.Item item = view.get_proxy_item ();

                    int64 delay = item.get_meta_integer ("delay");
                    if (delay == Midori.Delay.UNDELAYED) {
                        view.web_view.notify["uri"].connect ( () => {
                                this.uri_changed (view, view.web_view.uri);
                            });
                        view.web_view.notify["title"].connect ( () => {
                                this.data_changed (view);
                            });

                    }

                    view.notify["load-status"].disconnect (load_status);
                }
            }

            private void helper_data_changed (Midori.Browser browser, Midori.View view) {
               view.notify["load-status"].connect (load_status);

               view.new_view.connect (this.helper_duplicate_tab);
            }

            private void helper_reorder_tabs (GLib.PtrArray new_tabs) {
                CompareDataFunc<double?> helper_compare_data = (a, b) => {
                    if (a > b)
                        return 1;
                    else if(a < b)
                        return -1;
                    return 0;
                };

                GLib.CompareFunc<double?> helper_compare_func = (a,b) => {
                    return a == b ? 0 : -1;
                };

                this.browser.notebook.page_reordered.disconnect (this.tab_reordered);
                for(var i = 0; i < new_tabs.len; i++) {
                    Midori.View tab = new_tabs.index(i) as Midori.View;

                    unowned Katze.Item item = tab.get_proxy_item ();

                    double? sorting;
                    string? sorting_string = item.get_meta_string ("sorting");
                    if (sorting_string != null) { /* we have to use a seperate if condition to avoid a `possibly unassigned local variable` error */
                        if (double.try_parse (item.get_meta_string ("sorting"), out sorting)) {
                            this.tab_sorting.insert_sorted_with_data (sorting, helper_compare_data);

                            int index = this.tab_sorting.position (this.tab_sorting.find_custom (sorting, helper_compare_func));

                            this.browser.notebook.reorder_child (tab, index);
                        }
                    }
                }
                this.browser.notebook.page_reordered.connect_after (this.tab_reordered);
            }

            private void helper_duplicate_tab (Midori.View view, Midori.View new_view, Midori.NewView where, bool user_initiated) {
                unowned Katze.Item item = view.get_proxy_item ();
                unowned Katze.Item new_item = new_view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                int64 new_tab_id = new_item.get_meta_integer ("tabby-id");

                if (tab_id > 0 && tab_id == new_tab_id) {
                    new_item.set_meta_integer ("tabby-id", 0);
                }
            }

            construct {
                this.tab_sorting = new GLib.SList<double?> ();
            }
        }
    }

    namespace Local {
        private class Session : Base.Session {
            public int64 id { get; private set; }
            private unowned Sqlite.Database db;

            public override void add_item (Katze.Item item) {
                GLib.DateTime time = new DateTime.now_local ();
                string? sorting = item.get_meta_string ("sorting");
                string sqlcmd = "INSERT INTO `tabs` (`crdate`, `tstamp`, `session_id`, `uri`, `title`, `sorting`) VALUES (:tstamp, :tstamp, :session_id, :uri, :title, :sorting);";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to update database: %s"), db.errmsg);
                stmt.bind_int64 (stmt.bind_parameter_index (":tstamp"), time.to_unix ());
                stmt.bind_int64 (stmt.bind_parameter_index (":session_id"), this.id);
                stmt.bind_text (stmt.bind_parameter_index (":uri"), item.uri);
                stmt.bind_text (stmt.bind_parameter_index (":title"), item.name);
                if (sorting == null)
                    stmt.bind_double (stmt.bind_parameter_index (":sorting"), double.parse ("1"));
                else
                    stmt.bind_double (stmt.bind_parameter_index (":sorting"), double.parse (sorting));

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
                string sqlcmd = "UPDATE `tabs` SET uri = :uri WHERE session_id = :session_id AND id = :tab_id;";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to update database: %s"), db.errmsg ());
                stmt.bind_text (stmt.bind_parameter_index (":uri"), uri);
                stmt.bind_int64 (stmt.bind_parameter_index (":session_id"), this.id);
                stmt.bind_int64 (stmt.bind_parameter_index (":tab_id"), tab_id);
                if (stmt.step () != Sqlite.DONE)
                    critical (_("Failed to update database: %s"), db.errmsg ());
            }

            protected override void data_changed (Midori.View view) {
                unowned Katze.Item item = view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                string sqlcmd = "UPDATE `tabs` SET title = :title WHERE session_id = :session_id AND id = :tab_id;";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to update database: %s"), db.errmsg ());
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
                    double? sorting = this.get_tab_sorting (view);
                    item.set_meta_string ("sorting", sorting.to_string ());
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

            protected override void tab_switched (Midori.View? old_view, Midori.View? new_view) {
                GLib.DateTime time = new DateTime.now_local ();
                unowned Katze.Item item = new_view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                string sqlcmd = "UPDATE `tabs` SET tstamp = :tstamp WHERE session_id = :session_id AND id = :tab_id;";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to update database: %s"), db.errmsg ());
                stmt.bind_int64 (stmt.bind_parameter_index (":session_id"), this.id);
                stmt.bind_int64 (stmt.bind_parameter_index (":tab_id"), tab_id);
                stmt.bind_int64 (stmt.bind_parameter_index (":tstamp"), time.to_unix ());
                if (stmt.step () != Sqlite.DONE)
                    critical (_("Failed to update database: %s"), db.errmsg ());
            }

            protected override void tab_reordered (Gtk.Widget tab, uint pos) {
                Midori.View view = tab as Midori.View;

                double? sorting = this.get_tab_sorting (view);
                unowned Katze.Item item = view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                string sqlcmd = "UPDATE `tabs` SET sorting = :sorting WHERE session_id = :session_id AND id = :tab_id;";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to update database: %s"), db.errmsg ());
                stmt.bind_int64 (stmt.bind_parameter_index (":session_id"), this.id);
                stmt.bind_int64 (stmt.bind_parameter_index (":tab_id"), tab_id);
                stmt.bind_double (stmt.bind_parameter_index (":sorting"), sorting);

                if (stmt.step () != Sqlite.DONE)
                    critical (_("Failed to update database: %s"), db.errmsg ());

                item.set_meta_string ("sorting", sorting.to_string ());
            }

            public override void remove() {
                string sqlcmd = "DELETE FROM `tabs` WHERE session_id = :session_id;";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to update database: %s"), db.errmsg ());
                stmt.bind_int64 (stmt.bind_parameter_index (":session_id"), this.id);

                if (stmt.step () != Sqlite.DONE)
                    critical (_("Failed to update database: %s"), db.errmsg ());

                sqlcmd = "DELETE FROM `sessions` WHERE id = :session_id;";
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to update database: %s"), db.errmsg ());
                stmt.bind_int64 (stmt.bind_parameter_index (":session_id"), this.id);

                if (stmt.step () != Sqlite.DONE)
                    critical (_("Failed to update database: %s"), db.errmsg ());
            }

            public override void close() {
                base.close ();

                bool should_break = true;
                if (!this.browser.destroy_with_parent) {
                    foreach (Midori.Browser browser in APP.get_browsers ()) {
                        if (browser != this.browser && !browser.destroy_with_parent) {
                            should_break = false;
                            break;
                        }
                    }

                    if (should_break) {
                        return;
                    }
                }

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

                string sqlcmd = "SELECT id, uri, title, sorting FROM tabs WHERE session_id = :session_id ORDER BY tstamp DESC";
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
                    item.set_meta_string ("sorting", stmt.column_double (3).to_string ());
                    tabs.add_item (item);
                    result = stmt.step ();
                 }

                 return tabs;
            }

            public override double? get_max_sorting () {
                string sqlcmd = "SELECT MAX(sorting) FROM tabs WHERE session_id = :session_id";
                Sqlite.Statement stmt;
                if (this.db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK)
                    critical (_("Failed to select from database: %s"), db.errmsg ());
                stmt.bind_int64 (stmt.bind_parameter_index (":session_id"), this.id);
                int result = stmt.step ();
                if (!(result == Sqlite.DONE || result == Sqlite.ROW)) {
                    critical (_("Failed to select from database: %s"), db.errmsg ());
                } else if (result == Sqlite.ROW) {
                    double? sorting;
                    string? sorting_string = stmt.column_double (0).to_string ();
                    if (sorting_string != null) { /* we have to use a seperate if condition to avoid a `possibly unassigned local variable` error */
                        if (double.try_parse (sorting_string, out sorting)) {
                            return sorting;
                        }
                    }
                 }

                 return double.parse ("0");
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
        }

        private class Storage : Base.Storage {
            private Midori.Database database;
            private unowned Sqlite.Database db;

            public override Katze.Array get_sessions () {
                Katze.Array sessions = new Katze.Array (typeof (Session));

                string sqlcmd = """
                    SELECT id, closed FROM sessions WHERE closed = 0
                    UNION
                    SELECT * FROM (SELECT id, closed FROM sessions WHERE closed = 1 ORDER BY tstamp DESC LIMIT 1)
                    ORDER BY closed;
                """;
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
                    int64 closed = stmt.column_int64 (1);
                    if (closed == 0 || sessions.is_empty ()) {
                        sessions.add_item (new Session.with_id (this.db, id));
                    }
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

                try {
                    database = new Midori.Database ("tabby.db");
                } catch (Midori.DatabaseError schema_error) {
                    error (schema_error.message);
                }
                db = database.db;

                if (database.first_use) {
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
            }
        }
    }

    private class Manager : Midori.Extension {
        private Base.Storage storage;
        private bool load_session () {
            this.storage.restore_last_sessions ();
            return false;
        }

        private void set_open_uris (Midori.Browser browser) {
            Midori.App app = this.get_app ();
            unowned string?[] uris = app.get_data ("open-uris");

            if (uris != null) {
                Katze.Array tabs = new Katze.Array (typeof (Katze.Item));

                for(int i = 0; uris[i] != null; i++) {
                    Katze.Item item = new Katze.Item ();
                    item.name = uris[i];
                    item.uri = Midori.Sokoke.magic_uri (uris[i], true, true);
                    if (item.uri != null) {
                        tabs.add_item (item);
                    }
                }
                if (!tabs.is_empty()) {
                    browser.set_data ("tabby-open-uris", tabs);
                }
            }

            app.add_browser.disconnect (this.set_open_uris);
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
                if (browser.destroy_with_parent) {
                    /* remove js popup sessions */
                    session.remove ();
                }
            }
        }

        private void activated (Midori.App app) {
            APP = app;
            unowned string? restore_count = GLib.Environment.get_variable ("TABBY_RESTORE_COUNT");
            if (restore_count != null) {
                int count = int.parse (restore_count);
                if (count >= 1) {
                    IDLE_RESTORE_COUNT = count;
                }
            }

            /* FixMe: provide an option to replace Local.Storage with IStorage based Objects */
            this.storage = new Local.Storage (this.get_app ()) as Base.Storage;

            app.add_browser.connect (this.set_open_uris);
            app.add_browser.connect (this.browser_added);
            app.remove_browser.connect (this.browser_removed);

            GLib.Idle.add (this.load_session);
        }

        internal Manager () {
            GLib.Object (name: _("Tabby"),
                         description: _("Tab and session management."),
                         version: "0.1",
                         authors: "André Stösel <andre@stoesel.de>");

            this.activate.connect (this.activated);
        }
    }
}

public Midori.Extension extension_init () {
    return new Tabby.Manager ();
}
