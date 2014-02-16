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
    public enum Directive {
        ALLOW,
        BLOCK
    }

    public class Extension : Midori.Extension {
        HashTable<string, Directive?> cache;
        GLib.List <Subscription> subscriptions;

#if HAVE_WEBKIT2
        public Extension (WebKit.WebExtension web_extension) {
            init ();
            web_extension.page_created.connect (page_created);
        }

        void page_created (WebKit.WebPage web_page) {
            web_page.send_request.connect (send_request);
        }

        bool send_request (WebKit.WebPage web_page, WebKit.URIRequest request, WebKit.URIResponse? redirected_response) {
            return request_handled (web_page.uri, request.uri);
        }
#else
        public Extension () {
            GLib.Object (name: _("Advertisement blocker"),
                         description: _("Block advertisements according to a filter list"),
                         version: "2.0",
                         authors: "Christian Dywan <christian@twotoasts.de>");
            // TODO: install_string_list ("filters", null);
            activate.connect (extension_activated);
            open_preferences.connect (extension_preferences);
        }

        void extension_preferences () {
            open_dialog (null);
        }

        void open_dialog (string? uri) {
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
            /* TODO: Help */
            dialog.set_response_sensitive (Gtk.ResponseType.HELP, false);

            var hbox = new Gtk.HBox (false, 0);
            (dialog.get_content_area () as Gtk.Box).pack_start (hbox, true, true, 12);
            var vbox = new Gtk.VBox (false, 0);
            hbox.pack_start (vbox, true, true, 4);
            var button = new Gtk.Label (null);
            string description = """
                Type the address of a preconfigured filter list in the text entry
                and click "Add" to add it to the list.
                You can find more lists at %s %s.
                """.printf (
                "<a href=\"http://adblockplus.org/en/subscriptions\">adblockplus.org/en/subscriptions</a>",
                "<a href=\"http://easylist.adblockplus.org/\">easylist.adblockplus.org</a>");
            button.activate_link.connect ((uri)=>{
                var browser = Midori.Browser.get_for_widget (button);
                var view = browser.add_uri (uri);
                browser.tab = view;
                return true;
            });
            button.set_markup (description);
            button.set_line_wrap (true);
            vbox.pack_start (button, false, false, 4);

            var entry = new Gtk.Entry ();
            if (uri != null)
                entry.set_text (uri);
            vbox.pack_start (entry, false, false, 4);

            var liststore = new Gtk.ListStore (1, typeof (Subscription));
            var treeview = new Gtk.TreeView.with_model (liststore);
            treeview.set_headers_visible (false);
            var column = new Gtk.TreeViewColumn ();
            var renderer_toggle = new Gtk.CellRendererToggle ();
            column.pack_start (renderer_toggle, false);
            // TODO: column.set_cell_data_func
            // TODO: column.toggled.connect
            treeview.append_column (column);

            column = new Gtk.TreeViewColumn ();
            var renderer_text = new Gtk.CellRendererText ();
            column.pack_start (renderer_text, false);
            renderer_text.set ("editable", true);
            // TODO: renderer_text.edited.connect
            column.set_cell_data_func (renderer_text, (column, renderer, model, iter) => {
                Subscription sub;
                liststore.get (iter, 0, out sub);
                renderer.set ("text", sub.uri);
            });
            treeview.append_column (column);

            var scrolled = new Gtk.ScrolledWindow (null, null);
            scrolled.set_policy (Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC);
            scrolled.add (treeview);
            vbox.pack_start (scrolled);

            foreach (Subscription sub in subscriptions)
                liststore.insert_with_values (null, 0, 0, sub);
            // TODO: row-inserted row-changed row-deleted
            // TODO vbox with add/ edit/ remove/ down/ up

            dialog.get_content_area ().show_all ();

            dialog.response.connect ((response)=>{ dialog.destroy (); });
            dialog.show ();
        }

        void extension_activated (Midori.App app) {
            init ();
            foreach (var browser in app.get_browsers ())
                browser_added (browser);
            app.add_browser.connect (browser_added);
        }

        void browser_added (Midori.Browser browser) {
            foreach (var tab in browser.get_tabs ())
                tab_added (tab);
            browser.add_tab.connect (tab_added);
        }

        void tab_added (Midori.View view) {
            view.web_view.resource_request_starting.connect (resource_requested);
            view.web_view.navigation_policy_decision_requested.connect (navigation_requested);
        }

        void resource_requested (WebKit.WebView web_view, WebKit.WebFrame frame,
            WebKit.WebResource resource, WebKit.NetworkRequest request, WebKit.NetworkResponse? response) {

            if (request_handled (web_view.uri, request.uri))
                request.set_uri ("about:blank");
        }

        bool navigation_requested (WebKit.WebFrame frame, WebKit.NetworkRequest request,
            WebKit.WebNavigationAction action, WebKit.WebPolicyDecision decision) {

            string uri = request.uri;
            if (uri.has_prefix ("abp:")) {
                uri = uri.replace ("abp://", "abp:");
                if (uri.has_prefix ("abp:subscribe?location=")) {
                    /* abp://subscripe?location=http://example.com&title=foo */
                    string[] parts = uri.substring (23, -1).split ("&", 2);
                    decision.ignore ();
                    open_dialog (parts[0]);
                    return true;
                }
            }
            return false;
        }
#endif

        internal void init () {
            debug ("Adblock2");
            subscriptions = new GLib.List<Subscription> ();

#if HAVE_WEBKIT2
            string config_dir = GLib.Path.build_filename (GLib.Environment.get_user_config_dir (), "midori", "extensions", "libadblock2.so");
#else
            string? config_dir = get_config_dir ();
#endif

            if (config_dir != null) {
                string custom_list = GLib.Path.build_filename (config_dir, "custom.list");
                try {
                    subscriptions.append (new Subscription (Filename.to_uri (custom_list, null)));
                } catch (Error error) {
                    warning ("Failed to add custom list %s: %s", custom_list, error.message);
                }
            }

            string filename = GLib.Path.build_filename (config_dir, "config"); // use midori vapi
            var keyfile = new GLib.KeyFile ();
            try {
                keyfile.load_from_file (filename, GLib.KeyFileFlags.NONE);
                string[] filters = keyfile.get_string_list ("settings", "filters");
                foreach (string filter in filters) {
                    Subscription sub = new Subscription (filter);
                    subscriptions.append (sub);
                }
            } catch (FileError.NOENT exist_error) {
                /* It's no error if no config file exists */
            } catch (GLib.Error settings_error) {
                stderr.printf ("Error reading settings from %s: %s\n", filename, settings_error.message);
            }

            reload_rules ();
        }

        void reload_rules () {
            cache = new HashTable<string, Directive?> (str_hash, str_equal);
            foreach (Subscription sub in subscriptions) {
                try {
                    sub.parse ();
                } catch (GLib.Error error) {
                    warning ("Error parsing %s: %s", sub.uri, error.message);
                }
            }
       }

        bool request_handled (string page_uri, string request_uri) {
            /* Always allow the main page */
            if (request_uri == page_uri)
                return false;

            Directive? directive = cache.lookup (request_uri);
            if (directive == null) {
                foreach (Subscription sub in subscriptions) {
                    directive = sub.get_directive (request_uri, page_uri);
                    if (directive != null)
                        break;
                }
                if (directive == null)
                    directive = Directive.ALLOW;
                cache.insert (request_uri, directive);
            }
            return directive == Directive.BLOCK;
        }
    }

    static void debug (string format, ...) {
        bool debug_match = "adblock:match" in (Environment.get_variable ("MIDORI_DEBUG") ?? "");
        if (!debug_match)
            return;

        var args = va_list ();
        stdout.vprintf (format + "\n", args);
    }

    internal static string? fixup_regex (string prefix, string? src) {
        if (src == null)
            return null;

        var fixed = new StringBuilder ();
        fixed.append(prefix);

        uint i = 0;
        if (src[0] == '*')
            i++;
        uint l = src.length;
        while (i < l) {
            char c = src[i];
            switch (c) {
                case '*':
                    fixed.append (".*"); break;
                case '|':
                case '^':
                case '+':
                    break;
                case '?':
                case '[':
                case ']':
                    fixed.append_printf ("\\%c", c); break;
                default:
                    fixed.append_c (c); break;
            }
            i++;
        }
        return fixed.str;
    }
}

