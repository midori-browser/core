/*
 Copyright (C) 2013 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    /*
     * Since: 0.5.6
     */
    public errordomain DatabaseError {
        OPEN,
        NAMING,
        FILENAME,
        EXECUTE,
        COMPILE,
        TYPE,
    }

    /*
     * Since: 0.5.8
     */
    public delegate bool DatabaseCallback () throws DatabaseError;

    /*
     * Since: 0.5.7
     */
    public class DatabaseStatement : GLib.Object, GLib.Initable {
        public Sqlite.Statement? stmt { get { return _stmt; } }
        protected Sqlite.Statement _stmt = null;
        public Database? database { get; set construct; }
        public string? query { get; set construct; }
        private int64 last_row_id = -1;

        public DatabaseStatement (Database database, string query) throws DatabaseError {
            Object (database: database, query: query);
            init ();
        }

        public virtual bool init (GLib.Cancellable? cancellable = null) throws DatabaseError {
            int result = database.db.prepare_v2 (query, -1, out _stmt, null);
            if (result != Sqlite.OK)
                throw new DatabaseError.COMPILE ("Failed to compile statement: %s".printf (database.db.errmsg ()));
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
                if (database.trace)
                    stdout.printf ("%s=%s ", pname, text);
            } else if (ptype == typeof (int64)) {
                int64 integer = args.arg ();
                stmt.bind_int64 (pindex, integer);
                if (database.trace)
                    stdout.printf ("%s=%s ", pname, integer.to_string ());
            } else if (ptype == typeof (double)) {
                double stuntman = args.arg ();
                stmt.bind_double (pindex, stuntman);
                if (database.trace)
                    stdout.printf ("%s=%s ", pname, stuntman.to_string ());
            } else
                throw new DatabaseError.TYPE ("Invalid type '%s' for '%s' in statement: %s".printf (ptype.name (), pname, query));
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
                throw new DatabaseError.EXECUTE (database.db.errmsg ());
            last_row_id = database.db.last_insert_rowid ();
            return result == Sqlite.ROW;
        }

        /*
         * Returns the id of the last inserted row.
         * It is an error to ask for an id without having inserted a row.
         * Since: 0.5.8
         */
        public int64 row_id () throws DatabaseError {
            if (last_row_id == -1)
                throw new DatabaseError.EXECUTE ("No row id");
            return last_row_id;
        }

        private int column_index (string name) throws DatabaseError {
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

    /*
     * Since: 0.5.6
     */
    public class Database : GLib.Object, GLib.Initable {
        internal bool trace = false;
        public Sqlite.Database? db { get { return _db; } }
        protected Sqlite.Database? _db = null;
        public string path { get; protected set; default = ":memory:"; }

        /*
         * A new database successfully opened for the first time.
         * Old or additional data should be opened if this is true.
         */
        public bool first_use { get; protected set; default = false; }

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
                return Midori.Paths.get_config_filename_for_writing (path);
            return path;
        }

        public virtual bool init (GLib.Cancellable? cancellable = null) throws DatabaseError {
            string real_path = resolve_path (path);
            bool exists = exists(real_path);

            if (Sqlite.Database.open_v2 (real_path, out _db) != Sqlite.OK)
                throw new DatabaseError.OPEN ("Failed to open database %s".printf (real_path));

            string token = Environment.get_variable ("MIDORI_DEBUG") ?? "";
            string basename = Path.get_basename (path);
            string[] parts = basename.split (".");
            trace = ("db:" + parts[0]) in token;
            if (trace) {
                stdout.printf ("§§ Tracing %s\n", path);
                db.profile ((sql, nanoseconds) => {
                    /* sqlite as of this writing isn't more precise than ms */
                    string milliseconds = (nanoseconds / 1000000).to_string ();
                    stdout.printf ("§§ %s: %s (%sms)\n", path, sql, milliseconds);
                });
            }

            if (db.exec ("PRAGMA journal_mode = WAL; PRAGMA cache_size = 32100;") != Sqlite.OK)
                db.exec ("PRAGMA synchronous = NORMAL; PRAGMA temp_store = MEMORY;");
            db.exec ("PRAGMA count_changes = OFF;");

            if (real_path == ":memory:")
                return true;

            int64 user_version;
            Sqlite.Statement stmt;
            if (db.prepare_v2 ("PRAGMA user_version;", -1, out stmt, null) != Sqlite.OK)
                throw new DatabaseError.EXECUTE ("Failed to compile statement %s".printf (db.errmsg ()));
            if (stmt.step () != Sqlite.ROW)
                throw new DatabaseError.EXECUTE ("Failed to get row %s".printf (db.errmsg ()));
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
	      bool exists;
#if !HAVE_WIN32	
            exists = Posix.access (path, Posix.F_OK) == 0;
#else
	     var folder = File.new_for_path (path);
	     exists = folder.query_exists();
#endif
	     return exists;
	}


        /*
         * Since: 0.5.8
         */

        public bool attach (string path, string alias) throws DatabaseError {
            string real_path = resolve_path (path);
            if (!exists (real_path))
                throw new DatabaseError.OPEN ("Failed to attach database %s".printf (path));
            return exec ("ATTACH DATABASE '%s' AS '%s';".printf (real_path, alias));
        }

        public bool exec_script (string filename) throws DatabaseError {
            string basename = Path.get_basename (path);
            string[] parts = basename.split (".");
            if (!(parts != null && parts[0] != null && parts[1] != null))
                throw new DatabaseError.NAMING ("Failed to deduce schema filename from %s".printf (path));
            string schema_filename = Midori.Paths.get_res_filename (parts[0] + "/" + filename + ".sql");
            string schema;
            try {
                FileUtils.get_contents (schema_filename, out schema, null);
            } catch (Error error) {
                throw new DatabaseError.FILENAME ("Failed to open schema: %s".printf (schema_filename));
            }
            transaction (()=> { return exec (schema); });
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
                throw new DatabaseError.EXECUTE (db.errmsg ());
            return true;
        }

        /*
         * Prepare a statement with optionally binding parameters by name.
         * See also DatabaseStatement.bind().
         * Since: 0.5.7
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
            if (trace)
                stdout.printf ("\n");
            return statement;
        }
    }
}
