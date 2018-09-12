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
        public bool pinned { get; set; }
        public bool secure { get; protected set; }
        public string link_uri { get; protected set; }

        [GtkChild]
        Gtk.Popover popover;
        [GtkChild]
        Gtk.Label message;
        [GtkChild]
        Gtk.Entry entry;
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
                display_title = (title != null && title != "") ? title : display_uri;
                item.title = display_title;
            });
        }

        public Tab (Tab? related, WebKit.WebContext web_context,
                    string? uri = null, string? title = null) {
            Object (related_view: related, web_context: web_context, visible: true);

            var settings = get_settings ();
            settings.user_agent = Config.CORE_USER_AGENT;
            settings.enable_developer_extras = true;

            if (pinned) {
                var extensions = Plugins.get_default ().plug<TabActivatable> ("tab", this);
                extensions.extension_added.connect ((info, extension) => ((TabActivatable)extension).activate ());
                extensions.foreach ((extensions, info, extension) => { extensions.extension_added (info, extension); });
                load_uri (uri ?? "internal:speed-dial");
            } else {
                load_uri_delayed.begin (uri, title);
            }
        }

        async void load_uri_delayed (string? uri, string? title) {
            display_uri = uri ?? "internal:speed-dial";
            display_title = title ?? display_uri;
            item = new DatabaseItem (display_uri, display_title, 0);

            // Get title from history
            try {
                var history = HistoryDatabase.get_default ();
                var items = yield history.query (display_title, 1);
                var item = items.nth_data (0);
                if (item != null) {
                    display_title = item.title;
                    this.item = item;
                }
            } catch (DatabaseError error) {
                debug ("Failed to lookup title in history: %s", error.message);
            }
        }

        public override bool focus_in_event (Gdk.EventFocus event) {
            // Delayed load on focus
            if (display_uri != uri) {
                var extensions = Plugins.get_default ().plug<TabActivatable> ("tab", this);
                extensions.extension_added.connect ((info, extension) => ((TabActivatable)extension).activate ());
                extensions.foreach ((extensions, info, extension) => { extensions.extension_added (info, extension); });
                load_uri (display_uri);
            }
            return true;
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
                item = new DatabaseItem (uri, display_title, new DateTime.now_local ().to_unix ());
                try {
                    var history = HistoryDatabase.get_default ();
                    history.insert.begin (item);
                } catch (DatabaseError error) {
                    debug ("Failed to insert history item: %s", error.message);
                }
                secure = get_tls_info (null, null);
            }
        }

        public override void insecure_content_detected (WebKit.InsecureContentEvent event) {
            secure = false;
        }

        public override bool web_process_crashed () {
            return display_error ("face-sad", _("Oops - %s").printf (uri), _("Something went wrong with '%s'.").printf (uri));
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

            bool clear = hit.context_is_link ()
                      || hit.context_is_image ()
                      || hit.context_is_media ()
                      || hit.context_is_selection ();
            if (clear) {
                menu.remove_all ();
            }
            if (hit.context_is_link () && !hit.link_uri.has_prefix ("javascript:")) {
                menu.append (new WebKit.ContextMenuItem.from_stock_action_with_label (WebKit.ContextMenuAction.OPEN_LINK_IN_NEW_WINDOW, _("Open Link in New _Tab")));
                if (!App.incognito) {
                    var action = new Gtk.Action ("link-window", _("Open Link in New _Window"), null, null);
                    action.activate.connect (() => {
                        var browser = new Browser ((App)Application.get_default ());
                        browser.add (new Tab (null, browser.web_context, hit.link_uri));
                        browser.show ();
                    });
                    menu.append (new WebKit.ContextMenuItem (action));
                }
                var action = new Gtk.Action ("link-incognito", _("Open Link in New _Private Window"), null, null);
                action.activate.connect (() => {
                    var browser = new Browser.incognito ((App)Application.get_default ());
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
                menu.append (new WebKit.ContextMenuItem.from_stock_action (WebKit.ContextMenuAction.DOWNLOAD_IMAGE_TO_DISK));
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
                var action = new Gtk.Action ("text-search", _("Search %s for \"%s\"").printf ("DuckDuckGo", label), null, null);
                action.activate.connect (() => {
                    // Allow DuckDuckGo to distinguish Midori and in turn share revenue
                    string uri = "https://duckduckgo.com/?q=%s&t=midori".printf (Uri.escape_string (text, ":/", true));
                    var tab = new Tab (this, web_context, uri);
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
            message.label = dialog.get_message ();
            // Render inactive without setting sensitive which affects the popover
            opacity = 0.3;
            popover.closed.connect (() => {
                opacity = 1.0;
            });

            switch (dialog.get_dialog_type ()) {
                case WebKit.ScriptDialogType.ALERT:
                    break;
                case WebKit.ScriptDialogType.CONFIRM:
                case WebKit.ScriptDialogType.BEFORE_UNLOAD_CONFIRM:
                    confirm.visible = true;
                    popover.closed.connect (() => {
                        dialog.confirm_set_confirmed (false);
                    });
                    confirm.clicked.connect (() => {
                        dialog.confirm_set_confirmed (true);
                    });
                    break;
                case WebKit.ScriptDialogType.PROMPT:
                    entry.placeholder_text = dialog.prompt_get_default_text ();
                    entry.visible = true;
                    confirm.visible = true;
                    popover.closed.connect (() => {
                        dialog.prompt_set_text ("");
                    });
                    confirm.clicked.connect (() => {
                        dialog.prompt_set_text (entry.text);
                    });
                    break;
            }
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
                    create (action);
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
