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
        public abstract Katze.Array get_saved_sessions ();
        public abstract Base.Session get_new_session ();
        public abstract void restore_last_sessions ();
        public abstract void import_session (Katze.Array tabs);
    }

    public interface ISession : GLib.Object {
        public abstract Katze.Array get_tabs ();
        /* Add one tab to the database */
        public abstract void add_item (Katze.Item item);
        /* Attach to a browser */
        public abstract void attach (Midori.Browser browser);
        /* Attach to a browser and populate it with tabs from the database */
        public abstract void restore (Midori.Browser browser);
        /* Remove all tabs from the database */
        public abstract void remove ();
        /* Run when a browser is closed */
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

            public abstract Katze.Array get_saved_sessions ();
            public abstract Base.Session get_new_session ();

            public void start_new_session () {
                Katze.Array sessions = new Katze.Array (typeof (Session));
                this.init_sessions (sessions);
            }

            public void restore_last_sessions () {
                Katze.Array sessions = this.get_saved_sessions ();
                this.init_sessions (sessions);
            }

            private void init_sessions (Katze.Array sessions) {
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
                    // See midori_browser_step_history: don't add to history
                    item.set_meta_string ("history-step", "ignore");
                    i += 1024;
                    session.add_item (item);
                }
            }
        }

        public abstract class Session : GLib.Object, ISession {
            protected GLib.SList<double?> tab_sorting;

            public Midori.Browser? browser { get; protected set; default = null; }
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
            public abstract double get_max_sorting ();

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
                    /* Using get here to avoid MidoriMidoriStartup in generated C with Vala 0.20.1 */
                    int load_on_startup;
                    APP.settings.get ("load-on-startup", out load_on_startup);

                    Katze.Item item = new Katze.Item ();

                    if (load_on_startup == Midori.MidoriStartup.BLANK_PAGE) {
                        item.uri = "about:dial";
                    } else {
                        item.uri = "about:home";
                    }

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
                bool should_delay = false;

                int load_on_startup;
                APP.settings.get ("load-on-startup", out load_on_startup);
                should_delay = load_on_startup == Midori.MidoriStartup.DELAYED_PAGES;

                if (APP.crashed == true) {
                    delay = true;
                    should_delay = true;
                }

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

                            if (delay && should_delay)
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
                if (this.state == SessionState.CLOSED) {
                    assert (this.browser == null);
                } else {
                    this.state = SessionState.CLOSED;

                    this.browser.add_tab.disconnect (this.tab_added);
                    this.browser.add_tab.disconnect (this.helper_data_changed);
                    this.browser.remove_tab.disconnect (this.tab_removed);
                    this.browser.switch_tab.disconnect (this.tab_switched);
                    this.browser.delete_event.disconnect (this.delete_event);
                    this.browser.notebook.page_reordered.disconnect (this.tab_reordered);

                    this.browser = null;
                }
            }

