/*
   Copyright (C) 2013 Paweł Forysiuk <tuxator@o2.pl>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   See the file COPYING for the full license text.
*/

using Gtk;
using Midori;
using WebKit;
using Sqlite;

namespace ClipNotes {

        private class Sidebar : Gtk.VBox, Midori.Viewable {
        Gtk.Toolbar? toolbar = null;
        Gtk.Label note_label;
        Gtk.TreeView notes_tree_view;
        Gtk.TextView note_text_view = new Gtk.TextView ();
        Gtk.ListStore notes_list_store = new Gtk.ListStore (2, typeof (string), typeof (string));

        public unowned string get_stock_id () {
            return Gtk.STOCK_EDIT;
        }

        public unowned string get_label () {
            return _("Notes");
        }

        public Gtk.Widget get_toolbar () {
            if (toolbar == null) {
                toolbar = new Gtk.Toolbar ();
                var new_note_button = new Gtk.ToolButton.from_stock (Gtk.STOCK_EDIT);
                new_note_button.label = _("New Note");
                new_note_button.tooltip_text = _("Creates a new empty note, urelated to opened pages");
                new_note_button.use_underline  = true;
                new_note_button.is_important = true;
                new_note_button.show ();
                new_note_button.clicked.connect (() => {
                    stdout.printf ("IMPLEMENT ME: would add new empty note INSERT INTO NOTES ....\n");
                });
                toolbar.insert (new_note_button, -1);
            } // if (toolbar != null)
            return toolbar;
        } // get_toolbar

        public Sidebar () {
            Gtk.TreeViewColumn column;
            Gtk.TreeIter iter;

            notes_tree_view = new Gtk.TreeView.with_model (notes_list_store);
            notes_tree_view.headers_visible = true;

            column = new Gtk.TreeViewColumn ();
            Gtk.CellRendererText renderer_title = new Gtk.CellRendererText ();
            column.set_title (_("Notes"));
            column.pack_start (renderer_title, true);
            column.set_cell_data_func (renderer_title, on_renderer_note_title);
            notes_tree_view.append_column (column);


            notes_list_store.append (out iter);
            notes_list_store.set (iter, 0, "test note nr1");
            notes_list_store.append (out iter);
            notes_list_store.set (iter, 0, "test note nr2");


            notes_tree_view.show ();
            pack_start (notes_tree_view, false, false, 0);

            note_label = new Gtk.Label (null);
            note_label.set_text (_("Note clipped from: some_uri"));
            note_label.show ();
            pack_start (note_label, false, false, 0);

            note_text_view.show ();
            pack_start (note_text_view, true, true, 0);
        } // Sidebar()

        private void on_renderer_note_title (Gtk.CellLayout column, Gtk.CellRenderer renderer,
            Gtk.TreeModel model, Gtk.TreeIter iter) {

            string note_title;
            model.get (iter, 0, out note_title);
            renderer.set ("text", note_title,
                "ellipsize", Pango.EllipsizeMode.END);
        } // on_renderer_note_title
    } // Sidebar

    private class Manager : Midori.Extension {
            internal GLib.List<Gtk.Widget> widgets;

        void tab_added (Midori.Browser browser, Midori.Tab tab) {

            tab.context_menu.connect (add_menu_items);

        } // tab_added

        void note_add_new (string title, string uri, string note_content)
        {
//            GLib.DateTime time = new DateTime.now_local ();
            stdout.printf ("add note title %s uri %s note: %s \n", title, uri, note_content);
        }

        void add_menu_items (Midori.Tab tab, WebKit.HitTestResult hit_test_result, Midori.ContextAction menu) {
            if ((hit_test_result.context & WebKit.HitTestResultContext.SELECTION) == 0)
                return;

            var view = tab as Midori.View;
            var action = new Gtk.Action ("Notes", _("Copy text to note"), null, null);
            action.activate.connect ((action)=> {
                if (view.has_selection () == true)
                {
                    string selected_text = view.get_selected_text ();
                    string uri = view.get_display_uri ();
                    string title = view.get_display_title ();
                    note_add_new (title, uri, selected_text);
                }
            });

            menu.add (action);
        } // add_menu_items

        void browser_added (Midori.Browser browser) {
            var viewable = new Sidebar ();
            viewable.show ();
            browser.panel.append_page (viewable);
            widgets.append (viewable);

            foreach (var tab in browser.get_tabs ())
                tab_added (browser, tab);

            browser.add_tab.connect (tab_added);
        } // browser_added

        void activated (Midori.App app) {
            widgets = new GLib.List<Gtk.Widget> ();
            foreach (var browser in app.get_browsers ())
                browser_added (browser);
        } // activated

        void deactivated () {
            var app = get_app ();
            app.add_browser.disconnect (browser_added);
            foreach (var widget in widgets)
                widget.destroy ();
        } // deactivated

        internal Manager () {
            GLib.Object (name: _("Notes"),
                         description: _("Save text clips from websites as notes"),
                         version: "0.1" + Midori.VERSION_SUFFIX,
                         authors: "Paweł Forysiuk");

            this.activate.connect (activated);
            this.deactivate.connect (deactivated);
        } // Manager ()
    } // Manager : Midori.Extension
} // namespace Clipnotes

public Midori.Extension extension_init () {
    return new ClipNotes.Manager ();
}
