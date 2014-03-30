/*
 Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    protected class Tally : Gtk.EventBox {
        public Midori.Tab tab { get; set; }
        Gtk.Spinner spinner;
        public Gtk.Label label;
        Gtk.HBox box;
        public Gtk.Image icon;
        Gtk.Alignment align;
        Gtk.Button close;

        public bool close_button_left { get; set; default = false; }
        public bool close_button_visible { get; set; default = false; }

        protected Tally (Midori.Tab tab) {
            this.tab = tab;
            box = new Gtk.HBox (false, 1);
            add (box);

            spinner = new Gtk.Spinner ();
            spinner.active = true;
            /* Ensure the spinner is the size of the icon */
            int icon_size = 16;
            Gtk.icon_size_lookup_for_settings (get_settings (),
                Gtk.IconSize.MENU, out icon_size, null);
            spinner.set_size_request (icon_size, icon_size);
            box.pack_start (spinner, false, false, 0);
            label = new Gtk.Label (null);
            label.set_alignment (0.5f, 0.5f);
            label.set_padding (0, 0);
            box.pack_start (label, true, true, 0);
            close = new Gtk.Button ();
            close.relief = Gtk.ReliefStyle.NONE;
            close.focus_on_click = false;
#if !HAVE_GTK3
            close.name = "midori-close-button";
            close.style_set.connect (close_style_set);
#endif
            icon = new Gtk.Image.from_gicon (new ThemedIcon.with_default_fallbacks ("window-close-symbolic"), Gtk.IconSize.MENU);
            close.add (icon);
            align = new Gtk.Alignment (1.0f, 0.5f, 0.0f, 0.0f);
            align.add (close);
            box.pack_start (align, false, false, 0);
            close.clicked.connect (close_clicked);
            icon = new Gtk.Image.from_gicon (new ThemedIcon.with_default_fallbacks ("text-html-symbolic"), Gtk.IconSize.MENU);
            box.pack_start (icon, false, false, 0);
            box.show_all ();

            tab.notify["uri"].connect (uri_changed);
            tab.notify["title"].connect (title_changed);
            tab.notify["icon"].connect (icon_changed);
            tab.notify["minimized"].connect (minimized_changed);
            tab.notify["progress"].connect (progress_changed);
            tab.colors_changed.connect (colors_changed);
            update_label ();
            label.visible = !tab.minimized;
            spinner.visible = tab.progress > 0.0;
            icon.visible = !spinner.visible;
            update_color ();

            notify["close-button-left"].connect (close_button_left_changed);
            notify_property ("close-button-left");
            notify["close-button-visible"].connect (close_button_visible_changed);
            notify_property ("close-button-visible");
        }

#if !HAVE_GTK3
        void close_style_set (Gtk.Style? previous_style) {
            Gtk.Requisition size;
            close.child.size_request (out size);
            close.set_size_request (size.width, size.height);
        }
