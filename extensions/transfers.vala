/*
   Copyright (C) 2009-2013 Christian Dywan <christian@twotoasts.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

namespace Gtk {
    extern static void widget_size_request (Gtk.Widget widget, out Gtk.Requisition requisition);
}

namespace Sokoke {
    extern static void widget_get_text_size (Gtk.Widget widget, string sample, out int width, out int height);
}

namespace Transfers {
    private class Transfer : GLib.Object {
        internal uint poll_source = 0;
        internal WebKit.Download download;

        internal virtual signal void changed ()
        {
            string tooltip = Midori.Download.calculate_tooltip (download);
            this.set_data ("tooltip", tooltip);
        }
        internal signal void remove ();
        internal signal void removed ();

        internal int action { get {
            return Midori.Download.get_type (download);
        } }
        internal double progress { get {
            return Midori.Download.get_progress (download);
        } }
#if HAVE_WEBKIT2
        public bool succeeded { get; protected set; default = false; }
        public bool finished { get; protected set; default = false; }
        internal string destination { get {
            return download.destination;
        } }
#else
        internal bool succeeded { get {
            return download.status == WebKit.DownloadStatus.FINISHED;
        } }
        internal bool finished { get {
            return Midori.Download.is_finished (download);
        } }
        internal string destination { get {
            return download.destination_uri;
        } }
#endif

        internal Transfer (WebKit.Download download) {
            poll_source = Timeout.add(1000/10, () => {
                changed ();
                return true;
            });
            this.download = download;
            #if HAVE_WEBKIT2
            download.finished.connect (() => {
                succeeded = finished = true;
                changed ();
                Source.remove (poll_source);
                poll_source = 0;
            });
            download.failed.connect (() => {
                succeeded = false;
                finished = true;
                changed ();
                Source.remove (poll_source);
                poll_source = 0;
            });
            #else
            download.notify["status"].connect (() => {
                changed ();
                if (download.status == WebKit.DownloadStatus.FINISHED || download.status == WebKit.DownloadStatus.ERROR) {
                    Source.remove (poll_source);
                    poll_source = 0;
                }
            });
            #endif
        }
    }

    static bool pending_transfers (Katze.Array array) {
        foreach (GLib.Object item in array.get_items ()) {
            var transfer = item as Transfer;
            if (!transfer.finished)
                return true;
        }
        return false;
    }

    private class Sidebar : Gtk.VBox, Midori.Viewable {
        Gtk.Toolbar? toolbar = null;
        Gtk.ToolButton clear;
        Gtk.ListStore store = new Gtk.ListStore (1, typeof (Transfer));
        Gtk.TreeView treeview;
        Katze.Array array;

        public unowned string get_stock_id () {
            return Midori.Stock.TRANSFER;
        }

        public unowned string get_label () {
            return _("Transfers");
        }

        public Gtk.Widget get_toolbar () {
            if (toolbar == null) {
                toolbar = new Gtk.Toolbar ();
                toolbar.set_icon_size (Gtk.IconSize.BUTTON);
                toolbar.insert (new Gtk.ToolItem (), -1);
                var separator = new Gtk.SeparatorToolItem ();
                separator.draw = false;
                separator.set_expand (true);
                toolbar.insert (separator, -1);
                clear = new Gtk.ToolButton.from_stock (Gtk.STOCK_CLEAR);
                clear.label = _("Clear All");
                clear.is_important = true;
                clear.clicked.connect (clear_clicked);
                clear.sensitive = !array.is_empty ();
                toolbar.insert (clear, -1);
                toolbar.show_all ();
            }
            return toolbar;
        }

        void clear_clicked () {
            foreach (GLib.Object item in array.get_items ()) {
                var transfer = item as Transfer;
                if (transfer.finished)
                    transfer.remove ();
            }
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
            Gtk.CellRendererProgress renderer_progress = new Gtk.CellRendererProgress ();
            column.pack_start (renderer_progress, true);
            column.set_expand (true);
            column.set_cell_data_func (renderer_progress, on_render_text);
            treeview.append_column (column);

            column = new Gtk.TreeViewColumn ();
            Gtk.CellRendererPixbuf renderer_button = new Gtk.CellRendererPixbuf ();
            column.pack_start (renderer_button, false);
            column.set_cell_data_func (renderer_button, on_render_button);
            treeview.append_column (column);

            treeview.row_activated.connect (row_activated);
            treeview.button_release_event.connect (button_released);
            treeview.popup_menu.connect (menu_popup);
            treeview.show ();
            pack_start (treeview, true, true, 0);

            this.array = array;
            array.add_item.connect (transfer_added);
            array.remove_item.connect_after (transfer_removed);
            foreach (GLib.Object item in array.get_items ())
                transfer_added (item);
        }

        void row_activated (Gtk.TreePath path, Gtk.TreeViewColumn column) {
            Gtk.TreeIter iter;
            if (store.get_iter (out iter, path)) {
                Transfer transfer;
                store.get (iter, 0, out transfer);

                try {
                    if (Midori.Download.action_clear (transfer.download, treeview))
                        transfer.remove ();
                } catch (Error error) {
                    // Failure to open is the only known possibility here
                    GLib.warning (_("Failed to open download: %s"), error.message);
                }
            }
        }

        bool button_released (Gdk.EventButton event) {
            if (event.button == 3)
                return show_popup_menu (event);
            return false;
        }

        bool menu_popup () {
            return show_popup_menu (null);
        }

        bool show_popup_menu (Gdk.EventButton? event) {
            Gtk.TreeIter iter;
            if (treeview.get_selection ().get_selected (null, out iter)) {
                Transfer transfer;
                store.get (iter, 0, out transfer);

                var menu = new Gtk.Menu ();
                var menuitem = new Gtk.ImageMenuItem.from_stock (Gtk.STOCK_OPEN, null);
                menuitem.activate.connect (() => {
                    try {
                        Midori.Download.open (transfer.download, treeview);
                    } catch (Error error_open) {
                        GLib.warning (_("Failed to open download: %s"), error_open.message);
                    }
                });
                menuitem.sensitive = transfer.succeeded;
                menu.append (menuitem);
                menuitem = new Gtk.ImageMenuItem.with_mnemonic (_("Open Destination _Folder"));
                menuitem.image = new Gtk.Image.from_stock (Gtk.STOCK_DIRECTORY, Gtk.IconSize.MENU);
                menuitem.activate.connect (() => {
                    var folder = GLib.File.new_for_uri (transfer.destination);
                    (Midori.Browser.get_for_widget (this).tab as Midori.Tab).open_uri (folder.get_parent ().get_uri ());
                });
                menu.append (menuitem);
                menuitem = new Gtk.ImageMenuItem.with_mnemonic (_("Copy Link Loc_ation"));
                menuitem.activate.connect (() => {
                    string uri = transfer.destination;
                    get_clipboard (Gdk.SELECTION_PRIMARY).set_text (uri, -1);
                    get_clipboard (Gdk.SELECTION_CLIPBOARD).set_text (uri, -1);
                });
                menuitem.image = new Gtk.Image.from_stock (Gtk.STOCK_COPY, Gtk.IconSize.MENU);
                menu.append (menuitem);
                menu.show_all ();
                Katze.widget_popup (treeview, menu, null, Katze.MenuPos.CURSOR);

                return true;
            }
            return false;
        }

        int tree_sort_func (Gtk.TreeModel model, Gtk.TreeIter a, Gtk.TreeIter b) {
            Transfer transfer1, transfer2;
            model.get (a, 0, out transfer1);
            model.get (b, 0, out transfer2);
            return (transfer1.finished ? 1 : 0) - (transfer2.finished ? 1 : 0);
        }

        void transfer_changed (GLib.Object item) {
            treeview.queue_draw ();
        }

        void transfer_added (GLib.Object item) {
            var transfer = item as Transfer;
            Gtk.TreeIter iter;
            store.append (out iter);
            store.set (iter, 0, transfer);
            transfer.changed.connect (() => transfer_changed (transfer));
            clear.sensitive = true;
        }

        void transfer_removed (GLib.Object item) {
            var transfer = item as Transfer;
            transfer.changed.disconnect (transfer_changed);
            Gtk.TreeIter iter;
            if (store.iter_children (out iter, null)) {
                do {
                    Transfer found;
                    store.get (iter, 0, out found);
                    if (transfer == found) {
                        store.remove (iter);
                        break;
                    }
                } while (store.iter_next (ref iter));
            }
            if (array.is_empty ())
                clear.sensitive = false;
        }

        void on_render_icon (Gtk.CellLayout column, Gtk.CellRenderer renderer,
            Gtk.TreeModel model, Gtk.TreeIter iter) {

            Transfer transfer;
            model.get (iter, 0, out transfer);
            string content_type = Midori.Download.get_content_type (transfer.download, null);
            var icon = GLib.ContentType.get_icon (content_type) as ThemedIcon;
            icon.append_name ("text-html");
            renderer.set ("gicon", icon,
                          "stock-size", Gtk.IconSize.DND,
                          "xpad", 1, "ypad", 12);
        }

        void on_render_text (Gtk.CellLayout column, Gtk.CellRenderer renderer,
            Gtk.TreeModel model, Gtk.TreeIter iter) {

            Transfer transfer;
            model.get (iter, 0, out transfer);
            renderer.set ("text", transfer.get_data<string>("tooltip") ?? "",
                          "value", (int)(transfer.progress * 100));
        }

        void on_render_button (Gtk.CellLayout column, Gtk.CellRenderer renderer,
            Gtk.TreeModel model, Gtk.TreeIter iter) {

            Transfer transfer;
            model.get (iter, 0, out transfer);
            string stock_id = Midori.Download.action_stock_id (transfer.download);
            renderer.set ("stock-id", stock_id,
                          "stock-size", Gtk.IconSize.MENU);
        }
    }

    private class TransferButton : Gtk.ToolItem {
        Transfer transfer;
        Gtk.ProgressBar progress;
        Gtk.Image icon;
        Gtk.Button button;

        public TransferButton (Transfer transfer) {
            this.transfer = transfer;

            var box = new Gtk.HBox (false, 0);
            progress = new Gtk.ProgressBar ();
#if HAVE_GTK3
            progress.show_text = true;
#endif
            progress.ellipsize = Pango.EllipsizeMode.MIDDLE;
            string filename = Midori.Download.get_basename_for_display (transfer.destination);
            progress.text = filename;
            int width;
            Sokoke.widget_get_text_size (progress, "M", out width, null);
            progress.set_size_request (width * 10, 1);
            box.pack_start (progress, false, false, 0);

            icon = new Gtk.Image ();
            button = new Gtk.Button ();
            button.relief = Gtk.ReliefStyle.NONE;
            button.focus_on_click = false;
            button.clicked.connect (button_clicked);
            button.add (icon);
            box.pack_start (button, false, false, 0);

            add (box);
            show_all ();

            transfer.changed.connect (transfer_changed);
            transfer_changed ();
            transfer.removed.connect (transfer_removed);
        }

        void button_clicked () {
            try {
                if (Midori.Download.action_clear (transfer.download, button))
                    transfer.remove ();
            } catch (Error error) {
                // Failure to open is the only known possibility here
                GLib.warning (_("Failed to open download: %s"), error.message);
            }
        }

        void transfer_changed () {
            progress.fraction = Midori.Download.get_progress (transfer.download);
            progress.tooltip_text = transfer.get_data<string>("tooltip") ?? "";
            string stock_id = Midori.Download.action_stock_id (transfer.download);
            icon.set_from_stock (stock_id, Gtk.IconSize.MENU);
        }

        void transfer_removed () {
            destroy ();
        }
    }

    private class Toolbar : Gtk.Toolbar {
        Katze.Array array;
        Gtk.ToolButton clear;

        void clear_clicked () {
            foreach (GLib.Object item in array.get_items ()) {
                var transfer = item as Transfer;
                if (transfer.finished)
                    array.remove_item (item);
            }
        }

        public Toolbar (Katze.Array array) {
            set_icon_size (Gtk.IconSize.BUTTON);
            set_style (Gtk.ToolbarStyle.BOTH_HORIZ);
            show_arrow = false;

            clear = new Gtk.ToolButton.from_stock (Gtk.STOCK_CLEAR);
            clear.label = _("Clear All");
            clear.is_important = true;
            clear.clicked.connect (clear_clicked);
            clear.sensitive = !array.is_empty ();
            insert (clear, -1);
            clear.show ();
            clear.sensitive = false;

            this.array = array;
            array.add_item.connect (transfer_added);
            array.remove_item.connect_after (transfer_removed);
            foreach (GLib.Object item in array.get_items ())
                transfer_added (item);
        }

        void transfer_added (GLib.Object item) {
            var transfer = item as Transfer;
            /* Newest item on the left */
            insert (new TransferButton (transfer), 0);
            clear.sensitive = true;
            show ();

            Gtk.Requisition req;
            Gtk.widget_size_request (parent, out req);
            int reqwidth = req.width;
            int winwidth;
            (get_toplevel () as Gtk.Window).get_size (out winwidth, null);
            if (reqwidth > winwidth)
                clear_clicked ();
        }

        void transfer_removed (GLib.Object item) {
            clear.sensitive = pending_transfers (array);
            if (array.is_empty ())
                hide ();
        }
    }

    private class Manager : Midori.Extension {
        internal Katze.Array array;
        internal GLib.List<Gtk.Widget> widgets;
        internal GLib.List<string> notifications;
        internal uint notification_timeout;

        void download_added (WebKit.Download download) {
            var transfer = new Transfer (download);
            transfer.remove.connect (transfer_remove);
            transfer.changed.connect (transfer_changed);
            array.remove_item.connect (transfer_removed);
            array.add_item (transfer);
        }

        bool notification_timeout_triggered () {
            notification_timeout = 0;
            if (notifications.length () > 0) {
                string filename = notifications.nth_data(0);
                string msg;
                if (notifications.length () == 1)
                    msg = _("The file '<b>%s</b>' has been downloaded.").printf (filename);
                else
                    msg = _("'<b>%s</b>' and %d other files have been downloaded.").printf (filename, notifications.length ());
                get_app ().send_notification (_("Transfer completed"), msg);
                notifications = new GLib.List<string> ();
            }
            return false;
        }

        void transfer_changed (Transfer transfer) {
            if (transfer.succeeded) {
                /* FIXME: The following 2 blocks ought to be done in core */
                if (transfer.action == Midori.DownloadType.OPEN) {
                    try {
                        if (Midori.Download.action_clear (transfer.download, widgets.nth_data (0)))
                            transfer.remove ();
                    } catch (Error error) {
                        // Failure to open is the only known possibility here
                        GLib.warning (_("Failed to open download: %s"), error.message);
                    }
                }

                string uri = transfer.destination;
                string filename = Midori.Download.get_basename_for_display (uri);
                var item = new Katze.Item ();
                item.uri = uri;
                item.name = filename;
                Midori.Browser.update_history (item, "download", "create");
                if (!Midori.Download.has_wrong_checksum (transfer.download))
                    Gtk.RecentManager.get_default ().add_item (uri);

                notifications.append (filename);
                if (notification_timeout == 0) {
                    notification_timeout_triggered ();
                    notification_timeout = Midori.Timeout.add_seconds (60, notification_timeout_triggered);
                }
            }
        }

        void transfer_remove (Transfer transfer) {
            array.remove_item (transfer);
        }

        void transfer_removed (GLib.Object item) {
            var transfer = item as Transfer;
            transfer.removed ();
        }

#if HAVE_GTK3
        bool browser_closed (Gtk.Widget widget, Gdk.EventAny event) {
#else
        bool browser_closed (Gtk.Widget widget, Gdk.Event event) {
#endif
            var browser = widget as Midori.Browser;
            if (pending_transfers (array)) {
                var dialog = new Gtk.MessageDialog (browser,
                    Gtk.DialogFlags.DESTROY_WITH_PARENT,
                    Gtk.MessageType.WARNING, Gtk.ButtonsType.NONE,
                    "%s",
                    _("Some files are being downloaded"));
                dialog.title = _("Some files are being downloaded");
                dialog.add_buttons (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                                    _("_Quit Midori"), Gtk.ResponseType.ACCEPT);
                dialog.format_secondary_text (
                    _("The transfers will be cancelled if Midori quits."));
                bool cancel = dialog.run () != Gtk.ResponseType.ACCEPT;
                dialog.destroy ();
                return cancel;
            }
            return false;
        }

        void browser_added (Midori.Browser browser) {
            var viewable = new Sidebar (array);
            viewable.show ();
            browser.panel.append_page (viewable);
            widgets.append (viewable);
            var toolbar = new Toolbar (array);
#if HAVE_GTK3
            browser.statusbar.pack_end (toolbar, false, false);
#else
            browser.statusbar.pack_start (toolbar, false, false);
#endif
            widgets.append (toolbar);
            // TODO: popover
            // TODO: progress in dock item
            browser.add_download.connect (download_added);
            browser.delete_event.connect (browser_closed);
        }

        void activated (Midori.App app) {
            array = new Katze.Array (typeof (Transfer));
            widgets = new GLib.List<Gtk.Widget> ();
            notifications = new GLib.List<string> ();
            notification_timeout = 0;
            foreach (var browser in app.get_browsers ())
                browser_added (browser);
            app.add_browser.connect (browser_added);
        }

        void deactivated () {
            var app = get_app ();
            app.add_browser.disconnect (browser_added);
            foreach (var browser in app.get_browsers ()) {
                browser.add_download.disconnect (download_added);
                browser.delete_event.disconnect (browser_closed);
            }
            foreach (var widget in widgets)
                widget.destroy ();
            array.remove_item.disconnect (transfer_removed);
        }

        internal Manager () {
            GLib.Object (name: _("Transfer Manager"),
                         description: _("View downloaded files"),
                         version: "0.1" + Midori.VERSION_SUFFIX,
                         authors: "Christian Dywan <christian@twotoasts.de>");

            this.activate.connect (activated);
            this.deactivate.connect (deactivated);
        }
    }
}

public Midori.Extension extension_init () {
    return new Transfers.Manager ();
}

