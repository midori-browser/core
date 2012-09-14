/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Sokoke {
    extern static bool show_uri (Gdk.Screen screen, string uri, uint32 timestamp) throws Error;
    extern static bool message_dialog (Gtk.MessageType type, string short, string detailed, bool modal);
}

namespace Midori {
    namespace Download {
        public static bool is_finished (WebKit.Download download) {
            switch (download.status) {
                case WebKit.DownloadStatus.FINISHED:
                case WebKit.DownloadStatus.CANCELLED:
                case WebKit.DownloadStatus.ERROR:
                    return true;
                default:
                    return false;
            }
        }

        public static int get_type (WebKit.Download download) {
            return download.get_data<int> ("midori-download-type");
        }

        public static void set_type (WebKit.Download download, int type) {
            download.set_data<int> ("midori-download-type", type);
        }

        public static double get_progress (WebKit.Download download) {
            /* Avoid a bug in WebKit */
            if (download.status == WebKit.DownloadStatus.CREATED)
                return 0.0;
            return download.progress;
        }

        public static string get_tooltip (WebKit.Download download) {
            string filename = Path.get_basename (download.destination_uri);
            /* i18n: Download tooltip (size): 4KB of 43MB */
            string size = _("%s of %s").printf (
                format_size (download.current_size),
                format_size (download.total_size));

            /* Finished, no speed or remaining time */
            if (is_finished (download) || download.status == WebKit.DownloadStatus.CREATED)
                return "%s\n%s".printf (filename, size);

            uint64 total_size = download.total_size, current_size = download.current_size;
            double elapsed = download.get_elapsed_time (),
               diff = elapsed / current_size,
               estimated = (total_size - current_size) * diff;
            int hour = 3600, minute = 60;
            int hours = (int)(estimated / hour),
                minutes = (int)((estimated - (hours * hour)) / minute),
                seconds = (int)((estimated - (hours * hour) - (minutes * minute)));
            string hours_ = ngettext ("%d hour", "%d hours", hours).printf (hours);
            string minutes_ = ngettext ("%d minute", "%d minutes", minutes).printf (minutes);
            string seconds_ = ngettext ("%d second", "%d seconds", seconds).printf (seconds);
            double last_time = download.get_data<int> ("last-time");
            uint64 last_size = download.get_data<uint64> ("last-size");

            string eta = "";
            if (estimated > 0) {
                if (hours > 0)
                    eta = hours_ + ", " + minutes_;
                else if (minutes >= 10)
                    eta = minutes_;
                else if (minutes < 10 && minutes > 0)
                    eta = minutes_ + ", " + seconds_;
                else if (seconds > 0)
                    eta = seconds_;
                if (eta != "")
            /* i18n: Download tooltip (estimated time) : - 1 hour, 5 minutes remaning */
                    eta = _(" - %s remaining").printf (eta);
            }

            string speed;
            if (elapsed != last_time) {
                speed = format_size ((uint64)(
                    (current_size - last_size) / (elapsed - last_time)));
            }
            else
                /* i18n: Unknown number of bytes, used for transfer rate like ?B/s */
                speed = _("?B");
            /* i18n: Download tooltip (transfer rate): (130KB/s) */
            speed = _(" (%s/s)").printf (speed);
            if (elapsed - last_time > 5.0) {
                download.set_data<int> ("last-time", (int)elapsed);
                download.set_data<uint64> ("last-size", current_size);
            }

            return "%s\n%s %s%s".printf (filename, size, speed, eta);
        }

        public static string get_content_type (WebKit.Download download, string? mime_type) {
            string? content_type = ContentType.guess (download.suggested_filename, null, null);
            if (content_type == null) {
                content_type = ContentType.from_mime_type (mime_type);
                if (content_type == null)
                    content_type = ContentType.from_mime_type ("application/octet-stream");
            }
            return content_type;
        }

        public static bool has_wrong_checksum (WebKit.Download download) {
            int status = download.get_data<int> ("checksum-status");
            if (status == 0) {
                /* Link Fingerprint */
                string? original_uri = download.network_request.get_data<string> ("midori-original-uri");
                if (original_uri == null)
                    original_uri = download.get_uri ();
                string? fingerprint;
                ChecksumType checksum_type = URI.get_fingerprint (original_uri, out fingerprint, null);
                /* By default, no wrong checksum */
                status = 2;
                if (fingerprint != null) {
                    try {
                        string filename = Filename.from_uri (download.destination_uri);
                        string contents;
                        size_t length;
                        bool y = FileUtils.get_contents (filename, out contents, out length);
                        string checksum = Checksum.compute_for_string (checksum_type, contents, length);
                        /* Checksums are case-insensitive */
                        if (!y || fingerprint.ascii_casecmp (checksum) != 0)
                            status = 1; /* wrong checksum */
                    }
                    catch (Error error) {
                        status = 1; /* wrong checksum */
                    }
                }
                download.set_data<int> ("checksum-status", status);
            }
            return status == 1;
        }

        public static bool action_clear (WebKit.Download download, Gtk.Widget widget) throws Error {
            switch (download.status) {
                case WebKit.DownloadStatus.CREATED:
                case WebKit.DownloadStatus.STARTED:
                    download.cancel ();
                    break;
                case WebKit.DownloadStatus.FINISHED:
                    if (open (download, widget))
                        return true;
                    break;
                case WebKit.DownloadStatus.CANCELLED:
                    return true;
                default:
                    critical ("action_clear: %d", download.status);
                    warn_if_reached ();
                    break;
            }
            return false;
        }

