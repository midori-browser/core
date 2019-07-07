/*
 Copyright (C) 2009-2018 Christian Dywan <christian@twotoats.de>
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Adblock {
    public class Button : Gtk.Button {
        public string icon_name { get; protected set; }
        Settings settings = Settings.get_default ();

        construct {
            action_name = "win.adblock-status";
            tooltip_text = _("Advertisement blocker");
            var image = new Gtk.Image.from_icon_name (icon_name, Gtk.IconSize.BUTTON);
            bind_property ("icon-name", image, "icon-name");
            image.use_fallback = true;
            image.show ();
            add (image);
            settings.notify["enabled"].connect (update_icon);
            update_icon ();
            show ();
        }

        void update_icon () {
            icon_name = "security-%s-symbolic".printf (settings.enabled ? "high" : "low");
        }

        public Button (Midori.Browser browser) {
            var action = new SimpleAction ("adblock-status", null);
            action.activate.connect (() => {
                settings.enabled = !settings.enabled;
                browser.tab.reload ();
            });
            browser.notify["uri"].connect (() => {
                action.set_enabled (browser.uri.has_prefix ("http"));
            });
            browser.add_action (action);
            browser.application.set_accels_for_action ("win.adblock-status", { });
        }
    }

    public class Frontend : Object, Midori.BrowserActivatable {
        public Midori.Browser browser { owned get; set; }

        public void activate () {
            var button = new Button (browser);
            browser.add_button (button);
            deactivate.connect (() => {
                button.destroy ();
            });

            browser.web_context.register_uri_scheme ("abp", (request) => {
                if (request.get_uri ().has_prefix ("abp:subscribe?location=")) {
                    // abp://subscripe?location=http://example.com&title=foo
                    var sub = new Subscription (request.get_uri ().substring (23, -1));
                    debug ("Adding %s to filters\n", sub.uri);
                    Settings.get_default ().add (sub);
                    sub.active = true;
                    request.get_web_view ().stop_loading ();
                } else {
                    request.finish_error (new FileError.NOENT (_("Invalid URI")));
                }
            });
        }
    }

    public class RequestFilter : Peas.ExtensionBase, Peas.Activatable {
        public Object object { owned get; construct; }

        public void activate () {
            string? page_uri;
            object.get ("uri", out page_uri);
            object.connect ("signal::send-request", handle_request, page_uri);
        }

        bool handle_request (Object request, Object? response, string? page_uri) {
            string? uri;
            request.get ("uri", out uri);
            if (page_uri == null) {
                // Note: 'this' is the WebKit.WebPage object in this context
                get ("uri", out page_uri);
            }
            return get_directive_for_uri (uri, page_uri) == Directive.BLOCK;
        }

        Directive get_directive_for_uri (string request_uri, string page_uri) {
            var settings = Settings.get_default ();

            if (!settings.enabled)
                return Directive.ALLOW;

            // Always allow the main page
            if (request_uri == page_uri)
                return Directive.ALLOW;

            // No adblock on non-http(s) schemes
            if (!request_uri.has_prefix ("http"))
                return Directive.ALLOW;

            Directive? directive = null;
            foreach (var sub in settings) {
                directive = sub.get_directive (request_uri, page_uri);
                if (directive != null)
                    break;
            }

            if (directive == null) {
                directive = Directive.ALLOW;
            }
            return directive;
        }

        public void deactivate () {
        }

        public void update_state () {
        }
    }

    public class Preferences : Object, Midori.PreferencesActivatable {
        public Midori.Preferences preferences { owned get; set; }

        public void activate () {
            var box = new Midori.LabelWidget (_("Configure Advertisement filters"));
            var listbox = new Gtk.ListBox ();
            listbox.selection_mode = Gtk.SelectionMode.NONE;
            var settings = Settings.get_default ();
            foreach (var sub in settings) {
                var row = new Gtk.Box (Gtk.Orientation.HORIZONTAL, 4);
                var button = new Gtk.CheckButton.with_label (sub.title);
                button.tooltip_text = sub.uri;
                sub.bind_property ("active", button, "active", BindingFlags.SYNC_CREATE | BindingFlags.BIDIRECTIONAL);
                row.pack_start (button);
                if (!settings.default_filters.contains (sub.uri.split ("&")[0])) {
                    var remove = new Gtk.Button.from_icon_name ("list-remove-symbolic");
                    remove.relief = Gtk.ReliefStyle.NONE;
                    remove.clicked.connect (() => {
                        settings.remove (sub);
                        row.destroy ();
                    });
                    row.pack_end (remove, false);
                }
                listbox.insert (row, -1);
            }

            // Provide guidance to adding additional filters via the abp: scheme
            var label = new Gtk.Label (
                _("You can find more lists by visiting following sites:\n %s, %s\n").printf (
                "<a href=\"https://adblockplus.org/en/subscriptions\">AdblockPlus</a>",
                "<a href=\"https://easylist.to\">EasyList</a>"));
            label.use_markup = true;
            label.activate_link.connect ((uri) => {
                var files = new File[1];
                files[0] = File.new_for_uri (uri);
                Application.get_default ().open (files, "");
                return true;
            });
            listbox.insert (label, -1);

            box.add (listbox);
            box.show_all ();
            preferences.add (_("Privacy"), box);
            deactivate.connect (() => {
                box.destroy ();
            });
        }
    }
}

[ModuleInit]
public void peas_register_types(TypeModule module) {
    ((Peas.ObjectModule)module).register_extension_type (
        typeof (Midori.BrowserActivatable), typeof (Adblock.Frontend));
    ((Peas.ObjectModule)module).register_extension_type (
        typeof (Peas.Activatable), typeof (Adblock.RequestFilter));
    ((Peas.ObjectModule)module).register_extension_type (
        typeof (Midori.PreferencesActivatable), typeof (Adblock.Preferences));
}