#if HAVE_WEBKIT2
Adblock.Extension? filter;
public static void webkit_web_extension_initialize (WebKit.WebExtension web_extension) {
    filter = new Adblock.Extension (web_extension);
}
#else
public Midori.Extension extension_init () {
    return new Adblock.Extension ();
}
#endif

// FIXME: Allow test to build with WK2
#if !HAVE_WEBKIT2
struct TestCaseLine {
    public string line;
    public string fixed;
}

const TestCaseLine[] lines = {
    { null, null },
    { "!", "!" },
    { "@@", "@@" },
    { "##", "##" },
    { "[", "\\[" },
    { "+advert/", "advert/" },
    { "*foo", "foo" },
    { "f*oo", "f.*oo" },
    { "?foo", "\\?foo" },
    { "foo?", "foo\\?" },
    { ".*foo/bar", "..*foo/bar" },
    { "http://bla.blub/*", "http://bla.blub/.*" },
    { "bag?r[]=*cpa", "bag\\?r\\[\\]=.*cpa" },
    { "(facebookLike,", "(facebookLike," },
};

void test_adblock_fixup_regexp () {
    foreach (var line in lines) {
        Katze.assert_str_equal (line.line, Adblock.fixup_regex ("", line.line), line.fixed);
    }
}

struct TestCasePattern {
    public string uri;
    public Adblock.Directive directive;
}

