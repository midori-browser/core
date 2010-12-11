/*
   Copyright (C) 2010 André Stösel <Midori-Plugin@PyIT.de>

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

enum TabTreeCells {
    TREE_CELL_PIXBUF,
    TREE_CELL_STRING,
    TREE_CELL_POINTER,
    TREE_CELL_COUNT
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

        while (new_index < 0 || new_index >= model.length)
            new_index = new_index < 0 ? model.length + new_index : new_index - model.length;

        path = new Gtk.TreePath.from_indices (new_index);
        this.treeview.set_cursor (path, column, false);
    }
    public abstract void make_update ();
}

private class TabWindow : HistoryWindow {
    protected Gtk.HBox? hbox;
    protected Gtk.VBox? vbox;
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
        this.treeview.set_fixed_height_mode (true);
        sw.add (treeview);

        this.treeview.set_model (store);
        this.treeview.set ("headers-visible", false);

        this.treeview.insert_column_with_attributes (
            TabTreeCells.TREE_CELL_PIXBUF, "Icon",
            new CellRendererPixbuf (), "pixbuf", 0);
        this.treeview.insert_column_with_attributes (
            TabTreeCells.TREE_CELL_STRING, "Title",
            new CellRendererText (), "text", 1);

        Requisition requisition;
        int height;
        int max_lines = 10;
        this.treeview.size_request (out requisition);
        if (store.length > max_lines) {
            height = requisition.height / store.length * max_lines + 2;
        } else {
            height = requisition.height + 2;
        }
        sw.set_size_request (320, height);

        this.show_all ();
    }
    public override void make_update () {
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
}

private class NewTabWindow : TabWindow {
    protected bool first_step = true;
    protected override void insert_rows (Gtk.ListStore store) {
        Gtk.TreeIter iter;
        unowned GLib.PtrArray list = this.browser.get_data<GLib.PtrArray> ("history-list-tab-history-new");
        store_append_row (list, store, out iter);

        if ((int)list.len == 0) {
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
    public NewTabWindow (Midori.Browser browser) {
        base (browser);
    }
}

private class HistoryList : Midori.Extension {
    protected uint modifier_count;
    protected HistoryWindow? history_window;
    protected ulong[] tmp_sig_ids = new ulong[2];
    public bool key_press (Gdk.EventKey event_key) {
        if (event_key.is_modifier > 0) {
            this.modifier_count++;
        }
        return false;
    }
    public bool key_release (Gdk.EventKey event_key, Browser browser) {
        if (event_key.is_modifier > 0) {
            this.modifier_count--;
            if (this.modifier_count == 0) {
                browser.disconnect (this.tmp_sig_ids[0]);
                browser.disconnect (this.tmp_sig_ids[1]);
                this.history_window.make_update ();
                this.history_window.destroy ();
                this.history_window = null;
            }
        }
        return false;
    }
    public void walk (Gtk.Action action, Browser browser, Type type, int step) {
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
    void browser_added (Midori.Browser browser) {
        var acg = new Gtk.AccelGroup ();
        browser.add_accel_group (acg);
        var action_group = browser.get_action_group ();

        Gtk.Action action;

        action = new Gtk.Action ("HistoryListNextTab",
            _("Next Tab (History List)"),
            _("Next tab from history"), null);
        action.activate.connect ((a) => {
            this.walk (a, browser, typeof (TabWindow), 1);
        });
        action_group.add_action_with_accel (action, "<Ctrl>Tab");
        action.set_accel_group (acg);
        action.connect_accelerator ();

        action = new Gtk.Action ("HistoryListPreviousTab",
            _("Previous Tab (History List)"),
            _("Previous tab from history"), null);
        action.activate.connect ((a) => {
            this.walk (a, browser, typeof (TabWindow), -1);
        });
        action_group.add_action_with_accel (action, "<Ctrl><Shift>Tab");
        action.set_accel_group (acg);
        action.connect_accelerator ();

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

        browser.set_data<GLib.PtrArray*> ("history-list-tab-history",
                                          new GLib.PtrArray ());
        browser.set_data<GLib.PtrArray*> ("history-list-tab-history-new",
                                          new GLib.PtrArray ());
        foreach (var tab in browser.get_tabs ())
            tab_added (browser, tab);
        browser.add_tab.connect (tab_added);
        browser.remove_tab.connect (tab_removed);
        browser.notify["tab"].connect (this.tab_changed);
    }
    void browser_removed (Midori.Browser browser) {
        string[] callbacks = { "HistoryListNextTab", "HistoryListPreviousTab" };

        Gtk.ActionGroup action_group;
        action_group = browser.get_action_group ();
        for (int i = 0; i < callbacks.length; i++) {
            Gtk.Action action = action_group.get_action (callbacks[i]);
            if (action != null)
                action_group.remove_action (action);
        }

        browser.add_tab.disconnect (tab_added);
        browser.remove_tab.disconnect (tab_removed);
        browser.notify["tab"].disconnect (this.tab_changed);
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
    }
    void tab_changed (GLib.Object window, GLib.ParamSpec pspec) {
        Midori.Browser browser = window as Midori.Browser;
        Midori.View view = null;
        browser.get ("tab", ref view);

        unowned GLib.PtrArray list = browser.get_data<GLib.PtrArray> ("history-list-tab-history");
        unowned GLib.PtrArray list_new = browser.get_data<GLib.PtrArray> ("history-list-tab-history-new");
        list.remove (view);
        list_new.remove (view);
        list.add (view);
    }
    void activated (Midori.App app) {
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
    internal HistoryList () {
        GLib.Object (name: _("History List"),
                     description: _("Switch tabs with Ctrl+Tab sorted by last usage"),
                     version: "0.2",
                     authors: "André Stösel <Midori-Plugin@PyIT.de>");
        activate.connect (activated);
        deactivate.connect (deactivated);
    }
}

public Midori.Extension extension_init () {
    return new HistoryList ();
}

