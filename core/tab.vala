/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public interface TabActivatable : Peas.ExtensionBase {
        public abstract Tab tab { owned get; set; }
        public abstract void activate ();
        public signal void deactivate ();
    }

    [GtkTemplate (ui = "/ui/tab.ui")]
    public class Tab : WebKit.WebView {
        public string id { owned get { return "%p".printf (this); } }
        public double progress { get; protected set; }
        public new bool can_go_back { get; protected set; }
        public new bool can_go_forward { get; protected set; }
        public DatabaseItem? item { get; protected set; default = null; }
        public string display_uri { get; protected set; }
        public string display_title { get; protected set; }
        public string? color { get; set; default = null; }
        public bool pinned { get; set; }
        public bool secure { get; protected set; }
        bool was_error = false;
        internal TlsCertificate? tls { get; protected set; default = null; }
        public string link_uri { get; protected set; }

        [GtkChild]
        internal Gtk.Popover popover;
        [GtkChild]
        Gtk.Label message;
        [GtkChild]
        Gtk.Button confirm;

        construct {
            notify["estimated-load-progress"].connect (update_progress);
            notify["is-loading"].connect (update_progress);
            notify["uri"].connect ((pspec) => {
                display_uri = uri;
                can_go_back = base.can_go_back ();
                can_go_forward = base.can_go_forward ();
            });
            notify["title"].connect ((pspec) => {
                if (title != null && title != "") {
                    display_title = title;
                    item.title = display_title;
                }
            });
            notify["pinned"].connect ((pspec) => {
                // Undelay if needed
                if (display_uri != uri) {
                    load_uri (display_uri);
                }
            });
        }

        public Tab (Tab? related, WebKit.WebContext web_context,
                    string? uri = null, string? title = null) {

            // One content manager per web context
            var content = web_context.get_data<WebKit.UserContentManager> ("user-content-manager");
            if (content == null) {
                content = new WebKit.UserContentManager ();
                web_context.set_data<WebKit.UserContentManager> ("user-content-manager", content);
            }

            Object (related_view: related, web_context: web_context, user_content_manager: content, visible: true);

            var rgba = Gdk.RGBA ();
            rgba.parse ("#dedede");
            set_background_color (rgba);

            var settings = get_settings ();
            settings.user_agent += " %s".printf (Config.CORE_USER_AGENT_VERSION);
            bind_property ("pinned", settings, "enable-developer-extras", BindingFlags.SYNC_CREATE | BindingFlags.INVERT_BOOLEAN);
            var core_settings = CoreSettings.get_default ();
            settings.javascript_can_open_windows_automatically = true;
            settings.allow_modal_dialogs = true;
            settings.enable_javascript = core_settings.enable_javascript;
            core_settings.notify["enable-javascript"].connect ((pspec) => {
                settings.enable_javascript = core_settings.enable_javascript;
            });
            core_settings.bind_property ("enable-caret-browsing", settings, "enable-caret-browsing", BindingFlags.SYNC_CREATE);

            if (uri != null) {
                display_uri = uri;
                display_title = (title != null && title != "") ? title : uri;
            } else {
                display_uri = "internal:speed-dial";
                display_title = _("Speed Dial");
            }
            item = new DatabaseItem (display_uri, null, 0);

            var extensions = Plugins.get_default ().plug<TabActivatable> ("tab", this);
            extensions.extension_added.connect ((info, extension) => ((TabActivatable)extension).activate ());
            extensions.extension_removed.connect ((info, extension) => ((TabActivatable)extension).deactivate ());
            extensions.foreach ((extensions, info, extension) => { extensions.extension_added (info, extension); });

            if (pinned) {
                load_uri (display_uri);
            } else {
                load_uri_delayed.begin (uri, title);
            }
        }

        async void load_uri_delayed (string? uri, string? title) {
            // Get title from history
            try {
                var history = HistoryDatabase.get_default (web_context.is_ephemeral ());
                var item = yield history.lookup (display_uri);
                if (item != null) {
                    if (item.title != null && item.title != "") {
                        display_title = item.title;
                    }
                    this.item = item;
                }
            } catch (DatabaseError error) {
                debug ("Failed to lookup title in history: %s", error.message);
            }
        }

        void update_progress (ParamSpec pspec) {
            // Update back/ forward state here since there's no signal
            can_go_back = base.can_go_back ();
            can_go_forward = base.can_go_forward ();

            if (is_loading && estimated_load_progress < 1.0) {
                // When loading we want to see at minimum 10% progress
                progress = estimated_load_progress.clamp (0.1, 1.0);
            } else {
                // When we are finished, we don't want to *see* progress anymore
                progress = 0.0;
            }
        }

        public override void load_changed (WebKit.LoadEvent load_event) {
            if (load_event == WebKit.LoadEvent.COMMITTED) {
                item = new DatabaseItem (uri, null, new DateTime.now_local ().to_unix ());
                // Don't add errors to history or update the certificate
                if (was_error) {
                    was_error = false;
                    return;
                }
                was_error = false;
                TlsCertificate tls;
                secure = get_tls_info (out tls, null);
                this.tls = tls;
                // Don't add internal or blank pages to history
                if (uri.has_prefix ("internal:") || uri.has_prefix ("about:")) {
                    return;
                }
                // Don't add anything in private browsing mode
                if (web_context.is_ephemeral ()) {
                    return;
                }
                try {
                    var history = HistoryDatabase.get_default ();
                    history.insert.begin (item);
                } catch (DatabaseError error) {
                    debug ("Failed to insert history item: %s", error.message);
                }
            }
        }

        public override void insecure_content_detected (WebKit.InsecureContentEvent event) {
            secure = false;
        }

        public override bool web_process_crashed () {
            this.tls = null;
            return display_error ("face-sad", _("Oops - %s").printf (uri), _("Something went wrong with '%s'.").printf (uri));
        }

        public override bool load_failed_with_tls_errors (string uri, GLib.TlsCertificate tls, GLib.TlsCertificateFlags flags) {
            display_uri = uri;
            this.tls = tls;
            return display_error ("channel-insecure", _("Security unknown"), _("Something went wrong with '%s'.").printf (uri));
        }

        public override bool load_failed (WebKit.LoadEvent load_event, string uri, Error load_error) {
            // The unholy trinity; also ignored in Webkit's default error handler:
            // A plugin will take over. That's expected, it's not fatal.
            if (load_error is WebKit.PluginError.WILL_HANDLE_LOAD) {
                return false;
            }
            // Mostly initiated by JS redirects.
            if (load_error is WebKit.NetworkError.CANCELLED) {
                return false;
            }
            // A frame load is cancelled because of a download.
            if (load_error is WebKit.PolicyError.FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE) {
                return false;
            }

            // Forward URIs that WebKit can't show to external handler
            if (load_error is WebKit.PolicyError.CANNOT_SHOW_URI) {
                try {
                    Gtk.show_uri (get_screen (), uri, Gtk.get_current_event_time ());
                } catch (Error error) {
                    critical ("Failed to open %s: %s", uri, error.message);
                }
                return true;
            }

            var monitor = NetworkMonitor.get_default ();
            string hostname = new Soup.URI (uri).host;
            string? title = null;
            string? message = null;
            if (!monitor.network_available) {
                title = _("You are not connected to a network");
                message = _("Your computer must be connected to a network to reach “%s”. " +
                            "Connect to a wireless access point or attach a network cable and try again.").printf (hostname);
            } else {
                try {
                    monitor.can_reach (NetworkAddress.parse_uri (Config.PROJECT_WEBSITE, 80));
                    title = _("Midori can't find the page you're looking for");
                    message = _("The page located at “%s” cannot be found. " +
                                "Check the web address for misspelled words and try again.").printf (hostname);
                } catch (Error error) {
                    title = _("You are not connected to the Internet");
                    message = _("Your computer appears to be connected to a network, but can't reach “%s”. " +
                                "Check your network settings and try again.").printf (hostname);
                }
            }
            display_uri = uri;
            this.tls = null;
            return display_error ("network-error", title, message, load_error.message);
        }

        bool display_error (string icon_name, string title, string message, string? description=null) {
            try {
                string stylesheet = (string)resources_lookup_data ("/data/about.css",
                                                                    ResourceLookupFlags.NONE).get_data ();
                string html = ((string)resources_lookup_data ("/data/error.html",
                                                             ResourceLookupFlags.NONE).get_data ())
                    .replace ("{stylesheet}", stylesheet)
                    .replace ("{icon}", icon_name)
                    .replace ("{title}", title)
                    .replace ("{message}", message)
                    .replace ("{description}", description ?? "")
                    .replace ("{tryagain}", "<span>%s</span>".printf (_("Try Again")))
                    .replace ("{uri}", display_uri);
                load_alternate_html (html, display_uri, display_uri);
                was_error = true;
                return true;
            } catch (Error error) {
                critical ("Failed to display error: %s", error.message);
            }
            return false;
        }

        public override void mouse_target_changed (WebKit.HitTestResult result, uint modifiers) {
            link_uri = result.link_uri;
        }

        public override bool context_menu (WebKit.ContextMenu menu,
            Gdk.Event event, WebKit.HitTestResult hit) {

            if (hit.context_is_editable ()) {
                return false;
            }

            // No context menu for pinned tabs
            if (pinned) {
                return true;
            }

            bool clear = hit.context_is_link ()
                      || hit.context_is_image ()
                      || hit.context_is_media ()
                      || hit.context_is_selection ();
            if (clear) {
                menu.remove_all ();
            }
            if (hit.context_is_link () && !hit.link_uri.has_prefix ("javascript:")) {
                menu.append (new WebKit.ContextMenuItem.from_stock_action_with_label (WebKit.ContextMenuAction.OPEN_LINK_IN_NEW_WINDOW, _("Open Link in New _Tab")));
                var action = new Gtk.Action ("link-window", _("Open Link in New _Window"), null, null);
                action.activate.connect (() => {
                    var browser = web_context.is_ephemeral ()
                        ? new Browser.incognito ((App)Application.get_default ())
                        : new Browser ((App)Application.get_default ());
                    browser.add (new Tab (null, browser.web_context, hit.link_uri));
                    browser.show ();
                });
                menu.append (new WebKit.ContextMenuItem (action));
                menu.append (new WebKit.ContextMenuItem.separator ());
                menu.append (new WebKit.ContextMenuItem.from_stock_action (WebKit.ContextMenuAction.DOWNLOAD_LINK_TO_DISK));
                menu.append (new WebKit.ContextMenuItem.from_stock_action (WebKit.ContextMenuAction.COPY_LINK_TO_CLIPBOARD));
            }
            if (hit.context_is_image ()) {
                menu.append (new WebKit.ContextMenuItem.separator ());
                menu.append (new WebKit.ContextMenuItem.from_stock_action_with_label (WebKit.ContextMenuAction.DOWNLOAD_IMAGE_TO_DISK, _("Save I_mage")));
                menu.append (new WebKit.ContextMenuItem.from_stock_action (WebKit.ContextMenuAction.COPY_IMAGE_TO_CLIPBOARD));
                menu.append (new WebKit.ContextMenuItem.from_stock_action (WebKit.ContextMenuAction.COPY_IMAGE_URL_TO_CLIPBOARD));
            }
            if (hit.context_is_media ()) {
                menu.append (new WebKit.ContextMenuItem.separator ());
                menu.append (new WebKit.ContextMenuItem.from_stock_action (WebKit.ContextMenuAction.COPY_VIDEO_LINK_TO_CLIPBOARD));
                menu.append (new WebKit.ContextMenuItem.from_stock_action (WebKit.ContextMenuAction.DOWNLOAD_VIDEO_TO_DISK));
            }
            if (hit.context_is_selection ()) {
                menu.append (new WebKit.ContextMenuItem.separator ());
                menu.append (new WebKit.ContextMenuItem.from_stock_action (WebKit.ContextMenuAction.COPY));
                // Selected text, ellipsized if > 32 characters
                string? text = Gtk.Clipboard.get_for_display (get_display (), Gdk.SELECTION_PRIMARY).wait_for_text ();
                string? label = ((text != null && text.length > 32) ? text.substring (0, 32) + "…" : text).delimit ("\n", ' ');
                var action = new Gtk.Action ("text-search", _("Search for %s").printf (label), null, null);
                action.activate.connect (() => {
                    var settings = CoreSettings.get_default ();
                    var tab = new Tab (null, web_context, settings.uri_for_search (text));
                    ((Browser)get_toplevel ()).add (tab);
                });
                menu.append (new WebKit.ContextMenuItem (action));
            }
            if (clear) {
                menu.append (new WebKit.ContextMenuItem.separator ());
                menu.append (new WebKit.ContextMenuItem.from_stock_action (WebKit.ContextMenuAction.INSPECT_ELEMENT));
            }
            return false;
        }

        public override bool print (WebKit.PrintOperation operation) {
            operation.run_dialog (get_toplevel () as Gtk.Window);
            return true;
        }

        public override void close () {
            destroy ();
        }

        public override bool script_dialog (WebKit.ScriptDialog dialog) {
            switch (dialog.get_dialog_type ()) {
                case WebKit.ScriptDialogType.ALERT:
                    message.label = dialog.get_message ();
                    confirm.hide ();
                    popover.show ();
                    break;
                case WebKit.ScriptDialogType.CONFIRM:
                case WebKit.ScriptDialogType.BEFORE_UNLOAD_CONFIRM:
                    string hostname = new Soup.URI (uri).host;
                    dialog.confirm_set_confirmed(((Browser)get_toplevel ()).prompt (hostname, dialog.get_message (), _("_Confirm")) != null);
                    break;
                case WebKit.ScriptDialogType.PROMPT:
                    string hostname = new Soup.URI (uri).host;
                    dialog.prompt_set_text(((Browser)get_toplevel ()).prompt (hostname, dialog.get_message (), _("_Confirm"), dialog.prompt_get_default_text ()));
                    break;
            }
            return true;
        }

        public override bool show_notification (WebKit.Notification webkit_notification) {
            // Don't show notifications for the visible tab
            if (get_mapped ()) {
                return false;
            }

            var notification = new Notification (webkit_notification.title);
            if (favicon != null) {
                var image = (Cairo.ImageSurface)favicon;
                var pixbuf = Gdk.pixbuf_get_from_surface (image, 0, 0, image.get_width (), image.get_height ());
                notification.set_icon ((Icon)pixbuf);
            }
            notification.set_body (webkit_notification.body);
            // Use a per-host ID to avoid collisions, but neglect the tag
            string hostname = new Soup.URI (uri).host;
            Application.get_default ().send_notification ("web-%s".printf (hostname), notification);
            return true;
        }

        public override bool permission_request (WebKit.PermissionRequest permission) {
            if (permission is WebKit.GeolocationPermissionRequest) {
                string hostname = new Soup.URI (uri).host;
                message.label = _("%s wants to know your location.").printf (hostname);
            } else if (permission is WebKit.NotificationPermissionRequest) {
                permission.allow ();
                return true;
            } else {
                message.label = permission.get_type ().name ();
            }
            confirm.label = _("_Allow");
            confirm.show ();
            confirm.clicked.connect (() => {
                permission.allow ();
                popover.hide ();
            });
            popover.closed.connect (() => {
                permission.deny ();
            });
            popover.show ();
            return true;
        }

        public override bool decide_policy (WebKit.PolicyDecision decision, WebKit.PolicyDecisionType type) {
            switch (type) {
                case WebKit.PolicyDecisionType.NAVIGATION_ACTION:
                    var action = ((WebKit.NavigationPolicyDecision)decision).navigation_action;
                    if (action.is_user_gesture ()) {
                        // Middle click or ^click for new tab
                        bool has_ctrl = (action.get_modifiers () & Gdk.ModifierType.CONTROL_MASK) != 0;
                        if (action.get_mouse_button () == 2
                            || (has_ctrl && action.get_mouse_button () == 1)) {
                            var tab = ((Tab)create (action));
                            tab.load_request (action.get_request ());
                            tab.ready_to_show ();
                            decision.ignore ();
                            return true;
                        }
                    }
                    break;
                case WebKit.PolicyDecisionType.NEW_WINDOW_ACTION:
                    var action = ((WebKit.NavigationPolicyDecision)decision).navigation_action;
                    var tab = ((Tab)create (action));
                    tab.set_data<bool> ("foreground", true);
                    tab.load_request (action.get_request ());
                    tab.ready_to_show ();
                    decision.ignore ();
                    return true;
                case WebKit.PolicyDecisionType.RESPONSE:
                    var response_decision = ((WebKit.ResponsePolicyDecision)decision);
                    if (!response_decision.is_mime_type_supported ()) {
                        decision.download ();
                        return true;
                    }
                    break;
            }
            return false;
        }
    }
}
