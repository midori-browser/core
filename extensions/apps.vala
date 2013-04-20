/*
   Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

namespace Apps {
    const string EXEC_PREFIX = PACKAGE_NAME + " -a ";

    private class Launcher : GLib.Object, GLib.Initable {
        internal GLib.File file;
        internal string name;
        internal string icon_name;
        internal string exec;
        internal string uri;

        internal static async void create (GLib.File folder, string uri, string title) {
            /* Strip LRE leading character and / */
            string filename = title.delimit ("‪/", ' ') + ".desktop";
            string exec = EXEC_PREFIX + uri;
            string name = title;
            // TODO: Midori.Paths.get_icon save to png
            string icon_name = Midori.Stock.WEB_BROWSER;
            string contents = """
                [Desktop Entry]
                Version=1.0
                Type=Application
                Name=%s
                Exec=%s
                TryExec=%s
                Icon=%s
                Categories=Network;
                """.printf (name, exec, PACKAGE_NAME, icon_name);
            var file = folder.get_child (filename);
            try {
                var stream = yield file.replace_async (null, false, GLib.FileCreateFlags.NONE);
                yield stream.write_async (contents.data);
            }
            catch (Error error) {
                // TODO GUI infobar
                warning ("Failed to create new launcher: %s", error.message);
            }
        }

        internal Launcher (GLib.File file) {
            this.file = file;
        }

        bool init (GLib.Cancellable? cancellable) throws GLib.Error {
            if (!file.get_basename ().has_suffix (".desktop"))
                return false;

            var keyfile = new GLib.KeyFile ();
            keyfile.load_from_file (file.get_path (), GLib.KeyFileFlags.NONE);
            exec = keyfile.get_string ("Desktop Entry", "Exec");
            if (!exec.has_prefix (EXEC_PREFIX))
                return false;

            name = keyfile.get_string ("Desktop Entry", "Name");
            icon_name = keyfile.get_string ("Desktop Entry", "Icon");
            uri = exec.replace (EXEC_PREFIX, "");
            return true;
        }
    }

    private class Sidebar : Gtk.VBox, Midori.Viewable {
        Gtk.Toolbar? toolbar = null;
        Gtk.ListStore store = new Gtk.ListStore (1, typeof (Launcher));
        Gtk.TreeView treeview;
        Katze.Array array;

        public unowned string get_stock_id () {
            return Midori.Stock.WEB_BROWSER;
        }

        public unowned string get_label () {
            return _("Applications");
        }

        public Gtk.Widget get_toolbar () {
            if (toolbar == null) {
                toolbar = new Gtk.Toolbar ();
                toolbar.set_icon_size (Gtk.IconSize.BUTTON);
            }
            return toolbar;
        }

        public Sidebar (Katze.Array array) {
            Gtk.TreeViewColumn column;

            treeview = new Gtk.TreeView.with_model (store);
            treeview.headers_visible = false;

            store.set_sort_column_id (0, Gtk.SortType.ASCENDING);
            store.set_sort_func (0, tree_sort_func);

            column = new Gtk.TreeViewColumn ();
            Gtk.CellRendererPixbuf renderer_icon = new Gtk.CellRendererPixbuf ();
            column.pack_start (renderer_icon, false);
            column.set_cell_data_func (renderer_icon, on_render_icon);
            treeview.append_column (column);

            column = new Gtk.TreeViewColumn ();
            column.set_sizing (Gtk.TreeViewColumnSizing.AUTOSIZE);
            Gtk.CellRendererText renderer_text = new Gtk.CellRendererText ();
            column.pack_start (renderer_text, true);
            column.set_expand (true);
            column.set_cell_data_func (renderer_text, on_render_text);
            treeview.append_column (column);

            treeview.show ();
            pack_start (treeview, true, true, 0);

            this.array = array;
            array.add_item.connect (launcher_added);
            array.remove_item.connect (launcher_removed);
            foreach (GLib.Object item in array.get_items ())
                launcher_added (item);
        }

        private int tree_sort_func (Gtk.TreeModel model, Gtk.TreeIter a, Gtk.TreeIter b) {
            Launcher launcher1, launcher2;
            model.get (a, 0, out launcher1);
            model.get (b, 0, out launcher2);
            return strcmp (launcher1.name, launcher2.name);
        }
        void launcher_added (GLib.Object item) {
            var launcher = item as Launcher;
            Gtk.TreeIter iter;
            store.append (out iter);
            store.set (iter, 0, launcher);
        }

        void launcher_removed (GLib.Object item) {
            // TODO remove iter
        }

        private void on_render_icon (Gtk.CellLayout column, Gtk.CellRenderer renderer,
            Gtk.TreeModel model, Gtk.TreeIter iter) {

            Launcher launcher;
            model.get (iter, 0, out launcher);
            if (launcher.icon_name != null)
                renderer.set ("icon-name", launcher.icon_name);
            else
                renderer.set ("stock-id", Gtk.STOCK_FILE);
            renderer.set ("stock-size", Gtk.IconSize.BUTTON,
                          "xpad", 4);
        }

        private void on_render_text (Gtk.CellLayout column, Gtk.CellRenderer renderer,
            Gtk.TreeModel model, Gtk.TreeIter iter) {

            Launcher launcher;
            model.get (iter, 0, out launcher);
            renderer.set ("markup",
                Markup.printf_escaped ("<b>%s</b>\n%s",
                    launcher.name, launcher.uri),
                          "ellipsize", Pango.EllipsizeMode.END);
        }
    }

    private class Manager : Midori.Extension {
        internal Katze.Array array;
        internal GLib.File app_folder;
        internal GLib.FileMonitor? monitor;
        internal GLib.List<Gtk.Widget> widgets;

        void app_changed (GLib.File file, GLib.File? other, GLib.FileMonitorEvent event) {
            try {
                switch (event) {
                case GLib.FileMonitorEvent.DELETED:
                    // TODO array.remove_item ();
                    break;
                case GLib.FileMonitorEvent.CREATED:
                    var launcher = new Launcher (file);
                    if (launcher.init ())
                        array.add_item (launcher);
                    break;
                case GLib.FileMonitorEvent.CHANGED:
                    // TODO
                    break;
                }
            }
            catch (Error error) {
                warning ("Application changed: %s", error.message);
            }
        }

        async void populate_apps () {
            var data_dir = File.new_for_path (Midori.Paths.get_user_data_dir ());
            app_folder = data_dir.get_child ("applications");
            try {
                try {
                    app_folder.make_directory_with_parents (null);
                }
                catch (IOError folder_error) {
                    if (!(folder_error is IOError.EXISTS))
                        throw folder_error;
                }

                monitor = app_folder.monitor_directory (0, null);
                monitor.changed.connect (app_changed);

                var enumerator = yield app_folder.enumerate_children_async ("standard::name", 0);
                while (true) {
                    var files = yield enumerator.next_files_async (10);
                    if (files == null)
                        break;
                    foreach (var info in files) {
                        var desktop_file = app_folder.get_child (info.get_name ());
                        try {
                            var launcher = new Launcher (desktop_file);
                            if (launcher.init ())
                                array.add_item (launcher);
                        }
                        catch (Error error) {
                            warning ("Failed to parse launcher: %s", error.message);
                        }
                    }
                }
            }
            catch (Error io_error) {
                monitor = null;
                warning ("Failed to list .desktop files (%s): %s",
                         app_folder.get_path (), io_error.message);
            }
        }

        void tool_menu_populated (Midori.Browser browser, Gtk.Menu menu) {
            var menuitem = new Gtk.MenuItem.with_mnemonic (_("Create _Launcher"));
            menuitem.show ();
            menu.append (menuitem);
            menuitem.activate.connect (() => {
                var view = browser.tab as Midori.View;
                Launcher.create.begin (app_folder,
                    view.get_display_uri (), view.get_display_title ());
            });
        }

        void browser_added (Midori.Browser browser) {
            var viewable = new Sidebar (array);
            viewable.show ();
            browser.panel.append_page (viewable);
            browser.populate_tool_menu.connect (tool_menu_populated);
            // TODO website context menu
            widgets.append (viewable);
        }

        void activated (Midori.App app) {
            array = new Katze.Array (typeof (Launcher));
            populate_apps.begin ();
            widgets = new GLib.List<Gtk.Widget> ();
            foreach (var browser in app.get_browsers ())
                browser_added (browser);
            app.add_browser.connect (browser_added);
        }

        void deactivated () {
            var app = get_app ();
            if (monitor != null)
                monitor.changed.disconnect (app_changed);
            app.add_browser.disconnect (browser_added);
            foreach (var widget in widgets)
                widget.destroy ();
        }

        internal Manager () {
            GLib.Object (name: _("Web App Manager"),
                         description: _("Manage websites installed as applications"),
                         version: "0.1" + Midori.VERSION_SUFFIX,
                         authors: "Christian Dywan <christian@twotoasts.de>");

            this.activate.connect (activated);
            this.deactivate.connect (deactivated);
        }
    }
}

public Midori.Extension extension_init () {
    return new Apps.Manager ();
}