#if HAVE_GTK3
            protected bool delete_event (Gtk.Widget widget, Gdk.EventAny event) {
#else
            protected bool delete_event (Gtk.Widget widget, Gdk.Event event) {
#endif

                this.close ();
                return false;

            }

            protected double get_tab_sorting (Midori.View view) {
                int this_pos = this.browser.notebook.page_num (view);
                Midori.View prev_view = this.browser.notebook.get_nth_page (this_pos - 1) as Midori.View;
                Midori.View next_view = this.browser.notebook.get_nth_page (this_pos + 1) as Midori.View;

                string prev_meta_sorting = null;
                string next_meta_sorting = null;
                double prev_sorting, next_sorting, this_sorting;

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

                    double sorting;
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
            private Midori.Database database;

            public override void add_item (Katze.Item item) {
                GLib.DateTime time = new DateTime.now_local ();
                string? sorting = item.get_meta_string ("sorting") ?? "1";
                string sqlcmd = "INSERT INTO `tabs` (`crdate`, `tstamp`, `session_id`, `uri`, `title`, `sorting`) VALUES (:crdate, :tstamp, :session_id, :uri, :title, :sorting);";

                int64 tstamp = item.get_meta_integer ("tabby-tstamp");
                if (tstamp < 0) { // new tab without focus
                    tstamp = 0;
                }

                try {
                    var statement = database.prepare (sqlcmd,
                        ":crdate", typeof (int64), time.to_unix (),
                        ":tstamp", typeof (int64), tstamp,
                        ":session_id", typeof (int64), this.id,
                        ":uri", typeof (string), item.uri,
                        ":title", typeof (string), item.name,
                        ":sorting", typeof (double), double.parse (sorting));
                    statement.exec ();
                    int64 tab_id = statement.row_id ();
                    item.set_meta_integer ("tabby-id", tab_id);
                } catch (Error error) {
                    critical (_("Failed to update database: %s"), error.message);
                }
            }

            protected override void uri_changed (Midori.View view, string uri) {
                unowned Katze.Item item = view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                string sqlcmd = "UPDATE `tabs` SET uri = :uri WHERE session_id = :session_id AND id = :tab_id;";
                try {
                    database.prepare (sqlcmd,
                                      ":uri", typeof (string), uri,
                                      ":session_id", typeof (int64), this.id,
                                      ":tab_id", typeof (int64), tab_id).exec ();
                } catch (Error error) {
                    critical (_("Failed to update database: %s"), error.message);
                }
            }

            protected override void data_changed (Midori.View view) {
                unowned Katze.Item item = view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                string sqlcmd = "UPDATE `tabs` SET title = :title WHERE session_id = :session_id AND id = :tab_id;";
                try {
                    database.prepare (sqlcmd,
                                      ":title", typeof (string), view.get_display_title (),
                                      ":session_id", typeof (int64), this.id,
                                      ":tab_id", typeof (int64), tab_id).exec ();
                } catch (Error error) {
                    critical (_("Failed to update database: %s"), error.message);
                }
            }

            protected override void tab_added (Midori.Browser browser, Midori.View view) {
                unowned Katze.Item item = view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                if (tab_id < 1) {
                    double sorting = this.get_tab_sorting (view);
                    item.set_meta_string ("sorting", sorting.to_string ());
                    this.add_item (item);
                }
           }

            protected override void tab_removed (Midori.Browser browser, Midori.View view) {
                unowned Katze.Item item = view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                /* FixMe: mark as deleted */
                string sqlcmd = "DELETE FROM `tabs` WHERE session_id = :session_id AND id = :tab_id;";
                try {
                    database.prepare (sqlcmd,
                                      ":session_id", typeof (int64), this.id,
                                      ":tab_id", typeof (int64), tab_id).exec ();
                } catch (Error error) {
                    critical (_("Failed to update database: %s"), error.message);
                }
            }

            protected override void tab_switched (Midori.View? old_view, Midori.View? new_view) {
                GLib.DateTime time = new DateTime.now_local ();
                unowned Katze.Item item = new_view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                int64 tstamp = time.to_unix();
                item.set_meta_integer ("tabby-tstamp", tstamp);
                string sqlcmd = "UPDATE `tabs` SET tstamp = :tstamp WHERE session_id = :session_id AND id = :tab_id;";
                try {
                    database.prepare (sqlcmd,
                        ":session_id", typeof (int64), this.id,
                        ":tab_id", typeof (int64), tab_id,
                        ":tstamp", typeof (int64), tstamp).exec ();
                } catch (Error error) {
                    critical (_("Failed to update database: %s"), error.message);
                }
            }

            protected override void tab_reordered (Gtk.Widget tab, uint pos) {
                Midori.View view = tab as Midori.View;

                double sorting = this.get_tab_sorting (view);
                unowned Katze.Item item = view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                string sqlcmd = "UPDATE `tabs` SET sorting = :sorting WHERE session_id = :session_id AND id = :tab_id;";
                try {
                    database.prepare (sqlcmd,
                        ":session_id", typeof (int64), this.id,
                        ":tab_id", typeof (int64), tab_id,
                        ":sorting", typeof (double), sorting).exec ();
                } catch (Error error) {
                    critical (_("Failed to update database: %s"), error.message);
                }

                item.set_meta_string ("sorting", sorting.to_string ());
            }

            public override void remove() {
                string sqlcmd = """
                    DELETE FROM `tabs` WHERE session_id = :session_id;
                    DELETE FROM `sessions` WHERE id = :session_id;
                    """;
                try {
                    database.prepare (sqlcmd,
                        ":session_id", typeof (int64), this.id). exec ();
                } catch (Error error) {
                    critical (_("Failed to update database: %s"), error.message);
                }
            }

            public override void close () {
                /* base.close may unset this.browser, so hold onto it */
                Midori.Browser? my_browser = this.browser;
                base.close ();

                bool should_break = true;
                if (my_browser != null && !my_browser.destroy_with_parent) {
                    foreach (Midori.Browser browser in APP.get_browsers ()) {
                        if (browser != my_browser && !browser.destroy_with_parent) {
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
                try {
                    database.prepare (sqlcmd,
                        ":session_id", typeof (int64), this.id,
                        ":tstamp", typeof (int64), time.to_unix ()).exec ();
                } catch (Error error) {
                    critical (_("Failed to update database: %s"), error.message);
                }
            }

            public override Katze.Array get_tabs() {
                Katze.Array tabs = new Katze.Array (typeof (Katze.Item));

                string sqlcmd = "SELECT id, uri, title, sorting FROM tabs WHERE session_id = :session_id ORDER BY tstamp DESC";
                try {
                    var statement = database.prepare (sqlcmd,
                        ":session_id", typeof (int64), this.id);
                    while (statement.step ()) {
                        Katze.Item item = new Katze.Item ();
                        int64 id = statement.get_int64 ("id");
                        string uri = statement.get_string ("uri");
                        string title = statement.get_string ("title");
                        double sorting = statement.get_double ("sorting");
                        item.uri = uri;
                        item.name = title;
                        item.set_meta_integer ("tabby-id", id);
                        item.set_meta_string ("sorting", sorting.to_string ());
                        // See midori_browser_step_history: don't add to history
                        item.set_meta_string ("history-step", "ignore");
                        tabs.add_item (item);
                    }
                } catch (Error error) {
                    critical (_("Failed to select from database: %s"), error.message);
                }
                return tabs;
            }

            public override double get_max_sorting () {
                string sqlcmd = "SELECT MAX(sorting) FROM tabs WHERE session_id = :session_id";
                try {
                    var statement = database.prepare (sqlcmd,
                        ":session_id", typeof (int64), this.id);
                    statement.step ();
                    double sorting = statement.get_double ("MAX(sorting)");
                    if (!sorting.is_nan ()) {
                        return sorting;
                    }
                } catch (Error error) {
                    critical (_("Failed to select from database: %s"), error.message);
                }

                 return 0.0;
            }

            internal Session (Midori.Database database) {
                this.database = database;

                GLib.DateTime time = new DateTime.now_local ();

                string sqlcmd = "INSERT INTO `sessions` (`tstamp`) VALUES (:tstamp);";

                try {
                    var statement = database.prepare (sqlcmd,
                        ":tstamp", typeof (int64), time.to_unix ());
                    statement.exec ();
                    this.id = statement.row_id ();
                } catch (Error error) {
                    critical (_("Failed to update database: %s"), error.message);
                }
            }

            internal Session.with_id (Midori.Database database, int64 id) {
                this.database = database;
                this.id = id;

                GLib.DateTime time = new DateTime.now_local ();
                string sqlcmd = "UPDATE `sessions` SET closed = 0, tstamp = :tstamp WHERE id = :session_id;";

                try {
                    database.prepare (sqlcmd,
                        ":session_id", typeof (int64), this.id,
                        ":tstamp", typeof (int64), time.to_unix ()).exec ();
                } catch (Error error) {
                    critical (_("Failed to update database: %s"), error.message);
                }
            }
        }

        private class Storage : Base.Storage {
            private Midori.Database database;

            public override Katze.Array get_saved_sessions () {
                Katze.Array sessions = new Katze.Array (typeof (Session));

                string sqlcmd = """
                    SELECT id, closed FROM sessions WHERE closed = 0
                    UNION
                    SELECT * FROM (SELECT id, closed FROM sessions WHERE closed = 1 ORDER BY tstamp DESC LIMIT 1)
                    ORDER BY closed;
                """;
                try {
                    var statement = database.prepare (sqlcmd);
                    while (statement.step ()) {
                        int64 id = statement.get_int64 ("id");
                        int64 closed = statement.get_int64 ("closed");
                        if (closed == 0 || sessions.is_empty ()) {
                            sessions.add_item (new Session.with_id (this.database, id));
                        }
                    }
                } catch (Error error) {
                    critical (_("Failed to select from database: %s"), error.message);
                }

                if (sessions.is_empty ()) {
                    sessions.add_item (new Session (this.database));
                }

                return sessions;
            }

            public override void import_session (Katze.Array tabs) {
                try {
                    database.transaction (()=>{
                        base.import_session(tabs); return true;
                    });
                } catch (Error error) {
                    critical (_("Failed to select from database: %s"), error.message);
                }
            }

            public override Base.Session get_new_session () {
                return new Session (this.database) as Base.Session;
            }

            internal Storage (Midori.App app) {
                GLib.Object (app: app);

                try {
                    database = new Midori.Database ("tabby.db");
                } catch (Midori.DatabaseError schema_error) {
                    error (schema_error.message);
                }

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
            /* Using get here to avoid MidoriMidoriStartup in generated C with Vala 0.20.1 */
            int load_on_startup;
            APP.settings.get ("load-on-startup", out load_on_startup);
            if (load_on_startup == Midori.MidoriStartup.BLANK_PAGE
             || load_on_startup == Midori.MidoriStartup.HOMEPAGE) {
                this.storage.start_new_session ();
            } else {
                this.storage.restore_last_sessions ();
            }

            /* FIXME: execute_commands should be called before session creation */
            GLib.Idle.add (this.execute_commands);

            return false;
        }

        private bool execute_commands () {
            Midori.App app = this.get_app ();
            unowned string?[] commands = app.get_data ("execute-commands");

            if (commands != null) {
                app.send_command (commands);
            }

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
            Base.Session? session = browser.get_data<Base.Session> ("tabby-session");
            if (session == null) {
                session = this.storage.get_new_session () as Base.Session;
                browser.set_data<Base.Session> ("tabby-session", session);
                session.attach (browser);
            }
        }

        private void browser_removed (Midori.Browser browser) {
            Base.Session? session = browser.get_data<Base.Session> ("tabby-session");
            if (session == null) {
                GLib.warning ("missing session");
            } else {
                session.close ();

                /* Using get here to avoid MidoriMidoriStartup in generated C with Vala 0.20.1 */
                int load_on_startup;
                APP.settings.get ("load-on-startup", out load_on_startup);

                if (browser.destroy_with_parent
                 || load_on_startup < Midori.MidoriStartup.LAST_OPEN_PAGES) {
                    /* Remove js popups and close if not restoring on startup */
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

        private void deactivated () {
            /* set_open_uris will disconnect itself if called,
               but it may have been called before we are deactivated */
            APP.add_browser.disconnect (this.set_open_uris);
            APP.add_browser.disconnect (this.browser_added);
            APP.remove_browser.disconnect (this.browser_removed);
            APP = null;

            this.storage = null;
        }

        internal Manager () {
            GLib.Object (name: _("Tabby"),
                         description: _("Tab and session management."),
                         version: "0.1",
                         authors: "André Stösel <andre@stoesel.de>");

            this.activate.connect (this.activated);
            this.deactivate.connect (this.deactivated);
        }
    }
}

public Midori.Extension extension_init () {
    return new Tabby.Manager ();
}
