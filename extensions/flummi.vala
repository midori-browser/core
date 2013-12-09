/*
   Copyright (C) 2013 André Stösel <andre@stoesel.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

namespace Flummi {
    private class Manager : Midori.Extension {
        private bool bounce () {
            try {
                Midori.App app = this.get_app ();
                Midori.Browser? browser = app.browser;

                if (browser == null || browser.tab == null) {
                    return true;
                }


                Midori.Database database = new Midori.Database ("flummi.db");
                unowned Sqlite.Database db = database.db;

                string sqlcmd = "SELECT id, once, command FROM tasks ORDER BY id;";

                Sqlite.Statement stmt;
                if (db.prepare_v2 (sqlcmd, -1, out stmt, null) != Sqlite.OK) {
                    GLib.critical ("Failed to select from database: %s", db.errmsg ());
                    return false;
                }

                int result = stmt.step ();
                if (!(result == Sqlite.DONE || result == Sqlite.ROW)) {
                    GLib.critical ("Failed to select from database: %s", db.errmsg ());
                    return false;
                }

                Sqlite.Statement del_stmt;
                sqlcmd = "DELETE FROM `tasks` WHERE id = :task_id;";
                if (db.prepare_v2 (sqlcmd, -1, out del_stmt, null) != Sqlite.OK) {
                    GLib.critical ("Failed to update database: %s", db.errmsg ());
                    return false;
                }

                while (result == Sqlite.ROW) {
                    int64 id = stmt.column_int64 (0);
                    int64 once = stmt.column_int64 (1);
                    string command = stmt.column_text (2);

                    string[] commands = { command };

                    if (!app.send_command (commands)) {
                        GLib.critical ("Command failed: %s", command);
                        return false;
                    }

                    if (once > 0) {
                        del_stmt.bind_int64 (del_stmt.bind_parameter_index (":task_id"), id);
                        if (del_stmt.step () != Sqlite.DONE) {
                            GLib.critical ("Failed to delete record %lf.\nError: %s", id, db.errmsg ());
                            return false;
                        }
                    }

                    result = stmt.step ();
                }
            } catch (Midori.DatabaseError schema_error) {
                GLib.error (schema_error.message);
            }

            return false;
        }

        private void activated (Midori.App app) {
            GLib.Idle.add (this.bounce);
        }

        internal Manager () {
            GLib.Object (name: _("Flummi"),
                         description: _("This extension provides a task queue for update jobs or recurring events."),
                         version: "0.1",
                         authors: "André Stösel <andre@stoesel.de>");

            this.activate.connect (this.activated);
        }
    }
}

public Midori.Extension extension_init () {
    return new Flummi.Manager ();
}
