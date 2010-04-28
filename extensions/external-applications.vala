/*
 Copyright (C) 2010 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

using Gtk;
using Midori;

public class ExternalApplications : Midori.Extension {
    Dialog? dialog;
    void tab_added (Widget tab) {
        /* */
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
    extension.authors = "nobody";
    return extension;
}

