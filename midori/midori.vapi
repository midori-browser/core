/* Copyright (C) 2010 Christian Dywan <christian@twotoasts.de>
   This file is licensed under the terms of the expat license, see the file EXPAT. */

public const string PACKAGE_NAME;

[CCode (cprefix = "Midori", lower_case_cprefix = "midori_")]
namespace Midori {
    public const string VERSION_SUFFIX;
    namespace Stock {
        public const string WEB_BROWSER;
        public const string TRANSFER;
        public const string PLUGINS;
    }

    [CCode (cheader_filename = "midori/midori.h")]
    public static unowned Midori.Browser web_app_new (string? config,
        string? webapp, [CCode (array_length = false)] string[]? uris, [CCode (array_length = false)] string[]? commands, int reset, string? block);
    public static unowned Midori.Browser private_app_new (string? config,
        string? webapp, [CCode (array_length = false)] string[]? uris, [CCode (array_length = false)] string[]? commands, int reset, string? block);
    public static unowned App normal_app_new (string? config, string nickname, bool diagnostic,
        string? webapp, [CCode (array_length = false)] string[]? uris, [CCode (array_length = false)] string[]? commands, int reset, string? block);
    public static void normal_app_on_quit (App app);

    [CCode (cheader_filename = "midori/midori.h")]
    public class App : GLib.Object {
        public App (string? name=null);
        public static void setup ([CCode (array_length_pos = 0.9)] ref unowned string[] args, [CCode (array_length = false)] GLib.OptionEntry[]? entries);
        public static void set_instance_is_running (bool is_running);
        public Browser create_browser ();
        public GLib.List<weak Browser> get_browsers ();

        [NoAccessorMethod]
        public string name { get; set; }
        [NoAccessorMethod]
        public Midori.WebSettings settings { owned get; set; }
        [NoAccessorMethod]
        public Katze.Array bookmarks { get; set; }
        [NoAccessorMethod]
        public Katze.Array trash { get; set; }
        [NoAccessorMethod]
        public Katze.Array search_engines { get; set; }
        [NoAccessorMethod]
        public Katze.Array history { get; set; }
        [NoAccessorMethod]
        public Katze.Array extensions { owned get; set; }
        [NoAccessorMethod]
        public Katze.Array browsers { get; }
        public Browser? browser { get; }

        [HasEmitter]
        public signal void add_browser (Browser browser);
        public signal void remove_browser (Browser browser);
        [HasEmitter]
        public signal void quit ();
        public void send_notification (string title, string message);
    }

    [CCode (cheader_filename = "midori/midori.h")]
    public class Browser : Gtk.Window {
        public Browser ();
        public unowned Gtk.Widget add_item (Katze.Item item);
        public unowned Gtk.Widget add_uri (string uri);
        public unowned View get_nth_tab (int n);
        public GLib.List<weak View> get_tabs ();
        public void block_action (Gtk.Action action);
        public void unblock_action (Gtk.Action action);
        public unowned Gtk.ActionGroup get_action_group ();
        public static unowned Browser get_for_widget (Gtk.Widget widget);
        public unowned string[] get_toolbar_actions ();
        public Katze.Array proxy_array { get; }

        [NoAccessorMethod]
        public Gtk.MenuBar menubar { owned get; }
        [NoAccessorMethod]
        public Gtk.Toolbar navigationbar { owned get; }
        [NoAccessorMethod]
        public Gtk.Notebook notebook { owned get; }
        [NoAccessorMethod]
        public Midori.Panel panel { owned get; }
        [NoAccessorMethod]
        public string uri { owned get; set; }
        public Gtk.Widget? tab { get; set; }
        [NoAccessorMethod]
        public uint load_status { get; }
        [NoAccessorMethod]
        public Gtk.Statusbar statusbar { owned get; }
        [NoAccessorMethod]
        public string statusbar_text { owned get; set; }
        [NoAccessorMethod]
        public Midori.WebSettings settings { owned get; set; }
        [NoAccessorMethod]
        public Katze.Array? bookmarks { owned get; set; }
        [NoAccessorMethod]
        public Katze.Array? trash { owned get; set; }
        [NoAccessorMethod]
        public Katze.Array? search_engines { owned get; set; }
        [NoAccessorMethod]
        public Katze.Array? history { owned get; set; }
        [NoAccessorMethod]
        public bool show_tabs { get; set; }

        public signal Browser new_window (Browser? browser);
        [HasEmitter]
        public signal void add_tab (View tab);
        public signal void remove_tab (View tab);
        public void close_tab (View tab);
        public signal void switch_tab (View? old_view, View? new_view);
        [HasEmitter]
        public signal void activate_action (string name);
        public signal void add_download (WebKit.Download download);
        public signal void populate_tool_menu (Gtk.Menu menu);
        [HasEmitter]
        public signal void quit ();
        public signal void send_notification (string title, string message);
        public static void update_history (Katze.Item item, string type, string event);
    }

