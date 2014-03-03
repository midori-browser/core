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
        Config config;
        HashTable<string, Directive?> cache;

#if HAVE_WEBKIT2
        public Extension.WebExtension (WebKit.WebExtension web_extension) {
            /* FIXME: mode, config */
            Midori.Paths.init (Midori.RuntimeMode.NORMAL, null);
            init ();
            web_extension.page_created.connect (page_created);
        }

        void page_created (WebKit.WebPage web_page) {
            web_page.send_request.connect (send_request);
        }

        bool send_request (WebKit.WebPage web_page, WebKit.URIRequest request, WebKit.URIResponse? redirected_response) {
            return request_handled (web_page.uri, request.uri);
        }
#endif

        public Extension () {
            GLib.Object (name: _("Advertisement blocker"),
                         description: _("Block advertisements according to a filter list"),
                         version: "2.0",
                         authors: "Christian Dywan <christian@twotoasts.de>");
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
            column.set_cell_data_func (renderer_toggle, (column, renderer, model, iter) => {
                Subscription sub;
                liststore.get (iter, 0, out sub);
                renderer.set ("active", sub.active,
                              "sensitive", !sub.uri.has_suffix ("custom.list"));
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
                renderer.set ("text", sub.uri);
            });
            treeview.append_column (column);

            var scrolled = new Gtk.ScrolledWindow (null, null);
            scrolled.set_policy (Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC);
            scrolled.add (treeview);
            vbox.pack_start (scrolled);

            foreach (Subscription sub in config)
                liststore.insert_with_values (null, 0, 0, sub);
            // TODO: row-inserted row-changed row-deleted
            // TODO vbox with add/ edit/ remove/ down/ up

            dialog.get_content_area ().show_all ();

            dialog.response.connect ((response)=>{ dialog.destroy (); });
            dialog.show ();
        }

        void extension_activated (Midori.App app) {
#if HAVE_WEBKIT2
            string cache_dir = Midori.Paths.get_cache_dir ();
            string wk2path = Path.build_path (Path.DIR_SEPARATOR_S, cache_dir, "wk2ext");
            Midori.Paths.mkdir_with_parents (wk2path);
            string filename = "libadblock." + GLib.Module.SUFFIX;
            var wk2link = File.new_for_path (wk2path).get_child (filename);
            var library = File.new_for_path (Midori.Paths.get_lib_path (PACKAGE_NAME)).get_child (filename);
            try {
                wk2link.make_symbolic_link (library.get_path ());
            } catch (IOError.EXISTS exist_error) {
                /* It's no error if the file already exists. */
            } catch (Error error) {
                critical ("Failed to create WebKit2 link: %s", error.message);
            }
#endif
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
            view.navigation_requested.connect (navigation_requested);
#if !HAVE_WEBKIT2
            view.web_view.resource_request_starting.connect (resource_requested);
            view.notify["load-status"].connect ((pspec) => {
                if (view.load_status == Midori.LoadStatus.FINISHED)
                    inject_css (view, view.uri);
            });
#endif
            view.context_menu.connect (context_menu);
        }

        void context_menu (WebKit.HitTestResult hit_test_result, Midori.ContextAction menu) {
            string label, uri;
            if ((hit_test_result.context & WebKit.HitTestResultContext.IMAGE) != 0) {
                label = _("Bl_ock image");
                uri = hit_test_result.image_uri;
            } else if ((hit_test_result.context & WebKit.HitTestResultContext.LINK) != 0)  {
                label = _("Bl_ock link");
                uri = hit_test_result.link_uri;
            } else
                return;
            var action = new Gtk.Action ("BlockElement", label, null, null);
            action.activate.connect ((action) => {
                edit_rule_dialog (uri);
            });
            menu.add (action);
        }

        void edit_rule_dialog (string uri) {
            var dialog = new Gtk.Dialog.with_buttons (_("Edit rule"),
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
            entry.set_text (uri);
            hbox.pack_start (entry, true, true, 0);

            dialog.get_content_area ().show_all ();

            dialog.set_default_response (Gtk.ResponseType.ACCEPT);
            if (dialog.run () != Gtk.ResponseType.ACCEPT)
                return;

            string new_rule = entry.get_text ();
            dialog.destroy ();
            config.add_custom_rule (new_rule);
        }

        bool navigation_requested (Midori.Tab tab, string uri) {
            if (uri.has_prefix ("abp:")) {
                string new_uri = uri.replace ("abp://", "abp:");
                if (new_uri.has_prefix ("abp:subscribe?location=")) {
                    /* abp://subscripe?location=http://example.com&title=foo */
                    string[] parts = new_uri.substring (23, -1).split ("&", 2);
                    open_dialog (parts[0]);
                    return true;
                }
            }
            return false;
        }

#if !HAVE_WEBKIT2
        void resource_requested (WebKit.WebView web_view, WebKit.WebFrame frame,
            WebKit.WebResource resource, WebKit.NetworkRequest request, WebKit.NetworkResponse? response) {

            if (request_handled (web_view.uri, request.uri))
                request.set_uri ("about:blank");
        }

        void inject_css (Midori.View view, string page_uri) {
            /* Don't block ads on internal pages */
            if (!Midori.URI.is_http (page_uri))
                return;
            string domain = Midori.URI.parse_hostname (page_uri, null);
            string[] subdomains = domain.split (".");
            if (subdomains == null)
                return;
            int cnt = subdomains.length - 1;
            var subdomain = new StringBuilder (subdomains[cnt]);
            subdomain.prepend_c ('.');
            cnt--;
            var code = new StringBuilder ();
            bool debug_element = "adblock:element" in (Environment.get_variable ("MIDORI_DEBUG") ?? "");
            string hider_css;

            /* Hide elements that were blocked, otherwise we will get "broken image" icon */
            cache.foreach ((key, val) => {
                if (val == Adblock.Directive.BLOCK)
                    code.append ("img[src*=\"%s\"] , iframe[src*=\"%s\"] , ".printf (key, key));
            });
            if (debug_element)
                hider_css = " { background-color: red; border: 4px solid green; }";
            else
                hider_css = " { visiblility: hidden; width: 0; height: 0; }";

            code.truncate (code.len -3);
            code.append (hider_css);
            if (debug_element)
                stdout.printf ("hider css: %s\n", code.str);
            view.inject_stylesheet (code.str);

            code.erase ();
            int blockscnt = 0;
            while (cnt >= 0) {
                subdomain.prepend (subdomains[cnt]);
                string? style = null;
                foreach (Subscription sub in config) {
                    foreach (var feature in sub) {
                        if (feature is Adblock.Element) {
                            style = (feature as Adblock.Element).lookup (subdomain.str);
                            break;
                        }
                    }
                }
                if (style != null) {
                    code.append (style);
                    code.append_c (',');
                    blockscnt++;
                }
                subdomain.prepend_c ('.');
                cnt--;
            }
            if (blockscnt == 0)
                return;
            code.truncate (code.len - 1);

            if (debug_element)
                hider_css = " { background-color: red !important; border: 4px solid green !important; }";
            else
                hider_css = " { display: none !important }";

            code.append (hider_css);
            view.inject_stylesheet (code.str);
            if (debug_element)
                stdout.printf ("css: %s\n", code.str);
        }
#endif

        internal void init () {
            debug ("Adblock2");

            string config_dir = Midori.Paths.get_extension_config_dir ("adblock");
            config = new Config (config_dir);
            reload_rules ();
        }

        void reload_rules () {
            cache = new HashTable<string, Directive?> (str_hash, str_equal);
            foreach (Subscription sub in config) {
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

            /* Skip adblock on internal pages */
            if (Midori.URI.is_blank (page_uri))
                return false;

            /* Skip adblock on favicons and non http schemes */
            if (!Midori.URI.is_http (request_uri) || request_uri.has_suffix ("favicon.ico"))
                return false;

            Directive? directive = cache.lookup (request_uri);
            if (directive == null) {
                foreach (Subscription sub in config) {
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
    filter = new Adblock.Extension.WebExtension (web_extension);
}
#endif

public Midori.Extension extension_init () {
    return new Adblock.Extension ();
}

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
    { "(facebookLike,", "(facebookLike," }
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
    { "http://google.com", Adblock.Directive.ALLOW }
};

string pretty_directive (Adblock.Directive? directive) {
    if (directive == null)
        return "none";
    return directive.to_string ();
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
    try {
        sub.parse ();
    } catch (Error error) {
        GLib.error (error.message);
    }
    foreach (var pattern in patterns) {
        Adblock.Directive? directive = sub.get_directive (pattern.uri, "");
        if (directive == null)
            directive = Adblock.Directive.ALLOW;
        if (directive != pattern.directive) {
            error ("%s expected for %s but got %s",
                   pretty_directive (pattern.directive), pattern.uri, pretty_directive (directive));
        }
    }
}

struct TestUpdateExample {
    public string content;
    public bool result;
}

 const TestUpdateExample[] examples = {
        { "[Adblock Plus 1.1]\n! Last modified: 05 Sep 2010 11:00 UTC\n! This list expires after 48 hours\n", true },
        { "[Adblock Plus 1.1]\n! Last modified: 05.09.2010 11:00 UTC\n! Expires: 2 days (update frequency)\n", true },
        { "[Adblock Plus 1.1]\n! Updated: 05 Nov 2024 11:00 UTC\n! Expires: 5 days (update frequency)\n", false },
        { "[Adblock]\n! dutchblock v3\n! This list expires after 14 days\n|http://b*.mookie1.com/\n", false },
        { "[Adblock Plus 2.0]\n! Last modification time (GMT): 2012.11.05 13:33\n! Expires: 5 days (update frequency)\n", true },
        { "[Adblock Plus 2.0]\n! Last modification time (GMT): 2012.11.05 13:33\n", true },
        { "[Adblock]\n ! dummy,  i dont have any dates\n", false }
    };

void test_subscription_update () {
    string uri;
    FileIOStream iostream;
    File file;
    try {
        file = File.new_tmp ("midori_adblock_update_test_XXXXXX", out iostream);
        uri = file.get_uri ();
    } catch (Error error) {
        GLib.error (error.message);
    }
    var sub = new Adblock.Subscription (uri);
    var updater = new Adblock.Updater ();
    sub.add_feature (updater);

    foreach (var example in examples) {
        try {
            file.replace_contents (example.content.data, null, false, FileCreateFlags.NONE, null);
            updater.last_mod_meta = null;
            updater.expires_meta = null;
            sub.parse ();
        } catch (Error error) {
            GLib.error (error.message);
        }
        if (example.result == true)
            assert (updater.needs_updating());
        else
            assert (!updater.needs_updating());
    }
}

public void extension_test () {
    Test.add_func ("/extensions/adblock2/parse", test_adblock_fixup_regexp);
    Test.add_func ("/extensions/adblock2/pattern", test_adblock_pattern);
    Test.add_func ("/extensions/adblock2/update", test_subscription_update);
}

