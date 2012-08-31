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
    extern static string build_thumbnail_path (string uri);
}

namespace Midori {
    public class SpeedDial : GLib.Object {
        public GLib.KeyFile keyfile;

        public SpeedDial (string filename, string? fallback = null) {
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
                        string uri = keyfile.get_string (tile, "uri");
                        if (img != null && uri[0] != '\0' && uri[0] != '#') {
                            uchar[] decoded = Base64.decode (img);
                            FileUtils.set_data (Sokoke.build_thumbnail_path (uri), decoded);
                        }
                        keyfile.remove_key (tile, "img");
                    }
                    catch (GLib.Error img_error) {
                        /* img and uri can be missing */
                    }
                }
            }
        }

        public static string get_next_free_slot_fk (KeyFile keyfile) {
            uint slot_count = 0;
            foreach (string tile in keyfile.get_groups ()) {
                try {
                    if (keyfile.has_key (tile, "uri"))
                        slot_count++;
                }
                catch (KeyFileError error) { }
            }

            uint slot = 1;
            while (slot <= slot_count) {
                string tile = "Dial %u".printf (slot);
                if (!keyfile.has_group (tile))
                    return "s%u".printf (slot);
                slot++;
            }
            return "s%u".printf (slot_count + 1);
        }

        public static void add_fk (string id, string uri, string title, Gdk.Pixbuf img, KeyFile keyfile) {
            keyfile.set_string (id, "uri", uri);
            keyfile.set_string (id, "title", title);

            Katze.mkdir_with_parents (Path.build_path (Path.DIR_SEPARATOR_S,
                Paths.get_cache_dir (), "thumbnails"), 0700);
            string filename = Sokoke.build_thumbnail_path (uri);
            try {
                img.save (filename, "png", null, "compression", "7", null);
            }
            catch (Error error) {
                critical ("Failed to save speed dial thumbnail: %s", error.message);
            }
        }

        public static string? get_html_fk (KeyFile? keyfile,
            bool close_buttons_left, GLib.Object view, bool load_missing) throws Error {

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
                foreach (string tile in keyfile.get_groups ()) {
                    try {
                        if (keyfile.has_key (tile, "uri"))
                            slot_count++;
                    }
                    catch (KeyFileError error) { }
                }

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
                            string thumb_filename = Sokoke.build_thumbnail_path (uri);
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
                                    /* FIXME: midori_view_speed_dial_get_thumb (view, tile, uri); */
                                    critical ("FIXME midori_view_speed_dial_get_thumb");
                            }
                            markup.append_printf ("""
                                <div class="shortcut" id="s%u"><div class="preview">
                                <a class="cross" href="#" onclick='clearShortcut("s%u");'></a>
                                <a href="%s"><img src="data:image/png;base64,%s" title='%s'></a>
                                </div><div class="title" onclick='renameShortcut("s%u");'>%s</div></div>
                                """,
                                slot, slot, uri, encoded ?? "", title, slot, title ?? "");
                        }
                        else if (tile != "settings")
                            keyfile.remove_group (tile);
                    }
                    catch (KeyFileError error) { }
                }

                markup.append_printf ("""
                    <div class="shortcut" id="s%u"><div class="preview new">
                    <a class="add" href="#" onclick='return getAction("s%u");'></a>
                    </div><div class="title">%s</div></div>
                    """,
                    slot_count + 1, slot_count + 1, _("Click to add a shortcut"));
                markup.append_printf ("</div>\n</body>\n</html>\n");
                return markup.str;
            }

            return null;
        }
    }
}

