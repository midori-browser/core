/*
   Copyright (C) 2010-2011 André Stösel <andre@stoesel.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

using Gtk;
using Gdk;
using WebKit;
using Midori;

namespace HistoryList {
    enum TabTreeCells {
        TREE_CELL_PIXBUF,
        TREE_CELL_STRING,
        TREE_CELL_POINTER,
        TREE_CELL_COUNT
    }

    enum TabClosingBehavior {
        NONE,
        LAST,
        NEW
    }

    enum TabClosingBehaviorModel {
        TEXT,
        VALUE
    }

    private abstract class HistoryWindow : Gtk.Window {
        public Midori.Browser browser { get; construct set; }
        protected Gtk.TreeView? treeview = null;

        public HistoryWindow (Midori.Browser browser) {
            GLib.Object (type: Gtk.WindowType.POPUP,
                         window_position: Gtk.WindowPosition.CENTER,
                         browser: browser);
        }

        public virtual void walk (int step) {
            Gtk.TreePath? path;
            Gtk.TreeViewColumn? column;

            this.treeview.get_cursor (out path, out column);

            unowned int[] indices = path.get_indices ();
            int new_index = indices[0] + step;

            var model = this.treeview.get_model () as Gtk.ListStore;

            int length = model.iter_n_children(null);
            while (new_index < 0 || new_index >= length)
                new_index = new_index < 0 ? length + new_index : new_index - length;

            path = new Gtk.TreePath.from_indices (new_index);
            this.treeview.set_cursor (path, column, false);
        }

        public abstract void make_update ();
        public abstract void clean_up ();
    }

    private class TabWindow : HistoryWindow {
        protected Gtk.HBox? hbox;
        protected Gtk.VBox? vbox;
        protected bool is_dirty = false;

        protected void store_append_row (GLib.PtrArray list, Gtk.ListStore store, out Gtk.TreeIter iter) {
            for (var i = list.len; i > 0; i--) {
                Midori.View view = list.index (i - 1) as Midori.View;

                Gdk.Pixbuf? icon = null;
                view.get ("icon", ref icon);

                unowned string title = view.get_display_title ();

                store.append (out iter);
                store.set (iter, TabTreeCells.TREE_CELL_PIXBUF, icon,
                                 TabTreeCells.TREE_CELL_STRING, title,
                                 TabTreeCells.TREE_CELL_POINTER, view);
            }
        }

        protected virtual void insert_rows (Gtk.ListStore store) {
            Gtk.TreeIter iter;
            unowned GLib.PtrArray list = this.browser.get_data<GLib.PtrArray> ("history-list-tab-history");
            unowned GLib.PtrArray list_new = this.browser.get_data<GLib.PtrArray> ("history-list-tab-history-new");
            store_append_row (list, store, out iter);
            store_append_row (list_new, store, out iter);
        }

        public TabWindow (Midori.Browser browser) {
            base (browser);

            this.vbox = new Gtk.VBox (false, 1);
            this.add (this.vbox);

            this.hbox = new Gtk.HBox (false, 1);

            var sw = new Gtk.ScrolledWindow (null, null);
            sw.set_policy (PolicyType.NEVER , PolicyType.AUTOMATIC);
            sw.set_shadow_type (ShadowType.ETCHED_IN);
            this.hbox.pack_start (sw, true, true, 0);

            var store = new Gtk.ListStore (TabTreeCells.TREE_CELL_COUNT,
                typeof (Gdk.Pixbuf), typeof (string), typeof (void*));

            this.insert_rows (store);

            this.vbox.pack_start (this.hbox, true, true, 0);

            this.treeview = new Gtk.TreeView.with_model (store);
            sw.add (treeview);

            this.treeview.set_model (store);
            this.treeview.set ("headers-visible", false);

            this.treeview.insert_column_with_attributes (
                -1, "Icon",
                new CellRendererPixbuf (), "pixbuf", TabTreeCells.TREE_CELL_PIXBUF);
            this.treeview.insert_column_with_attributes (
                -1, "Title",
                new CellRendererText (), "text", TabTreeCells.TREE_CELL_STRING);

            Requisition requisition;
            int height;
            int max_lines = 10;
#if HAVE_GTK3
            requisition = Requisition();
            this.treeview.get_preferred_width(out requisition.width, null);
            this.treeview.get_preferred_height(out requisition.height, null);
#else
            this.treeview.size_request (out requisition);
#endif
            int length = store.iter_n_children(null);
            if (length > max_lines) {
                height = requisition.height / length * max_lines + 2;
            } else {
                height = requisition.height + 2;
            }
            sw.set_size_request (320, height);

            this.show_all ();
        }

        public override void make_update () {
            this.is_dirty = true;

            Gtk.TreePath? path;
            Gtk.TreeViewColumn? column;

            this.treeview.get_cursor (out path, out column);

            var model = this.treeview.get_model () as Gtk.ListStore;

            Gtk.TreeIter iter;
            unowned Midori.View? view = null;

            model.get_iter (out iter, path);
            model.get (iter, TabTreeCells.TREE_CELL_POINTER, out view);
            this.browser.set ("tab", view);
        }

        public override void clean_up () {
            if(this.is_dirty) {
                Gtk.TreePath? path;
                Gtk.TreeViewColumn? column;

                this.treeview.get_cursor (out path, out column);

                path = new Gtk.TreePath.from_indices (0);
                this.treeview.set_cursor (path, column, false);

                this.make_update ();
                this.is_dirty = false;
            }
        }
    }

    private class NewTabWindow : TabWindow {
        protected bool old_tabs = false;
        protected bool first_step = true;

        protected override void insert_rows (Gtk.ListStore store) {
            Gtk.TreeIter iter;
            unowned GLib.PtrArray list = this.browser.get_data<GLib.PtrArray> ("history-list-tab-history-new");
            store_append_row (list, store, out iter);

            if ((int)list.len == 0) {
                this.old_tabs = true;
                var label = new Gtk.Label (_("There are no unvisited tabs"));
                this.vbox.pack_start (label, true, true, 0);
                unowned GLib.PtrArray list_old = this.browser.get_data<GLib.PtrArray> ("history-list-tab-history");
                store_append_row (list_old, store, out iter);
            }
        }

        public override void walk (int step) {
            if (this.first_step == false || step != 1) {
                base.walk (step);
            }
            this.first_step = false;
        }

        public override void clean_up () {
            if(this.is_dirty) {
                if(this.old_tabs) {
                    base.clean_up ();
                } else {
                    unowned GLib.PtrArray list = this.browser.get_data<GLib.PtrArray> ("history-list-tab-history");
                    Midori.View view = list.index (list.len - 1) as Midori.View;
                    this.browser.set ("tab", view);
                }
            }
        }

        public NewTabWindow (Midori.Browser browser) {
            base (browser);
        }
    }

    private class PreferencesDialog : Dialog {
        protected Manager hl_manager;
        protected ComboBox closing_behavior;

        public PreferencesDialog (Manager manager) {
            this.hl_manager = manager;

            this.title = _("Preferences for %s").printf( _("History-List"));
            if (this.get_class ().find_property ("has-separator") != null)
                this.set ("has-separator", false);
            this.border_width = 5;
            this.set_modal (true);
            this.set_default_size (350, 100);
            this.create_widgets ();

            this.response.connect (response_cb);
        }

        private void response_cb (Dialog source, int response_id) {
            switch (response_id) {
                case ResponseType.APPLY:
                    int value;
                    TreeIter iter;

                    this.closing_behavior.get_active_iter (out iter);
                    var model = this.closing_behavior.get_model ();
                    model.get (iter, TabClosingBehaviorModel.VALUE, out value);

                    this.hl_manager.set_integer ("TabClosingBehavior", value);
                    this.hl_manager.preferences_changed ();

                    this.destroy ();
                    break;
                case ResponseType.CANCEL:
                    this.destroy ();
                    break;
            }
        }

        private void create_widgets () {
            ListStore model;
            TreeIter iter;
            TreeIter? active_iter = null;

            var table = new Table (1, 2, true);
            var renderer = new CellRendererText ();

            var label = new Label ( _("Tab closing behavior"));
            table.attach_defaults (label, 0, 1, 0, 1);

            var tab_closing_behavior = this.hl_manager.get_integer ("TabClosingBehavior");

            model = new ListStore (2, typeof (string), typeof (int));

            model.append (out iter);
            model.set (iter, TabClosingBehaviorModel.TEXT, _("Do nothing"),
                             TabClosingBehaviorModel.VALUE, TabClosingBehavior.NONE);
            if (TabClosingBehavior.NONE == tab_closing_behavior)
                active_iter = iter;

            model.append (out iter);
            model.set (iter, TabClosingBehaviorModel.TEXT, _("Switch to last viewed tab"),
                             TabClosingBehaviorModel.VALUE, TabClosingBehavior.LAST);
            if (TabClosingBehavior.LAST == tab_closing_behavior)
                active_iter = iter;

            model.append (out iter);
            model.set (iter, TabClosingBehaviorModel.TEXT, _("Switch to newest tab"),
                             TabClosingBehaviorModel.VALUE, TabClosingBehavior.NEW);
            if (TabClosingBehavior.NEW == tab_closing_behavior)
                active_iter = iter;

            this.closing_behavior = new ComboBox.with_model (model);
            this.closing_behavior.set_active_iter (active_iter);
            this.closing_behavior.pack_start (renderer, true);
            this.closing_behavior.set_attributes (renderer, "text", 0);

            table.attach_defaults (this.closing_behavior, 1, 2, 0, 1);

#if HAVE_GTK3
            (get_content_area() as Gtk.Box).pack_start (table, false, true, 0);
#else
            this.vbox.pack_start (table, false, true, 0);
#endif

            this.add_button (Gtk.STOCK_CANCEL, ResponseType.CANCEL);
            this.add_button (Gtk.STOCK_APPLY, ResponseType.APPLY);

            this.show_all ();
        }
    }

    private class Manager : Midori.Extension {
        public signal void preferences_changed ();

        protected uint escKeyval;
        protected uint modifier_count;
        protected int closing_behavior;
        protected HistoryWindow? history_window;
        protected ulong[] tmp_sig_ids = new ulong[2];
        protected bool ignoreNextChange = false;

        public void preferences_changed_cb () {
            this.closing_behavior = this.get_integer ("TabClosingBehavior");
        }

        public bool key_press (Gdk.EventKey event_key) {
            if (event_key.is_modifier > 0) {
                this.modifier_count++;
            }
            return false;
        }

        public bool key_release (Gdk.EventKey event_key, Browser browser) {
            if (event_key.is_modifier > 0) {
                this.modifier_count--;
            }
            if (this.modifier_count == 0 || event_key.keyval == this.escKeyval) {
                browser.disconnect (this.tmp_sig_ids[0]);
                browser.disconnect (this.tmp_sig_ids[1]);
                if (this.modifier_count == 0) {
                    this.history_window.make_update ();
                } else {
                    this.modifier_count = 0;
                    this.history_window.clean_up ();
                }
                this.history_window.destroy ();
                this.history_window = null;
            }
            return false;
        }

        public void walk (Gtk.Action action, Browser browser, Type type, int step) {
            Midori.View? view = null;
            view = browser.get_data<Midori.View?> ("history-list-last-change");
            if (view != null) {
                this.tab_list_resort (browser, view);
                browser.set_data<Midori.View?> ("history-list-last-change", null);
            }

            if (this.history_window == null || this.history_window.get_type () != type) {
                if (this.history_window == null) {
                    this.modifier_count = Midori.Sokoke.gtk_action_count_modifiers (action);
                    this.tmp_sig_ids[0] = browser.key_press_event.connect ((ek) => {
                        return this.key_press (ek);
                    });
                    this.tmp_sig_ids[1] = browser.key_release_event.connect ((ek) => {
                        return this.key_release (ek, browser);
                    });
                } else {
                    this.history_window.destroy ();
                    this.history_window = null;
                }
                /*
                    Bug:  https://bugzilla.gnome.org/show_bug.cgi?id=618750
                    Code: this.history_window = (Gtk.Window) GLib.Object.new (type);
                */
                if (type == typeof (TabWindow)) {
                    this.history_window = new TabWindow (browser);
                } else if (type == typeof (NewTabWindow)) {
                    this.history_window = new NewTabWindow (browser);
                }
            }
            var hw = this.history_window as HistoryWindow;
            hw.walk (step);
        }

        public void special_function (Gtk.Action action, Browser browser) {
            if (this.history_window != null) {
                this.ignoreNextChange = true;
                this.history_window.make_update ();
            }
        }

        void browser_added (Midori.Browser browser) {
            ulong sidTabNext, sidTabPrevious;
            var acg = new Gtk.AccelGroup ();
            browser.add_accel_group (acg);
            var action_group = browser.get_action_group ();

            Gtk.Action action;

            action = action_group.get_action ("TabNext");
            browser.block_action (action);
            sidTabNext = action.activate.connect ((a) => {
                this.walk (a, browser, typeof (TabWindow), 1);
            });

            action = action_group.get_action ("TabPrevious");
            browser.block_action (action);
            sidTabPrevious = action.activate.connect ((a) => {
                this.walk (a, browser, typeof (TabWindow), -1);
            });

            action = new Gtk.Action ("HistoryListNextNewTab",
                _("Next new Tab (History List)"),
                _("Next new tab from history"), null);
            action.activate.connect ((a) => {
                this.walk (a, browser, typeof (NewTabWindow), 1);
            });
            action_group.add_action_with_accel (action, "<Ctrl>1");
            action.set_accel_group (acg);
            action.connect_accelerator ();

            action = new Gtk.Action ("HistoryListPreviousNewTab",
                _("Previous new Tab (History List)"),
                _("Previous new tab from history"), null);
            action.activate.connect ((a) => {
                this.walk (a, browser, typeof (NewTabWindow), -1);
            });
            action_group.add_action_with_accel (action, "<Ctrl>2");
            action.set_accel_group (acg);
            action.connect_accelerator ();

            action = new Gtk.Action ("HistoryListSpecialFunction",
                _("Display tab in background (History List)"),
                _("Display the current selected tab in background"), null);
            action.activate.connect ((a) => {
                this.special_function (a, browser);
            });
            action_group.add_action_with_accel (action, "<Ctrl>3");
            action.set_accel_group (acg);
            action.connect_accelerator ();

            browser.set_data<ulong> ("history-list-sid-tab-next", sidTabNext);
            browser.set_data<ulong> ("history-list-sid-tab-previous", sidTabPrevious);

            browser.set_data<GLib.PtrArray*> ("history-list-tab-history",
                                              new GLib.PtrArray ());
            browser.set_data<GLib.PtrArray*> ("history-list-tab-history-new",
                                              new GLib.PtrArray ());
            browser.set_data<Midori.View?> ("history-list-last-change", null);

            foreach (var tab in browser.get_tabs ())
                tab_added (browser, tab);
            browser.add_tab.connect (tab_added);
            browser.remove_tab.connect (tab_removed);
            browser.switch_tab.connect (this.tab_changed);
        }

        void browser_removed (Midori.Browser browser) {
            string[] callbacks = { "HistoryListNextNewTab", "HistoryListPreviousNewTab",
                                   "HistoryListSpecialFunction" };
            ulong sidTabNext, sidTabPrevious;
            sidTabNext = browser.get_data<ulong> ("history-list-sid-tab-next");
            sidTabPrevious = browser.get_data<ulong> ("history-list-sid-tab-previous");

            Gtk.Action action;
            Gtk.ActionGroup action_group;
            action_group = browser.get_action_group ();

            action = action_group.get_action ("TabNext");
            action.disconnect (sidTabNext);
            browser.unblock_action (action);
            action = action_group.get_action ("TabPrevious");
            action.disconnect (sidTabPrevious);
            browser.unblock_action (action);

            for (int i = 0; i < callbacks.length; i++) {
                action = action_group.get_action (callbacks[i]);
                if (action != null)
                    action_group.remove_action (action);
            }

            browser.add_tab.disconnect (tab_added);
            browser.remove_tab.disconnect (tab_removed);
            browser.switch_tab.disconnect (this.tab_changed);
        }

        void tab_added (Midori.Browser browser, Midori.View view) {
            unowned GLib.PtrArray list = browser.get_data<GLib.PtrArray> ("history-list-tab-history-new");
            list.add (view);
        }

        void tab_removed (Midori.Browser browser, Midori.View view) {
            unowned GLib.PtrArray list = browser.get_data<GLib.PtrArray> ("history-list-tab-history");
            unowned GLib.PtrArray list_new = browser.get_data<GLib.PtrArray> ("history-list-tab-history-new");
            list.remove (view);
            list_new.remove (view);

            if (this.closing_behavior == TabClosingBehavior.LAST || this.closing_behavior == TabClosingBehavior.NEW) {
                browser.set_data<Midori.View?> ("history-list-last-change", null);

                if ((int) list.len > 0 || (int) list_new.len > 0) {
                    TabWindow hw;
                    if (this.closing_behavior == TabClosingBehavior.LAST)
                        hw = new TabWindow (browser);
                    else
                        hw = new NewTabWindow (browser);
                    hw.make_update ();
                    hw.destroy ();
                }
            }
        }

        void tab_changed (Midori.View? old_view, Midori.View? new_view) {
            if(this.ignoreNextChange) {
                this.ignoreNextChange = false;
            } else {
                Midori.Browser? browser = Midori.Browser.get_for_widget (new_view);
                Midori.View? last_view
                    = browser.get_data<Midori.View?> ("history-list-last-change");

                if (last_view != null) {
                    this.tab_list_resort (browser, last_view);
                }
                browser.set_data<Midori.View?> ("history-list-last-change", new_view);
            }
        }

        void tab_list_resort (Midori.Browser browser, Midori.View view) {
            unowned GLib.PtrArray list = browser.get_data<GLib.PtrArray> ("history-list-tab-history");
            unowned GLib.PtrArray list_new = browser.get_data<GLib.PtrArray> ("history-list-tab-history-new");
            list.remove (view);
            list_new.remove (view);
            list.add (view);
        }

        void activated (Midori.App app) {
            this.preferences_changed ();
            foreach (var browser in app.get_browsers ())
                browser_added (browser);
            app.add_browser.connect (browser_added);
        }

        void deactivated () {
            var app = get_app ();
            foreach (var browser in app.get_browsers ())
                browser_removed (browser);
            app.add_browser.disconnect (browser_added);
        }

        void show_preferences () {
            var dialog = new PreferencesDialog (this);
            dialog.show ();
        }

        internal Manager () {
            GLib.Object (name: _("History List"),
                         description: _("Move to the last used tab when switching or closing tabs"),
                         version: "0.4" + Midori.VERSION_SUFFIX,
                         authors: "André Stösel <andre@stoesel.de>");

            this.install_integer ("TabClosingBehavior", TabClosingBehavior.LAST);

            this.activate.connect (activated);
            this.deactivate.connect (deactivated);
            this.open_preferences.connect (show_preferences);
            this.preferences_changed.connect (preferences_changed_cb);
        }
        construct {
            this.escKeyval = Gdk.keyval_from_name ("Escape");
        }
    }
}

public Midori.Extension extension_init () {
    return new HistoryList.Manager ();
}

