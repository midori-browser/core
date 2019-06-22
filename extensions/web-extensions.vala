/*
 Copyright (C) 2019 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace WebExtension {
    public class Extension : Object {
        HashTable<string, Bytes> _files;
        public File file { get; protected set; }

        public string name { get; set; }
        public string description { get; set; }
        public string? background_page { get; owned set; }
        public List<string> background_scripts { get; owned set; }
        public List<string> content_scripts { get; owned set; }
        public List<string> content_styles { get; owned set; }
        public Action? browser_action { get; set; }
        public Action? sidebar { get; set; }

        public Extension (File file) {
            Object (file: file, name: file.get_basename ());
        }

        public void add_resource (string resource, Bytes data) {
            if (_files == null) {
                _files = new HashTable<string, Bytes> (str_hash, str_equal);
            }
            _files.insert (resource, data);
        }

        public async Bytes get_resource (string resource) throws Error {
            // Strip ./ or / prefix
            string _resource = resource.has_prefix (".") ? resource.substring (1, -1) : resource;
            _resource = _resource.has_prefix ("/") ? _resource.substring (1, -1) : _resource;

            if (_files != null && _files.contains (_resource)) {
                return _files.lookup (_resource);
            }
            var child = file.get_child (_resource);
            if (child.query_exists ()) {
                uint8[] data;
                if (yield child.load_contents_async (null, out data, null)) {
                    return new Bytes (data);
                }
            }
            throw new FileError.IO ("Failed to open '%s': Not found in %s".printf (resource, name));
        }
    }

    public class Action : Object {
        public string? icon { get; protected set; }
        public string? title { get; protected set; }
        public string? popup { get; protected set; }

        public Action (string? icon, string? title, string? popup) {
            Object (icon: icon, title: title, popup: popup);
        }
    }

    public class ExtensionManager : Object {
        static ExtensionManager? _default = null;
        internal HashTable<string, Extension> extensions;

        public delegate void ExtensionManagerForeachFunc (Extension extension);
        public void @foreach (ExtensionManagerForeachFunc func) {
            extensions.foreach ((key, value) => {
                func (value);
            });
        }

        // Note: Can't use the actual type here
        // parameter 1 of type 'WebExtensionExtension' for signal
        // "WebExtensionExtensionManager::extension_added" is not a value type
        public signal void extension_added (Object extension);

        string? pick_default_icon (Json.Object action) {
            if (action.has_member ("default_icon")) {
                var node = action.get_member ("default_icon");
                if (node != null) {
                    if (node.get_node_type () == Json.NodeType.OBJECT) {
                        foreach (var size in node.get_object ().get_members ()) {
                            return node.get_object ().get_string_member (size);
                        }
                    } else if (node.get_node_type () == Json.NodeType.VALUE) {
                        return node.get_string ();
                    }
                }
            }
            return null;
        }

        public static ExtensionManager get_default () {
            if (_default == null) {
                _default = new ExtensionManager ();
                _default.extensions = new HashTable<string, Extension> (str_hash, str_equal);
            }
            return _default;
        }

        public async void load_from_folder (WebKit.UserContentManager content, File folder) throws Error {
            debug ("Load web extensions from %s", folder.get_path ());
            var enumerator = yield folder.enumerate_children_async (FileAttribute.STANDARD_NAME, 0);
            FileInfo info;
            while ((info = enumerator.next_file ()) != null) {
                var file = folder.get_child (info.get_name ());
                string id = file.get_basename ();
                if (!Midori.CoreSettings.get_default ().get_plugin_enabled (id)) {
                    continue;
                }

                var extension = extensions.lookup (id);
                if (extension == null) {
                    InputStream? stream = null;
                    extension = new Extension (file);

                    try {
                        // Try reading from a ZIP archive ie. .crx (Chrome/ Opera/ Vivaldi), .nex (Opera) or .xpi (Firefox)
                        if (Regex.match_simple ("\\.(crx|nex|xpi)", file.get_basename (),
                            RegexCompileFlags.CASELESS, RegexMatchFlags.NOTEMPTY)) {
                            var archive = new Archive.Read ();
                            archive.support_format_zip ();
                            if (archive.open_filename (file.get_path (), 10240) == Archive.Result.OK) {
                                unowned Archive.Entry entry;
                                while (archive.next_header (out entry) == Archive.Result.OK) {
                                    if (entry.pathname () == "manifest.json") {
                                        uint8[] buffer;
                                        int64 offset;
                                        archive.read_data_block (out buffer, out offset);
                                        stream = new MemoryInputStream.from_data (buffer, free);
                                    } else {
                                        uint8[] buffer;
                                        int64 offset;
                                        archive.read_data_block (out buffer, out offset);
                                        if (buffer.length > 0) {
                                            extension.add_resource (entry.pathname (), new Bytes (buffer));
                                        }
                                    }
                                }

                                if (stream == null) {
                                    throw new FileError.IO ("Failed to open '%s': no manifest.json".printf (file.get_path ()));
                                }
                            } else {
                                throw new FileError.IO ("Failed to open '%s': %s".printf (file.get_path (), archive.error_string ()));
                            }
                        } else {
                            // If we find a manifest, this is a web extension
                            var manifest_file = file.get_child ("manifest.json");
                            if (manifest_file.query_exists ()) {
                                stream = new DataInputStream (yield manifest_file.read_async ());
                            } else {
                                continue;
                            }
                        }

                        var json = new Json.Parser ();
                        yield json.load_from_stream_async (stream);

                        debug ("Loading web extension %s from %s", id, file.get_path ());
                        var manifest = json.get_root ().get_object ();
                        if (manifest.has_member ("name")) {
                            extension.name = manifest.get_string_member ("name");
                        }

                        if (manifest.has_member ("background")) {
                            var background = manifest.get_object_member ("background");
                            if (background != null) {
                                if (background.has_member ("page")) {
                                    extension.background_page = background.get_string_member ("page");
                                }

                                if (background.has_member ("scripts")) {
                                    foreach (var element in background.get_array_member ("scripts").get_elements ()) {
                                        extension.background_scripts.append (element.get_string ());
                                    }
                                }
                            }
                        }

                        if (manifest.has_member ("browser_action")) {
                            var action = manifest.get_object_member ("browser_action");
                            if (action != null) {
                                extension.browser_action = new Action (
                                    pick_default_icon (action),
                                    action.has_member ("default_title") ? action.get_string_member ("default_title") : null,
                                    action.has_member ("default_popup") ? action.get_string_member ("default_popup") : null);
                            }
                        }

                        if (manifest.has_member ("sidebar_action")) {
                            var sidebar = manifest.has_member ("sidebar_action") ? manifest.get_object_member ("sidebar_action") : null;
                            if (sidebar != null) {
                                extension.sidebar = new Action (
                                    pick_default_icon (sidebar),
                                    sidebar.has_member ("default_title") ? sidebar.get_string_member ("default_title") : null,
                                    sidebar.has_member ("default_panel") ? sidebar.get_string_member ("default_panel") : null);
                            }
                        }

                        if (manifest.has_member ("content_scripts")) {
                            foreach (var element in manifest.get_array_member ("content_scripts").get_elements ()) {
                                var content_script = element.get_object ();
                                if (content_script.has_member ("js")) {
                                    foreach (var js in content_script.get_array_member ("js").get_elements ()) {
                                        extension.content_scripts.append (js.get_string ());
                                    }
                                }

                                if (content_script.has_member ("css")) {
                                    foreach (var css in content_script.get_array_member ("css").get_elements ()) {
                                        extension.content_styles.append (css.get_string ());
                                    }
                                }
                            }
                        }

                        extensions.insert (id, extension);
                        extension_added (extension);
                    } catch (Error error) {
                        warning ("Failed to load extension '%s': %s\n", extension.name, error.message);
                    }
                }

                foreach (var filename in extension.content_scripts) {
                    try {
                        var script = yield extension.get_resource (filename);
                        content.add_script (new WebKit.UserScript ((string)(script.get_data ()),
                                            WebKit.UserContentInjectedFrames.TOP_FRAME,
                                            WebKit.UserScriptInjectionTime.END,
                                            null, null));
                    } catch (Error error) {
                        warning ("Failed to inject content script for '%s': %s", extension.name, filename);
                    }
                }
                foreach (var filename in extension.content_styles) {
                    try {
                        var stylesheet = yield extension.get_resource (filename);
                        content.add_style_sheet (new WebKit.UserStyleSheet ((string)(stylesheet.get_data ()),
                                                 WebKit.UserContentInjectedFrames.TOP_FRAME,
                                                 WebKit.UserStyleLevel.USER,
                                                 null, null));
                    } catch (Error error) {
                        warning ("Failed to inject content stylesheet for '%s': %s", extension.name, filename);
                    }
                }
            }
        }

        Midori.App app { get { return Application.get_default () as Midori.App; } }
        Midori.Browser browser { get { return app.active_window as Midori.Browser; } }

        void web_extension_message_received (WebKit.WebView web_view, WebKit.JavascriptResult result) {
            unowned JS.GlobalContext context = result.get_global_context ();
            unowned JS.Value value = result.get_value ();
            if (value.is_object (context)) {
                var object = value.to_object (context);
                string? fn = js_to_string (context, object.get_property (context, new JS.String.create_with_utf8_cstring ("fn")));
                if (fn != null && fn.has_prefix ("tabs.create")) {
                    var args = object.get_property (context, new JS.String.create_with_utf8_cstring ("args")).to_object (context);
                    string? url = js_to_string (context, args.get_property (context, new JS.String.create_with_utf8_cstring ("url")));
                    var tab = browser.add_tab (null, browser, url);
                    var promise = object.get_property (context, new JS.String.create_with_utf8_cstring ("promise")).to_number (context);
                    debug ("Calling back to promise #%.f".printf (promise));
                    web_view.run_javascript.begin ("promises[%.f].resolve({id:%s});".printf (promise, tab.id));
                } else if (fn != null && fn.has_prefix ("tabs.executeScript")) {
                    var args = object.get_property (context, new JS.String.create_with_utf8_cstring ("args")).to_object (context);
                    string? results = null;
                    string? code = js_to_string (context, args.get_property (context, new JS.String.create_with_utf8_cstring ("code")));
                    if (code != null) {
                        results = "[true]";
                        browser.tab.run_javascript.begin (code);
                    }
                    var promise = object.get_property (context, new JS.String.create_with_utf8_cstring ("promise")).to_number (context);
                    debug ("Calling back to promise #%.f".printf (promise));
                    web_view.run_javascript.begin ("promises[%.f].resolve(%s);".printf (promise, results ?? "[undefined]"));
                } else if (fn != null && fn.has_prefix ("notifications.create")) {
                    var args = object.get_property (context, new JS.String.create_with_utf8_cstring ("args")).to_object (context);
                    string? message = js_to_string (context, args.get_property (context, new JS.String.create_with_utf8_cstring ("message")));
                    string? title = js_to_string (context, args.get_property (context, new JS.String.create_with_utf8_cstring ("title")));
                    var notification = new Notification (title);
                    notification.set_body (message);
                    // Use per-extension ID to avoid collisions
                    string extension_uri = web_view.uri;
                    app.send_notification (extension_uri, notification);
                } else {
                    warning ("Unsupported Web Extension API: %s", fn);
                }
            } else {
                warning ("Unexpected non-object value posted to Web Extension API: %s", js_to_string (context, value));
            }
        }

        public void install_api (WebKit.WebView web_view) {
            web_view.get_settings ().enable_write_console_messages_to_stdout = true;

            var content = web_view.get_user_content_manager ();
            if (content.register_script_message_handler ("midori")) {
                content.script_message_received.connect ((result) => {
                    web_extension_message_received (web_view, result);
                });
                try {
                    string script = (string)resources_lookup_data ("/data/web-extension-api.js",
                                                                   ResourceLookupFlags.NONE).get_data ();
                    content.add_script (new WebKit.UserScript ((string)script,
                                        WebKit.UserContentInjectedFrames.ALL_FRAMES,
                                        WebKit.UserScriptInjectionTime.START,
                                        null, null));

                } catch (Error error) {
                    critical ("Failed to setup WebExtension API: %s", error.message);
                }
            } else {
                warning ("Failed to install WebExtension API handler");
            }

        }
    }

    public class WebView : WebKit.WebView {
        public WebView (Extension extension, string? uri = null) {
            Object (visible: true);

            var manager = ExtensionManager.get_default ();
            manager.install_api (this);

            if (uri != null) {
                string id = extension.file.get_basename ();
                load_uri ("extension:///%s/%s".printf (id, uri));
            } else {
                load_html ("<body></body>", extension.file.get_uri ());
            }
        }

        public override bool context_menu (WebKit.ContextMenu menu,
            Gdk.Event event, WebKit.HitTestResult hit) {

            if (hit.context_is_editable ()) {
                return false;
            }

            return true;
        }

        public override void close () {
            destroy ();
        }

        public override bool web_process_crashed () {
            load_alternate_html ("<body><button onclick='location.reload();'>Reload</button></body>", uri, uri);
            return true;
        }
    }

    public class Button : Gtk.MenuButton {
        public Button (Extension extension) {
            tooltip_text = extension.browser_action.title ?? extension.name;
            visible = true;
            focus_on_click = false;
            var icon = new Gtk.Image.from_icon_name ("midori-symbolic", Gtk.IconSize.BUTTON);
            icon.use_fallback = true;
            icon.visible = true;
            if (extension.browser_action.icon != null) {
                debug ("Icon for %s: %s\n",
                       extension.name,
                       extension.browser_action.icon);
                load_icon.begin (extension, icon);
            }
            if (extension.browser_action.popup != null) {
                popover = new Gtk.Popover (this);
                popover.add (new WebView (extension, extension.browser_action.popup));
            }
            add (icon);
        }

        async void load_icon (Extension extension, Gtk.Image icon) {
            // Ensure the icon fits the size of a button in the toolbar
            int icon_width = 16, icon_height = 16;
            Gtk.icon_size_lookup (Gtk.IconSize.BUTTON, out icon_width, out icon_height);
            // Take scale factor into account
            icon_width *= scale_factor;
            icon_height *= scale_factor;
            try {
                var image = yield extension.get_resource (extension.browser_action.icon);
                // Note: The from_bytes variant has no autodetection
                var stream = new MemoryInputStream.from_data (image.get_data (), free);
                icon.pixbuf = yield new Gdk.Pixbuf.from_stream_at_scale_async (stream, icon_width, icon_height, true);
            } catch (Error error) {
                warning ("Failed to set icon for %s: %s", extension.name, error.message);
            }
        }
    }

    static string? js_to_string (JS.GlobalContext context, JS.Value value) {
        if (!value.is_string (context)) {
            return null;
        }
        var str = value.to_string_copy (context);
        uint8[] buffer = new uint8[str.get_maximum_utf8_cstring_size ()];
        str.get_utf8_cstring (buffer);
        return ((string)buffer);
    }

    public class Browser : Object, Midori.BrowserActivatable {
        public Midori.Browser browser { owned get; set; }

        async void extension_scheme (WebKit.URISchemeRequest request) {
            string[] path = request.get_path ().substring (1, -1).split ("/", 2);
            string id = path[0];
            string resource = path[1];
            var manager = ExtensionManager.get_default ();
            var extension = manager.extensions.lookup (id);
            try {
                if (extension != null) {
                    var data = yield extension.get_resource (resource);
                    var stream = new MemoryInputStream.from_data (data.get_data (), free);
                    request.finish (stream, data.length, "text/html");
                }
            } catch (Error error) {
                request.finish_error (error);
                critical ("Failed to render %s: %s", request.get_path (), error.message);
            }
            request.unref ();
        }

        async void install_extension (Extension extension) throws Error {
            if (extension.browser_action != null) {
                browser.add_button (new Button (extension as Extension));
            }

            if (extension.sidebar != null) {
                var scrolled = new Gtk.ScrolledWindow (null, null);
                var web_view = new WebView (extension, extension.browser_action.popup);
                scrolled.show ();
                scrolled.add (web_view);
                browser.add_panel (scrolled);
                scrolled.parent.child_set (scrolled, "title", extension.sidebar.title);
            }

            // Employ a delay to avoid delaying startup with many extensions
            uint src = Timeout.add (500, install_extension.callback);
            yield;
            Source.remove (src);

            // Insert the background page in the browser, as a hidden widget
            var background = new WebView (extension, extension.background_page);
            (((Gtk.Container)browser.get_child ())).add (background);

            foreach (var filename in extension.background_scripts) {
                var script = yield extension.get_resource (filename);
                if (script == null) {
                    warning ("Failed to load background script for '%s': %s", extension.name, filename);
                    continue;
                }
                background.get_user_content_manager ().add_script (new WebKit.UserScript ((string)(script.get_data ()),
                    WebKit.UserContentInjectedFrames.TOP_FRAME,
                    WebKit.UserScriptInjectionTime.END,
                    null, null));
            }
        }

        public void activate () {
            if (browser.is_locked) {
                return;
            }

            var context = WebKit.WebContext.get_default ();
            context.register_uri_scheme ("extension", (request) => {
                request.ref ();
                extension_scheme.begin (request);
            });

            var manager = ExtensionManager.get_default ();
            manager.extension_added.connect ((extension) => {
                install_extension.begin ((Extension)extension);
            });
            manager.foreach ((extension) => {
                install_extension.begin ((Extension)extension);
            });

            browser.tabs.add.connect (tab_added);
            if (browser.tab != null) {
                tab_added (browser.tab);
            }
        }

        void tab_added (Gtk.Widget widget) {
            browser.tabs.add.disconnect (tab_added);

            var manager = ExtensionManager.get_default ();
            var tab = widget as Midori.Viewable;

            var content = ((WebKit.WebView)tab).get_user_content_manager ();
            // Try and load plugins from build folder
            var builtin_path = ((Midori.App)Application.get_default ()).exec_path.get_parent ().get_child ("extensions");
            manager.load_from_folder.begin (content, builtin_path);
            // System-wide plugins
            manager.load_from_folder.begin (content, File.new_for_path (Config.PLUGINDIR));
            // Plugins installed by the user
            string user_path = Path.build_path (Path.DIR_SEPARATOR_S,
                Environment.get_user_data_dir (), Config.PROJECT_NAME, "extensions");
            manager.load_from_folder.begin (content, File.new_for_path (user_path));
        }
    }
}

[ModuleInit]
public void peas_register_types(TypeModule module) {
    ((Peas.ObjectModule)module).register_extension_type (
        typeof (Midori.BrowserActivatable), typeof (WebExtension.Browser));
}
