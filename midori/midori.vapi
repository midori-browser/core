/* Copyright (C) 2010 Christian Dywan <christian@twotoasts.de>
   This file is licensed under the terms of the expat license, see the file EXPAT. */

[CCode (cprefix = "Midori", lower_case_cprefix = "midori_")]
namespace Midori {
    [CCode (cheader_filename = "midori/midori.h")]
    public class App : GLib.Object {
        public App ();
        public Browser create_browser ();
        public GLib.List<weak Browser> get_browsers ();

        [NoAccessorMethod]
        public string name { get; set; }
        [NoAccessorMethod]
        public Midori.WebSettings settings { get; set; }
        [NoAccessorMethod]
        public GLib.Object bookmarks { get; set; }
        [NoAccessorMethod]
        public GLib.Object trash { get; set; }
        [NoAccessorMethod]
        public GLib.Object search_engines { get; set; }
        [NoAccessorMethod]
        public GLib.Object history { get; set; }
        [NoAccessorMethod]
        public GLib.Object extensions { get; set; }
        [NoAccessorMethod]
        public GLib.Object browsers { get; }
        public Browser? browser { get; }

        [HasEmitter]
        public signal void add_browser (Browser browser);
        public signal void remove_browser (Browser browser);
        [HasEmitter]
        public signal void quit ();
    }
    public class Browser : Gtk.Window {
        public Browser ();
        public int add_item (GLib.Object item);
        public int add_uri (string uri);
        public unowned Gtk.Widget get_nth_tab (int n);
        public GLib.List<weak Gtk.Widget> get_tabs ();
        public unowned Gtk.ActionGroup get_action_group ();
        public unowned Browser get_for_widget (Gtk.Widget widget);
        public unowned string[] get_toolbar_actions ();
        public unowned GLib.Object get_proxy_items ();

        [NoAccessorMethod]
        public Gtk.MenuBar menubar { get; }
        [NoAccessorMethod]
        public Gtk.Toolbar navigationbar { get; }
        [NoAccessorMethod]
        public Gtk.Notebook notebook { get; }
        [NoAccessorMethod]
        public Gtk.Widget panel { get; }
        [NoAccessorMethod]
        public string uri { get; set; }
        [NoAccessorMethod]
        public Gtk.Widget tab { get; set; }
        [NoAccessorMethod]
        public uint load_status { get; }
        [NoAccessorMethod]
        public Gtk.Statusbar statusbar { get; }
        [NoAccessorMethod]
        public string statusbar_text { get; set; }
        public Midori.WebSettings settings { get; set; }
        [NoAccessorMethod]
        public GLib.Object bookmarks { get; set; }
        [NoAccessorMethod]
        public GLib.Object trash { get; set; }
        [NoAccessorMethod]
        public GLib.Object search_engines { get; set; }
        [NoAccessorMethod]
        public GLib.Object history { get; set; }
        [NoAccessorMethod]
        public bool show_tabs { get; set; }

        public signal Browser new_window (Browser? browser);
        [HasEmitter]
        public signal void add_tab (Gtk.Widget tab);
        [HasEmitter]
        public signal void remove_tab (Gtk.Widget tab);
        [HasEmitter]
        public signal void activate_action (string name);
        public signal void add_download (GLib.Object download);
        public signal void populate_tool_menu (Gtk.Menu menu);
        [HasEmitter]
        public signal void quit ();
    }

    public class Extension : GLib.Object {
        [CCode (has_construct_function = false)]
        public Extension ();
        public unowned Midori.App get_app ();

        [NoAccessorMethod]
        public string name { get; set; }
        [NoAccessorMethod]
        public string description { get; set; }
        [NoAccessorMethod]
        public string version { get; set; }
        [NoAccessorMethod]
        public string authors { get; set; }

        public signal void activate (Midori.App app);
        public signal void deactivate ();
    }

    public class WebSettings : WebKit.WebSettings {
        public WebSettings ();
    }
}

