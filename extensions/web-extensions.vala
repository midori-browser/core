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
        public File file { get; protected set; }
        public string name { get; set; }
        public string description { get; set; }
        public string? background_page { get; owned set; }
        public List<string> background_scripts { get; owned set; }
        public List<string> content_scripts { get; owned set; }
        public List<string> content_styles { get; owned set; }
        public Action? browser_action { get; set; }

        public Extension (File file) {
            Object (file: file, name: file.get_basename ());
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
        HashTable<string, Extension> extensions;

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
                string id = Checksum.compute_for_string (ChecksumType.MD5, file.get_path ());
                var extension = extensions.lookup (id);
                if (extension == null) {
                    extension = new Extension (file);

                    // If we find a manifest, this is a web extension
                    var manifest_file = file.get_child ("manifest.json");
                    if (!manifest_file.query_exists ()) {
                        continue;
                    }

                    try {
                        var json = new Json.Parser ();
                        yield json.load_from_stream_async (new DataInputStream (manifest_file.read ()));
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
                                    action.has_member ("default_icon") ? action.get_string_member ("default_icon") : null,
                                    action.has_member ("default_title") ? action.get_string_member ("default_title") : null,
                                    action.has_member ("default_popup") ? action.get_string_member ("default_popup") : null);
                            }
                        }

                        if (manifest.has_member ("content_scripts")) {
                            var content_scripts = manifest.get_object_member ("content_scripts");
                            if (content_scripts != null && content_scripts.has_member ("js")) {
                                foreach (var element in content_scripts.get_array_member ("js").get_elements ()) {
                                    extension.content_scripts.append (element.get_string ());
                                }
                            }

                            if (content_scripts != null && content_scripts.has_member ("css")) {
                                foreach (var element in content_scripts.get_array_member ("css").get_elements ()) {
                                    extension.content_styles.append (element.get_string ());
                                }
                            }
                        }

                        debug ("Loaded %s from %s", extension.name, file.get_path ());
                        extensions.insert (id, extension);
                        extension_added (extension);
                    } catch (Error error) {
                        warning ("Failed to load extension '%s': %s\n", extension.name, error.message);
                    }
                }

                foreach (var filename in extension.content_scripts) {
                    uint8[] script;
                    yield file.get_child (filename).load_contents_async (null, out script, null);
                    content.add_script (new WebKit.UserScript ((string)script,
                                        WebKit.UserContentInjectedFrames.TOP_FRAME,
                                        WebKit.UserScriptInjectionTime.END,
                                        null, null));
                }
                foreach (var filename in extension.content_styles) {
                    uint8[] stylesheet;
                    yield file.get_child (filename).load_contents_async (null, out stylesheet, null);
                    content.add_style_sheet (new WebKit.UserStyleSheet ((string)stylesheet,
                                             WebKit.UserContentInjectedFrames.TOP_FRAME,
                                             WebKit.UserStyleLevel.USER,
                                             null, null));
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
                    var tab = new Midori.Tab (null, browser.tab.web_context, url);
                    browser.add (tab);
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
            web_view.get_settings ().enable_write_console_messages_to_stdout = true; // XXX

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
                load_uri (extension.file.get_child (uri).get_uri ());
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
                       extension.file.get_child (extension.browser_action.icon).get_path ());
                // Ensure the icon fits the size of a button in the toolbar
                int icon_width = 16, icon_height = 16;
                Gtk.icon_size_lookup (Gtk.IconSize.BUTTON, out icon_width, out icon_height);
                // Take scale factor into account
                icon_width *= scale_factor;
                icon_height *= scale_factor;
                try {
                    string filename = extension.file.get_child (extension.browser_action.icon).get_path ();
                    icon.pixbuf = new Gdk.Pixbuf.from_file_at_scale (filename, icon_width, icon_height, true);
                } catch (Error error) {
                    warning ("Failed to set icon for %s: %s", extension.name, error.message);
                }
            }
            if (extension.browser_action.popup != null) {
                popover = new Gtk.Popover (this);
                popover.add (new WebView (extension, extension.browser_action.popup));
            }
            add (icon);
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

        async void install_extension (Extension extension) throws Error {
            if (extension.browser_action != null) {
                browser.add_button (new Button (extension as Extension));
            }

            // Employ a delay to avoid delaying startup with many extensions
            uint src = Timeout.add (500, install_extension.callback);
            yield;
            Source.remove (src);

            // Insert the background page in the browser, as a hidden widget
            var background = new WebView (extension, extension.background_page);
            (((Gtk.Container)browser.get_child ())).add (background);

            foreach (var filename in extension.background_scripts) {
                uint8[] script;
                yield extension.file.get_child (filename).load_contents_async (null, out script, null);
                background.get_user_content_manager ().add_script (new WebKit.UserScript ((string)script,
                    WebKit.UserContentInjectedFrames.TOP_FRAME,
                    WebKit.UserScriptInjectionTime.END,
                    null, null));
            }
        }

        public void activate () {
            if (browser.is_locked) {
                return;
            }

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
            var tab = widget as Midori.Tab;

            var content = tab.get_user_content_manager ();
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
