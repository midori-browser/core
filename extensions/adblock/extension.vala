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

    public enum State {
        ENABLED,
        DISABLED,
        BLOCKED
    }

    public class Extension : Midori.Extension {
        internal Config config;
        internal Subscription custom;
        internal HashTable<string, Directive?> cache;
        internal StatusIcon status_icon;
        internal SubscriptionManager manager;
        internal State state;

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
            activate.connect (extension_activated);
            open_preferences.connect (extension_preferences);
        }

        void extension_preferences () {
            manager.add_subscription (null);
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

            var toggle_button = new StatusIcon.IconButton ();
            toggle_button.set_status (config.enabled ? "enabled" : "disabled");
            browser.statusbar.pack_start (toggle_button, false, false, 3);
            toggle_button.show ();
            toggle_button.clicked.connect (status_icon.icon_clicked);
            status_icon.toggle_buttons.append (toggle_button);
        }


        void tab_added (Midori.View view) {
            view.web_view.resource_request_starting.connect (resource_requested);
            view.web_view.navigation_policy_decision_requested.connect (navigation_requested);
            view.notify["load-status"].connect ((pspec) => {
                if (config.enabled) {
                    if (view.load_status == Midori.LoadStatus.FINISHED)
                        inject_css (view, view.uri);
                }
            });
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
                CustomRulesEditor custom_rules_editor = new CustomRulesEditor (custom);
                custom_rules_editor.set_uri (uri);
                custom_rules_editor.show();
            });
            menu.add (action);
        }

        Adblock.State adblock_get_state (Adblock.Directive directive)
        {
            if (directive == Directive.BLOCK)
                return State.BLOCKED;
            if (config.enabled)
                return State.ENABLED;
            else
                return State.DISABLED;
        }

        void resource_requested (WebKit.WebView web_view, WebKit.WebFrame frame,
            WebKit.WebResource resource, WebKit.NetworkRequest request, WebKit.NetworkResponse? response) {

            if (request_handled (web_view.uri, request.uri)) {
                request.set_uri ("about:blank");
                state = adblock_get_state (get_directive_for_uri (web_view.uri));
                status_icon.set_state (state);
            }
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
                    manager.add_subscription (parts[0]);
                    return true;
                }
            }
            state = adblock_get_state (get_directive_for_uri (request.uri));
            status_icon.set_state (state);
            return false;
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
            string hider_css;

            bool debug_element;
            if ("adblock:element" in (Environment.get_variable ("MIDORI_DEBUG") ?? ""))
                debug_element = true;
            else
                debug_element = status_icon.debug_element_toggled;

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
            cache = new HashTable<string, Directive?> (str_hash, str_equal);
            load_config ();
            status_icon = new StatusIcon (config);
            manager = new SubscriptionManager (config);
            foreach (Subscription sub in config) {
                try {
                    sub.parse ();
                } catch (GLib.Error error) {
                    warning ("Error parsing %s: %s", sub.uri, error.message);
                }
            }
            config.notify["size"].connect (subscriptions_added_removed);
            manager.description_label.activate_link.connect (open_link);
        }

        bool open_link (string uri) {
            var browser = get_app ().browser;
            var view = browser.add_uri (uri);
            browser.tab = view;
            return true;
        }

        void subscriptions_added_removed (ParamSpec pspec) {
            cache.remove_all ();
        }

        void load_config () {
            string config_dir = Midori.Paths.get_extension_config_dir ("adblock");
            string presets = Midori.Paths.get_extension_preset_filename ("adblock", "config");
            string filename = Path.build_filename (config_dir, "config");
            config = new Config (filename, presets);
            string custom_list = GLib.Path.build_filename (config_dir, "custom.list");
            try {
                custom = new Subscription (Filename.to_uri (custom_list, null));
                custom.mutable = false;
                custom.title = _("Custom");
                config.add (custom);
            } catch (Error error) {
                custom = null;
                warning ("Failed to add custom list %s: %s", custom_list, error.message);
            }
        }

        public Adblock.Directive get_directive_for_uri (string request_uri, string? page_uri = null) {
            if (!config.enabled)
                return Directive.ALLOW;

            if (page_uri != null) {
                /* Always allow the main page */
                if (request_uri == page_uri)
                    return Directive.ALLOW;

                /* Skip adblock on internal pages */
                if (Midori.URI.is_blank (page_uri))
                    return Directive.ALLOW;
            }

            /* Skip adblock on favicons and non http schemes */
            if (!Midori.URI.is_http (request_uri) || request_uri.has_suffix ("favicon.ico"))
                return Directive.ALLOW;

            Directive? directive = cache.lookup (request_uri);
            if (directive == null) {
                foreach (Subscription sub in config) {
                    if (page_uri == null)
                        page_uri = request_uri;
                    directive = sub.get_directive (request_uri, page_uri);
                    if (directive != null)
                        break;
                }
                if (directive == null)
                    directive = Directive.ALLOW;
                cache.insert (request_uri, directive);
                if (directive == Directive.BLOCK)
                    cache.insert (page_uri, directive);
            }
            return directive;
        }

        internal bool request_handled (string page_uri, string request_uri) {
            return get_directive_for_uri (request_uri, page_uri) == Directive.BLOCK;
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

#if !HAVE_WEBKIT2
static string? tmp_folder = null;
string get_test_file (string contents) {
    if (tmp_folder == null)
        tmp_folder = Midori.Paths.make_tmp_dir ("adblockXXXXXX");
    string checksum = Checksum.compute_for_string (ChecksumType.MD5, contents);
    string file = Path.build_path (Path.DIR_SEPARATOR_S, tmp_folder, checksum);
    try {
        FileUtils.set_contents (file, contents, -1);
    } catch (Error file_error) {
        GLib.error (file_error.message);
    }
    return file;
}

struct TestCaseConfig {
    public string content;
    public uint size;
    public bool enabled;
}

const TestCaseConfig[] configs = {
    { "", 0, true },
    { "[settings]", 0, true },
    { "[settings]\nfilters=foo;", 1, true },
    { "[settings]\nfilters=foo;\ndisabled=true", 1, false }
};

void test_adblock_config () {
    assert (new Adblock.Config (null, null).size == 0);

    foreach (var conf in configs) {
        var config = new Adblock.Config (get_test_file (conf.content), null);
        if (config.size != conf.size)
            error ("Wrong size %s rather than %s:\n%s",
                   config.size.to_string (), conf.size.to_string (), conf.content);
        if (config.enabled != conf.enabled)
            error ("Wrongly got enabled=%s rather than %s:\n%s",
                   config.enabled.to_string (), conf.enabled.to_string (), conf.content);
    }
}

struct TestCaseSub {
    public string uri;
    public bool active;
}

const TestCaseSub[] subs = {
    { "http://foo.com", true },
    { "http://bar.com", false },
    { "https://spam.com", true },
    { "https://eggs.com", false },
    { "file:///bla", true },
    { "file:///blub", false }
};

void test_adblock_subs () {
    var config = new Adblock.Config (get_test_file ("""
[settings]
filters=http://foo.com;http-//bar.com;https://spam.com;http-://eggs.com;file:///bla;file-///blub;http://foo.com;
"""), null);

    assert (config.enabled);
    foreach (var sub in subs) {
        bool found = false;
        foreach (var subscription in config) {
            if (subscription.uri == sub.uri) {
                assert (subscription.active == sub.active);
                found = true;
            }
        }
        if (!found)
            error ("%s not found", sub.uri);
    }

    /* 6 unique URLs, 1 duplicate */
    assert (config.size == 6);
    /* Duplicates aren't added again either */
    assert (!config.add (new Adblock.Subscription ("https://spam.com")));

    /* Saving the config and loading it should give back identical results */
    config.save ();
    var copy = new Adblock.Config (config.path, null);
    assert (copy.size == config.size);
    assert (copy.enabled == config.enabled);
    for (int i = 0; i < config.size; i++) {
        assert (copy[i].uri == config[i].uri);
        assert (copy[i].active == config[i].active);
    }
    /* Enabled status should be saved and loaded */
    config.enabled = false;
    copy = new Adblock.Config (config.path, null);
    assert (copy.enabled == config.enabled);
    /* Flipping individual active values should be retained after saving */
    foreach (var sub in config)
        sub.active = !sub.active;
    copy = new Adblock.Config (config.path, null);
    for (uint i = 0; i < config.size; i++) {
        if (config[i].active != copy[i].active) {
            string contents;
            try {
                FileUtils.get_contents (config.path, out contents, null);
            } catch (Error file_error) {
                error (file_error.message);
            }
            error ("%s is %s but should be %s:\n%s",
                   copy[i].uri, copy[i].active ? "active" : "disabled", config[i].active ? "active" : "disabled", contents);
        }
    }

    /* Adding and removing works, changes size */
    var s = new Adblock.Subscription ("http://en.de");
    assert (config.add (s));
    assert (config.size == 7);
    config.remove (s);
    assert (config.size == 6);
    /* If it was removed before we should be able to add it again */
    assert (config.add (s));
    assert (config.size == 7);
}

void test_adblock_init () {
    /* No config */
    var extension = new Adblock.Extension ();
    extension.init ();
    assert (extension.config.enabled);
    /* Defaults plus custom */
    if (extension.config.size != 3)
        error ("Expected 3 initial subs, got %s".printf (
               extension.config.size.to_string ()));
    assert (extension.cache.size () == 0);

    /* Add new subscription */
    string path = Midori.Paths.get_res_filename ("adblock.list");
    string uri;
    try {
        uri = Filename.to_uri (path, null);
    } catch (Error error) {
        GLib.error (error.message);
    }
    var sub = new Adblock.Subscription (uri);
    extension.config.add (sub);
    assert (extension.cache.size () == 0);
    assert (extension.config.size == 4);
    try {
        sub.parse ();
    } catch (GLib.Error error) {
        GLib.error (error.message);
    }
    /* The page itself never hits */
    assert (!extension.request_handled ("https://ads.bogus.name/blub", "https://ads.bogus.name/blub"));
    /* Favicons don't either */
    assert (!extension.request_handled ("https://foo.com", "https://ads.bogus.name/blub/favicon.ico"));
    assert (extension.cache.size () == 0);
    /* Some sanity checks to be sure there's no earlier problem */
    assert (sub.title == "Exercise");
    assert (sub.get_directive ("https://ads.bogus.name/blub", "") == Adblock.Directive.BLOCK);
    /* A rule hit should add to the cache */
    assert (extension.request_handled ("https://foo.com", "https://ads.bogus.name/blub"));
    assert (extension.cache.size () > 0);
    /* Disabled means no request should be handled */
    extension.config.enabled = false;
    assert (!extension.request_handled ("https://foo.com", "https://ads.bogus.name/blub"));
    /* Removing a subscription should clear the cache */
    extension.config.remove (sub);
    assert (extension.cache.size () == 0);
    assert (extension.config.size == 3);
    /* Now let's add a custom rule */
    extension.config.enabled = true;
    extension.custom.add_rule ("*.png");
    assert (!extension.request_handled ("https://foo.com", "http://alpha.beta.com/images/yota.png"));
    assert (extension.cache.size () > 0);
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

string pretty_date (DateTime? date) {
    if (date == null)
        return "N/A";
    return date.to_string ();
}

struct TestUpdateExample {
    public string content;
    public bool result;
    public bool valid;
}

 const TestUpdateExample[] examples = {
        { "[Adblock Plus 1.1]\n! Last modified: 05 Sep 2010 11:00 UTC\n! This list expires after 48 hours\n", true, true },
        { "[Adblock Plus 1.1]\n! Last modified: 05.09.2010 11:00 UTC\n! Expires: 2 days (update frequency)\n", true, true },
        { "[Adblock Plus 1.1]\n! Updated: 05 Nov 2024 11:00 UTC\n! Expires: 5 days (update frequency)\n", false, true },
        { "[Adblock]\n! dutchblock v3\n! This list expires after 14 days\n|http://b*.mookie1.com/\n", false, true },
        { "[Adblock Plus 2.0]\n! Last modification time (GMT): 2012.11.05 13:33\n! Expires: 5 days (update frequency)\n", true, true },
        { "[Adblock Plus 2.0]\n! Last modification time (GMT): 2012.11.05 13:33\n", true, true },
        { "[Adblock]\n ! dummy,  i dont have any dates\n", false, true },
        { "\n", false, false }
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
            sub.clear ();
            sub.parse ();
        } catch (Error error) {
            GLib.error (error.message);
        }
        if (example.valid != sub.valid)
            error ("Subscription expected to be %svalid but %svalid:\n%s",
                   example.valid ? "" : "in", sub.valid ? "" : "in", example.content);
        if (example.result != updater.needs_update)
            error ("Update%s expected for:\n%s\nLast Updated: %s\nExpires: %s",
                   example.result ? "" : " not", example.content,
                   pretty_date (updater.last_updated), pretty_date (updater.expires));
    }
}

public void extension_test () {
    Test.add_func ("/extensions/adblock2/config", test_adblock_config);
    Test.add_func ("/extensions/adblock2/subs", test_adblock_subs);
    Test.add_func ("/extensions/adblock2/init", test_adblock_init);
    Test.add_func ("/extensions/adblock2/parse", test_adblock_fixup_regexp);
    Test.add_func ("/extensions/adblock2/pattern", test_adblock_pattern);
    Test.add_func ("/extensions/adblock2/update", test_subscription_update);
}
#endif

