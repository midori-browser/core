/*
   Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

namespace Apps {
    const string APP_PREFIX = PACKAGE_NAME + " -a ";
    const string PROFILE_PREFIX = PACKAGE_NAME + " -c ";

    private class Launcher : GLib.Object, GLib.Initable {
        internal GLib.File file;
        internal string name;
        internal string icon_name;
        internal string exec;
        internal string uri;

        internal static async void create (string prefix, GLib.File folder, string uri, string title, Gtk.Widget proxy) {
            /* Strip LRE leading character and / */
            string name = title.delimit ("‪/", ' ').strip();
            string filename = Midori.Download.clean_filename (name);
            string exec;
#if HAVE_WIN32
            string doubleslash_uri = uri.replace ("\\", "\\\\");
            string quoted_uri = GLib.Shell.quote (doubleslash_uri);
            exec = prefix + quoted_uri;
#else
            exec = prefix + uri;
#endif
            try {
                folder.make_directory_with_parents (null);
            }
            catch (Error error) {
                /* It's not an error if the folder already exists;
                   any fatal problems will fail further down the line */
            }

            string icon_name = Midori.Stock.WEB_BROWSER;
            try {
                var pixbuf = Midori.Paths.get_icon (uri, null);
                if (pixbuf == null)
                    throw new FileError.EXIST ("No favicon loaded");
                string icon_filename = folder.get_child ("icon.png").get_path ();
                pixbuf.save (icon_filename, "png", null, "compression", "7", null);
#if HAVE_WIN32
                string doubleslash_icon = icon_filename.replace ("\\", "\\\\");
                icon_name = doubleslash_icon;
#else
                icon_name = icon_filename;
#endif
            }
            catch (Error error) {
                GLib.warning (_("Failed to fetch application icon in %s: %s"), folder.get_path (), error.message);
            }
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
            var file = folder.get_child ("desc");
            var browser = proxy.get_toplevel () as Midori.Browser;
            try {
                var stream = yield file.replace_async (null, false, GLib.FileCreateFlags.NONE);
                yield stream.write_async (contents.data);
                // Create a launcher/ menu
#if HAVE_WIN32
                Midori.Sokoke.create_win32_desktop_lnk (prefix, title, uri);
#else
                var data_dir = File.new_for_path (Midori.Paths.get_user_data_dir ());
                yield file.copy_async (data_dir.get_child ("applications").get_child (filename + ".desktop"),
                    GLib.FileCopyFlags.NONE);
#endif

                browser.send_notification (_("Launcher created"),
                    _("You can now run <b>%s</b> from your launcher or menu").printf (name));
            }
            catch (Error error) {
                warning (_("Failed to create new launcher: %s").printf (error.message));
                browser.send_notification (_("Error creating launcher"),
                    _("Failed to create new launcher: %s").printf (error.message));
            }
        }

        internal Launcher (GLib.File file) {
            this.file = file;
        }

        bool init (GLib.Cancellable? cancellable) throws GLib.Error {
            var keyfile = new GLib.KeyFile ();
            keyfile.load_from_file (file.get_child ("desc").get_path (), GLib.KeyFileFlags.NONE);

            exec = keyfile.get_string ("Desktop Entry", "Exec");
            if (!exec.has_prefix (APP_PREFIX) && !exec.has_prefix (PROFILE_PREFIX))
                return false;

            name = keyfile.get_string ("Desktop Entry", "Name");
            icon_name = keyfile.get_string ("Desktop Entry", "Icon");
            uri = exec.replace (APP_PREFIX, "").replace (PROFILE_PREFIX, "");
            return true;
        }
    }

    private class Sidebar : Gtk.VBox, Midori.Viewable {
        Gtk.Toolbar? toolbar = null;
        Gtk.ListStore store = new Gtk.ListStore (1, typeof (Launcher));
        Gtk.TreeView treeview;
        Katze.Array array;
        GLib.File app_folder;
        GLib.File profile_folder;

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

                var profile = new Gtk.ToolButton.from_stock (Gtk.STOCK_ADD);
                profile.label = _("New _Profile");
                profile.tooltip_text = _("Creates a new, independant profile and a launcher");
                profile.use_underline = true;
                profile.is_important = true;
                profile.show ();
                profile.clicked.connect (() => {
                    string uuid = g_dbus_generate_guid ();
                    string config = Path.build_path (Path.DIR_SEPARATOR_S,
                        Midori.Paths.get_user_data_dir (), PACKAGE_NAME, "profiles", uuid);
                    Launcher.create.begin (PROFILE_PREFIX, profile_folder.get_child (uuid),
                        config, _("Midori (%s)").printf (uuid), this);
                });
                toolbar.insert (profile, -1);

                var app = new Gtk.ToolButton.from_stock (Gtk.STOCK_ADD);
                app.label = _("New _App");
                app.tooltip_text = _("Creates a new app for a specific site");
                app.use_underline = true;
                app.is_important = true;
                app.show ();
                app.clicked.connect (() => {
                    var view = (get_toplevel () as Midori.Browser).tab as Midori.View;
                    string checksum = Checksum.compute_for_string (ChecksumType.MD5, view.get_display_uri (), -1);
                    Launcher.create.begin (APP_PREFIX, app_folder.get_child (checksum),
                        view.get_display_uri (), view.get_display_title (), this);
                });
                toolbar.insert (app, -1);
            }
            return toolbar;
        }

        void row_activated (Gtk.TreePath path, Gtk.TreeViewColumn column) {
            Gtk.TreeIter iter;
            if (store.get_iter (out iter, path)) {
                Launcher launcher;
                store.get (iter, 0, out launcher);
                try {
                    GLib.Process.spawn_command_line_async (launcher.exec);
                }
                catch (Error error) {
                    var browser = get_toplevel () as Midori.Browser;
                    browser.send_notification (_("Error launching"), error.message);
                }
            }
        }

        bool button_released (Gdk.EventButton event) {
            Gtk.TreePath? path;
            Gtk.TreeViewColumn column;
            if (treeview.get_path_at_pos ((int)event.x, (int)event.y, out path, out column, null, null)) {
                if (path != null) {
                    if (column == treeview.get_column (2)) {
                        Gtk.TreeIter iter;
                        if (store.get_iter (out iter, path)) {
                            Launcher launcher;
                            store.get (iter, 0, out launcher);
                            try {
                                launcher.file.trash (null);
                                store.remove (iter);
#if HAVE_WIN32
                                string filename = Midori.Download.clean_filename (launcher.name);
                                var lnk_filename = Midori.Sokoke.get_win32_desktop_lnk_path_from_title (filename);
                                var lnk_file = File.new_for_path (lnk_filename);
                                lnk_file.trash ();
#else
                                var data_dir = File.new_for_path (Midori.Paths.get_user_data_dir ());
                                string filename = Midori.Download.clean_filename (launcher.name);
                                data_dir.get_child ("applications").get_child (filename + ".desktop").trash ();
#endif
                            }
                            catch (Error error) {
                                GLib.critical (error.message);
                            }
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        public Sidebar (Katze.Array array, GLib.File app_folder, GLib.File profile_folder) {
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

            column = new Gtk.TreeViewColumn ();
            Gtk.CellRendererPixbuf renderer_button = new Gtk.CellRendererPixbuf ();
            column.pack_start (renderer_button, false);
            column.set_cell_data_func (renderer_button, on_render_button);
            treeview.append_column (column);

            treeview.row_activated.connect (row_activated);
            treeview.button_release_event.connect (button_released);
            treeview.show ();
            pack_start (treeview, true, true, 0);

            this.array = array;
            array.add_item.connect (launcher_added);
            array.remove_item.connect (launcher_removed);
            foreach (GLib.Object item in array.get_items ())
                launcher_added (item);

            this.app_folder = app_folder;
            this.profile_folder = profile_folder;
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

            try {
                int icon_width = 48, icon_height = 48;
                Gtk.icon_size_lookup_for_settings (get_settings (),
                    Gtk.IconSize.DIALOG, out icon_width, out icon_height);
                var pixbuf = new Gdk.Pixbuf.from_file_at_size (launcher.icon_name, icon_width, icon_height);
                renderer.set ("pixbuf", pixbuf);
            }
            catch (Error error) {
                renderer.set ("icon-name", launcher.icon_name);
            }
            renderer.set ("stock-size", Gtk.IconSize.DIALOG,
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

        void on_render_button (Gtk.CellLayout column, Gtk.CellRenderer renderer,
            Gtk.TreeModel model, Gtk.TreeIter iter) {

            renderer.set ("stock-id", Gtk.STOCK_DELETE,
                          "stock-size", Gtk.IconSize.MENU);
        }
    }

    private class Manager : Midori.Extension {
        internal Katze.Array array;
        internal GLib.File app_folder;
        internal GLib.File profile_folder;
        internal GLib.List<GLib.FileMonitor> monitors;
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

        async void populate_apps (File app_folder) {
            try {
                try {
                    app_folder.make_directory_with_parents (null);
                }
                catch (IOError folder_error) {
                    if (!(folder_error is IOError.EXISTS))
                        throw folder_error;
                }

                var monitor = app_folder.monitor_directory (0, null);
                monitor.changed.connect (app_changed);
                monitors.append (monitor);

                var enumerator = yield app_folder.enumerate_children_async ("standard::name", 0);
                while (true) {
                    var files = yield enumerator.next_files_async (10);
                    if (files == null)
                        break;
                    foreach (var info in files) {
                        var file = app_folder.get_child (info.get_name ());
                        try {
                            var launcher = new Launcher (file);
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
                warning ("Failed to list apps (%s): %s",
                         app_folder.get_path (), io_error.message);
            }
        }

        void tool_menu_populated (Midori.Browser browser, Gtk.Menu menu) {
            var menuitem = new Gtk.MenuItem.with_mnemonic (_("Create _Launcher"));
            menuitem.show ();
            menu.append (menuitem);
            menuitem.activate.connect (() => {
                var view = browser.tab as Midori.View;
                string checksum = Checksum.compute_for_string (ChecksumType.MD5, view.get_display_uri (), -1);
                Launcher.create.begin (APP_PREFIX, app_folder.get_child (checksum),
                    view.get_display_uri (), view.get_display_title (), browser);
            });
        }

        void browser_added (Midori.Browser browser) {
            var viewable = new Sidebar (array, app_folder, profile_folder);
            viewable.show ();
            browser.panel.append_page (viewable);
            browser.populate_tool_menu.connect (tool_menu_populated);
            // TODO website context menu
            widgets.append (viewable);
        }

        void activated (Midori.App app) {
            array = new Katze.Array (typeof (Launcher));
            var data_dir = File.new_for_path (Midori.Paths.get_user_data_dir ()).get_child (PACKAGE_NAME);
            monitors = new GLib.List<GLib.FileMonitor> ();
            app_folder = data_dir.get_child ("apps");
            populate_apps.begin (app_folder);
            profile_folder = data_dir.get_child ("profiles");
            populate_apps.begin (profile_folder);
            widgets = new GLib.List<Gtk.Widget> ();
            foreach (var browser in app.get_browsers ())
                browser_added (browser);
            app.add_browser.connect (browser_added);
        }

        void deactivated () {
            var app = get_app ();
            foreach (var monitor in monitors)
                monitor.changed.disconnect (app_changed);
            monitors = null;

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

