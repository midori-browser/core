/*
 Copyright (C) 2012-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Sokoke {
    extern static bool message_dialog (Gtk.MessageType type, string short, string detailed, bool modal);
}

namespace Midori {
    namespace Download {
        public static bool is_finished (WebKit.Download download) {
#if !HAVE_WEBKIT2
            switch (download.status) {
                case WebKit.DownloadStatus.FINISHED:
                case WebKit.DownloadStatus.CANCELLED:
                case WebKit.DownloadStatus.ERROR:
                    return true;
                default:
                    return false;
            }
#else
            if (download.estimated_progress == 1)
                return true;
            return false;
#endif
        }

        public static int get_type (WebKit.Download download) {
            return download.get_data<int> ("midori-download-type");
        }

        public static void set_type (WebKit.Download download, int type) {
            download.set_data<int> ("midori-download-type", type);
        }

#if HAVE_WEBKIT2
        public static string get_filename (WebKit.Download download) {
            return download.get_data<string> ("midori-download-filename");
        }
        public static void set_filename (WebKit.Download download, string name) {
            download.set_data<string> ("midori-download-filename", name);
        }
#endif
        public static double get_progress (WebKit.Download download) {
#if !HAVE_WEBKIT2
            /* Avoid a bug in WebKit */
            if (download.status == WebKit.DownloadStatus.CREATED)
                return 0.0;
            return download.progress;
#else
            return download.estimated_progress;