        public static string action_stock_id (WebKit.Download download) {
            switch (download.status) {
                case WebKit.DownloadStatus.CREATED:
                case WebKit.DownloadStatus.STARTED:
                    return Gtk.Stock.CANCEL;
                case WebKit.DownloadStatus.FINISHED:
                    if (has_wrong_checksum (download))
                        return Gtk.Stock.DIALOG_WARNING;
                    return Gtk.Stock.OPEN;
                case WebKit.DownloadStatus.CANCELLED:
                    return Gtk.Stock.CLEAR;
                default:
                    critical ("action_stock_id: %d", download.status);
                    warn_if_reached ();
                    return Gtk.Stock.MISSING_IMAGE;
            }
        }

        public static bool open (WebKit.Download download, Gtk.Widget widget) throws Error {
            if (!has_wrong_checksum (download))
                return Sokoke.show_uri (widget.get_screen (),
                    download.destination_uri, Gtk.get_current_event_time ());

            Sokoke.message_dialog (Gtk.MessageType.WARNING,
                _("The downloaded file is erroneous."),
    _("The checksum provided with the link did not match. This means the file is probably incomplete or was modified afterwards."),
                true);
            return false;
        }

        public unowned string fallback_extension (string? extension, string mime_type) {
            if (extension != null && extension[0] != '\0')
                return extension;
            if ("css" in mime_type)
                return ".css";
            if ("javascript" in mime_type)
                return ".js";
            if ("html" in mime_type)
                return ".htm";
            if ("plain" in mime_type)
                return ".txt";
            return "";
        }

        public string clean_filename (string filename) {
            #if HAVE_WIN32
            return filename.delimit ("/\\<>:\"|?*", '_');
            #else
            return filename.delimit ("/", '_');
            #endif
        }

        public string get_suggested_filename (WebKit.Download download) {
            /* https://bugs.webkit.org/show_bug.cgi?id=83161
               https://d19vezwu8eufl6.cloudfront.net/nlp/slides%2F03-01-FormalizingNB.pdf */
            return clean_filename (download.get_suggested_filename ());
        }

        public string get_filename_suggestion_for_uri (string mime_type, string uri) {
            /* Try to provide a good default filename, UTF-8 encoded */
            string filename = clean_filename (Soup.URI.decode (uri));
            /* Take the rest of the URI if needed */
            if (filename.has_suffix ("/") || uri.index_of_char ('.') == -1)
                return Path.build_filename (filename, fallback_extension (null, mime_type));
            return filename;
        }

        public static string? get_extension_for_uri (string uri, out string basename) {
            if (&basename != null)
                basename = null;
            /* Find the last slash and the last period *after* the last slash. */
            int last_slash = uri.last_index_of_char ('/');
            /* Huh, URI without slashes? */
            if (last_slash == -1)
                return null;
            int period = uri.last_index_of_char ('.', last_slash);
            if (period == -1)
                return null;
            /* The extension, or "." if it ended with a period */
            string extension = uri.substring (period, -1);
            if (&basename != null)
                basename = uri.substring (0, period);
            return extension;

        }

        public string get_unique_filename (string filename) {
            if (Posix.access (filename, Posix.F_OK) != 0) {
                string basename;
                string? extension = get_extension_for_uri (filename, out basename);
                string? new_filename = null;
                int i = -1;
                do {
                    new_filename = "%s-%d%s".printf (basename, i++, extension ?? "");
                } while (Posix.access (new_filename, Posix.F_OK) == 0);
                return new_filename;
            }
            return filename;
        }

        public string prepare_destination_uri (WebKit.Download download, string? folder) {
            string suggested_filename = get_suggested_filename (download);
            string basename = File.new_for_uri (suggested_filename).get_basename ();
            string download_dir;
            if (folder == null) {
                download_dir = Paths.get_tmp_dir ();
                Katze.mkdir_with_parents (download_dir, 0700);
            }
            else
                download_dir = folder;
            string destination_filename = Path.build_filename (download_dir, basename);
            try {
                return Filename.to_uri (get_unique_filename (destination_filename));
            }
            catch (Error error) {
                return "file://" + destination_filename;
            }
        }

        public static bool has_enough_space (WebKit.Download download, string uri) {
            var folder = File.new_for_uri (uri).get_parent ();
            bool can_write;
            uint64 free_space;
            try {
                var info = folder.query_filesystem_info ("access::can-write,filesystem::free");
                can_write = info.get_attribute_boolean ("access::can-write");
                free_space = info.get_attribute_uint64 ("filesystem::free");
            }
            catch (Error error) {
                can_write = false;
                free_space = 0;
            }
            if (free_space < download.total_size || !can_write) {
                string message;
                string detailed_message;
                if (!can_write) {
                    message = _("The file \"%s\" can't be saved in this folder.").printf (
                        Path.get_basename (uri));
                    detailed_message = _("You don't have permission to write in this location.");
                }
                else if (free_space < download.total_size) {
                    message = _("There is not enough free space to download \"%s\".").printf (
                        Path.get_basename (uri));
                    detailed_message = _("The file needs %s but only %s are left.").printf (
                        format_size (download.total_size), format_size (free_space));
                }
                else
                    assert_not_reached ();
                Sokoke.message_dialog (Gtk.MessageType.ERROR, message, detailed_message, false);
                return false;
            }
            return true;
        }
    }
}
