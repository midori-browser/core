/*
 Copyright (C) 2013-2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class HistoryDatabase : Database {
        static HistoryDatabase? _default = null;
        static HistoryDatabase? _default_incognito = null;

        public static HistoryDatabase get_default (bool incognito=false) throws DatabaseError {
            if (incognito) {
                _default_incognito = _default_incognito ?? new HistoryDatabase (true);
                return _default_incognito;
            }
            _default = _default ?? new HistoryDatabase (false);
            return _default;
        }

        HistoryDatabase (bool incognito) throws DatabaseError {
            Object (path: "history.db", readonly: incognito);
            init ();

            try {
                exec ("SELECT day FROM history LIMIT 1");
            } catch (Error error) {
                exec_script ("Day");
            }
        }

        public async bool clear (int64 maximum_age=0) throws DatabaseError {
            unowned string sqlcmd = """
                DELETE FROM history WHERE date >= :maximum_age;
                DELETE FROM search WHERE date >= :maximum_age;
                """;
            var statement = prepare (sqlcmd,
                ":maximum_age", typeof (int64), maximum_age);
            return statement.exec ();
        }
   }
}
