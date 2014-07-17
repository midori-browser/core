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
        string expires_meta;
        string last_mod_meta;
        public DateTime last_updated { get; set; }
        public DateTime expires { get; set; }
        public bool needs_update { get; set; }

        public Updater () {
        }

        public override void clear () {
            expires_meta = null;
            last_mod_meta = null;
            last_updated = null;
            expires = null;
            needs_update = false;
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

        public override bool parsed (File file) {
            process_dates (file);
            /* It's not an error to have no update headers, we go for defaults */
            return true;
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

        void process_dates (File file) {
            DateTime now = new DateTime.now_local ();
            last_updated = null;
            expires = null;

            /* We have "last modification" metadata */
            if (last_mod_meta != null && (last_mod_meta.contains (" ") && last_mod_meta[0].isdigit () == true)) {
                int h = 0, min = 0, d, m, y;
                /* Date in a form of: 20.08.2012 12:34 */
                if (last_mod_meta.contains (".") || last_mod_meta.contains("-")) {
                    string[] parts = last_mod_meta.split (" ", 2);
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
                    string[] parts = last_mod_meta.split (" ", 4);
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

                last_updated = new DateTime.local (y, m, d, h, min, 0.0);
            } else {
                /* FIXME: use file modification date if there's no update header
                try {
                    string modified = FileAttribute.TIME_MODIFIED;
                    var info = file.query_filesystem_info (modified);
                    last_updated = new DateTime.from_timeval_local (info.get_modification_time ());
                } catch (Error error) {
                    last_updated = now;
                }
                 */
                last_updated = now;
            }

            /* We have "expires" metadata */
            if (expires_meta != null) {
                if (expires_meta.contains ("days")) {
                    string[] parts = expires_meta.split (" ");
                    expires = last_updated.add_days (int.parse (parts[0]));
                } else if (expires_meta.contains ("hours")) {
                    string[] parts = expires_meta.split (" ");
                    expires = last_updated.add_hours (int.parse (parts[0]));
                }
            } else {
                /* No expire metadata found, assume x days */
                int days_to_expire = 7;
                expires = last_updated.add_days (days_to_expire);
            }

            /* Check if we are past expire date */
            needs_update = now.compare (expires) == 1;
        }
    }
}
