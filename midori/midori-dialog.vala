/*
 Copyright (C) 2011-2013 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    namespace Timeout {
        public uint add_seconds (uint interval, owned SourceFunc function) {
            if (Test.test_idle_timeouts)
                return GLib.Idle.add (function);
            return GLib.Timeout.add_seconds (interval, function);
        }
        public uint add (uint interval, owned SourceFunc function) {
            if (Test.test_idle_timeouts)
                return GLib.Idle.add (function);
            return GLib.Timeout.add (interval, function);
        }
    }

    namespace Test {
        public void init ([CCode (array_length_pos = 0.9)] ref unowned string[] args) {
            GLib.Test.init (ref args);

            /* Always log to stderr */
            Log.set_handler (null,
                LogLevelFlags.LEVEL_MASK | LogLevelFlags.FLAG_FATAL | LogLevelFlags.FLAG_RECURSION,
                (domain, log_levels, message) => {
                stderr.printf ("** %s\n", message);
            });
        }

        internal static uint test_max_timeout = 0;
        internal static string? test_first_try = null;
        public void grab_max_timeout () {
            int seconds = (Environment.get_variable ("MIDORI_TIMEOUT") ?? "42").to_int ();
            test_first_try = "once";
            test_max_timeout = GLib.Timeout.add_seconds (seconds > 0 ? seconds / 2 : 0, ()=>{
                stderr.printf ("Timed out %s%s\n", test_first_try,
                    MainContext.default ().pending () ? " (loop)" : "");
                if (test_first_try == "twice")
                    Process.exit (0);
                test_first_try = "twice";
                MainContext.default ().wakeup ();
                return true;
                });
        }
        public void release_max_timeout () {
            assert (test_max_timeout > 0);
            GLib.Source.remove (test_max_timeout);
            test_max_timeout = 0;
        }

        internal static bool test_idle_timeouts = false;
        public void idle_timeouts () {
            test_idle_timeouts = true;
        }

        public abstract class Job : GLib.Object {
            bool done;
            public abstract async void run (Cancellable cancellable) throws GLib.Error;
            async void run_wrapped (Cancellable cancellable) {
                try {
                    yield run (cancellable);
                } catch (Error error) {
                    GLib.error (error.message);
                }
                done = true;
            }
            public void run_sync () {
                var loop = MainContext.default ();
                var cancellable = new Cancellable ();
                done = false;
                run_wrapped.begin (cancellable);
                do { loop.iteration (true); } while (!done);
            }
        }

        public void log_set_fatal_handler_for_icons () {
            GLib.Test.log_set_fatal_handler ((domain, log_levels, message)=> {
                return !message.contains ("Error loading theme icon")
                    && !message.contains ("Could not find the icon")
                    && !message.contains ("Junk at end of value")
                    && !message.contains ("gtk_notebook_get_tab_label: assertion `GTK_IS_WIDGET (child)' failed")
                    && !message.contains ("get_column_number: assertion `i < gtk_tree_view_get_n_columns (treeview)' failed");
            });

        }

        internal static Gtk.ResponseType test_response = Gtk.ResponseType.NONE;
        public void set_dialog_response (Gtk.ResponseType response) {
            test_response = response;
        }

        internal static string? test_filename = null;
        public void set_file_chooser_filename (string filename) {
            test_filename = filename;
        }
    }

    public static void show_message_dialog (Gtk.MessageType type, string short, string detailed, bool modal) {
        var dialog = new Gtk.MessageDialog (null, 0, type, Gtk.ButtonsType.OK, "%s", short);
        dialog.format_secondary_text ("%s", detailed);
        if (modal) {
            dialog.run ();
            dialog.destroy ();
        } else {
            dialog.response.connect ((response) => {
                dialog.destroy ();
            });
            dialog.show ();
        }
    }

    public class FileChooserDialog : Gtk.FileChooserDialog {
        public FileChooserDialog (string title, Gtk.Window? window, Gtk.FileChooserAction action) {
            /* Creates a new file chooser dialog to Open or Save and Cancel.
               The positive response is %Gtk.ResponseType.OK. */
            unowned string stock_id = Gtk.Stock.OPEN;
            if (action == Gtk.FileChooserAction.SAVE)
                stock_id = Gtk.Stock.SAVE;
            this.title = title;
            transient_for = window;
            this.action = action;
            add_buttons (Gtk.Stock.CANCEL, Gtk.ResponseType.CANCEL,
                         stock_id, Gtk.ResponseType.OK);
            icon_name = stock_id;
        }
    }

    namespace Dialog {
        public static new int run (Gtk.Dialog dialog) {
            if (Test.test_response != Gtk.ResponseType.NONE) {
                if (Test.test_filename != null && dialog is Gtk.FileChooser)
                    (dialog as Gtk.FileChooser).set_filename (Test.test_filename);
                return Test.test_response;
            }
            return dialog.run ();
        }
    }
}

