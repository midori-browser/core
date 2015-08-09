/*
 Copyright (C) 2009-2014 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Adblock {


    public class StatusIcon : Midori.ContextAction  {
        Config config;
        SubscriptionManager manager;
        public State state;
        public bool debug_element_toggled;

        public StatusIcon (Adblock.Config config, SubscriptionManager manager) {
            GLib.Object (name: "AdblockStatusMenu");

            this.config = config;
            this.manager = manager;
            this.debug_element_toggled = false;

            var item = new Midori.ContextAction ("Preferences",
                _("Preferences"), null, Gtk.STOCK_PREFERENCES);
            item.activate.connect (() => {
                manager.add_subscription (null);
            });
            add (item);

            add (null);

            var checkitem = new Gtk.ToggleAction ("Disable", _("Disable"), null, null);
            checkitem.set_active (!config.enabled);
            checkitem.toggled.connect (() => {
                config.enabled = !checkitem.active;
                set_state (config.enabled ? Adblock.State.ENABLED : Adblock.State.DISABLED);
            });
            add (checkitem);

            var hideritem = new Gtk.ToggleAction ("HiddenElements",
                _("Display hidden elements"), null, null);
            hideritem.set_active (debug_element_toggled);
            hideritem.toggled.connect (() => {
                this.debug_element_toggled = hideritem.active;
            });
            add (hideritem);
            set_status (config.enabled ? "enabled" : "disabled");
        }

        void set_status (string status) {
            gicon = new GLib.ThemedIcon ("adblock-%s".printf (status));
        }

        public void set_state (Adblock.State state) {
            this.state = state;

            if (this.state == State.BLOCKED) {
                set_status ("blocked");
                tooltip = _("Blocking");
            } else if (this.state == State.ENABLED) {
                set_status ("enabled");
                tooltip = _("Enabled");
            } else if (this.state == State.DISABLED) {
                set_status ("disabled");
                tooltip = _("Disabled");
            } else
                assert_not_reached ();
        }
    }

    public class SubscriptionManager {
        Gtk.TreeView treeview;
        Gtk.ListStore liststore;
        Adblock.Config config;
        public Gtk.Label description_label;
        string description;

        public SubscriptionManager (Config config) {
            this.config = config;
            this.liststore = new Gtk.ListStore (1, typeof (Subscription));
            this.description_label = new Gtk.Label (null);
            this.description = _("Type the address of a preconfigured filter list in the text entry and hit Enter.\n");
            this.description += _("You can find more lists by visiting following sites:\n %s, %s\n".printf (
                "<a href=\"http://adblockplus.org/en/subscriptions\">adblockplus.org/en/subscriptions</a>",
                "<a href=\"http://easylist.adblockplus.org/\">easylist.adblockplus.org</a>"
            ));
        }

        public void add_subscription (string? uri) {
            var dialog = new Gtk.Dialog.with_buttons (_("Configure Advertisement filters"),
                null,
#if !HAVE_GTK3
                Gtk.DialogFlags.NO_SEPARATOR |
#endif
                Gtk.DialogFlags.DESTROY_WITH_PARENT,
                Gtk.STOCK_HELP, Gtk.ResponseType.HELP,
                Gtk.STOCK_CLOSE, Gtk.ResponseType.CLOSE);
#if HAVE_GTK3
            dialog.get_widget_for_response (Gtk.ResponseType.HELP).get_style_context ().add_class ("help_button");
#endif
            dialog.set_icon_name (Gtk.STOCK_PROPERTIES);
            dialog.set_response_sensitive (Gtk.ResponseType.HELP, false);

            var hbox = new Gtk.HBox (false, 0);
            (dialog.get_content_area () as Gtk.Box).pack_start (hbox, true, true, 12);
            var vbox = new Gtk.VBox (false, 0);
            hbox.pack_start (vbox, true, true, 4);
            this.description_label.set_markup (this.description);
            this.description_label.set_line_wrap (true);
            vbox.pack_start (this.description_label, false, false, 4);

            var entry = new Gtk.Entry ();
            if (uri != null)
                entry.set_text (uri);
            vbox.pack_start (entry, false, false, 4);

            liststore = new Gtk.ListStore (1, typeof (Subscription));
            treeview = new Gtk.TreeView.with_model (liststore);
            treeview.set_headers_visible (false);
            var column = new Gtk.TreeViewColumn ();
            var renderer_toggle = new Gtk.CellRendererToggle ();
            column.pack_start (renderer_toggle, false);
            column.set_cell_data_func (renderer_toggle, (column, renderer, model, iter) => {
                Subscription sub;
                liststore.get (iter, 0, out sub);
                renderer.set ("active", sub.active,
                              "sensitive", sub.mutable);
            });
            renderer_toggle.toggled.connect ((path) => {
                Gtk.TreeIter iter;
                if (liststore.get_iter_from_string (out iter, path)) {
                    Subscription sub;
                    liststore.get (iter, 0, out sub);
                    sub.active = !sub.active;
                }
            });
            treeview.append_column (column);

            column = new Gtk.TreeViewColumn ();
            var renderer_text = new Gtk.CellRendererText ();
            column.pack_start (renderer_text, false);
            renderer_text.set ("editable", true);
            // TODO: renderer_text.edited.connect
            column.set_cell_data_func (renderer_text, (column, renderer, model, iter) => {
                Subscription sub;
                liststore.get (iter, 0, out sub);
                string status = "";
                foreach (unowned Feature feature in sub) {
                    var updater = feature as Adblock.Updater;
                    if (updater != null) {
                        if (updater.last_updated != null)
                            status = updater.last_updated.format (_("Last update: %x %X"));
                    }
                }
                if (!sub.valid)
                    status = _("File incomplete - broken download?");
                renderer.set ("markup", (Markup.printf_escaped ("<b>%s</b>\n%s",
                    sub.title ?? sub.uri, status)));
            });
            treeview.append_column (column);

            column = new Gtk.TreeViewColumn ();
            Gtk.CellRendererPixbuf renderer_button = new Gtk.CellRendererPixbuf ();
            column.pack_start (renderer_button, false);
            column.set_cell_data_func (renderer_button, on_render_button);
            treeview.append_column (column);

            var scrolled = new Gtk.ScrolledWindow (null, null);
            scrolled.set_policy (Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC);
            scrolled.add (treeview);
            vbox.pack_start (scrolled);
            int height;
            treeview.create_pango_layout ("a\nb").get_pixel_size (null, out height);
            scrolled.set_size_request (-1, height * 5);

            foreach (unowned Subscription sub in config)
                liststore.insert_with_values (null, 0, 0, sub);
            treeview.button_release_event.connect (button_released);

            entry.activate.connect (() => {
                string? parsed_uri = Adblock.parse_subscription_uri (entry.text);
                if (parsed_uri != null) {
                    var sub = new Subscription (parsed_uri);
                    if (config.add (sub)) {
                        liststore.insert_with_values (null, 0, 0, sub);
                        try {
                            sub.parse ();
                        } catch (GLib.Error error) {
                            warning ("Error parsing %s: %s", sub.uri, error.message);
                        }
                    }
                }
                entry.text = "";
            });

            dialog.get_content_area ().show_all ();

            dialog.response.connect ((response)=>{ dialog.destroy (); });
            dialog.show ();
        }

        void on_render_button (Gtk.CellLayout column, Gtk.CellRenderer renderer,
            Gtk.TreeModel model, Gtk.TreeIter iter) {

            Subscription sub;
            liststore.get (iter, 0, out sub);

            renderer.set ("stock-id", sub.mutable ? Gtk.STOCK_DELETE : null,
                          "stock-size", Gtk.IconSize.MENU);
        }

        public bool button_released (Gdk.EventButton event) {
            Gtk.TreePath? path;
            Gtk.TreeViewColumn column;
            if (treeview.get_path_at_pos ((int)event.x, (int)event.y, out path, out column, null, null)) {
                if (path != null) {
                    if (column == treeview.get_column (2)) {
                        Gtk.TreeIter iter;
                        if (liststore.get_iter (out iter, path)) {
                            Subscription sub;
                            liststore.get (iter, 0, out sub);
                            if (sub.mutable) {
                                config.remove (sub);
                                liststore.remove (iter);
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        }
    }

    class CustomRulesEditor {
        Gtk.Dialog dialog;
        Subscription custom;
        public string? rule { get; set; }

        public CustomRulesEditor (Subscription custom) {
            this.custom = custom;
        }

        public void set_uri (string uri) {
            this.rule = uri;
        }

        public void show () {
             this.dialog = new Gtk.Dialog.with_buttons (_("Edit rule"),
                null,
#if !HAVE_GTK3
                Gtk.DialogFlags.NO_SEPARATOR |
#endif
                Gtk.DialogFlags.DESTROY_WITH_PARENT,
                Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                Gtk.STOCK_ADD, Gtk.ResponseType.ACCEPT);
            dialog.set_icon_name (Gtk.STOCK_ADD);
            dialog.resizable = false;

            var hbox = new Gtk.HBox (false, 8);
            var sizegroup = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
            hbox.border_width = 5;
            var label = new Gtk.Label.with_mnemonic (_("_Rule:"));
            sizegroup.add_widget (label);
            hbox.pack_start (label, false, false, 0);
            (dialog.get_content_area () as Gtk.Box).pack_start (hbox, false, true, 0);

            var entry = new Gtk.Entry ();
            sizegroup.add_widget (entry);
            entry.activates_default = true;
            entry.set_text (this.rule);
            hbox.pack_start (entry, true, true, 0);

            dialog.get_content_area ().show_all ();

            dialog.set_default_response (Gtk.ResponseType.ACCEPT);
            if (dialog.run () != Gtk.ResponseType.ACCEPT)
                return;

            this.rule = entry.get_text ();
            this.dialog.destroy ();
            custom.add_rule (this.rule);
        }
    }

}
