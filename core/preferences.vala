/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public interface PreferencesActivatable : Object {
        public abstract Preferences preferences { owned get; set; }
        public abstract void activate ();
    }

    public class LabelWidget : Gtk.Box {
        public string title { get; construct set; }
        public Gtk.Widget? widget { get; construct set; }

        public LabelWidget (string title, Gtk.Widget? widget=null) {
            Object (title: title, widget: widget);
        }

        construct {
            bool header = widget == null;
            orientation = header ? Gtk.Orientation.VERTICAL : Gtk.Orientation.HORIZONTAL;
            halign = Gtk.Align.START;
            var label = new Gtk.Label.with_mnemonic (header ? "<b>%s</b>".printf (title) : title);
            label.use_markup = header;
            label.halign = Gtk.Align.START;
            pack_start (label, false, false, 4);
            if (widget != null) {
                label.mnemonic_widget = widget;
                set_center_widget (widget);
            }
        }
    }

    [GtkTemplate (ui = "/ui/preferences.ui")]
    public class Preferences : Gtk.Window {
        [GtkChild]
        Gtk.StackSwitcher switcher;
        [GtkChild]
        Gtk.Box content_box;
        [GtkChild]
        Gtk.Stack categories;

        public Preferences (Gtk.Window parent) {
            Object (transient_for: parent);
        }

        construct {
            // Make headerbar (titlebar) the topmost bar if CSD is disabled
            if (!get_settings ().gtk_dialogs_use_header) {
                switcher.ref ();
                set_titlebar (null);
                content_box.pack_start (switcher, false, false);
                switcher.unref ();
                switcher.homogeneous = true;
            }

            var settings = CoreSettings.get_default ();

            Gtk.Box box = new LabelWidget (_("Behavior"));
            var checkbox = new Gtk.CheckButton.with_mnemonic (_("Enable Spell Checking"));
            settings.bind_property ("enable-spell-checking", checkbox, "active", BindingFlags.SYNC_CREATE | BindingFlags.BIDIRECTIONAL);
            box.add (checkbox);
            checkbox = new Gtk.CheckButton.with_mnemonic (_("Enable scripts"));
            settings.bind_property ("enable-javascript", checkbox, "active", BindingFlags.SYNC_CREATE | BindingFlags.BIDIRECTIONAL);
            box.add (checkbox);
            box.show_all ();
            add (_("Browsing"), box);

            box = new LabelWidget (_("_Tabs"));
            checkbox = new Gtk.CheckButton.with_mnemonic (_("Close Buttons on Tabs"));
            settings.bind_property ("close-buttons-on-tabs", checkbox, "active", BindingFlags.SYNC_CREATE | BindingFlags.BIDIRECTIONAL);
            box.add (checkbox);
            box.show_all ();
            add (_("Browsing"), box);

            box = new Gtk.Box (Gtk.Orientation.VERTICAL, 4);
            box.add (new PeasGtk.PluginManagerView (null));
            box.show_all ();
            add (_("Extensions"), box);

            var extensions = Plugins.get_default ().plug<PreferencesActivatable> ("preferences", this);
            extensions.extension_added.connect ((info, extension) => ((PreferencesActivatable)extension).activate ());
            extensions.foreach ((extensions, info, extension) => { extensions.extension_added (info, extension); });
        }

        protected override bool key_press_event (Gdk.EventKey event) {
            // Close on Escape like a Gtk.Dialog
            if (event.keyval == Gdk.Key.Escape) {
                close ();
                return true;
            }
            return false;
        }

        /*
         * Add a new category of preferences to be shown in the dialog.
         * An appropriate margin will automatically be added.
         */
        public new void add (string label, Gtk.Widget widget) {
            var category = categories.get_child_by_name (label) as Gtk.Box;
            if (category == null) {
                category = new Gtk.Box (Gtk.Orientation.VERTICAL, 4);
                category.margin = 12;
                category.show ();
                categories.add_titled (category, label, label);
            }
            category.pack_start (widget, false, false, 4);
        }
    }
}
