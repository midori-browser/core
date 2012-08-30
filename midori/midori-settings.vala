/*
 Copyright (C) 2008-2012 Christian Dywan <christian@twotoasts.de>
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

    public class Settings : WebKit.WebSettings {
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

        public string? location_entry_search { get; set; default = null; }
        /* Since: 0.1.7 */
        public int clear_private_data { get; set; default = 0; }
        /* Since: 0.2.9 */
        public string? clear_data { get; set; default = null; }

        public bool compact_sidepanel { get; set; default = false; }
        /* Since: 0.2.2 */
        public bool open_panels_in_windows { get; set; default = false; }
        public string toolbar_items { get; set; default =
            "TabNew,Back,NextForward,ReloadStop,BookmarkAdd,Location,Search,Trash,CompactMenu"; }
        /* Since: 0.1.4 */
        // [Deprecated (since = "0.4.7")]
        public bool find_while_typing { get; set; default = false; }

        /* Since: 0.4.7 */
        public bool delay_saving (string property) {
            return property.has_prefix ("last-") || property.has_suffix ("-width");
        }
    }
}
