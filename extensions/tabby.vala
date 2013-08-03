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
    }

    public interface ISession : GLib.Object {
        public abstract Katze.Array get_tabs ();
        public abstract void restore (Midori.Browser browser);
    }

    namespace Base {
        /* each base class should connect to all necessary signals and provide an abstract function to handle them */

        public abstract class Storage : GLib.Object, IStorage {
            public Midori.App app { get; construct; }

            public abstract Katze.Array get_sessions ();
            public abstract Base.Session get_new_session ();
            public void restore_last_sessions () {
                Katze.Array sessions = this.get_sessions ();
                GLib.List<unowned Katze.Item> items = sessions.get_items ();
                foreach (Katze.Item item in items) {
                    Session session = item as Session;
                    Midori.Browser browser = this.app.create_browser ();

                    browser.set_data<Base.Session> ("tabby-session", session as Base.Session);

                    app.add_browser (browser);
                    browser.show ();

                    session.restore (browser);
                }
            }
        }

        public abstract class Session : GLib.Object, ISession {
            public abstract void uri_changed (Midori.View view, string uri);
            public abstract void tab_added (Midori.Browser browser, Midori.View view);
            public abstract void tab_removed (Midori.Browser browser, Midori.View view);
            public abstract Katze.Array get_tabs ();

            public void restore (Midori.Browser browser) {
                Katze.Array tabs = this.get_tabs ();

                if(tabs.is_empty ()) {
                    Katze.Item item = new Katze.Item ();
                    item.uri = "about:home";
                    tabs.add_item (item);
                }

                /* FixMe: tabby-session should be set in this function */
                //browser.set_data<Base.Session> ("tabby-session", this);// as Base.Session);

                browser.add_tab.connect (this.tab_added);
                browser.add_tab.connect (this.helper_uri_changed);
                browser.remove_tab.connect (this.tab_removed);

                GLib.List<unowned Katze.Item> items = tabs.get_items ();
                unowned GLib.List<unowned Katze.Item> u_items = items;

                GLib.Idle.add (() => {
                    /* Note: we need to use `items` for something to maintain a valid reference */
                    if (items.length () > 0) {
                        for (int i = 0; i < 3; i++) {
                            if (u_items == null)
                                return false;

                            Katze.Item t_item = u_items.data<Katze.Item>;
                            browser.add_item (t_item);

                            u_items = u_items.next;
                        }
                    }
                    return u_items != null;
                });
            }

            private void helper_uri_changed (Midori.Browser browser, Midori.View view) {
                /* FixMe: skip first event while restrong the session */
                view.web_view.notify["uri"].connect ( () => {
                    this.uri_changed (view, view.web_view.uri);
                });
            }
        }
    }

    namespace Local {
        private class Session : Base.Session {
            public int64 id { get; private set; }
            private SQLHeavy.VersionedDatabase db;

            protected override void uri_changed (Midori.View view, string uri) {
                unowned Katze.Item item = view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                this.db.execute ("UPDATE `tabs` SET uri = :uri WHERE session_id = :session_id AND id = :tab_id;",
                    ":uri", typeof (string), uri,
                    ":session_id", typeof (int64), this.id,
                    ":tab_id", typeof (int64), tab_id);
            }

            protected override void tab_added (Midori.Browser browser, Midori.View view) {
                unowned Katze.Item item = view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                if (tab_id < 1) {
                    GLib.DateTime time = new DateTime.now_local ();
                    tab_id = this.db.execute_insert (
                        "INSERT INTO `tabs` (`crdate`, `tstamp`, `session_id`, `uri`) VALUES (:tstamp, :tstamp, :session_id, :uri);",
                        ":tstamp", typeof (int64), time.to_unix (),
                        ":session_id", typeof (int64), this.id,
                        ":uri", typeof (string), "about:blank");
                    item.set_meta_integer ("tabby-id", tab_id);
                }
            }

            protected override void tab_removed (Midori.Browser browser, Midori.View view) {
                unowned Katze.Item item = view.get_proxy_item ();
                int64 tab_id = item.get_meta_integer ("tabby-id");
                /* FixMe: marke es deleted */
                this.db.execute ("DELETE FROM `tabs` WHERE session_id = :session_id AND id = :tab_id;",
                    ":session_id", typeof(int64), this.id,
                    ":tab_id", typeof (int64), tab_id);
            }

            public override Katze.Array get_tabs() {
                Katze.Array tabs = new Katze.Array (typeof (Katze.Item));
                SQLHeavy.QueryResult results = this.db.execute ("SELECT id, uri FROM tabs WHERE session_id = :session_id", 
                    ":session_id", typeof(int64), this.id);

                for (; !results.finished; results.next ()) {
                    Katze.Item item = new Katze.Item ();
                    item.uri = results.fetch_string (1);
                    item.set_meta_integer ("tabby-id", results.fetch_int64 (0));
                    tabs.add_item (item);
                }

                return tabs;
            }

            internal Session (SQLHeavy.VersionedDatabase db) {
                this.db = db;

                GLib.DateTime time = new DateTime.now_local ();

                this.id = this.db.execute_insert ("INSERT INTO `sessions` (`tstamp`) VALUES (:tstamp);",
                    ":tstamp", typeof (int64), time.to_unix ());
            }

            internal Session.with_id (SQLHeavy.VersionedDatabase db, int64 id) {
                this.db = db;
                this.id = id;
            }
        }

        private class Storage : Base.Storage {
            protected SQLHeavy.VersionedDatabase db;

            public override Katze.Array get_sessions () {
                SQLHeavy.QueryResult results = this.db.execute ("SELECT id FROM sessions");

                Katze.Array sessions = new Katze.Array (typeof (Session));

                for (; !results.finished; results.next ()) {
                    sessions.add_item (new Session.with_id (this.db, results.fetch_int64 (0)));
                }

                if (sessions.is_empty ()) {
                    sessions.add_item (new Session (this.db));
                }

                return sessions;
            }

            public override Base.Session get_new_session () {
                return new Session (this.db) as Base.Session;
            }

            internal Storage (Midori.App app) {
                GLib.Object (app: app);

                this.db = new SQLHeavy.VersionedDatabase ("tabby.db", "/var/tmp/tabby-sql/");
                this.db.profiling_data = new SQLHeavy.ProfilingDatabase ("debug.db");
                this.db.enable_profiling = true;
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
                session.restore (browser);
            }
        }

        private void activated (Midori.App app) {
            /* FixMe: provide an option to replace Local.Storage with IStorage based Objects */
            this.storage = new Local.Storage (this.get_app ()) as Base.Storage;

            app.add_browser.connect (browser_added);

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
