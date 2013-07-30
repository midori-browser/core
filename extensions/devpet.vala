/*
   Copyright (C) 2013 André Stösel <andre@stoesel.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

Gtk.IconTheme theme;

namespace DevPet {
    enum TreeCells {
        MESSAGE,
        STOCK,
        COUNT
    }

    private class LogWindow : Gtk.Window {
        private Manager manager;

        private void clear_list () {
            this.manager.clear_list ();
            this.destroy ();
        }

        private void create_content () {
            this.title = "Midori - DevPet";
            this.set_default_size (500, 250);
            this.destroy.connect (this.manager.log_window_closed);

            Gtk.VBox vbox = new Gtk.VBox (false, 1);
            this.add (vbox);

            Gtk.ScrolledWindow scroll_windows = new Gtk.ScrolledWindow (null, null);
            scroll_windows.set_policy (Gtk.PolicyType.NEVER , Gtk.PolicyType.AUTOMATIC);
            scroll_windows.set_shadow_type (Gtk.ShadowType.ETCHED_IN);


            Gtk.Button clear = new Gtk.Button.from_stock ("gtk-clear");
            clear.clicked.connect (this.clear_list);

            vbox.pack_start (scroll_windows, true, true, 0);
            vbox.pack_start (clear, false, false, 0);


            Gtk.TreeView treeview = new Gtk.TreeView.with_model (this.manager.list_store);
            scroll_windows.add (treeview);

            treeview.insert_column_with_attributes (
                -1, "Type",
                new Gtk.CellRendererPixbuf (), "pixbuf", TreeCells.STOCK);
            treeview.insert_column_with_attributes (
                -1, "Message",
                new Gtk.CellRendererText (), "text", TreeCells.MESSAGE);

            this.show_all ();
        }

        internal LogWindow (Manager manager) {
            GLib.Object (type: Gtk.WindowType.TOPLEVEL,
                         window_position: Gtk.WindowPosition.CENTER);

            this.manager = manager;
            this.create_content ();
        }
    }

    private class Manager : Midori.Extension {
        public Gtk.ListStore list_store;
        private Gtk.StatusIcon trayicon;
        private LogWindow? log_window;
        private GLib.LogFunc default_log_func;
        private GLib.LogLevelFlags icon_flag = GLib.LogLevelFlags.LEVEL_DEBUG;

        public void clear_list() {
            this.icon_flag = GLib.LogLevelFlags.LEVEL_DEBUG;
            this.trayicon.set_visible (false);
            this.list_store.clear ();
        }

        public void log_window_closed () {
            this.log_window = null;
        }

        private unowned string get_stock_from_log_level (GLib.LogLevelFlags flags) {
            if ((flags & LogLevelFlags.LEVEL_CRITICAL) == flags || (flags & LogLevelFlags.LEVEL_ERROR) == flags) {
                return Gtk.Stock.DIALOG_ERROR;
            } else if ((flags & LogLevelFlags.LEVEL_WARNING) == flags) {
                return Gtk.Stock.DIALOG_WARNING;
            }
            return Gtk.Stock.DIALOG_INFO;
        }

        private void log_handler(string? domain, GLib.LogLevelFlags flags, string message) {
            Gtk.TreeIter iter;
            unowned string stock = this.get_stock_from_log_level (flags);

            if (flags < this.icon_flag) {
                this.icon_flag = flags;
                this.trayicon.set_from_stock (stock);
            }

            this.list_store.append (out iter);
            this.list_store.set (iter, TreeCells.MESSAGE, message, TreeCells.STOCK, theme.load_icon (stock, 16, 0));

            this.trayicon.set_visible (true);
        }

        private void show_error_log () {
            if (this.log_window == null) {
                this.log_window = new LogWindow (this);
                this.log_window.show ();
            } else {
                if (this.log_window.is_active) {
                    this.log_window.hide ();
                } else {
                    this.log_window.present ();
                }
            }
        }

        private void activated (Midori.App app) {
            this.trayicon.set_visible (false);
            this.default_log_func = GLib.Log.default_handler;
            GLib.Log.set_default_handler (this.log_handler);
        }

        private void deactivated () {
            this.trayicon.set_visible (false);
            GLib.Log.set_default_handler (this.default_log_func);
        }

        internal Manager () {
            GLib.Object (name: _("DevPet"),
                         description: _("This extension shows glib error messages in systray."),
                         version: "0.1",
                         authors: "André Stösel <andre@stoesel.de>");

            this.trayicon = new Gtk.StatusIcon ();
            this.trayicon.set_tooltip_text ("Midori - DevPet");
            this.trayicon.activate.connect(this.show_error_log);

            this.list_store = new Gtk.ListStore (TreeCells.COUNT, typeof(string), typeof (Gdk.Pixbuf));

            this.activate.connect (this.activated);
            this.deactivate.connect (this.deactivated);
        }
    }
}

public Midori.Extension extension_init () {
    theme = Gtk.IconTheme.get_default ();
    return new DevPet.Manager ();
}

