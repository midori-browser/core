/*
 Copyright (C) 2013 Andre Auzi <aauzi@free.fr>
 Copyright (C) 2013 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class BookmarksDatabase : Midori.Database {
        public BookmarksDatabase () throws DatabaseError {
            Object (path: "bookmarks.db");
            preinit ();
            init ();
            exec ("PRAGMA foreign_keys = ON;");
        }

        protected void preinit () throws DatabaseError {
            string dbfile = Paths.get_config_filename_for_writing (path);
            string olddbfile = dbfile + ".old";
            string dbfile_v2 = Paths.get_config_filename_for_reading ("bookmarks_v2.db");

            if (Posix.access (dbfile_v2, Posix.F_OK) == 0) {
                if (Posix.access (dbfile, Posix.F_OK) == 0) {
                    if (Posix.access (olddbfile, Posix.F_OK) == 0)
                        Posix.unlink (olddbfile);
                    GLib.FileUtils.rename (dbfile, olddbfile);
                }

                GLib.FileUtils.rename (dbfile_v2, dbfile);

                if (Sqlite.Database.open_v2 (dbfile, out _db) != Sqlite.OK)
                    throw new DatabaseError.OPEN ("Failed to open database %s".printf (path));

                Sqlite.Statement stmt;
                if (db.prepare_v2 ("PRAGMA user_version;", -1, out stmt, null) != Sqlite.OK)
                    throw new DatabaseError.EXECUTE ("Failed to compile statement %s".printf (db.errmsg ()));
                if (stmt.step () != Sqlite.ROW)
                    throw new DatabaseError.EXECUTE ("Failed to get row %s".printf (db.errmsg ()));
                int64 user_version = stmt.column_int64 (0);
                
                if (user_version == 0) {
                    exec ("PRAGMA user_version = 1;");
                }

                _db = null;
            } else if (Posix.access (dbfile, Posix.F_OK) == 0) {

                if (Sqlite.Database.open_v2 (dbfile, out _db) != Sqlite.OK)
                    throw new DatabaseError.OPEN ("Failed to open database %s".printf (path));

                Sqlite.Statement stmt;
                if (db.prepare_v2 ("PRAGMA user_version;", -1, out stmt, null) != Sqlite.OK)
                    throw new DatabaseError.EXECUTE ("Failed to compile statement %s".printf (db.errmsg ()));
                if (stmt.step () != Sqlite.ROW)
                    throw new DatabaseError.EXECUTE ("Failed to get row %s".printf (db.errmsg ()));
                int64 user_version = stmt.column_int64 (0);

                _db = null;
                
                if (user_version == 0) {
                    if (Posix.access (olddbfile, Posix.F_OK) == 0)
                        Posix.unlink (olddbfile);

                    GLib.FileUtils.rename (dbfile, olddbfile);

                    if (Sqlite.Database.open_v2 (dbfile, out _db) != Sqlite.OK)
                        throw new DatabaseError.OPEN ("Failed to open database %s".printf (path));

                    exec_script ("Create");

                    attach (olddbfile, "old_db");
                    
                    bool failure = false;
                    try {
                        exec_script ("Import_old_db_bookmarks");
                    } catch (DatabaseError error) {
                        if (error is DatabaseError.EXECUTE)
                            failure = true;
                        else
                            throw error;
                    }
                    
                    /* try to get back to previous state */
                    if (failure)
                        exec ("ROLLBACK TRANSACTION;");
                    
                    exec ("DETACH DATABASE old_db;");
                    exec ("PRAGMA user_version = 1;");

                    _db = null;
                }
            }
        }
    }
}
