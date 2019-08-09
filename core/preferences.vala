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
        public signal void deactivate ();
    }

    public class LabelWidget : Gtk.Box {
        public string? title { get; construct set; }
        protected Gtk.Label label { get; set; }
        public Gtk.Widget? widget { get; construct set; }
        Gtk.SizeGroup sizegroup = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);

        public LabelWidget (string? title, Gtk.Widget? widget=null) {
            Object (title: title, widget: widget);
        }

        public override void add (Gtk.Widget widget) {
            base.add (widget);
            if (widget is LabelWidget) {
                sizegroup.add_widget (((LabelWidget)widget).label);
            }
        }

        internal LabelWidget.for_days (string title, Object object, string property) {
            var combo = new Gtk.ComboBoxText ();
            combo.append ("0", _("1 hour"));
            combo.append ("1", _("1 day"));
            combo.append ("7", _("1 week"));
            combo.append ("30", _("1 month"));
            combo.append ("365", _("1 year"));
            combo.show ();
            int64 current_value;
            object.get (property, out current_value);
            combo.active_id = current_value.to_string ();
            combo.notify["active-id"].connect ((pspec) => {
                object.set (property, combo.active_id.to_int64 ());
            });
            Object (title: title, widget: combo);
        }

        construct {
            bool header = widget == null;
            orientation = header ? Gtk.Orientation.VERTICAL : Gtk.Orientation.HORIZONTAL;
            halign = Gtk.Align.START;
            label = new Gtk.Label.with_mnemonic (header ? "<b>%s</b>".printf (title) : title);
            label.use_markup = header;
            label.halign = Gtk.Align.START;
            pack_start (label, false, false, 4);
            if (widget != null) {
                label.mnemonic_widget = widget;
                widget.margin = 4;
                set_center_widget (widget);
                if (widget is Gtk.Entry && !(widget is Gtk.SpinButton)) {
                    ((Gtk.Entry)widget).width_chars = 30;
                }
            }
        }
    }

    [GtkTemplate (ui = "/ui/preferences.ui")]
    public class Preferences : Gtk.Dialog {
        [GtkChild]
        Gtk.StackSwitcher switcher;
        [GtkChild]
        Gtk.Box content_box;
        [GtkChild]
        Gtk.Stack categories;
        Gtk.SearchEntry proxy;
        Gtk.SpinButton port;

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
            } else {
                // "for technical reasons, this property is declared as an integer"
                use_header_bar = 1;
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

            box = new LabelWidget (_("Search _with"));
            var combo = new Gtk.ComboBoxText ();
            combo.append ("https://duckduckgo.com/?q=%s", "Duck Duck Go");
            combo.append ("http://search.yahoo.com/search?p=", "Yahoo");
            combo.append ("http://www.google.com/search?q=%s", "Google");
            settings.bind_property ("location-entry-search", combo, "active-id", BindingFlags.SYNC_CREATE | BindingFlags.BIDIRECTIONAL);
            // Generic item for custom search option
            if (combo.get_active_text () == null) {
                string hostname = new Soup.URI (settings.location_entry_search).host;
                combo.append (settings.location_entry_search, hostname);
                combo.active_id = settings.location_entry_search;
            }
            box.add (combo);
            box.show_all ();
            add (_("Browsing"), box);

            box = new LabelWidget (_("_Tabs"));
            var entry = new Gtk.SearchEntry ();
            entry.primary_icon_name = null;
            entry.placeholder_text = Config.PROJECT_WEBSITE;
            // Render non-URL eg. about:search as empty
            entry.text = ("://" in settings.homepage || "." in settings.homepage) ? settings.homepage : "";
            entry.search_changed.connect (() => {
                if ("://" in entry.text || "." in entry.text || entry.text == "") {
                    entry.get_style_context ().remove_class ("error");
                    settings.homepage = entry.text;
                } else {
                    entry.get_style_context ().add_class ("error");
                }
            });
            var button = new LabelWidget (_("Homepage:"), entry);
            box.add (button);
            checkbox = new Gtk.CheckButton.with_mnemonic (_("Close Buttons on Tabs"));
            settings.bind_property ("close-buttons-on-tabs", checkbox, "active", BindingFlags.SYNC_CREATE | BindingFlags.BIDIRECTIONAL);
            box.add (checkbox);
            box.show_all ();
            add (_("Browsing"), box);

            box = new LabelWidget (_("Customize Toolbar"));
            checkbox = new Gtk.CheckButton.with_mnemonic (_("Show Homepage"));
            settings.bind_property ("homepage-in-toolbar", checkbox, "active", BindingFlags.SYNC_CREATE | BindingFlags.BIDIRECTIONAL);
            box.add (checkbox);
            box.show_all ();
            add (_("Browsing"), box);

            box = new LabelWidget (_("Proxy server"));
            var proxy_chooser = new Gtk.ComboBoxText ();
            proxy_chooser.append ("0", _("Automatic (GNOME or environment)"));
            proxy_chooser.append ("2", _("HTTP proxy server"));
            proxy_chooser.append ("1", _("No proxy server"));
            settings.bind_property ("proxy-type", proxy_chooser, "active", BindingFlags.SYNC_CREATE | BindingFlags.BIDIRECTIONAL);
            box.add (new LabelWidget (null, proxy_chooser));
            proxy = new Gtk.SearchEntry ();
            proxy.primary_icon_name = null;
            settings.bind_property ("http-proxy", proxy, "text", BindingFlags.SYNC_CREATE | BindingFlags.BIDIRECTIONAL);
            box.add (new LabelWidget (_("URI"), proxy));
            string proxy_types = "http https";
            foreach (unowned IOExtension proxy in IOExtensionPoint.lookup (IOExtensionPoint.PROXY).get_extensions ()) {
                if (!proxy_types.contains (proxy.get_name ())) {
                    proxy_types += " %s".printf (proxy.get_name ());
                }
            }
            proxy.search_changed.connect (() => {
                if (! ("://" in settings.http_proxy)) {
                    proxy.get_style_context ().add_class ("error");
                    return;
                }
                string[] parts = settings.http_proxy.split ("://", 2);
                if (parts[1] == "" || ":" in parts[1] || "/" in parts[1]) {
                    proxy.get_style_context ().add_class ("error");
                    return;
                }
                foreach (string type in proxy_types.split (" ")) {
                    if (parts[0] == type) {
                        proxy.get_style_context ().remove_class ("error");
                        return;
                    }
                }
                proxy.get_style_context ().add_class ("error");
            });
            box.add (new LabelWidget (_("Supported proxy types:"), new Gtk.Label (proxy_types)));
            port = new Gtk.SpinButton.with_range (1, 65535, 1);
            settings.bind_property ("http-proxy-port", port, "value", BindingFlags.SYNC_CREATE | BindingFlags.BIDIRECTIONAL);
            box.add (new LabelWidget (_("Port"), port));
            settings.notify["proxy-type"].connect (update_proxy_sensitivity);
            update_proxy_sensitivity ();
            box.show_all ();
            add (_("Network"), box);

            box = new LabelWidget (_("Cookies and Website data"));
            checkbox = new Gtk.CheckButton.with_mnemonic (_("Only accept Cookies from sites you visit"));
            settings.bind_property ("first-party-cookies-only", checkbox, "active", BindingFlags.SYNC_CREATE | BindingFlags.BIDIRECTIONAL);
            checkbox.tooltip_text = _("Block cookies sent by third-party websites");
            box.pack_start (checkbox, false, false, 4);
            box.show_all ();
            add (_("Privacy"), box);

            box = new LabelWidget (_("_History"));
            button = new LabelWidget.for_days (_("Delete pages from history after:"), settings, "maximum-history-age");
            button.tooltip_text = _("The maximum number of days to save the history for");
            box.pack_start (button, false, false, 4);
            box.show_all ();
            add (_("Privacy"), box);

            box = new Gtk.Box (Gtk.Orientation.VERTICAL, 4);
            box.add (new PeasGtk.PluginManagerView (null));
            box.show_all ();
            add (_("Extensions"), box);

            var extensions = Plugins.get_default ().plug<PreferencesActivatable> ("preferences", this);
            extensions.extension_added.connect ((info, extension) => ((PreferencesActivatable)extension).activate ());
            extensions.extension_removed.connect ((info, extension) => ((PreferencesActivatable)extension).deactivate ());
            extensions.foreach ((extensions, info, extension) => { extensions.extension_added (info, extension); });
        }

        void update_proxy_sensitivity () {
            var settings = CoreSettings.get_default ();
            proxy.sensitive = settings.proxy_type == ProxyType.HTTP;
            port.sensitive = settings.proxy_type == ProxyType.HTTP;
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
