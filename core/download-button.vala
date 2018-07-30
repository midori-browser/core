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
            popover.show ();
        }

        [GtkChild]
        public Gtk.Popover popover;
        [GtkChild]
        public Gtk.ListBox listbox;

        ListStore model = new ListStore (typeof (DownloadItem));

        construct {
            listbox.bind_model (model, create_row);
            popover.relative_to = this;

            var context = WebKit.WebContext.get_default ();
            context.download_started.connect (download_started);
        }

        void download_started (WebKit.Download download) {
            model.append (new DownloadItem.with_download (download));
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
        public WebKit.Download? download { get; protected set; default = null; }
        public bool loading { get; protected set; default = false; }
        public bool failed { get; protected set; default = false; }
        public void cancel () {
            if (download != null) {
                download.cancel ();
                download = null;
                loading = false;
            }
        }

        construct {
            notify["filename"].connect ((pspec) => {
                if (filename != null) {
                    try {
                        basename = Filename.display_basename (Filename.from_uri (filename));
                    } catch (ConvertError error) {
                        debug ("Failed to convert: %s", error.message);
                    }
                }
            });
        }

        public DownloadItem (string filename) {
            Object (filename: filename);
        }

        public DownloadItem.with_download (WebKit.Download download) {
            Object (download: download, loading: true);
            download.bind_property ("destination", this, "filename");
            download.bind_property ("estimated-progress", this, "progress");
            download.finished.connect (() => {
                download = null;
                loading = false;
            });
            download.failed.connect ((error) => {
                loading = false;
                failed = true;
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

        construct {
            cancel.clicked.connect (() => {
                item.cancel ();
            });
            open.clicked.connect (() => {
                try {
                    Gtk.show_uri (get_screen (), item.filename, Gtk.get_current_event_time ());
                } catch (Error error) {
                    critical ("Failed to open %s: %s", item.filename, error.message);
                }
            });
        }

        public DownloadRow (DownloadItem item) {
            Object (item: item);
            icon.gicon = item.icon;
            item.bind_property ("icon", icon, "gicon");
            filename.label = item.basename;
            item.bind_property ("basename", filename, "label");
            progress.fraction = item.progress;
            item.bind_property ("progress", progress, "fraction");
            progress.visible = item.loading;
            cancel.visible = item.loading;
            open.visible = !item.loading && !item.failed;
            item.notify["loading"].connect (update_buttons);
            item.notify["failed"].connect (update_buttons);
            error.visible = item.failed;
            item.bind_property ("failed", error, "visible");
        }

        void update_buttons (ParamSpec pspec) {
            progress.visible = item.loading;
            cancel.visible = item.loading;
            open.visible = !item.loading && !item.failed;
        }
    }
}
