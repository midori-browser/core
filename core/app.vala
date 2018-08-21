/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class App : Gtk.Application {
        public File? exec_path { get; protected set; default = null; }

        public static bool incognito = false;
        static bool version = false;
        const OptionEntry[] options = {
            { "private", 'p', 0, OptionArg.NONE, ref incognito, N_("Private browsing, no changes are saved"), null },
            { "version", 'V', 0, OptionArg.NONE, ref version, N_("Display version number"), null },
            { null }
        };
        const ActionEntry[] actions = {
            { "win-incognito-new", win_incognito_new_activated },
            { "quit", quit_activated },
        };

        public App () {
            Object (application_id: "org.midori-browser.midori",
                    flags: ApplicationFlags.HANDLES_OPEN);

            add_main_option_entries (options);
        }

        public override bool local_command_line (ref weak string[] args, out int exit_status) {
            exit_status = -1;
            // Get the executable path
            string executable = args[0];
            try {
                if (!Path.is_absolute (executable)) {
                    executable = Environment.find_program_in_path (executable);
                    if (FileUtils.test (executable, FileTest.IS_SYMLINK))
                        executable = FileUtils.read_link (executable);
                }
            } catch (FileError error) {
                debug ("Failed to look up exec path: %s", error.message);
            }
            exec_path = File.new_for_path (executable);

            return base.local_command_line (ref args, out exit_status);
        }

        public override void startup () {
            base.startup ();

            Intl.bindtextdomain (Config.PROJECT_NAME, null);
            Intl.bind_textdomain_codeset (Config.PROJECT_NAME, "UTF-8");
            Intl.textdomain (Config.PROJECT_NAME);

            Gtk.Window.set_default_icon_name (Config.PROJECT_NAME);

            var context = WebKit.WebContext.get_default ();
            context.register_uri_scheme ("internal", (request) => {
                request.ref ();
                internal_scheme.begin (request);
            });
            context.register_uri_scheme ("favicon", (request) => {
                request.ref ();
                favicon_scheme.begin (request);
            });
            context.register_uri_scheme ("stock", (request) => {
                request.ref ();
                stock_scheme.begin (request);
            });
            context.register_uri_scheme ("res", (request) => {
                try {
                    var stream = resources_open_stream (request.get_path (),
                                                        ResourceLookupFlags.NONE);
                    request.finish (stream, -1, null);
                } catch (Error error) {
                    request.finish_error (error);
                    critical ("Failed to load resource %s: %s", request.get_uri (), error.message);
                }
            });
            string config = Path.build_path (Path.DIR_SEPARATOR_S,
                Environment.get_user_config_dir (), Environment.get_prgname ());
            DirUtils.create_with_parents (config, 0700);
            string cookies = Path.build_filename (config, "cookies");
            context.get_cookie_manager ().set_persistent_storage (cookies, WebKit.CookiePersistentStorage.SQLITE);
            string cache = Path.build_path (Path.DIR_SEPARATOR_S,
                Environment.get_user_cache_dir (), Environment.get_prgname ());
            string icons = Path.build_path (Path.DIR_SEPARATOR_S, cache, "icondatabase");
            context.set_favicon_database_directory (icons);

            // Try and load web extensions from build folder
            var web_path = exec_path.get_parent ().get_child ("web");
            if (!web_path.query_exists (null)) {
                // Alternatively look for an installed path
                web_path = exec_path.get_parent ().get_parent ().get_child ("lib").get_child (Environment.get_prgname ());
            }
            context.set_web_extensions_directory (web_path.get_path ());
            context.initialize_web_extensions.connect (() => {
                context.set_web_extensions_initialization_user_data ("");
            });

            add_action_entries (actions, this);

            var action = new SimpleAction ("win-new", VariantType.STRING);
            action.activate.connect (win_new_activated);
            add_action (action);

            // Unset app menu if not handled by the shell
            if (!Gtk.Settings.get_default ().gtk_shell_shows_app_menu){
                app_menu = null;
            }
        }

        async void internal_scheme (WebKit.URISchemeRequest request) {
            try {
                var shortcuts = yield HistoryDatabase.get_default ().query (null, 9);
                string content = "";
                uint index = 0;
                foreach (var shortcut in shortcuts) {
                    index++;
                    content += """
                        <div class="shortcut">
                          <a href="%s" accesskey="%u">
                            <img src="%s" />
                            <span class="title">%s</span>
                          </a>
                        </div>""".printf (shortcut.uri, index, "favicon:///" + shortcut.uri, shortcut.title);
                }
                string stylesheet = (string)resources_lookup_data ("/data/about.css",
                                                                    ResourceLookupFlags.NONE).get_data ();
                string html = ((string)resources_lookup_data ("/data/speed-dial.html",
                                                             ResourceLookupFlags.NONE).get_data ())
                    .replace ("{title}", _("Speed Dial"))
                    .replace ("{icon}", "view-grid")
                    .replace ("{content}", content)
                    .replace ("{stylesheet}", stylesheet);
                var stream = new MemoryInputStream.from_data (html.data, free);
                request.finish (stream, html.length, "text/html");
            } catch (Error error) {
                request.finish_error (error);
                critical ("Failed to render %s: %s", request.get_uri (), error.message);
            }
            request.unref ();
        }

        void request_finish_pixbuf (WebKit.URISchemeRequest request, Gdk.Pixbuf pixbuf) throws Error {
            var output = new MemoryOutputStream (null, realloc, free);
            pixbuf.save_to_stream (output, "png");
            output.close ();
            uint8[] data = output.steal_data ();
            data.length = (int)output.get_data_size ();
            var stream = new MemoryInputStream.from_data (data, free);
            request.finish (stream, -1, null);
        }

        async void favicon_scheme (WebKit.URISchemeRequest request) {
            string page_uri = request.get_path ().substring (1, -1);
            try {
                var database = WebKit.WebContext.get_default ().get_favicon_database ();
                var surface = yield database.get_favicon (page_uri, null);
                if (surface != null) {
                    var image = (Cairo.ImageSurface)surface;
                    var icon = Gdk.pixbuf_get_from_surface (image, 0, 0, image.get_width (), image.get_height ());
                    request_finish_pixbuf (request, icon);
                }
            } catch (Error error) {
                request.finish_error (error);
                debug ("Failed to render favicon for %s: %s", page_uri, error.message);
            }
            request.unref ();
        }

        async void stock_scheme (WebKit.URISchemeRequest request) {
            string icon_name = request.get_path ().substring (1, -1);
            int icon_size = 48;
            Gtk.icon_size_lookup ((Gtk.IconSize)Gtk.IconSize.DIALOG, out icon_size, null);
            try {
                var icon = Gtk.IconTheme.get_default ().load_icon (icon_name, icon_size, Gtk.IconLookupFlags.FORCE_SYMBOLIC);
                request_finish_pixbuf (request, icon);
            } catch (Error error) {
                request.finish_error (error);
                critical ("Failed to load icon %s: %s", icon_name, error.message);
            }
            request.unref ();
        }

        void win_new_activated (Action action, Variant? parameter) {
            var browser = incognito
                ? new Browser.incognito (this)
                : new Browser (this);
            string? uri = parameter.get_string () != "" ? parameter.get_string () : null;
            browser.add (new Tab (null, browser.web_context, uri));
        }

        void win_incognito_new_activated () {
            var browser = new Browser.incognito (this);
            browser.add (new Tab (null, browser.web_context));
        }

        void quit_activated () {
            quit ();
        }

        protected override void window_added (Gtk.Window window) {
            base.window_added (window);
        }

        protected override void activate () {
            if (incognito) {
                activate_action ("win-incognito-new", null);
                return;
            }
            activate_action ("win-new", "");
        }

        protected override void open (File[] files, string hint) {
            var browser = incognito
                ? new Browser.incognito (this)
                : (active_window as Browser ?? new Browser (this));
            foreach (File file in files) {
                browser.add (new Tab (browser.tab, browser.web_context, file.get_uri ()));
            }
        }

        protected override int handle_local_options (VariantDict options) {
            if (version) {
                stdout.printf ("%s %s\n" +
                               "Copyright 2007-2018 Christian Dywan\n" +
                               "Please report comments, suggestions and bugs to:\n" +
                               "    %s\n" +
                               "Check for new versions at:\n" +
                               "    %s\n ",
                    Config.PROJECT_NAME, Config.CORE_VERSION,
                    Config.PROJECT_BUGS, Config.PROJECT_WEBSITE);
                return 0;
            }
            return -1;
        }
    }
}