const TestCasePattern[] patterns = {
    { "http://www.engadget.com/_uac/adpage.html", Adblock.Directive.BLOCK },
    { "http://test.dom/test?var=1", Adblock.Directive.BLOCK },
    { "http://ads.foo.bar/teddy", Adblock.Directive.BLOCK },
    { "http://ads.fuu.bar/teddy", Adblock.Directive.ALLOW },
    { "https://ads.bogus.name/blub", Adblock.Directive.BLOCK },
    // FIXME { "http://ads.bla.blub/kitty", Adblock.Directive.BLOCK },
    // FIXME { "http://ads.blub.boing/soda", Adblock.Directive.BLOCK },
    { "http://ads.foo.boing/beer", Adblock.Directive.ALLOW },
    { "https://testsub.engine.adct.ru/test?id=1", Adblock.Directive.BLOCK },
    { "http://test.ltd/addyn/test/test?var=adtech;&var2=1", Adblock.Directive.BLOCK },
    { "http://add.doubleclick.net/pfadx/aaaa.mtvi", Adblock.Directive.BLOCK },
    { "http://add.doubleclick.net/pfadx/aaaa.mtv", Adblock.Directive.ALLOW },
    { "http://objects.tremormedia.com/embed/xml/list.xml?r=", Adblock.Directive.BLOCK },
    { "http://qq.videostrip.c/sub/admatcherclient.php", Adblock.Directive.ALLOW },
    { "http://qq.videostrip.com/sub/admatcherclient.php", Adblock.Directive.BLOCK },
    { "http://qq.videostrip.com/sub/admatcherclient.php", Adblock.Directive.BLOCK },
    { "http://br.gcl.ru/cgi-bin/br/test", Adblock.Directive.BLOCK },
    { "https://bugs.webkit.org/buglist.cgi?query_format=advanced&short_desc_type=allwordssubstr&short_desc=&long_desc_type=substring&long_desc=&bug_file_loc_type=allwordssubstr&bug_file_loc=&keywords_type=allwords&keywords=&bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&emailassigned_to1=1&emailtype1=substring&email1=&emailassigned_to2=1&emailreporter2=1&emailcc2=1&emailtype2=substring&email2=&bugidtype=include&bug_id=&votes=&chfieldfrom=&chfieldto=Now&chfieldvalue=&query_based_on=gtkport&field0-0-0=keywords&type0-0-0=anywordssubstr&value0-0-0=Gtk%20Cairo%20soup&field0-0-1=short_desc&type0-0-1=anywordssubstr&value0-0-1=Gtk%20Cairo%20soup%20autoconf%20automake%20autotool&field0-0-2=component&type0-0-2=equals&value0-0-2=WebKit%20Gtk", Adblock.Directive.ALLOW },
    { "http://www.engadget.com/2009/09/24/google-hits-android-rom-modder-with-a-cease-and-desist-letter/", Adblock.Directive.ALLOW },
    { "http://karibik-invest.com/es/bienes_raices/search.php?sqT=19&sqN=&sqMp=&sqL=0&qR=1&sqMb=&searchMode=1&action=B%FAsqueda", Adblock.Directive.ALLOW },
    { "http://google.com", Adblock.Directive.ALLOW },
};

