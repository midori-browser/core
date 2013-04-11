/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2011 Peter Hatina <phatina@redhat.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    [CCode (cprefix = "MIDORI_WINDOW_")]
    public enum WindowState {
        NORMAL,
        MINIMIZED,
        MAXIMIZED,
        FULLSCREEN
    }
    /* Since: 0.1.3 */
#if !HAVE_WEBKIT2
    public class Settings : WebKit.WebSettings {
        /* Since: 0.5.0 */
        public bool enable_javascript { get {
            return enable_scripts;
        } set {
            enable_scripts = value;
        } }
        /* Since: 0.5.1 */
        public string default_charset { owned get {
            return default_encoding;
        } set {
            default_encoding = value;
        } }
        /* Since: 0.1.3 */
        public bool zoom_text_and_images { get; set; default = true; }
#else
    public class Settings : WebKit.Settings {
        public bool zoom_text_and_images { get {
            return !zoom_text_only;
        } set {
            zoom_text_only = !value;
        } }
        public bool enable_spell_checking { get {
            return WebKit.WebContext.get_default ().get_spell_checking_enabled ();
        } set {
            WebKit.WebContext.get_default ().set_spell_checking_enabled (value);
        } }
#endif
        public bool remember_last_window_size { get; set; default = true; }
        public int last_window_width { get; set; default = 0; }
        public int last_window_height { get; set; default = 0; }
        public int last_panel_position { get; set; default = 0; }
        public int last_panel_page { get; set; default = 0; }
        public int last_web_search { get; set; default = 0; }
        /* Since: 0.4.3 */
        // [IntegerType (min = 10, max = int.max)]
        public int search_width { get; set; default = 200; }
        /* Since: 0.4.7 */
        public bool last_inspector_attached { get; set; default = false; }
        /* Since: 0.1.3 */
        public WindowState last_window_state { get; set; default = WindowState.NORMAL; }

        /* Since: 0.4.8 */
        public uint inactivity_reset { get; set; default = 0; }

        GLib.Regex? block_uris_regex = null;
        /* Since: 0.4.8 */
        public string? block_uris { get {
            return block_uris_regex != null ? block_uris_regex.get_pattern () : null;
        } set {
#if !HAVE_WEBKIT2
            if (value == null || value == "")
                return;
            if (block_uris_regex == null)
                WebKit.get_default_session ().request_queued.connect ((msg) => {
                    if (block_uris_regex.match (msg.uri.to_string (false)))
                        msg.uri = new Soup.URI ("http://.invalid");
                });
            try {
                block_uris_regex = new GLib.Regex (value);
            }
            catch (Error error) {
                critical ("block-uris: %s", error.message);
            }
#endif
        } }

        /**
         * Overrides the browser window title if given.
         * Examples are "Help Viewer" or "L:D_N:application_ID:midori_O:URL"
         *
         * Since: 0.5.1
         **/
        public string? custom_title { get; set; default = null; }

        public string? location_entry_search { get; set; default = null; }
        /* Since: 0.1.7 */
        public int clear_private_data { get; set; default = 0; }
        /* Since: 0.2.9 */
        public string? clear_data { get; set; default = null; }

        public bool compact_sidepanel { get; set; default = false; }
        /* Since: 0.2.2 */
        public bool open_panels_in_windows { get; set; default = false; }
        /* Since: 0.1.3 */
        public bool right_align_sidepanel { get; set; default = false; }

        public bool show_menubar { get; set; default = false; }
        public bool show_navigationbar { get; set; default = true; }
        public bool show_bookmarkbar { get; set; default = false; }
        public bool show_panel { get; set; default = false; }
        public bool show_statusbar { get; set; default = true; }
        /* Since: 0.1.2 */
        public bool show_crash_dialog { get; set; default = true; }
        public string toolbar_items { get; set; default =
            "TabNew,Back,NextForward,ReloadStop,BookmarkAdd,Location,Search,Trash,CompactMenu"; }
        /* Since: 0.1.4 */
        // [Deprecated (since = "0.4.7")]
        public bool find_while_typing { get; set; default = false; }

        public bool open_popups_in_tabs { get; set; default = true; }
        /* Since: 0.2.0 */
        // [Deprecated (since = "0.4.9")]
        public bool kinetic_scrolling { get; set; default = true; }
        // [Deprecated (since = "0.5.0")]
        public bool middle_click_opens_selection { get; set; default = true; }
        public bool flash_window_on_new_bg_tabs { get; set; default = false; }

        public bool close_buttons_on_tabs { get; set; default = true; }
        public bool open_tabs_in_the_background { get; set; default = true; }
        public bool open_tabs_next_to_current { get; set; default = true; }
        public bool always_show_tabbar { get; set; default = true; }

        public string homepage { get; set; default = "about:search"; }
        public string tabhome { get; set; default = "about:dial"; }
        static string default_download_folder () {
            return Environment.get_user_special_dir (UserDirectory.DOWNLOAD)
                ?? Environment.get_home_dir ();
        }
        public string download_folder { get; set; default = default_download_folder (); }
        public string? text_editor { get; set; default = null; }
        /* Since: 0.1.6 */
        public string? news_aggregator { get; set; default = null; }

        public string http_proxy { get; set; default = null; }
        /* Since: 0.4.2 */
        // [IntegerType (min = 1, max = 65535)]
        public int http_proxy_port { get; set; default = 8080; }
        /* Since: 0.3.4 */
        // [IntegerType (min = 0, int.max)]
        public int maximum_cache_size { get; set; default = 100; }
        /* Since: 0.3.4 */
        public bool strip_referer { get; set; default = true; }
        /* Since: 0.4.2 */
        public bool first_party_cookies_only { get; set; default = true; }
        // [IntegerType (min = 0, int.max)]
        public int maximum_cookie_age { get; set; default = 30; }
        // [IntegerType (min = 0, int.max)]
        public int maximum_history_age { get; set; default = 30; }

        /* Since: 0.4.7 */
        public bool delay_saving (string property) {
            return property.has_prefix ("last-")
                || property == "user-stylesheet-uri"
                || property.has_suffix ("-width");
        }
    }
}
