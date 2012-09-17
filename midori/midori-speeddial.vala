/*
 Copyright (C) 2011-2012 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Katze {
    extern static string mkdir_with_parents (string pathname, int mode);
}

namespace Sokoke {
    extern static string js_script_eval (void* ctx, string script, void* error);
}

namespace Midori {
    public errordomain SpeedDialError {
        INVALID_MESSAGE,
        NO_ACTION,
        NO_ID,
        NO_URL,
        NO_TITLE,
        NO_ID2,
        INVALID_ACTION,
    }

    public class SpeedDial : GLib.Object {
        string filename;
        string? html = null;
        List<Spec> thumb_queue = null;
        WebKit.WebView thumb_view = null;
        Spec? spec = null;

        public GLib.KeyFile keyfile;
        public bool close_buttons_left { get; set; default = false; }
        public signal void refresh ();

        public class Spec {
            public string dial_id;
            public string uri;
            public Spec (string dial_id, string uri) {
                this.dial_id = dial_id;
                this.uri = uri;
            }
        }

        public SpeedDial (string new_filename, string? fallback = null) {
            filename = new_filename;
            keyfile = new GLib.KeyFile ();
            try {
                keyfile.load_from_file (filename, GLib.KeyFileFlags.NONE);
            }
            catch (GLib.Error io_error) {
                string json;
                size_t len;
                try {
                    FileUtils.get_contents (fallback ?? (filename + ".json"),
                                            out json, out len);
                }
                catch (GLib.Error fallback_error) {
                    json = "'{}'";
                    len = 4;
                }

                var script = new StringBuilder.sized (len);
                script.append ("var json = JSON.parse (");
                script.append_len (json, (ssize_t)len);
                script.append ("""
        );
        var keyfile = '';
        for (var i in json['shortcuts']) {
        var tile = json['shortcuts'][i];
        keyfile += '[Dial ' + tile['id'].substring (1) + ']\n'
                +  'uri=' + tile['href'] + '\n'
                +  'img=' + tile['img'] + '\n'
                +  'title=' + tile['title'] + '\n\n';
        }
        var columns = json['width'] ? json['width'] : 3;
        var rows = json['shortcuts'] ? json['shortcuts'].length / columns : 0;
        keyfile += '[settings]\n'
                +  'columns=' + columns + '\n'
                +  'rows=' + (rows > 3 ? rows : 3) + '\n\n';
        keyfile;
                    """);

                try {
                    keyfile.load_from_data (
                        Sokoke.js_script_eval (null, script.str, null),
                        -1, 0);
                }
                catch (GLib.Error eval_error) {
                    GLib.critical ("Failed to parse %s as speed dial JSON: %s",
                                   fallback ?? (filename + ".json"), eval_error.message);
                }
                Katze.mkdir_with_parents (
                    Path.build_path (Path.DIR_SEPARATOR_S,
                                     Environment.get_user_cache_dir (),
                                     "midori", "thumbnails"), 0700);

                foreach (string tile in keyfile.get_groups ()) {
                    try {
                        string img = keyfile.get_string (tile, "img");
                        keyfile.remove_key (tile, "img");
                        string uri = keyfile.get_string (tile, "uri");
                        if (img != null && uri[0] != '\0' && uri[0] != '#') {
                            uchar[] decoded = Base64.decode (img);
                            FileUtils.set_data (build_thumbnail_path (uri), decoded);
                        }
                    }
                    catch (GLib.Error img_error) {
                        /* img and uri can be missing */
                    }
                }
            }
        }

        public string get_next_free_slot (out uint count = null) {
            uint slot_count = 0;
            foreach (string tile in keyfile.get_groups ()) {
                try {
                    if (keyfile.has_key (tile, "uri"))
                        slot_count++;
                }
                catch (KeyFileError error) { }
            }
            if (&count != null)
                count = slot_count;

            uint slot = 1;
            while (slot <= slot_count) {
                string tile = "Dial %u".printf (slot);
                if (!keyfile.has_group (tile))
                    return tile;
                slot++;
            }

            return "Dial %u".printf (slot_count + 1);
        }

        public void add (string uri, string title, Gdk.Pixbuf img) {
            string id = get_next_free_slot ();
            add_with_id (id, uri, title, img);
        }

        public void add_with_id (string id, string uri, string title, Gdk.Pixbuf img) {
            keyfile.set_string (id, "uri", uri);
            keyfile.set_string (id, "title", title);

            Katze.mkdir_with_parents (Path.build_path (Path.DIR_SEPARATOR_S,
                Paths.get_cache_dir (), "thumbnails"), 0700);
            string filename = build_thumbnail_path (uri);
            try {
                img.save (filename, "png", null, "compression", "7", null);
            }
            catch (Error error) {
                critical ("Failed to save speed dial thumbnail: %s", error.message);
            }
            save ();
        }

        string build_thumbnail_path (string filename) {
            string thumbnail = Checksum.compute_for_string (ChecksumType.MD5, filename) + ".png";
            return Path.build_filename (Paths.get_cache_dir (), "thumbnails", thumbnail);
        }

        public unowned string get_html () throws Error {
            bool load_missing = true;

            if (html != null)
                return html;

            string? head = null;
            string filename = Paths.get_res_filename ("speeddial-head.html");
            if (keyfile != null
             && FileUtils.get_contents (filename, out head, null)) {
                string header = head.replace ("{title}", _("Speed Dial")).
                    replace ("{click_to_add}", _("Click to add a shortcut")).
                    replace ("{enter_shortcut_address}", _("Enter shortcut address")).
                    replace ("{enter_shortcut_name}", _("Enter shortcut title")).
                    replace ("{are_you_sure}", _("Are you sure you want to delete this shortcut?"));
                var markup = new StringBuilder (header);

                uint slot_count = 1;
                string dial_id = get_next_free_slot (out slot_count);
                uint next_slot = dial_id.substring (5, -1).to_int ();

                /* Try to guess the best X by X grid size */
                uint grid_index = 3;
                while ((grid_index * grid_index) < slot_count)
                    grid_index++;

                /* Percent width size of one slot */
                uint slot_size = (100 / grid_index);

                /* No editing in private/ app mode or without scripts */
                markup.append_printf (
                    "%s<style>.cross { display:none }</style>%s" +
                    "<style> div.shortcut { height: %d%%; width: %d%%; }</style>\n",
                    Paths.is_readonly () ? "" : "<noscript>",
                    Paths.is_readonly () ? "" : "</noscript>",
                    slot_size + 1, slot_size - 4);

                /* Combined width of slots should always be less than 100%.
                 * Use half of the remaining percentage as a margin size */
                uint div_factor;
                if (slot_size * grid_index >= 100 && grid_index > 4)
                    div_factor = 8;
                else
                    div_factor = 2;
                uint margin = (100 - ((slot_size - 4) * grid_index)) / div_factor;
                if (margin > 9)
                    margin = margin % 10;

                markup.append_printf (
                    "<style> body { overflow:hidden } #content { margin-left: %u%%; }</style>", margin);
                if (close_buttons_left)
                    markup.append_printf (
                        "<style>.cross { left: -14px }</style>");

                foreach (string tile in keyfile.get_groups ()) {
                    try {
                        string uri = keyfile.get_string (tile, "uri");
                        if (uri != null && uri.str ("://") != null && tile.has_prefix ("Dial ")) {
                            string title = keyfile.get_string (tile, "title");
                            string thumb_filename = build_thumbnail_path (uri);
                            uint slot = tile.substring (5, -1).to_int ();
                            string encoded;
                            try {
                                uint8[] thumb;
                                FileUtils.get_data (thumb_filename, out thumb);
                                encoded = Base64.encode (thumb);
                            }
                            catch (FileError error) {
                                encoded = null;
                                if (load_missing)
                                    get_thumb (tile, uri);
                            }
                            markup.append_printf ("""
                                <div class="shortcut" id="%u"><div class="preview">
                                <a class="cross" href="#"></a>
                                <a href="%s"><img src="data:image/png;base64,%s" title='%s'></a>
                                </div><div class="title">%s</div></div>
                                """,
                                slot, uri, encoded ?? "", title, title ?? "");
                        }
                        else if (tile != "settings")
                            keyfile.remove_group (tile);
                    }
                    catch (KeyFileError error) { }
                }

                markup.append_printf ("""
                    <div class="shortcut" id="%u"><div class="preview new">
                    <a class="add" href="#"></a>
                    </div><div class="title">%s</div></div>
                    """,
                    next_slot,  _("Click to add a shortcut"));
                markup.append_printf ("</div>\n</body>\n</html>\n");
                html = markup.str;
            }
            else
                html = "";

            return html;
        }

        public void save_message (string message) throws Error {
            if (!message.has_prefix ("speed_dial-save-"))
                throw new SpeedDialError.INVALID_MESSAGE ("Invalid message '%s'", message);

            string msg = message.substring (16, -1);
            string[] parts = msg.split (" ", 4);
            if (parts[0] == null)
                throw new SpeedDialError.NO_ACTION ("No action.");
            string action = parts[0];

            if (parts[1] == null)
                throw new SpeedDialError.NO_ID ("No ID argument.");
            string dial_id = "Dial " + parts[1];

                if (action == "delete") {
                    string uri = keyfile.get_string (dial_id, "uri");
                    string file_path = build_thumbnail_path (uri);
                    keyfile.remove_group (dial_id);
                    FileUtils.unlink (file_path);
                }
                else if (action == "add") {
                    if (parts[2] == null)
                        throw new SpeedDialError.NO_URL ("No URL argument.");
                    keyfile.set_string (dial_id, "uri", parts[2]);
                    get_thumb (dial_id, parts[2]);
                }
                else if (action == "rename") {
                    if (parts[2] == null)
                        throw new SpeedDialError.NO_TITLE ("No title argument.");
                    string title = parts[2];
                    keyfile.set_string (dial_id, "title", title);
                }
                else if (action == "swap") {
                    if (parts[2] == null)
                        throw new SpeedDialError.NO_ID2 ("No ID2 argument.");
                    string dial2_id = "Dial " + parts[2];

                    string uri = keyfile.get_string (dial_id, "uri");
                    string title = keyfile.get_string (dial_id, "title");
                    string uri2 = keyfile.get_string (dial2_id, "uri");
                    string title2 = keyfile.get_string (dial2_id, "title");

                    keyfile.set_string (dial_id, "uri", uri2);
                    keyfile.set_string (dial2_id, "uri", uri);
                    keyfile.set_string (dial_id, "title", title2);
                    keyfile.set_string (dial2_id, "title", title);
                }
                else
                    throw new SpeedDialError.INVALID_ACTION ("Invalid action '%s'", action);

            save ();
        }

        void save () {
            html = null;

            try {
                FileUtils.set_contents (filename, keyfile.to_data ());
            }
            catch (Error error) {
                critical ("Failed to update speed dial: %s", error.message);
            }
            refresh ();
        }

        void load_status (GLib.Object thumb_view_, ParamSpec pspec) {
            if (thumb_view.load_status != WebKit.LoadStatus.FINISHED)
                return;

            return_if_fail (spec != null);
            #if HAVE_OFFSCREEN
            var img = (thumb_view.parent as Gtk.OffscreenWindow).get_pixbuf ();
            var pixbuf_scaled = img.scale_simple (240, 160, Gdk.InterpType.TILES);
            img = pixbuf_scaled;
            #else
            thumb_view.realize ();
            var img = midori_view_web_view_get_snapshot (thumb_view, 240, 160);
            #endif
            unowned string title = thumb_view.get_title ();
            add_with_id (spec.dial_id, spec.uri, title ?? spec.uri, img);

            thumb_queue.remove (spec);
            if (thumb_queue != null && thumb_queue.data != null) {
                spec = thumb_queue.data;
                thumb_view.load_uri (spec.uri);
            }
            else
                /* disconnect_by_func (thumb_view, load_status) */;
        }

        void get_thumb (string dial_id, string uri) {
            if (thumb_view == null) {
                thumb_view = new WebKit.WebView ();
                var settings = new WebKit.WebSettings ();
                settings. set ("enable-scripts", false,
                               "enable-plugins", false,
                               "auto-load-images", true,
                               "enable-html5-database", false,
                               "enable-html5-local-storage", false);
                if (settings.get_class ().find_property ("enable-java-applet") != null)
                    settings.set ("enable-java-applet", false);
                thumb_view.settings = settings;
                #if HAVE_OFFSCREEN
                var offscreen = new Gtk.OffscreenWindow ();
                offscreen.add (thumb_view);
                thumb_view.set_size_request (800, 600);
                offscreen.show_all ();
                #else
                /* What we are doing here is a bit of a hack. In order to render a
                   thumbnail we need a new view and load the url in it. But it has
                   to be visible and packed in a container. So we secretly pack it
                   into the notebook of the parent browser. */
                notebook.add (thumb_view);
                thumb_view.destroy.connect (Gtk.widget_destroyed);
                /* We use an empty label. It's not invisible but hard to spot. */
                notebook.set_tab_label (thumb_view, new Gtk.EventBox ());
                thumb_view.show ();
                #endif
            }

            thumb_queue.append (new Spec (dial_id, uri));
            if (thumb_queue.nth_data (1) != null)
                return;

            spec = thumb_queue.data;
            thumb_view.notify["load-status"].connect (load_status);
            thumb_view.load_uri (spec.uri);
        }
    }
}