    [CCode (cheader_filename = "midori/midori.h")]
    public class Panel : Gtk.HBox {
        public Panel ();
        public int append_page (Midori.Viewable viewable);
    }

    [CCode (cheader_filename = "midori/midori.h")]
    public class Extension : GLib.Object {
        [CCode (has_construct_function = false)]
        public Extension ();
        public unowned Midori.App get_app ();

        public void install_boolean (string name, bool default_value);
        public void install_integer (string name, int default_value);
        public void install_string (string name, string default_value);

        public bool get_boolean (string name);
        public int get_integer (string name);
        public unowned string get_string (string name);

        public void set_boolean (string name, bool value);
        public void set_integer (string name, int value);
        public void set_string (string name, string value);

        [NoAccessorMethod]
        public string? stock_id { get; set; }
        [NoAccessorMethod]
        public string name { owned get; set; }
        [NoAccessorMethod]
        public string description { owned get; set; }
        [NoAccessorMethod]
        public bool use_markup { get; set; }
        [NoAccessorMethod]
        public string version { owned get; set; }
        [NoAccessorMethod]
        public string authors { owned get; set; }
        [NoAccessorMethod]
        public string website { owned get; set; }
        [NoAccessorMethod]
        public string key { owned get; set; }

        public signal void activate (Midori.App app);
        public bool is_prepared ();
        public bool is_active ();
        public signal void deactivate ();
        public signal void open_preferences ();

        public static void load_from_folder (Midori.App app, [CCode (array_length = false)] string[]? keys, bool activate);
    }

    [CCode (cheader_filename = "midori/midori.h")]
    public class View : Tab {
        [CCode (type = "GtkWidget*")]
        public View.with_title (string? title=null, WebSettings? settings=null
            , bool append=false);
        public void set_uri (string uri);
        public void set_html (string data, string? uri=null, GLib.Object? frame=null);
        public unowned string get_display_uri ();
        public unowned string get_display_title ();
        public unowned string get_icon_uri ();
        public unowned string get_link_uri ();
        public bool has_selection ();
        public string get_selected_text ();
        public Gtk.MenuItem get_proxy_menu_item ();
        public Gtk.Widget duplicate ();
        public Gtk.Menu get_tab_menu ();
        public Pango.EllipsizeMode get_label_ellipsize ();
        public Gtk.Label get_proxy_tab_label ();
        public unowned Katze.Item get_proxy_item ();
        public void search_text (string text, bool case_sensitive, bool forward);
        public bool execute_script (string script, out string exception);
        public Gdk.Pixbuf get_snapshot (int width, int height);
        public void populate_popup (Gtk.Menu menu, bool manual);
        public void reload (bool from_cache);

        public string title { get; }
        public Gdk.Pixbuf icon { get; }
        public bool minimized { get; }
        public float zoom_level { get; }
        public Katze.Array news_feeds { get; }
        [NoAccessorMethod]
        public WebSettings settings { owned get; set; }
        public GLib.Object net { get; }

        [HasEmitter]
        public signal bool download_requested (WebKit.Download download);

    }

    [CCode (cheader_filename = "midori/midori.h")]
    public class LocationAction : Gtk.Action {
        public static string render_uri ([CCode (array_length = false)] string[] keys, string uri_escaped);
        public static string render_title ([CCode (array_length = false)] string[] keys, string title);
    }

    [CCode (cheader_filename = "midori/midori.h")]
    public class SearchAction : Gtk.Action {
        public static Katze.Item? get_engine_for_form (WebKit.WebView web_view, Pango.EllipsizeMode ellipsize);
    }

    [CCode (cheader_filename = "midori/midori-view.h", cprefix = "MIDORI_DOWNLOAD_")]
    public enum DownloadType {
        CANCEL,
        OPEN,
        SAVE,
        SAVE_AS,
        OPEN_IN_VIEWER
    }

    [CCode (cheader_filename = "midori/midori-view.h", cprefix = "MIDORI_DELAY_")]
    public enum Delay {
        UNDELAYED,
        DELAYED,
        PENDING_UNDELAY,
    }

    [CCode (cheader_filename = "midori/midori.h")]
    public class WebSettings : Midori.Settings {
        public WebSettings ();
        [NoAccessorMethod]
        public MidoriStartup load_on_startup { get; set; }
        public static bool has_plugin_support ();
        public static bool skip_plugin (string path);
    }

    [CCode (cheader_filename = "midori/midori-websettings.h", cprefix = "MIDORI_STARTUP_")]
    public enum MidoriStartup {
        BLANK_PAGE,
        HOMEPAGE,
        LAST_OPEN_PAGES,
        DELAYED_PAGES
    }

    [CCode (cheader_filename = "midori/sokoke.h", lower_case_cprefix = "sokoke_")]
    namespace Sokoke {
        public static uint gtk_action_count_modifiers (Gtk.Action action);
    }
}