#endif

        void close_clicked () {
            tab.destroy ();
        }

        void uri_changed (GLib.ParamSpec pspec) {
            label.label = tab.uri;
        }

        void title_changed (GLib.ParamSpec pspec) {
            update_label ();
        }

        void update_label () {
            string? title;
            tab.get ("title", out title);
            label.label = Midori.Tab.get_display_title (title, tab.uri);
            /* Use computed label below! */
            label.ellipsize = Midori.Tab.get_display_ellipsize (label.label, tab.uri);
            tooltip_text = label.label;
        }

        void icon_changed (GLib.ParamSpec pspec) {
            Icon? icon;
            tab.get ("icon", out icon);
            this.icon.set_from_gicon (icon, Gtk.IconSize.MENU);
        }

        void colors_changed () {
            update_color ();
        }

        void update_color () {
            visible_window = tab.fg_color != null || tab.bg_color != null;
            label.modify_fg (Gtk.StateType.NORMAL, tab.fg_color);
            label.modify_fg (Gtk.StateType.ACTIVE, tab.fg_color);
            modify_bg (Gtk.StateType.NORMAL, tab.bg_color);
            modify_bg (Gtk.StateType.ACTIVE, tab.bg_color);
        }

        void close_button_left_changed (GLib.ParamSpec pspec) {
            if (close_button_left) {
                box.reorder_child (align, 0);
                box.reorder_child (label, 1);
                box.reorder_child (icon, 2);
                box.reorder_child (spinner, 3);
            } else {
                box.reorder_child (spinner, 0);
                box.reorder_child (icon, 1);
                box.reorder_child (label, 2);
                box.reorder_child (align, 3);
            }
        }

        void close_button_visible_changed (GLib.ParamSpec pspec) {
            align.visible = !tab.minimized && close_button_visible;
        }

        void minimized_changed (GLib.ParamSpec pspec) {
            label.visible = !tab.minimized;
            notify_property ("close-button-visible");
        }

        void progress_changed (GLib.ParamSpec pspec) {
            spinner.visible = tab.progress > 0.0;
            icon.visible = !spinner.visible;
        }
    }

    public class Notebook : Gtk.EventBox {
        public Gtk.Notebook notebook;
        int last_tab_size = 0;

#if !HAVE_GTK3
        static const string style_fixup = """
            style "midori-close-button-style"
            {
            GtkWidget::focus-padding = 0
            GtkWidget::focus-line-width = 0
            xthickness = 0
            ythickness = 0
            }
            widget "*.midori-close-button" style "midori-close-button-style"
            """;
#endif

        /* Since: 0.5.7 */
        public uint count { get; private set; default = 0; }
        /* Since: 0.5.7 */
        public int index { get; set; default = -1; }
        /* Since: 0.5.7 */
        public Midori.Tab? tab {  get; set; default = null; }
        /* Since: 0.5.7 */
        private Midori.Tab? previous {  get; set; default = null; }

        /* Since: 0.5.7 */
        public bool close_buttons_left { get; set; default = true; }
        /* Since: 0.5.7 */
        public bool close_buttons_visible { get; set; default = true; }
        /* Since: 0.5.7 */
        public bool labels_visible { get; set; default = true; }

        /* Since: 0.5.7 */
        public signal void tab_context_menu (Midori.Tab tab, ContextAction menu);
        /* Since: 0.5.7 */
        public signal void context_menu (ContextAction menu);
        /* The current tab is about to switch, but the old tab still has focus.
           Since: 0.5.7 */
        public signal void tab_switched (Midori.Tab? old, Midori.Tab @new);
        /* A tab is about to move to a new position.
           Since: 0.5.7 */
        public signal void tab_moved (Midori.Tab tab, uint new_index);
        /* A tab is being dragging out of the window.
           Since: 0.5.7 */
        public signal void tab_detached (Midori.Tab tab, int x, int y);
        /* Since: 0.5.7 */
        public signal void new_tab ();

        [CCode (type = "GtkWidget*")]
        public Notebook () {
            visible_window = false;
            notebook = new Gtk.Notebook ();
            notebook.visible = notebook.scrollable = true;
            notebook.show_border = false;
            notebook.set ("group-name", PACKAGE_NAME);
            add (notebook);

#if HAVE_GTK3
            get_style_context ().add_class ("dynamic-notebook");
#else
            /* Remove the inner border between scrollbars and window border */
            Gtk.RcStyle rcstyle = new Gtk.RcStyle ();
            rcstyle.xthickness = 0;
            notebook.modify_style (rcstyle);
            Gtk.rc_parse_string (style_fixup);
#endif
            notify["index"].connect (index_changed);
            notify["tab"].connect (tab_changed);
            notify["labels-visible"].connect (labels_visible_changed);
            notify["close-buttons-visible"].connect (close_buttons_visible_changed);
            notify["close-buttons-left"].connect (close_buttons_left_changed);

            notebook.size_allocate.connect (size_allocated);
            notebook.switch_page.connect (page_switched);
            notebook.page_reordered.connect (page_moved);
            notebook.create_window.connect (window_created);

            var add = new Gtk.Button ();
            add.tooltip_text = _("Open a new tab");
            add.relief = Gtk.ReliefStyle.NONE;
            add.add (new Gtk.Image.from_gicon (new ThemedIcon.with_default_fallbacks ("tab-new-symbolic"), Gtk.IconSize.MENU));
            add.show_all ();
            notebook.set_action_widget (add, Gtk.PackType.START);
            add.clicked.connect (()=>{
                new_tab ();
            });
            take_incoming_uris (add);

            button_press_event.connect (button_pressed);
        }

        void take_incoming_uris (Gtk.Widget widget) {
            Gtk.drag_dest_set (widget, Gtk.DestDefaults.ALL, (Gtk.TargetEntry[])null, Gdk.DragAction.COPY);
            Gtk.drag_dest_add_text_targets (widget);
            Gtk.drag_dest_add_uri_targets (widget);
            widget.drag_drop.connect (uri_dropped);
            widget.drag_data_received.connect (uri_received);
        }

        bool uri_dropped (Gtk.Widget widget, Gdk.DragContext context, int x, int y, uint timestamp) {
            Gtk.drag_finish (context, false, false, timestamp);
            return true;
        }

        void uri_received (Gtk.Widget widget, Gdk.DragContext context, int x, int y, Gtk.SelectionData data, uint ttype, uint timestamp) {
            string[] uri = data.get_uris ();
            string drag_uri = uri != null ? uri[0] : data.get_text ();
            Midori.Tab drag_tab;
            if (widget is Tally)
                drag_tab = (widget as Tally).tab;
            else {
                new_tab ();
                // Browser will have focussed the new tab
                drag_tab = tab;
            }
            drag_tab.web_view.load_uri (drag_uri);
        }


        ~Notebook () {
            notebook.size_allocate.disconnect (size_allocated);
            notebook.switch_page.disconnect (page_switched);
            notebook.page_reordered.disconnect (page_moved);
            notebook.create_window.disconnect (window_created);
        }

        /* Since: 0.5.8 */
        public ContextAction get_context_action () {
            var menu = new Midori.ContextAction ("NotebookContextMenu", null, null, null);
            uint counter = 0;
            foreach (var child in notebook.get_children ()) {
                var tab = child as Midori.Tab;
                var tally = notebook.get_tab_label (tab) as Tally;
                var action = new Midori.ContextAction.escaped ("Tab%u".printf (counter), tally.label.label, null, null);
                action.gicon = tally.icon.gicon;
                action.activate.connect (()=>{
                    notebook.set_current_page (notebook.page_num (tab));
                });
                menu.add (action);
                counter++;
            }
            context_menu (menu);
            return menu;
        }

        bool button_pressed (Gdk.EventButton event) {
            /* Propagate events in logical label area */
            foreach (var child in notebook.get_children ()) {
                var tally = notebook.get_tab_label (tab) as Tally;
                Gtk.Allocation size;
                tally.get_allocation (out size);
                if (tally.get_mapped ()
                 && event.x_root >= size.x
                 && event.x_root <= (size.x + size.width)) {
                    tally.button_press_event (event);
                    return true;
                }
            }

            if (event.type == Gdk.EventType.2BUTTON_PRESS && event.button == 1
             || event.button == 2) {
                new_tab ();
                return true;
            }
            else if (event.button == 3) {
                var menu = get_context_action ();
                var popup = menu.create_menu (null, false);
                popup.show ();
                popup.attach_to_widget (this, null);
                popup.popup (null, null, null, event.button, event.time);
                return true;
            }
            return false;
        }

        public void insert (Midori.Tab tab, int index) {
            var tally = new Tally (tab);
            tally.close_button_left = close_buttons_left;
            tally.close_button_visible = close_buttons_visible;
            tally.button_press_event.connect (tab_button_pressed);
            tally.show ();
            tally.set_size_request (tab.minimized ? -1 : last_tab_size, -1);
            take_incoming_uris (tally);

            /* Minimum requirements for any tab */
            tab.can_focus = tab.visible = true;
            notebook.insert_page (tab, tally, index);
            notebook.set_tab_reorderable (tab, true);
            notebook.set_tab_detachable (tab, true);
            tab.destroy.connect (tab_removed);
            tab.notify["minimized"].connect (tab_minimized);
            count++;
            tab.ref ();
            relayout ();
        }

        void tab_removed () {
            count--;
            if (count > 0)
                relayout ();
        }

        void relayout () {
            Gtk.Allocation size;
            notebook.get_allocation (out size);
            resize (size.width);
        }

        /* Since: 0.5.8 */
        public ContextAction get_tab_context_action (Midori.Tab tab) {
            var menu = new Midori.ContextAction ("TabContextMenu", null, null, null);
            tab_context_menu (tab, menu);
            var action_window = new Midori.ContextAction ("TabWindowNew", _("Open in New _Window"), null, "window-new");
            action_window.activate.connect (()=>{
                tab_detached (tab, 128, 128);
            });
            menu.add (action_window);
            var action_minimize = new Midori.ContextAction ("TabMinimize", tab.minimized ? _("Show Tab _Label") : _("Show Tab _Icon Only"), null, null);
            action_minimize.activate.connect (()=>{
                tab.minimized = !tab.minimized;
            });
            menu.add (action_minimize);
            var action_right = new Midori.ContextAction ("TabCloseRight", ngettext ("Close Tab to the R_ight", "Close Tabs to the R_ight", count - 1), null, null);
            action_right.sensitive = count > 1;
            action_right.activate.connect (()=>{
                bool found_tab = false;
                foreach (var child in notebook.get_children ()) {
                    if (found_tab)
                        child.destroy ();
                    else
                        found_tab = child == tab;
                }
            });
            menu.add (action_right);
            var action_other = new Midori.ContextAction ("TabCloseOther", ngettext ("Close Ot_her Tab", "Close Ot_her Tabs", count - 1), null, null);
            action_other.sensitive = count > 1;
            action_other.activate.connect (()=>{
                foreach (var child in notebook.get_children ())
                    if (child != tab)
                        child.destroy ();
            });
            menu.add (action_other);
            var action_close = new Midori.ContextAction ("TabClose", null, null, Gtk.STOCK_CLOSE);
            action_close.activate.connect (()=>{
                tab.destroy ();
            });
            menu.add (action_close);
            return menu;
         }

        bool tab_button_pressed (Gtk.Widget label, Gdk.EventButton event) {
            Tally tally = label as Tally;
            if (event.button == 1) {
                /* Leave switching and dragging up to the notebook */
                return false;
            } else if (event.button == 2)
                tally.tab.destroy ();
            else if (event.button == 3) {
                var menu = get_tab_context_action (tally.tab);
                var popup = menu.create_menu (null, false);
                popup.show ();
                popup.attach_to_widget (this, null);
                popup.popup (null, null, null, event.button, event.time);
            }
            return true;
        }

        public void move (Midori.Tab tab, int index) {
            notebook.reorder_child (tab, index);
        }

        /* Chain up drawing manually to circumvent parent checks */
#if HAVE_GTK3
        public override bool draw (Cairo.Context cr) {
            notebook.draw (cr);
            return true;
        }
#else
        public override bool expose_event (Gdk.EventExpose event) {
            notebook.expose_event (event);
            return true;
        }
#endif

        public override void forall_internal (bool include_internal, Gtk.Callback callback) {
            if (include_internal)
                callback (notebook);
            foreach (var child in notebook.get_children ())
                callback (child);
        }

        /* Can't override Gtk.Container.remove because it checks the parent */
        public new void remove (Midori.Tab tab) {
            return_if_fail (notebook.get_children ().find (tab) != null);

            notebook.remove (tab);
            tab.destroy.disconnect (tab_removed);
            tab.notify["minimized"].disconnect (tab_minimized);
            tab_removed ();
            tab.unref ();
        }

        void tab_minimized (GLib.ParamSpec pspec) {
            var tally = notebook.get_tab_label (tab) as Tally;
            tally.set_size_request (tab.minimized ? -1 : last_tab_size, -1);
        }

        public Midori.Tab get_nth_tab (int index) {
            return notebook.get_nth_page (index) as Midori.Tab;
        }

        public int get_tab_index (Midori.Tab tab) {
            return notebook.page_num (tab);
        }

        void index_changed (GLib.ParamSpec pspec) {
            notebook.set_current_page (index);
        }

        void tab_changed (GLib.ParamSpec pspec) {
            notebook.set_current_page (notebook.page_num (tab));
        }

        void labels_visible_changed (GLib.ParamSpec pspec) {
            notebook.show_tabs = labels_visible;
        }

        void close_buttons_visible_changed (GLib.ParamSpec pspec) {
            foreach (var child in notebook.get_children ()) {
                var tally = notebook.get_tab_label (child) as Tally;
                tally.close_button_visible = close_buttons_visible;
            }
        }

        void close_buttons_left_changed (GLib.ParamSpec pspec) {
            foreach (var child in notebook.get_children ()) {
                var tally = notebook.get_tab_label (child) as Tally;
                tally.close_button_left = close_buttons_left;
            }
        }

#if HAVE_GTK3
        void size_allocated (Gtk.Allocation allocation) {
#else
        void size_allocated (Gdk.Rectangle allocation) {
#endif
            if (labels_visible && count > 0)
                resize (allocation.width);
        }

#if HAVE_GTK3
        void page_switched (Gtk.Widget new_tab, uint new_index) {
#else
        void page_switched (Gtk.NotebookPage new_tab, uint new_index) {
#endif
            tab_switched (previous, new_tab as Tab);
            previous = (Midori.Tab)new_tab;

            notify["index"].disconnect (index_changed);
            notify["tab"].disconnect (tab_changed);
            index = (int)new_index;
            tab = (Midori.Tab)new_tab;
            notify["index"].connect (index_changed);
            notify["tab"].connect (tab_changed);
        }

        void page_moved (Gtk.Widget moving_tab, uint new_index) {
            tab_moved (moving_tab as Midori.Tab, new_index);
            /* Indices change, current tab is not in the same position */
            notify["index"].disconnect (index_changed);
            index = (int)get_tab_index (tab);
            notify["index"].connect (index_changed);
        }

        unowned Gtk.Notebook window_created (Gtk.Widget tab, int x, int y) {
            tab_detached (tab as Tab, x, y);
            /* The API allows now, the cast is due to bindings not having ? */
            return (Gtk.Notebook)null;
        }

        void resize (int new_size) {
            int n = int.max (1, (int)count);
            new_size /= n;
            int icon_size = 16;
            Gtk.icon_size_lookup_for_settings (get_settings (),
                Gtk.IconSize.MENU, out icon_size, null);
            int max_size = 150;
            int min_size = icon_size;
            if (close_buttons_visible)
                min_size += icon_size;
            new_size = new_size.clamp (min_size, max_size);
            if ((new_size - last_tab_size).abs () < 3)
                return;

            last_tab_size = new_size;
            foreach (var child in notebook.get_children ()) {
                var tab = child as Midori.Tab;
                var tally = notebook.get_tab_label (child) as Tally;
                tally.set_size_request (tab.minimized ? -1 : last_tab_size, -1);
            }
        }
    }
}