#endif
        }

        public static string calculate_tooltip (WebKit.Download download) {
#if !HAVE_WEBKIT2
            string filename = Midori.Download.get_basename_for_display (download.destination_uri);
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

            string speed = "";
            uint64? last_size = download.get_data<uint64?> ("last-size");
            if (last_size != null && elapsed != last_time) {
                if (current_size != last_size) {
                    speed = format_size ((uint64)(
                        (current_size - last_size) / (elapsed - last_time)));
                    download.set_data ("last-speed", speed.dup ());
                }
                else {
                    speed = download.get_data ("last-speed");
                }
            }
            else
                /* i18n: Unknown number of bytes, used for transfer rate like ?B/s */
                speed = _("?B");
            /* i18n: Download tooltip (transfer rate): (130KB/s) */
            speed = _(" (%s/s)").printf (speed);

            if (elapsed - last_time > 0.0) {
                download.set_data<int> ("last-time", (int)elapsed);
                download.set_data<uint64?> ("last-size", current_size);
            }

            return "%s\n%s %s%s".printf (filename, size, speed, eta);
#else
            string filename = Midori.Download.get_basename_for_display (download.destination);

            string size = "%s".printf (format_size (download.get_received_data_length ()));
            string speed = "";
            speed = format_size ((uint64)((download.get_received_data_length () * 1.0) / download.get_elapsed_time ()));
            speed = _(" (%s/s)").printf (speed);
            string progress = "%d%%".printf( (int) (download.get_estimated_progress ()*100));
            if (is_finished (download))
                return "%s\n %s".printf (filename, size);
            return "%s\n %s - %s".printf (filename, speed, progress);
#endif
        }

        public static string get_content_type (WebKit.Download download, string? mime_type) {
#if HAVE_WEBKIT2
            string? content_type = ContentType.guess (download.response.suggested_filename == null ?
                          download.destination : download.response.suggested_filename,
                          null, null);
#else
            string? content_type = ContentType.guess (download.suggested_filename, null, null);
#endif
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
                #if HAVE_WEBKIT2
                string? original_uri = download.get_request ().uri;
                #else
                string? original_uri = download.network_request.get_data<string> ("midori-original-uri");
                if (original_uri == null)
                    original_uri = download.get_uri ();
                #endif
                string? fingerprint;
                ChecksumType checksum_type = URI.get_fingerprint (original_uri, out fingerprint, null);
                /* By default, no wrong checksum */
                status = 2;
                if (fingerprint != null) {
                    try {
                        #if HAVE_WEBKIT2
                        string filename = Filename.from_uri (download.destination);
                        #else
                        string filename = Filename.from_uri (download.destination_uri);
                        #endif
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
#if !HAVE_WEBKIT2
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
            #else

            if (download.estimated_progress < 1) {
                download.cancel ();
            } else {
                if (open (download, widget))
                    return true;
            }
#endif
            return false;
        }

        public static string action_stock_id (WebKit.Download download) {
#if !HAVE_WEBKIT2
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
                case WebKit.DownloadStatus.ERROR:
                    return Gtk.Stock.DIALOG_ERROR;
                default:
                    critical ("action_stock_id: %d", download.status);
                    warn_if_reached ();
                    return Gtk.Stock.MISSING_IMAGE;
            }
#else
            if (download.estimated_progress == 1)
                if (has_wrong_checksum (download))
                    return Gtk.Stock.DIALOG_WARNING;
                else
                    return Gtk.Stock.OPEN;
            return Gtk.Stock.CANCEL;
#endif
        }

        /* returns whether an application was successfully launched to handle the file */
        public static bool open (WebKit.Download download, Gtk.Widget widget) throws Error {
            if (has_wrong_checksum (download)) {
                Sokoke.message_dialog (Gtk.MessageType.WARNING,
                     _("The downloaded file is erroneous."),
                     _("The checksum provided with the link did not match. This means the file is probably incomplete or was modified afterwards."),
                     true);
                return true;
            } else {
                var browser = widget.get_toplevel ();
                Tab? tab = null;
                browser.get ("tab", &tab);
                if (tab != null)
                #if HAVE_WEBKIT2
                    return tab.open_uri (download.destination);
                #else
                    return tab.open_uri (download.destination_uri);
                #endif
            }
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
            return filename.delimit ("/\\<>:\"|?* ", '_');
            #else
            return filename.delimit ("/ ", '_');
            #endif
        }

        public string get_suggested_filename (WebKit.Download download) {
#if !HAVE_WEBKIT2
            /* https://bugs.webkit.org/show_bug.cgi?id=83161
               https://d19vezwu8eufl6.cloudfront.net/nlp/slides%2F03-01-FormalizingNB.pdf */
            return clean_filename (download.get_suggested_filename ());
#else
            string name = get_filename (download);
            if (name == null)
                return "";
            return name;
#endif
        }

        /**
         * Returns a filename of the form "name.ext" to use as a suggested name for
         * a download of the given uri
         */
        public string get_filename_suggestion_for_uri (string mime_type, string uri) {
            return_val_if_fail (Midori.URI.is_location (uri), uri);
            string filename = File.new_for_uri (uri).get_basename ();
            if (uri.index_of_char ('.') == -1)
                return Path.build_filename (filename, fallback_extension (null, mime_type));
            return filename;
        }

        public static string? get_extension_for_uri (string uri, out string basename = null) {
            basename = null;
            /* Find the last slash and the last period *after* the last slash. */
            int last_slash = uri.last_index_of_char ('/');
            /* Huh, URI without slashes? */
            if (last_slash == -1)
                return null;
            int period = uri.last_index_of_char ('.', last_slash);
            if (period == -1)
                return null;
            /* Exclude the query: ?query=foobar */
            int query = uri.last_index_of_char ('?', period);
            /* The extension, or "." if it ended with a period */
            string extension = uri.substring (period, query - period);
            basename = uri.substring (0, period);
            return extension;

        }

        public string get_unique_filename (string filename) {
            if (Posix.access (filename, Posix.F_OK) == 0) {
                string basename;
                string? extension = get_extension_for_uri (filename, out basename);
                string? new_filename = null;
                int i = 0;
                do {
                    new_filename = "%s-%d%s".printf (basename, i++, extension ?? "");
                } while (Posix.access (new_filename, Posix.F_OK) == 0);
                return new_filename;
            }
            return filename;
        }

        /**
         * Returns a string showing a file:// URI's intended filename on
         * disk, suited for displaying to a user.
         * 
         * The string returned is the basename (final path segment) of the
         * filename of the uri. If the uri is invalid, not file://, or has no
         * basename, the uri itself is returned.
         * 
         * Since: 0.5.7
         **/
        public static string get_basename_for_display (string uri) {
            try {
                string filename = Filename.from_uri (uri);
                if(filename != null && filename != "")
                    return Path.get_basename (filename);
            } catch (Error error) { }
            return uri;
        }

        public string prepare_destination_uri (WebKit.Download download, string? folder) {
            string suggested_filename = get_suggested_filename (download);
            string basename = Path.get_basename (suggested_filename);
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

        /**
         * Returns whether it seems possible to save @download to the path specified by
         * @destination_uri, considering space on disk and permissions
         */
        public static bool has_enough_space (WebKit.Download download, string destination_uri, bool quiet=false) {
#if !HAVE_WEBKIT2
            var folder = File.new_for_uri (destination_uri).get_parent ();
            bool can_write;
            uint64 free_space;
            try {
                var info = folder.query_filesystem_info ("filesystem::free");
                free_space = info.get_attribute_uint64 ("filesystem::free");
                info = folder.query_info ("access::can-write", 0);
                can_write = info.get_attribute_boolean ("access::can-write");
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
                        Midori.Download.get_basename_for_display (destination_uri));
                    detailed_message = _("You don't have permission to write in this location.");
                }
                else if (free_space < download.total_size) {
                    message = _("There is not enough free space to download \"%s\".").printf (
                        Midori.Download.get_basename_for_display (destination_uri));
                    detailed_message = _("The file needs %s but only %s are left.").printf (
                        format_size (download.total_size), format_size (free_space));
                }
                else
                    assert_not_reached ();
                if (!quiet)
                    Sokoke.message_dialog (Gtk.MessageType.ERROR, message, detailed_message, false);
                return false;
            }
#endif
            return true;
        }
    }
}
