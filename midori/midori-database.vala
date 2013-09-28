/*
 Copyright (C) 2013 Christian Dywan <christian@twotoats.de>

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
    }

    public class Database : GLib.Object, GLib.Initable {
        public Sqlite.Database? db { get { return _db; } }
        protected Sqlite.Database? _db = null;
        public string? path { get; protected set; default = ":memory:"; }

        /*
         * A new database successfully opened for the first time.
         * Old or additional data should be opened if this is true.
         */
        public bool first_use { get; protected set; default = false; }

        /*
         * If a filename is passed it's assumed to be in the config folder.
         * Otherwise the database is in memory only (useful for private browsing).
         */
        public Database (string? path) throws DatabaseError {
            Object (path: path);
            init ();
        }

        public virtual bool init (GLib.Cancellable? cancellable = null) throws DatabaseError {
            if (path == null)
                path = ":memory:";
            else if (!Path.is_absolute (path))
                path = Midori.Paths.get_config_filename_for_writing (path);
            bool exists = Posix.access (path, Posix.F_OK) == 0;

            if (Sqlite.Database.open_v2 (path, out _db) != Sqlite.OK)
                throw new DatabaseError.OPEN ("Failed to open database %s".printf (path));

            if (db.exec ("PRAGMA journal_mode = WAL; PRAGMA cache_size = 32100;") != Sqlite.OK)
                db.exec ("PRAGMA synchronous = NORMAL; PRAGMA temp_store = MEMORY;");
            db.exec ("PRAGMA count_changes = OFF;");

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
            schema = "BEGIN TRANSACTION; %s; COMMIT;".printf (schema);
            if (db.exec (schema) != Sqlite.OK)
                throw new DatabaseError.EXECUTE ("Failed to execute schema: %s".printf (schema));
            return true;
        }

        public bool exec (string query) throws DatabaseError {
            if (db.exec (query) != Sqlite.OK)
                throw new DatabaseError.EXECUTE (db.errmsg ());
            return true;
        }
    }
}
