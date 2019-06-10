/*
 Copyright (C) 2018 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    [GtkTemplate (ui = "/ui/download-button.ui")]
    public class DownloadButton : Gtk.Button {
        public virtual signal void show_downloads () {
            get_style_context ().remove_class (Gtk.STYLE_CLASS_SUGGESTED_ACTION);
            popover.show ();
        }

        [GtkChild]
        public Gtk.Popover popover;
        [GtkChild]
        public Gtk.Button clear;
        [GtkChild]
        public Gtk.ListBox listbox;

        string cache = File.new_for_path (Path.build_filename (
            Environment.get_user_cache_dir ())).get_uri ();
        ListStore model = new ListStore (typeof (DownloadItem));

        construct {
            listbox.bind_model (model, create_row);
            popover.relative_to = this;
            clear.clicked.connect (() => {
                hide ();
                model.remove_all ();
            });
        }

        internal WebKit.WebContext web_context { set {
            value.download_started.connect (download_started);
        } }


        void download_started (WebKit.Download download) {
            // Don't show cache files in the UI
            if (download.destination != null && download.destination.has_prefix (cache)) {
                return;
            }
            var item = new DownloadItem.with_download (download);
            item.finished.connect (() => {
                get_style_context ().add_class (Gtk.STYLE_CLASS_SUGGESTED_ACTION);
            });
            model.append (item);
        }

        public Gtk.Widget create_row (Object item) {
            visible = true;
            return new DownloadRow ((DownloadItem)item);
        }
    }

    public class DownloadItem : Object {
        public string? mime_type = null;
        public string content_type { owned get {
            string? content_type = ContentType.guess (filename, null, null);
            if (content_type == null) {
                content_type = ContentType.from_mime_type (mime_type);
                if (content_type == null)
                    content_type = ContentType.from_mime_type ("application/octet-stream");
            }
            return content_type;
        } }
        public Icon icon { owned get {
            var icon = GLib.ContentType.get_icon (content_type) as ThemedIcon;
            icon.append_name ("text-html-symbolic");
            return icon;
        } }

        public string? filename { get; protected set; default = null; }
        public string? basename { get; protected set; default = null; }
        public double progress { get; protected set; default = 0.0; }
        internal WebKit.Download? download { get; protected set; default = null; }
        public bool loading { get; protected set; default = false; }
        public string? error { get; protected set; default = null; }
        public void cancel () {
            if (download != null) {
                download.cancel ();
                download = null;
                loading = false;
            }
        }
        public signal void finished ();

        construct {
            notify["filename"].connect ((pspec) => {
                if (filename != null) {
                    try {
                        basename = Filename.display_basename (Filename.from_uri (filename));
                    } catch (ConvertError error) {
                        basename = filename;
                        critical ("Failed to convert: %s", error.message);
                    }
                }
            });
        }

        public DownloadItem (string filename) {
            Object (filename: filename);
        }

        internal DownloadItem.with_download (WebKit.Download download) {
            Object (download: download, loading: true);
            download.bind_property ("destination", this, "filename", BindingFlags.SYNC_CREATE);
            download.bind_property ("estimated-progress", this, "progress", BindingFlags.SYNC_CREATE);
            download.finished.connect (() => {
                download = null;
                loading = false;
                finished ();
            });
            download.failed.connect ((error) => {
                loading = false;
                this.error = error.message;
            });
        }
    }

    [GtkTemplate (ui = "/ui/download-row.ui")]
    public class DownloadRow : Gtk.ListBoxRow {
        public DownloadItem item { get; protected set; }

        [GtkChild]
        public Gtk.Image icon;
        [GtkChild]
        public Gtk.Label filename;
        [GtkChild]
        public Gtk.ProgressBar progress;
        [GtkChild]
        public Gtk.Button cancel;
        [GtkChild]
        public Gtk.Button open;
        [GtkChild]
        public Gtk.Image error;
        [GtkChild]
        public Gtk.Label status;

        construct {
            cancel.clicked.connect (() => {
                item.cancel ();
            });
            open.clicked.connect (() => {
                try {
                    Gtk.show_uri (get_screen (), item.filename, Gtk.get_current_event_time ());
                } catch (Error error) {
                    status.label = error.message;
                    critical ("Failed to open %s: %s", item.filename, error.message);
                }
            });
        }

        public DownloadRow (DownloadItem item) {
            Object (item: item);
            item.bind_property ("icon", icon, "gicon", BindingFlags.SYNC_CREATE);
            item.bind_property ("basename", filename, "label", BindingFlags.SYNC_CREATE);
            item.bind_property ("basename", filename, "tooltip-text", BindingFlags.SYNC_CREATE);
            item.bind_property ("progress", progress, "fraction", BindingFlags.SYNC_CREATE);
            status.bind_property ("label", status, "tooltip-text", BindingFlags.SYNC_CREATE);
            item.notify["loading"].connect (update_buttons);
            item.notify["error"].connect (update_buttons);
            update_buttons ();

            item.finished.connect (() => {
                var app = (App)Application.get_default ();
                if (item.error == null && app.active_window != null) {
                    var notification = new Notification (_("Transfer completed"));
                    notification.set_icon (item.icon);
                    notification.set_body (item.filename);
                    app.send_notification ("download-finished", notification);
                }
            });
        }

        void update_buttons () {
            progress.visible = item.loading;
            cancel.visible = item.loading;
            open.visible = !item.loading && item.error == null;
            error.visible = item.error != null;
            status.label = item.error ?? "";
            status.visible = item.error != null;
        }
    }
}
