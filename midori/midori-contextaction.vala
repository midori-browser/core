/*
 Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    /* A context action represents an item that can be shown in a menu
       or toolbar. Context actions can be nested as needed.
       Since: 0.5.5 */
    public class ContextAction : Gtk.Action {
        List<Gtk.ActionGroup> action_groups;
        List<Gtk.Action> children;
        public ContextAction (string name, string? label, string? tooltip, string? stock_id) {
            GLib.Object (name: name, label: label, tooltip: tooltip, stock_id: stock_id);
            action_groups = new List<Gtk.ActionGroup> ();
            children = new List<ContextAction> ();
        }

        /*
           The action label will be escaped for mnemonics so for example
           "a_fairy_tale" will not get accel keys on "f" or "t".

           Since: 0.5.8
         */
        public ContextAction.escaped (string name, string label, string? tooltip, string? stock_id) {
            string? escaped_label = label.replace ("_", "__");
            GLib.Object (name: name, label: escaped_label, tooltip: tooltip, stock_id: stock_id);
            action_groups = new List<Gtk.ActionGroup> ();
            children = new List<ContextAction> ();
        }

        public delegate void ActionActivateCallback (Gtk.Action action);
        public void add_simple (string name, string? label, string? tooltip, string? stock_id, ActionActivateCallback callback) {
            var action = new ContextAction (name, label, tooltip, stock_id);
            action.activate.connect (() => { callback (action); });
            add (action);
        }

        public void add (Gtk.Action? action) {
            if (action == null) {
                add (new SeparatorContextAction ());
                return;
            }

            children.append (action);
            if (action is ContextAction) {
                foreach (var action_group in action_groups)
                    (action as ContextAction).add_action_group (action_group);
            }
        }

        public void add_action_group (Gtk.ActionGroup action_group) {
            action_groups.append (action_group);
        }

        public void add_by_name (string name) {
            foreach (var action_group in action_groups) {
                var action = action_group.get_action (name);
                if (action != null) {
                    add (action);
                    return;
                }
            }
            warning ("Action %s not known to ContextAction", name);
        }

#if HAVE_WEBKIT2
        public WebKit.ContextMenu create_webkit_context_menu (WebKit.ContextMenu? default_menu) {
            var menu = default_menu ?? new WebKit.ContextMenu ();
            foreach (var action in children) {
                WebKit.ContextMenuItem menuitem;
                if (action is SeparatorContextAction)
                    menuitem = new WebKit.ContextMenuItem.separator ();
                else if (action is ContextAction
                 && (action as ContextAction).children.nth_data (0) != null) {
                    menuitem = new WebKit.ContextMenuItem (action);
                    menuitem.set_submenu ((action as ContextAction).create_webkit_context_menu (null));
                }
                else
                    menuitem = new WebKit.ContextMenuItem (action);
                // Visibility of the action is ignored, so we skip hidden items
                if (action.visible)
                    menu.append (menuitem);
            }
            return menu;
        }
#endif

        Gtk.ToolButton toolitem;
        public override unowned Gtk.Widget create_tool_item () {
            toolitem = base.create_tool_item () as Gtk.ToolButton;
            toolitem.clicked.connect (() => {
                var popup = create_menu (null, false);
                popup.show ();
                popup.attach_to_widget (toolitem, null);
                popup.popup (null, null, null, 1, Gtk.get_current_event_time ());
            });
            return toolitem;
        }

        public new Gtk.Menu create_menu (Gtk.Menu? default_menu, bool accels) {
            var menu = default_menu ?? new Gtk.Menu ();
            foreach (var action in children) {
                Gtk.MenuItem menuitem;
                if (action is SeparatorContextAction) {
                    menuitem = new Gtk.SeparatorMenuItem ();
                    menuitem.show ();
                }
                else if (action is ContextAction
                 && (action as ContextAction).children.nth_data (0) != null) {
                    menuitem = action.create_menu_item () as Gtk.MenuItem;
                    menuitem.submenu = (action as ContextAction).create_menu (null, accels);
                }
                else
                    menuitem = action.create_menu_item () as Gtk.MenuItem;
                /* Disable accels from the action in context menus */
                if (!accels) {
                    var accel_label = menuitem.get_child () as Gtk.AccelLabel;
                    if (accel_label != null)
                        accel_label.accel_closure = null;
                }
                menu.append (menuitem);
            }
            return menu;
        }

        public Gtk.Action? get_by_name (string name) {
            foreach (var action in children)
                if (action.name == name)
                    return action;
            return null;
        }
    }

    public class SeparatorContextAction : ContextAction {
        public SeparatorContextAction () {
            GLib.Object (name: "SeparatorContextAction", label: null, tooltip: null, stock_id: null);
        }
    }
}
