/*
   Copyright (C) 2014 Christian Dywan <christian@twotoasts.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

namespace ExternalApplications {
    private class Chooser : Gtk.VBox {
        Gtk.ListStore store = new Gtk.ListStore (1, typeof (AppInfo));
        Gtk.TreeView treeview;

        public Chooser (string uri, string content_type) {
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

            treeview.row_activated.connect (row_activated);
            treeview.show ();
            pack_start (treeview, true, true, 0);

            foreach (var app_info in AppInfo.get_all_for_type (content_type)) {
                if (!uri.has_prefix ("file://") && !app_info.supports_uris ())
                    continue;
                launcher_added (app_info);
            }

            if (store.iter_n_children (null) < 1) {
                foreach (var app_info in AppInfo.get_all ()) {
                    if (!uri.has_prefix ("file://") && !app_info.supports_uris ())
                        continue;
                    if (!app_info.should_show ())
                        continue;
                    launcher_added (app_info);
                }
            }
        }

        public AppInfo get_app_info () {
            Gtk.TreeIter iter;
            if (treeview.get_selection ().get_selected (null, out iter)) {
                AppInfo app_info;
                store.get (iter, 0, out app_info);
                return app_info;
            }
            assert_not_reached ();
        }

        void on_render_icon (Gtk.CellLayout column, Gtk.CellRenderer renderer,
            Gtk.TreeModel model, Gtk.TreeIter iter) {

            AppInfo app_info;
            model.get (iter, 0, out app_info);

            renderer.set ("gicon", app_info.get_icon (),
                          "stock-size", Gtk.IconSize.DIALOG,
                          "xpad", 4);
        }

        void on_render_text (Gtk.CellLayout column, Gtk.CellRenderer renderer,
            Gtk.TreeModel model, Gtk.TreeIter iter) {

            AppInfo app_info;
            model.get (iter, 0, out app_info);
            renderer.set ("markup",
                Markup.printf_escaped ("<b>%s</b>\n%s",
                    app_info.get_display_name (), app_info.get_description ()),
                          "ellipsize", Pango.EllipsizeMode.END);
        }

        void launcher_added (AppInfo app_info) {
            Gtk.TreeIter iter;
            store.append (out iter);
            store.set (iter, 0, app_info);
        }

        int tree_sort_func (Gtk.TreeModel model, Gtk.TreeIter a, Gtk.TreeIter b) {
            AppInfo app_info1, app_info2;
            model.get (a, 0, out app_info1);
            model.get (b, 0, out app_info2);
            return strcmp (app_info1.get_display_name (), app_info2.get_display_name ());
        }

        void row_activated (Gtk.TreePath path, Gtk.TreeViewColumn column) {
            Gtk.TreeIter iter;
            if (store.get_iter (out iter, path)) {
                AppInfo app_info;
                store.get (iter, 0, out app_info);
                selected (app_info);
            }
        }

        public signal void selected (AppInfo app_info);
    }

    private class Manager : Midori.Extension {
        bool open_app_info (AppInfo app_info, string uri, string content_type) {
            try {
                var uris = new List<File> ();
                uris.append (File.new_for_uri (uri));
                app_info.launch (uris, null);
                app_info.set_as_last_used_for_type (content_type);
                app_info.set_as_default_for_type (content_type);
                return true;
            } catch (Error error) {
                warning ("Failed to open \"%s\": %s", uri, error.message);
                return false;
            }
        }

        bool navigation_requested (WebKit.WebView web_view, WebKit.WebFrame frame, WebKit.NetworkRequest request,
            WebKit.WebNavigationAction action, WebKit.WebPolicyDecision decision) {

            string uri = request.uri;
            if (Midori.URI.is_http (uri) || Midori.URI.is_blank (uri))
                return false;

            decision.ignore ();

            string content_type = get_content_type (uri, null);
            var app_info = AppInfo.get_default_for_type (content_type, !uri.has_prefix ("file://"));
            if (app_info != null && open_app_info (app_info, uri, content_type))
                return true;
            if (open_with (uri, content_type, web_view))
                return true;
            return true;
        }

        string get_content_type (string uri, string? mime_type) {
            if (!uri.has_prefix ("file://")) {
                string protocol = uri.split(":", 2)[0];
                return "x-scheme-handler/" + protocol;
            } else if (mime_type == null) {
                bool uncertain;
                return ContentType.guess (uri, null, out uncertain);
            }
            return ContentType.from_mime_type (mime_type);
        }

        bool open_with (string uri, string content_type, Gtk.Widget widget) {
            string filename;
            if (uri.has_prefix ("file://"))
                filename = Midori.Download.get_basename_for_display (uri);
            else
                filename = uri;

            var browser = Midori.Browser.get_for_widget (widget);
            var dialog = new Gtk.Dialog.with_buttons (_("Choose application"),
                browser,
#if !HAVE_GTK3
                Gtk.DialogFlags.NO_SEPARATOR |
#endif
                Gtk.DialogFlags.DESTROY_WITH_PARENT,
                Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                Gtk.STOCK_OPEN, Gtk.ResponseType.ACCEPT);
            dialog.set_icon_name (Gtk.STOCK_OPEN);
            dialog.resizable = false;

            var vbox = new Gtk.VBox (false, 8);
            vbox.border_width = 8;
            (dialog.get_content_area () as Gtk.Box).pack_start (vbox, true, true, 8);
            var label = new Gtk.Label (_("Select an application to open \"%s\"".printf (filename)));
            label.ellipsize = Pango.EllipsizeMode.END;
            vbox.pack_start (label, false, false, 0);
            var chooser = new Chooser (uri, content_type);
            vbox.pack_start (chooser, true, true, 0);

            dialog.get_content_area ().show_all ();
            dialog.set_default_response (Gtk.ResponseType.ACCEPT);
            chooser.selected.connect ((app_info) => {
                dialog.response (Gtk.ResponseType.ACCEPT);
            });
            bool accept = dialog.run () == Gtk.ResponseType.ACCEPT;

            var app_info = chooser.get_app_info ();
            dialog.destroy ();

            if (!accept)
                return false;

            return open_app_info (app_info, uri, content_type);
        }

        void context_menu (Midori.Tab tab, WebKit.HitTestResult hit_test_result, Midori.ContextAction menu) {
            string uri;
            if ((hit_test_result.context & WebKit.HitTestResultContext.LINK) != 0)  {
                uri = hit_test_result.link_uri;
            } else
                return;
            var action = new Gtk.Action ("OpenWith", _("Open _with…"), null, null);
            action.activate.connect ((action) => {
                open_with (uri, get_content_type (uri, tab.mime_type), tab);
            });
            menu.add (action);
        }

        public void tab_added (Midori.Browser browser, Midori.View view) {
            view.web_view.navigation_policy_decision_requested.connect (navigation_requested);
            view.context_menu.connect (context_menu);
        }

        public void tab_removed (Midori.Browser browser, Midori.View view) {
            view.web_view.navigation_policy_decision_requested.disconnect (navigation_requested);
            view.context_menu.disconnect (context_menu);
        }

        void browser_added (Midori.Browser browser) {
            foreach (var tab in browser.get_tabs ())
                tab_added (browser, tab);
            browser.add_tab.connect (tab_added);
            browser.remove_tab.connect (tab_removed);
        }

        void activated (Midori.App app) {
            foreach (var browser in app.get_browsers ())
                browser_added (browser);
            app.add_browser.connect (browser_added);
        }

        void browser_removed (Midori.Browser browser) {
            foreach (var tab in browser.get_tabs ())
                tab_removed (browser, tab);
            browser.add_tab.disconnect (tab_added);
            browser.remove_tab.disconnect (tab_removed);
        }

        void deactivated () {
            var app = get_app ();
            foreach (var browser in app.get_browsers ())
                browser_removed (browser);
            app.add_browser.disconnect (browser_added);

        }

        internal Manager () {
            GLib.Object (name: "External Applications",
                         description: "Choose what to open unknown file types with",
                         version: "0.1" + Midori.VERSION_SUFFIX,
                         authors: "Christian Dywan <christian@twotoasts.de>");

            this.activate.connect (activated);
            this.deactivate.connect (deactivated);
        }
    }
}

public Midori.Extension extension_init () {
    return new ExternalApplications.Manager ();
}

