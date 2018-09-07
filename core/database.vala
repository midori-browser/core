/*
 Copyright (C) 2013-2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public errordomain DatabaseError {
        OPEN,
        NAMING,
        FILENAME,
        EXECUTE,
        COMPILE,
        TYPE,
    }

    public delegate bool DatabaseCallback () throws DatabaseError;

    public class DatabaseStatement : Object, Initable {
        Sqlite.Statement stmt = null;
        int64 last_row_id = -1;

        public Database? database { get; set construct; }
        public string? query { get; set construct; }

        public DatabaseStatement (Database database, string query) throws DatabaseError {
            Object (database: database, query: query);
            init ();
        }

        public virtual bool init (Cancellable? cancellable = null) throws DatabaseError {
            int result = database.db.prepare_v2 (query, -1, out stmt, null);
            if (result != Sqlite.OK)
                throw new DatabaseError.COMPILE ("Failed to compile statement '%s': %s".printf (query, database.errmsg));
            return true;
        }

        /*
         * Bind values to named parameters.
         * SQL: "SELECT foo FROM bar WHERE id = :session_id"
         * Vala: statement.bind(":session_id", typeof (int64), 12345);
         * Supported types: string, int64, double
         */
        public void bind (string pname, ...) throws DatabaseError {
            int pindex = stmt.bind_parameter_index (pname);
            if (pindex <= 0)
                throw new DatabaseError.TYPE ("No such parameter '%s' in statement: %s".printf (pname, query));
            var args = va_list ();
            Type ptype = args.arg ();
            if (ptype == typeof (string)) {
                string text = args.arg ();
                stmt.bind_text (pindex, text);
                database.debug ("%s=%s", pname, text);
            } else if (ptype == typeof (int64)) {
                int64 integer = args.arg ();
                stmt.bind_int64 (pindex, integer);
                database.debug ("%s=%s", pname, integer.to_string ());
            } else if (ptype == typeof (double)) {
                double stuntman = args.arg ();
                stmt.bind_double (pindex, stuntman);
                database.debug ("%s=%s", pname, stuntman.to_string ());
            } else {
                throw new DatabaseError.TYPE ("Invalid type '%s' for '%s' in statement: %s".printf (ptype.name (), pname, query));
            }
        }

        /*
         * Execute the statement, it's an error if there are more rows.
         */
        public bool exec () throws DatabaseError {
            if (step ())
                throw new DatabaseError.EXECUTE ("More rows available - use step instead of exec");
            return true;
        }

        /*
         * Proceed to the next row, returns false when the end is nigh.
         */
        public bool step () throws DatabaseError {
            int result = stmt.step ();
            if (result != Sqlite.DONE && result != Sqlite.ROW)
                throw new DatabaseError.EXECUTE (database.errmsg);
            last_row_id = database.last_row_id;
            return result == Sqlite.ROW;
        }

        /*
         * Returns the id of the last inserted row.
         * It is an error to ask for an id without having inserted a row.
         */
        public int64 row_id () throws DatabaseError {
            if (last_row_id == -1)
                throw new DatabaseError.EXECUTE ("No row id");
            return last_row_id;
        }

        int column_index (string name) throws DatabaseError {
            for (int i = 0; i < stmt.column_count (); i++) {
                if (name == stmt.column_name (i))
                    return i;
            }
            throw new DatabaseError.TYPE ("No such column '%s' in row: %s".printf (name, query));
        }

        /*
         * Get a string value by its named parameter, for example ":uri".
         * Returns null if not found.
         */
        public string? get_string (string name) throws DatabaseError {
            int index = column_index (name);
            int type = stmt.column_type (index);
            if (stmt.column_type (index) != Sqlite.TEXT && type != Sqlite.NULL)
                throw new DatabaseError.TYPE ("Getting '%s' with wrong type in row: %s".printf (name, query));
            return stmt.column_text (index);
        }

        /*
         * Get an integer value by its named parameter, for example ":day".
         * Returns 0 if not found.
         */
        public int64 get_int64 (string name) throws DatabaseError {
            int index = column_index (name);
            int type = stmt.column_type (index);
            if (type != Sqlite.INTEGER && type != Sqlite.NULL)
                throw new DatabaseError.TYPE ("Getting '%s' with value '%s' of wrong type %d in row: %s".printf (
                                              name, stmt.column_text (index), type, query));
            return stmt.column_int64 (index);
        }

        /*
         * Get a double value by its named parameter, for example ":session_id".
         * Returns double.NAN if not found.
         */
        public double get_double (string name) throws DatabaseError {
            int index = column_index (name);
            int type = stmt.column_type (index);
            if (type != Sqlite.FLOAT && type != Sqlite.NULL)
                throw new DatabaseError.TYPE ("Getting '%s' with wrong type in row: %s".printf (name, query));
            return type == Sqlite.NULL ? double.NAN : stmt.column_double (index);
        }
    }

    public class DatabaseItem : Object {
        public Database? database { get; set; }
        public string uri { get; set; }
        public string? title { get; set; }
        public int64 date { get; set; }

        public DatabaseItem (string uri, string? title, int64 date=0) {
            Object (uri: uri,
                    title: title,
                    date: date);
            notify["title"].connect ((pspec) => {
                if (database != null) {
                    database.update.begin (this);
                }
            });
        }

        /*
         * Delete the item, or no-op if it can't be deleted.
         */
        public async bool delete () {
            if (database != null) {
                try {
                    return yield database.delete (this);
                } catch (DatabaseError error) {
                    critical ("Failed to delete %s: %s", uri, error.message);
                }
            }
            return false;
        }
    }

    public const DebugKey[] keys = {
        { "historydatabase", DebugFlags.HISTORY },
    };

    public enum DebugFlags {
        NONE,
        HISTORY,
    }

    public class Database : Object, Initable, ListModel, Loggable {
        internal Sqlite.Database? db = null;
        string? _key = null;
        Cancellable? populate_cancellable = null;

        public string? table { get; protected set; default = null; }
        public string path { get; protected set; default = ":memory:"; }
        public string? key { get { return _key; } set {
            _key = value;
            if (populate_cancellable != null) {
                populate_cancellable.cancel ();
            }
            populate_cancellable = new Cancellable ();
            populate.begin (populate_cancellable);
        } }

        /*
         * A new database successfully opened for the first time.
         * Old or additional data should be opened if this is true.
         */
        public bool first_use { get; protected set; default = false; }

        /*
         * The ID of the last inserted row.
         */
        public int64 last_row_id { get { return db.last_insert_rowid (); } }

        /*
         * The error message of the last failed operation.
         */
        public string errmsg { get { return db.errmsg (); } }

        /*
         * If a filename is passed it's assumed to be in the config folder.
         * Otherwise the database is in memory only (useful for private browsing).
         */
        public Database (string path=":memory:") throws DatabaseError {
            Object (path: path);
            init ();
        }

        string resolve_path (string path) {
            if (path.has_prefix (":memory:"))
                return ":memory:";
            else if (!Path.is_absolute (path))
                return Path.build_filename (Environment.get_user_config_dir (),
                    Environment.get_prgname (), path);
            return path;
        }

        public virtual bool init (Cancellable? cancellable = null) throws DatabaseError {
            if (table == null) {
                string basename = Path.get_basename (path);
                string[] parts = basename.split (".");
                if (parts != null && parts[0] != null && parts[1] != null) {
                    table = parts[0];
                } else if (path == ":memory:") {
                    table = "memory";
                } else {
                    throw new DatabaseError.NAMING ("Failed to deduce table from %s".printf (path));
                }
            }

            string real_path = resolve_path (path);
            bool exists = exists (real_path);

            int flags = 0;
            if (App.incognito) {
                flags |= Sqlite.OPEN_READONLY;
            } else {
                flags |= Sqlite.OPEN_CREATE;
                flags |= Sqlite.OPEN_READWRITE;
            }

            if (Sqlite.Database.open_v2 (real_path, out db, flags) != Sqlite.OK) {
                throw new DatabaseError.OPEN ("Failed to open database %s".printf (real_path));
            }
            set_data<unowned Sqlite.Database> ("db", db);

            if (logging) {
                debug ("Tracing %s", path);
                db.profile ((sql, nanoseconds) => {
                    /* sqlite as of this writing isn't more precise than ms */
                    string milliseconds = (nanoseconds / 1000000).to_string ();
                    debug ("%s (%sms)", sql, milliseconds);
                });
            }

            if (db.exec ("PRAGMA journal_mode = WAL; PRAGMA cache_size = 32100;") != Sqlite.OK)
                db.exec ("PRAGMA synchronous = NORMAL; PRAGMA temp_store = MEMORY;");
            db.exec ("PRAGMA count_changes = OFF;");

            if (real_path == ":memory:") {
                return exec ("CREATE TABLE %s (uri text, title text, date integer)".printf (table));
            }

            int64 user_version;
            Sqlite.Statement stmt;
            if (db.prepare_v2 ("PRAGMA user_version;", -1, out stmt, null) != Sqlite.OK)
                throw new DatabaseError.EXECUTE ("Failed to compile statement %s".printf (errmsg));
            if (stmt.step () != Sqlite.ROW)
                throw new DatabaseError.EXECUTE ("Failed to get row %s".printf (errmsg));
            user_version = stmt.column_int64 (0);

            if (user_version == 0) {
                exec_script ("Create");
                user_version = 1;
                exec ("PRAGMA user_version = " + user_version.to_string ());
            }

            while (true) {
                try {
                    exec_script ("Update" + user_version.to_string ());
                } catch (DatabaseError error) {
                    if (error is DatabaseError.FILENAME)
                        break;
                    throw error;
                }
                user_version = user_version + 1;
                exec ("PRAGMA user_version = " + user_version.to_string ());
            }

            first_use = !exists;
            return true;
        }

        public bool exists (string path) {
             return FileUtils.test (path, FileTest.EXISTS);
        }

        public bool exec_script (string filename) throws DatabaseError {
            string schema_path = "/data/%s/%s.sql".printf (table, filename);
            try {
                var schema = resources_lookup_data (schema_path, ResourceLookupFlags.NONE);
                transaction (()=> { return exec ((string)schema.get_data ()); });
            } catch (Error error) {
                throw new DatabaseError.FILENAME ("Failed to open schema: %s".printf (schema_path));
            }
            return true;
        }

        public bool transaction (DatabaseCallback callback) throws DatabaseError {
            exec ("BEGIN TRANSACTION;");
            callback ();
            exec ("COMMIT;");
            return true;
        }

        public bool exec (string query) throws DatabaseError {
            if (db.exec (query) != Sqlite.OK)
                throw new DatabaseError.EXECUTE (errmsg);
            return true;
        }

        /*
         * Prepare a statement with optionally binding parameters by name.
         * See also DatabaseStatement.bind().
         */
        public DatabaseStatement prepare (string query, ...) throws DatabaseError {
            var statement = new DatabaseStatement (this, query);
            var args = va_list ();
            unowned string? pname = args.arg ();
            while (pname != null) {
                Type ptype = args.arg ();
                if (ptype == typeof (string)) {
                    string pvalue = args.arg ();
                    statement.bind (pname, ptype, pvalue);
                } else if (ptype == typeof (int64)) {
                    int64 pvalue = args.arg ();
                    statement.bind (pname, ptype, pvalue);
                } else if (ptype == typeof (double)) {
                    double pvalue = args.arg ();
                    statement.bind (pname, ptype, pvalue);
                } else
                    throw new DatabaseError.TYPE ("Invalid type '%s' in statement: %s".printf (ptype.name (), query));
                pname = args.arg ();
            }
            return statement;
        }

        /*
         * Delete an item from the database, where the URI matches.
         */
        public async bool delete (DatabaseItem item) throws DatabaseError {
            string sqlcmd = """
                DELETE FROM %s WHERE uri = :uri
                """.printf (table);
            DatabaseStatement statement;
            try {
                statement = prepare (sqlcmd,
                    ":uri", typeof (string), item.uri);
                if (statement.exec ()) {
                    if (_items != null) {
                        int index = _items.index (item);
                        _items.remove (item);
                        items_changed (index, 1, 0);
                    }
                    return true;
                }
            } catch (Error error) {
                critical (_("Failed to delete from %s: %s"), table, error.message);
            }
            return false;
        }

        /*
         * Determine if the item is in the database, where the URI matches.
         */
        public bool contains (DatabaseItem item) throws DatabaseError {
            string sqlcmd = """
                SELECT uri FROM %s WHERE uri = :uri
                """.printf (table);
            DatabaseStatement statement;
            try {
                statement = prepare (sqlcmd,
                    ":uri", typeof (string), item.uri);
                return statement.step ();
            } catch (Error error) {
                critical (_("Failed to select from %s: %s"), table, error.message);
            }
            return false;
        }

        /*
         * Query items from the database, matching filter if given.
         */
        public async virtual List<DatabaseItem>? query (string? filter=null, int64 max_items=15, Cancellable? cancellable=null) throws DatabaseError {
            string sqlcmd = """
                SELECT uri, title, date, count () AS ct FROM %s
                WHERE uri LIKE :filter OR title LIKE :filter
                GROUP BY uri
                ORDER BY ct DESC LIMIT :limit
                """.printf (table);
            DatabaseStatement statement;
            try {
                string real_filter = "%" + (filter ?? "").replace (" ", "%") + "%";
                statement = prepare (sqlcmd,
                    ":filter", typeof (string), real_filter,
                    ":limit", typeof (int64), max_items);
            } catch (Error error) {
                critical (_("Failed to select from %s: %s"), table, error.message);
                return null;
            }

            var items = new List<DatabaseItem> ();
            try {
                while (statement.step ()) {
                    string uri = statement.get_string ("uri");
                    string title = statement.get_string ("title");
                    int64 date = statement.get_int64 ("date");
                    var item = new DatabaseItem (uri, title, date);
                    item.database = this;
                    items.append (item);

                    uint src = Idle.add (query.callback);
                    yield;
                    Source.remove (src);

                    if (cancellable != null && cancellable.is_cancelled ())
                        return null;
                }
            } catch (Error error) {
                critical (_("Failed to select from %s: %s"), table, error.message);
            }

            if (cancellable != null && cancellable.is_cancelled ())
                return null;
            return items;
        }

        /*
         * Update an existing item, where URI and date match.
         */
        public async bool update (DatabaseItem item) throws DatabaseError {
            string sqlcmd = """
                UPDATE %s SET title=:title WHERE uri = :uri AND date=:date
                """.printf (table);
            DatabaseStatement statement;
            try {
                statement = prepare (sqlcmd,
                    ":uri", typeof (string), item.uri,
                    ":title", typeof (string), item.title,
                    ":date", typeof (int64), item.date);
                if (statement.exec ()) {
                    if (_items != null) {
                        items_changed (_items.index (item), 0, 0);
                    }
                    return true;
                }
            } catch (Error error) {
                critical (_("Failed to update %s: %s"), table, error.message);
            }
            return false;
        }

        /*
         * Insert an item into the database.
         */
        public async bool insert (DatabaseItem item) throws DatabaseError {
            item.database = this;

            string sqlcmd = """
                INSERT INTO %s (uri, title, date) VALUES (:uri, :title, :date)
                """.printf (table);
            var statement = prepare (sqlcmd,
                ":uri", typeof (string), item.uri,
                ":title", typeof (string), item.title,
                ":date", typeof (int64), item.date);
            if (statement.exec ()) {
                if (_items != null) {
                    _items.append (item);
                    items_changed (_items.index (item), 0, 1);
                }
                return true;
            }
            return false;
        }

        public Type get_item_type () {
            return typeof (DatabaseItem);
        }

        List<DatabaseItem>? _items = null;

        public Object? get_item (uint position) {
            return _items.nth_data (position);
        }

        public uint get_n_items () {
            if (_items == null) {
                if (populate_cancellable != null) {
                    populate_cancellable.cancel ();
                }
                populate_cancellable = new Cancellable ();
                populate.begin (populate_cancellable);
                return 0;
            }
            return _items.length ();
        }

        async void populate (Cancellable? cancellable) {
            try {
                uint old_length = _items.length ();
                _items = yield query (key);
                if (cancellable.is_cancelled ()) {
                    _items = null;
                } else {
                    items_changed (0, old_length, _items.length ());
                }
            } catch (DatabaseError error) {
                debug ("Failed to populate: %s", error.message);
            }
        }
    }
}
