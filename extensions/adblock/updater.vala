/*
 Copyright (C) 2014 Paweł Forysiuk <tuxator@o2.pl>
 Copyright (C) 2014 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Adblock {
    public class Updater : Feature {
        public string expires_meta { get; set; default = null; }
        public string last_mod_meta { get; set; default = null; }
        public int64 update_tstamp { get; set; default = 0; }
        public int64 last_mod_tstamp { get; set; default = 0; }
        public int64 last_check_tstamp { get; set; default = 0; }

        public Updater () {
        }

        public override bool header (string key, string value) {
            if (key.has_prefix ("Last mod") || key == "Updated") {
                last_mod_meta = value;
                return true;
            } else if (key == "Expires") {
                /* ! Expires: 5 days (update frequency) */
                expires_meta = value;
                return true;
            } else if (key.has_prefix ("! This list expires after")) {
                /* ! This list expires after 14 days */
                expires_meta = key.substring (26, -1);
                return true;
            }
            return false;
        }

        int get_month_from_string (string? month) {
            if (month == null)
                return 0;

            string[] months = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
            for (int i = 0; i<= months.length; i++)
            {
                if (month.has_prefix (months[i]))
                    return i+1;
            }
            return 0;
        }

        public bool needs_updating () {
            DateTime now = new DateTime.now_local ();
            DateTime expire_date = null;
            DateTime last_mod_date = null;
            string? last_mod = last_mod_meta;
            string? expires = expires_meta;

            /* We have "last modification" metadata */
            if (last_mod != null) {
                int h = 0, min = 0, d, m, y;
                /* Date in a form of: 20.08.2012 12:34 */
                if (last_mod.contains (".") || last_mod.contains("-")) {
                    string[] parts = last_mod.split (" ", 2);
                    string[] date_parts;
                    string split_char = " ";

                    /* contains time part ? */
                    if (parts[1] != "" && parts[1].contains (":")) {
                        string[] time_parts = parts[1].split (":", 2);
                        h = int.parse(time_parts[0]);
                        min = int.parse(time_parts[1]);
                    }

                    /* check if dot or dash was used as a delimiter */
                    if (parts[0].contains ("."))
                        split_char = ".";
                    else if (parts[0].contains ("-"))
                        split_char = "-";

                    date_parts = parts[0].split (split_char, 3);
                    m = int.parse(date_parts[1]);
                    if (date_parts[2].length == 4) {
                        y = int.parse(date_parts[2]);
                        d = int.parse(date_parts[0]);
                    } else {
                        y = int.parse(date_parts[0]);
                        d = int.parse(date_parts[2]);
                    }
                } else { /* Date in a form of: 20 Mar 2012 12:34 */
                    string[] parts = last_mod.split (" ", 4);
                    /* contains time part ? */
                    if (parts[3] != null && parts[3].contains (":")) {
                        string[] time_parts = parts[3].split (":", 2);
                        h = int.parse(time_parts[0]);
                        min = int.parse(time_parts[1]);
                    }

                    m = get_month_from_string (parts[1]);
                    if (parts[2].length == 4) {
                        y = int.parse(parts[2]);
                        d = int.parse(parts[0]);
                    } else {
                        y = int.parse(parts[0]);
                        d = int.parse(parts[2]);
                    }
                }

                last_mod_date = new DateTime.local (y, m, d, h, min, 0.0);
            }

            if (last_mod_date == null)
                last_mod_date = now;

            /* We have "expires" metadata */
            if (expires != null) {
                if (expires.contains ("days")) {
                    string[] parts = expires.split (" ");
                    expire_date = last_mod_date.add_days (int.parse (parts[0]));
                } else if (expires.contains ("hours")) {
                    string[] parts = expires.split (" ");
                    expire_date = last_mod_date.add_hours (int.parse (parts[0]));
                }
            } else {
                /* No expire metadata found, assume x days */
                int days_to_expire = 7;
                expire_date = last_mod_date.add_days (days_to_expire);
            }

            last_mod_tstamp = last_mod_date.to_unix ();
            last_check_tstamp = now.to_unix ();
            update_tstamp = expire_date.to_unix ();

            /* Check if we are past expire date */
            return now.compare (expire_date) == 1;
        }
    }
}
