/*
 Copyright (C) 2015 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class Window : Gtk.Window {
        Gtk.Widget? _toolbar = null;
        public Gtk.Widget? toolbar { get {
            if (_toolbar == null) {
#if HAVE_GTK3
                if (strcmp (Environment.get_variable ("GTK_CSD"), "1") == 0) {
                    var toolbar = new Gtk.HeaderBar ();
                    toolbar.show_close_button = true;
                    toolbar.show ();
                    toolbar.get_style_context ().add_class ("midori-titlebar");
                    _toolbar = toolbar;
                    return _toolbar;
                }
#endif
                var toolbar = new Gtk.Toolbar ();
                toolbar.show_arrow = true;
#if HAVE_GTK3
                toolbar.get_style_context ().add_class ("primary-toolbar");
                hide_titlebar_when_maximized = true;
#endif
                toolbar.popup_context_menu.connect ((x, y, button) => {
                    return button == 3 && context_menu (toolbar); });
                _toolbar = toolbar;
            }
            return _toolbar;
        } }

        public string actions { get; set; default = ""; }
        string extra_actions { get; set; default = ""; }
        List<Gtk.ActionGroup> action_groups;
        public signal bool context_menu (Gtk.Widget widget, Gtk.Action? action=null);
        Gtk.Box? box = null;
        List<Gtk.Widget> toolbars;

        Gtk.Widget? _contents = null;
        public Gtk.Widget? contents { get {
            return _contents;
        } set {
            if (_contents != null)
                box.remove (_contents);
            _contents = value;
            _contents.show ();
            if (box != null)
                box.pack_end (_contents, true, true, 0);
        } }

        public void add_action_group (Gtk.ActionGroup action_group) {
            action_groups.append (action_group);
        }

        public bool show_menubar { get; set; default = false; }

        [CCode (type = "GtkWidget*")]
        public Window () {
        }

        public Gtk.ToolItem? get_tool_item (string name) {
            /* Name is the empty string if actions has ,, or trailing , */
            if (name == "")
                return null;
            /* Shown in the notebook, no need to include in the toolbar */
            if (name == "TabNew")
                return null;
            foreach (unowned Gtk.ActionGroup action_group in action_groups) {
                var action = action_group.get_action (name);
                if (action != null) {
                    return create_tool_item (action);
                }
            }
            warning ("Action %s not known to Window", name);
            return null;
        }

        Gtk.ToolItem create_tool_item (Gtk.Action action) {
            var toolitem = (Gtk.ToolItem)action.create_tool_item ();
            /* Show label if button has no icon of any kind */
            if (action.icon_name == null && action.stock_id == null && action.gicon == null)
                toolitem.is_important = true;
            toolitem.get_child ().button_press_event.connect ((event) => {
                return event.button == 3 && context_menu (toolitem, action); });
            if (action.name == "CompactMenu")
            {
                toolitem.visible = !show_menubar;
                bind_property ("show-menubar", toolitem, "visible",
                    BindingFlags.DEFAULT | BindingFlags.INVERT_BOOLEAN);
            }
            return toolitem;
        }

        /**
         * Adds an action to the (browser) window.
         * Typically it will be displayed in the primary toolbar or headerbar.
         *
         * If @action is a ContextAction a menu will be displayed.
         *
         * Since: 0.6.0
         **/
        public void add_action (Gtk.Action action) {
            action_groups.nth_data (0).add_action (action);
            extra_actions += "," + action.name;
            update_toolbar ();
        }

        /**
         * Remove an action from the (browser) window.
         *
         * Since: 0.6.0
         **/
        public void remove_action (Gtk.Action action) {
            action_groups.nth_data (0).remove_action (action);
            extra_actions = extra_actions.replace ("," + action.name, "");
            update_toolbar ();
        }

        void update_toolbar () {
            var container = (Gtk.Container)_toolbar;
            foreach (unowned Gtk.Widget toolitem in container.get_children ())
                container.remove (toolitem);

            /* Always include app menu (visible depending on menubar) and extensions. */
            string all_actions;
            if ("CompactMenu" in actions)
                all_actions = actions.replace ("CompactMenu", extra_actions + ",CompactMenu");
            else
                all_actions = actions + "," + extra_actions + ",CompactMenu";
            string[] names = all_actions.split (",");

#if HAVE_GTK3
            var headerbar = _toolbar as Gtk.HeaderBar;
            if (headerbar != null) {
                var tail = new List<Gtk.ToolItem> ();
                foreach (unowned string name in names) {
                    var toolitem = get_tool_item (name);
                    if (toolitem == null)
                        continue;
                    var widget = toolitem.get_child ();
                    if (widget is Gtk.Alignment)
                        widget = ((Gtk.Bin)widget).get_child ();
                    if (name == "Location") {
                        widget.set ("margin-top", 1, "margin-bottom", 1);
                        ((Gtk.Entry)widget).width_chars = 36;
                        headerbar.custom_title = toolitem;
                        headerbar.custom_title.set (
                            "margin-start", 25, "margin-end", 25,
                            "margin-top", 5, "margin-bottom", 5);
                    } else if (name == "Search") {
                        ((Gtk.Entry)widget).width_chars = 12;
                        tail.append (toolitem);
                    } else if (all_actions.index_of (name) > all_actions.index_of ("Location"))
                        tail.append (toolitem);
                    else
                        headerbar.pack_start (toolitem);
                }

                /* Pack end appends, so we need to pack in reverse order */
                tail.reverse ();
                foreach (unowned Gtk.ToolItem toolitem in tail)
                    headerbar.pack_end (toolitem);

                set_titlebar (headerbar);
                return;
            }
#endif

            var toolbar = (Gtk.Toolbar)_toolbar;
            string? previous = null;
            Gtk.ToolItem? toolitem_previous = null;
            foreach (unowned string name in names) {
                var toolitem = get_tool_item (name);
                if (toolitem == null)
                    continue;
                if ((name == "Location" || name == "Search")
                 && (previous == "Location" || previous == "Search")) {
                    toolitem_previous.ref ();
                    toolbar.remove (toolitem_previous);
                    var paned = new Midori.PanedAction ();
                    paned.set_child1 (toolitem_previous, previous, previous != "Search", true);
                    paned.set_child2 (toolitem, name, name != "Search", true);
                    /* Midori.Settings.search-width on Midori.Browser.settings */
                    Midori.Settings? settings = null;
                    get ("settings", ref settings);
                    var sizeable = name == "Search" ? toolitem : toolitem_previous;
                    sizeable.size_allocate.connect ((allocation) => {
                        settings.set ("search-width", allocation.width);
                    });
                    var requester = previous == "Search" ? toolitem_previous : toolitem;
                    requester.set_size_request (settings.search_width, -1);
                    toolitem = (Gtk.ToolItem)paned.create_tool_item ();
                    previous = null;
                    toolitem_previous.unref ();
                    toolitem_previous = null;
                } else {
                    previous = name;
                    toolitem_previous = toolitem;
                }
                toolbar.insert (toolitem, -1);
            }
        }

        public void add_toolbar (Gtk.Widget toolbar) {
            var _toolbar = toolbar as Gtk.Toolbar;
            if (_toolbar != null) {
#if HAVE_GTK3
                get_style_context ().add_class ("secondary-toolbar");
#endif
                _toolbar.popup_context_menu.connect ((x, y, button) => {
                    return button == 3 && context_menu (toolbar);
                });
            }
            if (box == null)
                toolbars.append (toolbar);
            else
                box.pack_start (toolbar, false, false);
        }

        construct {
            box = new Gtk.VBox (false, 0);
            box.show ();
            add (box);
            foreach (unowned Gtk.Widget toolbar in toolbars) {
                if (toolbar is Gtk.MenuBar)
                    box.pack_start (toolbar, false, false);
            }
            if (toolbar is Gtk.Toolbar)
                box.pack_start (toolbar, false, false);
            foreach (unowned Gtk.Widget toolbar in toolbars) {
                if (!(toolbar is Gtk.MenuBar))
                    box.pack_start (toolbar, false, false);
            }
            if (_contents != null)
                box.pack_end (_contents, true, true, 0);
            if (actions != "")
                update_toolbar ();
            notify["actions"].connect ((pspec) => { update_toolbar (); });
        }
    }
}
