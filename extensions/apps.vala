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

        internal static string get_favicon_name_for_uri (string prefix, GLib.File folder, string uri, bool testing)
        {
            string icon_name = Midori.Stock.WEB_BROWSER;

            if (testing == true)
                return icon_name;

            if (prefix != PROFILE_PREFIX)
            {
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
            }
            return icon_name;
        }

        internal static string prepare_desktop_file (string prefix, string name, string uri, string title, string icon_name)
        {
            string exec;
#if HAVE_WIN32
            string doubleslash_uri = uri.replace ("\\", "\\\\");
            string quoted_uri = GLib.Shell.quote (doubleslash_uri);
            exec = prefix + quoted_uri;
#else
            exec = prefix + uri;
#endif
            var keyfile = new GLib.KeyFile ();
            string entry = "Desktop Entry";

            keyfile.set_string (entry, "Version", "1.0");
            keyfile.set_string (entry, "Type", "Application");
            keyfile.set_string (entry, "Name", name);
            keyfile.set_string (entry, "Exec", exec);
            keyfile.set_string (entry, "TryExec", PACKAGE_NAME);
            keyfile.set_string (entry, "Icon", icon_name);
            keyfile.set_string (entry, "Categories", "Network;");
            /*
               Using the sanitized URI as a class matches midori_web_app_new
               So dock type launchers can distinguish different apps with the same executable
             */
            if (exec.has_prefix (APP_PREFIX))
                keyfile.set_string (entry, "StartupWMClass", uri.delimit (":.\\/", '_'));

            return keyfile.to_data();
        }

        internal static File get_app_folder () {
            var data_dir = File.new_for_path (Midori.Paths.get_user_data_dir ()).get_child (PACKAGE_NAME);
            return data_dir.get_child ("apps");
        }

        internal static async File create_app (string uri, string title, Gtk.Widget? proxy) {
            string checksum = Checksum.compute_for_string (ChecksumType.MD5, uri, -1);
            var folder = get_app_folder ();
            yield Launcher.create (APP_PREFIX, folder.get_child (checksum),
                uri, title, proxy);
            return folder.get_child (checksum);
        }

        internal static File get_profile_folder () {
            var data_dir = File.new_for_path (Midori.Paths.get_user_data_dir ()).get_child (PACKAGE_NAME);
            return data_dir.get_child ("profiles");
        }

        internal static async File create_profile (Gtk.Widget? proxy) {
            string uuid = g_dbus_generate_guid ();
            string config = Path.build_path (Path.DIR_SEPARATOR_S,
                Midori.Paths.get_user_data_dir (), PACKAGE_NAME, "profiles", uuid);
            var folder = get_profile_folder ();
            yield Launcher.create (PROFILE_PREFIX, folder.get_child (uuid),
                config, _("Midori (%s)").printf (uuid), proxy);
            return folder.get_child (uuid);
        }

        internal static async void create (string prefix, GLib.File folder, string uri, string title, Gtk.Widget proxy) {
            /* Strip LRE leading character and / */
            string name = title.delimit ("‪/", ' ').strip();
            string filename = Midori.Download.clean_filename (name);
            string icon_name = Midori.Stock.WEB_BROWSER;
            bool testing = false;
            if (proxy == null)
                testing = true;
            var file = folder.get_child ("desc");

            try {
                folder.make_directory_with_parents (null);
            } catch (IOError.EXISTS exist_error) {
                /* It's no error if the folder already exists */
            } catch (Error error) {
                warning (_("Failed to create new launcher (%s): %s"), file.get_path (), error.message);
            }

            icon_name = get_favicon_name_for_uri (prefix, folder, uri, testing);
            string desktop_file = prepare_desktop_file (prefix, name, uri, title, icon_name);

            try {
                var stream = yield file.replace_async (null, false, GLib.FileCreateFlags.NONE);
                yield stream.write_async (desktop_file.data);
                // Create a launcher/ menu
#if HAVE_WIN32
                Midori.Sokoke.create_win32_desktop_lnk (prefix, filename, uri);
#else
                var data_dir = File.new_for_path (Midori.Paths.get_user_data_dir ());
                var desktop_dir = data_dir.get_child ("applications");
                try {
                    desktop_dir.make_directory_with_parents (null);
                } catch (IOError.EXISTS exist_error) {
                    /* It's no error if the folder already exists */
                }

                yield file.copy_async (desktop_dir.get_child (filename + ".desktop"),
                    GLib.FileCopyFlags.NONE);
#endif
                if (proxy != null) {
                    var browser = proxy.get_toplevel () as Midori.Browser;
                    browser.send_notification (_("Launcher created"),
                        _("You can now run <b>%s</b> from your launcher or menu").printf (name));
                }
            }
            catch (Error error) {
                warning (_("Failed to create new launcher (%s): %s"), file.get_path (), error.message);
                if (proxy != null) {
                    var browser = proxy.get_toplevel () as Midori.Browser;
                    browser.send_notification (_("Error creating launcher"),
                        _("Failed to create new launcher (%s): %s").printf (file.get_path (), error.message));
                }
            }
        }

        internal Launcher (GLib.File file) {
            this.file = file;
        }

        bool init (GLib.Cancellable? cancellable) throws GLib.Error {
            var keyfile = new GLib.KeyFile ();
            try {
                keyfile.load_from_file (file.get_child ("desc").get_path (), GLib.KeyFileFlags.NONE);
            } catch (Error desc_error) {
                throw new FileError.EXIST (_("No file \"desc\" found"));
            }

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

#if !HAVE_WIN32
                /* FIXME: Profiles are broken on win32 because of no multi instance support */
                var profile = new Gtk.ToolButton.from_stock (Gtk.STOCK_ADD);
                profile.label = _("New _Profile");
                profile.tooltip_text = _("Creates a new, independent profile and a launcher");
                profile.use_underline = true;
                profile.is_important = true;
                profile.show ();
                profile.clicked.connect (() => {
                    Launcher.create_profile.begin (this);
                });
                toolbar.insert (profile, -1);
#endif

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
            if (event.button != 1)
            	return false;
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

                                string filename = Midori.Download.clean_filename (launcher.name);
#if HAVE_WIN32
                                string lnk_filename = Midori.Sokoke.get_win32_desktop_lnk_path_for_filename (filename);
                                if (Posix.access (lnk_filename, Posix.F_OK) == 0) {
                                    var lnk_file = File.new_for_path (lnk_filename);
                                    lnk_file.trash ();
                                }
#else
                                var data_dir = File.new_for_path (Midori.Paths.get_user_data_dir ());
                                data_dir.get_child ("applications").get_child (filename + ".desktop").trash ();
#endif
                            }
                            catch (Error error) {
                                GLib.critical ("Failed to remove launcher (%s): %s", launcher.file.get_path (), error.message);
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
                warning ("Application changed (%s): %s", file.get_path (), error.message);
            }
        }

        async void populate_apps (File app_folder) {
            try {
                try {
                    app_folder.make_directory_with_parents (null);
                } catch (IOError.EXISTS exist_error) {
                    /* It's no error if the folder already exists */
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
                            warning ("Failed to parse launcher (%s): %s", file.get_path (), error.message);
                        }
                    }
                }
            }
            catch (Error io_error) {
                warning ("Failed to list apps (%s): %s",
                         app_folder.get_path (), io_error.message);
            }
        }

        void browser_added (Midori.Browser browser) {
            var accels = new Gtk.AccelGroup ();
            browser.add_accel_group (accels);
            var action_group = browser.get_action_group ();

            var action = new Gtk.Action ("CreateLauncher", _("Create _Launcher"),
                _("Creates a new app for a specific site"), null);
            action.activate.connect (() => {
                var view = browser.tab as Midori.View;
                Launcher.create_app.begin (view.get_display_uri (), view.get_display_title (), view);
            });
            action_group.add_action_with_accel (action, "<Ctrl><Shift>A");
            action.set_accel_group (accels);
            action.connect_accelerator ();

            var viewable = new Sidebar (array, app_folder, profile_folder);
            viewable.show ();
            browser.panel.append_page (viewable);
            widgets.append (viewable);
        }

        void activated (Midori.App app) {
            array = new Katze.Array (typeof (Launcher));
            monitors = new GLib.List<GLib.FileMonitor> ();
            app_folder = Launcher.get_app_folder ();
            populate_apps.begin (app_folder);
            /* FIXME: Profiles are broken on win32 because of no multi instance support */
            profile_folder = Launcher.get_profile_folder ();
#if !HAVE_WIN32
            populate_apps.begin (profile_folder);
#endif
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
            foreach (var browser in app.get_browsers ()) {
                var action_group = browser.get_action_group ();
                var action = action_group.get_action ("CreateLauncher");
                action_group.remove_action (action);
            }

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

class ExtensionsAppsDesktop : Midori.Test.Job {
    public static void test () { new ExtensionsAppsDesktop ().run_sync (); }
    public override async void run (Cancellable cancellable) throws GLib.Error {
        string uri = "http://example.com";
        string checksum = Checksum.compute_for_string (ChecksumType.MD5, uri, -1);
        var apps = Apps.Launcher.get_app_folder ().get_child (checksum);
        Midori.Paths.remove_path (apps.get_path ());

        var data_dir = File.new_for_path (Midori.Paths.get_user_data_dir ());
        var desktop_dir = data_dir.get_child ("applications");
        Midori.Paths.remove_path (desktop_dir.get_child ("Example.desktop").get_path ());

        var folder = yield Apps.Launcher.create_app (uri, "Example", null);
        var launcher = new Apps.Launcher (folder);
        launcher.init ();
        Katze.assert_str_equal (folder.get_path (), launcher.uri, uri);
        yield Apps.Launcher.create_profile (null);
    }
}

public void extension_test () {
    Test.add_func ("/extensions/apps/desktop", ExtensionsAppsDesktop.test);
}