string pretty_directive (Adblock.Directive? directive) {
    if (directive == null)
        return "none";
    if (directive == Adblock.Directive.ALLOW)
        return "allow";
    return "block";
}
 
void test_adblock_pattern () {
    string path = Midori.Paths.get_res_filename ("adblock.list");
    string uri;
    try {
        uri = Filename.to_uri (path, null);
    } catch (Error error) {
        GLib.error (error.message);
    }
    var sub = new Adblock.Subscription (uri);
    stderr.printf ("sub %s\n", uri);
    try {
        sub.parse ();
    } catch (Error error) {
        GLib.error (error.message);
    }
    foreach (var pattern in patterns) {
        stderr.printf ("checking %s\n", pattern.uri);
        Adblock.Directive? directive = sub.get_directive (pattern.uri, "");
        if (directive == null)
            directive = Adblock.Directive.ALLOW;
        if (directive != pattern.directive) {
            error ("%s expected for %s but got %s",
                   pretty_directive (pattern.directive), pattern.uri, pretty_directive (directive));
        }
    }
}

void test_subscription_update () {
}

void test_subscription_header_parse () {
    string? path;
    try {
        /* FIXME: use temp dir and file */
        path = "/tmp/subscription-XXXXXX";
        File sub_file = File.new_for_path (path);
        string uri = sub_file.get_uri ();

        string contents = """
        [Adblock Plus 2.0]
! Checksum: Wo8YP7cu8aORCwMkFlcR2g
! Version: 201402151830
! Title: EasyList
! Last modified: 21 Feb 2012 18:30 UTC
! Expires: 4 days (update frequency)
! Homepage: https://easylist.adblockplus.org/
! Licence: https://easylist-downloads.adblockplus.org/COPYING
!
!-----------------------General advert blocking filters-----------------------!
! *** easylist:easylist/easylist_general_block.txt ***
&ad_box_
! comment
&ad_channel=
""";

        sub_file.replace_contents (contents.data, null, false, FileCreateFlags.NONE, null);
        Adblock.Subscription sub = new Adblock.Subscription (uri);
        sub.parse ();
        assert (sub.needs_updating());
    } catch (Error error) {
        GLib.error (error.message);
    }

}

public void extension_test () {
    Test.add_func ("/extensions/adblock2/parse", test_adblock_fixup_regexp);
    Test.add_func ("/extensions/adblock2/pattern", test_adblock_pattern);
    Test.add_func ("/extensions/adblock2/update", test_subscription_update);
    Test.add_func ("/extensions/adblock2/header", test_subscription_header_parse);
}
#endif

