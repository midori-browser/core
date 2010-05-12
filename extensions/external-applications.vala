/*
 Copyright (C) 2010 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

using Gtk;
using WebKit;
using Midori;

public class ExternalApplications : Midori.Extension {
    Dialog? dialog;
    bool launch (string command, string uri) {
        try {
            var info = GLib.AppInfo.create_from_commandline (command, null, 0);
            var uris = new List<string>();
            uris.prepend (uri);
            info.launch_uris (uris, null);
            return true;
        }
        catch (GLib.Error error) {
            var error_dialog = new Gtk.MessageDialog (null, 0,
                Gtk.MessageType.ERROR, Gtk.ButtonsType.OK,
                "Failed to launch external application.");
            error_dialog.format_secondary_text (error.message);
            error_dialog.response.connect ((dialog, response)
                => { dialog.destroy (); });
            error_dialog.show ();
        }
        return false;
    }
    bool navigating (WebFrame web_frame, NetworkRequest request,
                     WebNavigationAction action, WebPolicyDecision decision) {
        string uri = request.get_uri ();
        if (uri.has_prefix ("ftp://")) {
            if (launch ("gftp", uri)) {
                decision.ignore ();
                return true;
            }
        }
        return false;
    }
    void tab_added (View tab) {
        var web_view = tab.get_web_view ();
        web_view.navigation_policy_decision_requested.connect (navigating);
    }
    void configure_external_applications () {
        if (dialog == null) {
            dialog = new Dialog.with_buttons ("Configure External Applications",
                get_app ().browser,
                DialogFlags.DESTROY_WITH_PARENT | DialogFlags.NO_SEPARATOR,
                STOCK_CLOSE, ResponseType.CLOSE);
            dialog.icon_name = STOCK_PROPERTIES;
            dialog.destroy.connect ((dialog) => { dialog = null; });
            dialog.response.connect ((dialog, response) => { dialog.destroy (); });
            dialog.show ();
        }
        else
            dialog.present ();
    }
    void tool_menu_populated (Menu menu) {
        var menuitem = new MenuItem.with_mnemonic ("Configure _External Applications...");
        menuitem.activate.connect (configure_external_applications);
        menuitem.show ();
        menu.append (menuitem);
    }
    void browser_added (Browser browser) {
        foreach (var tab in browser.get_tabs ())
            tab_added (tab);
        browser.add_tab.connect (tab_added);
        browser.populate_tool_menu.connect (tool_menu_populated);
    }
    void activated (Midori.App app) {
        foreach (var browser in app.get_browsers ())
            browser_added (browser);
        app.add_browser.connect (browser_added);
    }
    void deactivated () {
        var app = get_app ();
        app.add_browser.disconnect (browser_added);
        foreach (var browser in app.get_browsers ()) {
            foreach (var tab in browser.get_tabs ())
                /* */;
            browser.populate_tool_menu.disconnect (tool_menu_populated);
        }
    }
    internal ExternalApplications () {
        activate.connect (activated);
        deactivate.connect (deactivated);
    }
}

public Midori.Extension extension_init () {
    var extension = new ExternalApplications ();
    extension.name = "External Applications";
    extension.description = "Lalala";
    extension.version = "0.1";
    extension.authors = "Christian Dywan <christian@twotoasts.de>";
    return extension;
}

