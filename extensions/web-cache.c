/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

#include <midori/sokoke.h>
#include "config.h"

#include <glib/gstdio.h>
#include <stdlib.h>
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#define MAXLENGTH 1024 * 1024

static gchar*
web_cache_get_cached_path (MidoriExtension* extension,
                           const gchar*     uri)
{
    static const gchar* cache_path = NULL;
    gchar* checksum;
    gchar* folder;
    gchar* sub_path;
    gchar* encoded;
    gchar* ext;
    gchar* cached_filename;
    gchar* cached_path;

    if (!cache_path)
        cache_path = midori_extension_get_string (extension, "path");
    checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
    folder = g_strdup_printf ("%c%c", checksum[0], checksum[1]);
    sub_path = g_build_path (G_DIR_SEPARATOR_S, cache_path, folder, NULL);
    g_mkdir (sub_path, 0700);
    g_free (folder);

    encoded = soup_uri_encode (uri, "/");
    ext = g_strdup (g_strrstr (encoded, "."));
    /* Make sure ext isn't becoming too long */
    if (ext && ext[0] && ext[1] && ext[2] && ext[3] && ext[4])
        ext[4] = '\0';
    cached_filename = g_strdup_printf ("%s%s", checksum, ext ? ext : "");
    g_free (ext);
    g_free (encoded);
    g_free (checksum);
    cached_path = g_build_filename (sub_path, cached_filename, NULL);
    g_free (cached_filename);
    return cached_path;
}

static void
web_cache_save_headers (SoupMessage* msg,
                        gchar*       filename)
{
      gchar* dsc_filename = g_strdup_printf ("%s.dsc.tmp", filename);
      SoupMessageHeaders* hdrs = msg->response_headers;
      SoupMessageHeadersIter iter;
      const gchar* name, *value;
      FILE* dscfd;

      soup_message_headers_iter_init (&iter, hdrs);
      dscfd = g_fopen (dsc_filename, "w");
      while (soup_message_headers_iter_next (&iter, &name, &value))
          g_fprintf (dscfd, "%s: %s\n", name, value);
      fclose (dscfd);

      g_free (dsc_filename);
}

GHashTable*
web_cache_get_headers (gchar* filename)
{
    GHashTable* headers;
    FILE* file;
    gchar* dsc_filename;

    if (!filename)
        return NULL;

    /* use g_access() instead of g_file_test for better performance */
    if (g_access (filename, F_OK) != 0)
        return NULL;

    dsc_filename = g_strdup_printf ("%s.dsc", filename);
    headers = g_hash_table_new_full (g_str_hash, g_str_equal,
                               (GDestroyNotify)g_free,
                               (GDestroyNotify)g_free);

    if ((file = g_fopen (dsc_filename, "r")))
    {
        gchar line[128];
        while (fgets (line, 128, file))
        {
            gchar** data;

            if (line == NULL)
                continue;

            g_strchomp (line);
            data = g_strsplit (line, ":", 2);
            if (data[0] && data[1])
                g_hash_table_insert (headers, g_strdup (data[0]),
                                     g_strdup (g_strchug (data[1])));
            g_strfreev (data);
        }
        fclose (file);
        g_free (dsc_filename);
        return headers;
    }
    g_hash_table_destroy (headers);
    g_free (dsc_filename);
    return NULL;
}

static gboolean
web_cache_tmp_prepare (gchar* filename)
{
    gchar* tmp_filename = g_strdup_printf ("%s.tmp", filename);

    if (g_access (tmp_filename, F_OK) == 0)
    {
        g_free (tmp_filename);
        return FALSE;
    }
    g_file_set_contents (tmp_filename, "", -1, NULL);
    g_free (tmp_filename);
    return TRUE;
}

static void
web_cache_set_content_type (SoupMessage* msg,
                            SoupBuffer*  buffer)
{
    #if WEBKIT_CHECK_VERSION (1, 1, 15)
    const char *ct;
    SoupContentSniffer* sniffer = soup_content_sniffer_new ();
    ct = soup_content_sniffer_sniff (sniffer, msg, buffer, NULL);
    if (!ct)
        ct = soup_message_headers_get_one (msg->response_headers, "Content-Type");
    if (ct)
        g_signal_emit_by_name (msg, "content-sniffed", ct, NULL);
    #endif
}

static void
web_cache_message_finished_cb (SoupMessage* msg,
                               gchar*       filename)
{
    gchar* headers;
    gchar* tmp_headers;
    gchar* tmp_data;

    headers = g_strdup_printf ("%s.dsc", filename);
    tmp_headers = g_strdup_printf ("%s.dsc.tmp", filename);
    tmp_data = g_strdup_printf ("%s.tmp", filename);

    if (msg->status_code == SOUP_STATUS_OK)
    {
        g_rename (tmp_data, filename);
        g_rename (tmp_headers, headers);
    }
    else
    {
        g_unlink (tmp_data);
        g_unlink (tmp_headers);
    }

    g_free (headers);
    g_free (tmp_headers);
    g_free (tmp_data);
}

static void
web_cache_message_got_chunk_cb (SoupMessage* msg,
                                SoupBuffer*  chunk,
                                gchar*       filename)
{
    GFile *file;
    GOutputStream *stream;
    gchar *tmp_filename;

    if (!chunk->data || !chunk->length)
        return;

    tmp_filename = g_strdup_printf ("%s.tmp", filename);
    file = g_file_new_for_path (tmp_filename);
    if ((stream = (GOutputStream*)g_file_append_to (file, 0, NULL, NULL)))
    {
        g_output_stream_write (stream, chunk->data, chunk->length, NULL, NULL);
        g_object_unref (stream);
    }
    g_object_unref (file);
    g_free (tmp_filename);
}

static void
web_cache_message_rewrite (SoupMessage*  msg,
                           gchar*        filename)
{
    GHashTable* cache_headers = web_cache_get_headers (filename);
    GHashTableIter iter;
    SoupBuffer *buffer;
    gpointer key, value;
    char *data;
    gsize length;

    if (!cache_headers)
        return;

    soup_message_set_status (msg, SOUP_STATUS_OK);
    g_hash_table_iter_init (&iter, cache_headers);
    while (g_hash_table_iter_next (&iter, &key, &value))
        soup_message_headers_replace (msg->response_headers, key, value);
    g_signal_emit_by_name (msg, "got-headers", NULL);

    msg->response_body = soup_message_body_new ();
    g_file_get_contents (filename, &data, &length, NULL);
    if (data && length)
    {
        buffer = soup_buffer_new (SOUP_MEMORY_TEMPORARY, data, length);
        web_cache_set_content_type (msg, buffer);
        soup_message_body_append_buffer (msg->response_body, buffer);
        g_signal_emit_by_name (msg, "got-chunk", buffer, NULL);
        soup_buffer_free (buffer);
    }
    soup_message_got_body (msg);
    g_free (data);
}

static void
web_cache_mesage_got_headers_cb (SoupMessage* msg,
                                 gchar*       filename)
{
    SoupURI* soup_uri = soup_message_get_uri (msg);
    gchar* uri;
    const gchar* nocache;
    SoupMessageHeaders *hdrs = msg->response_headers;

    /* Skip files downloaded by the user */
    if (g_object_get_data (G_OBJECT (msg), "midori-web-cache-download"))
        return;

    /* Skip big files */
    const char* cl = soup_message_headers_get_one (hdrs, "Content-Length");
    if (cl && atoi (cl) > MAXLENGTH)
        return;

    nocache = soup_message_headers_get_one (hdrs, "Pragma");
    if (!nocache)
        nocache = soup_message_headers_get_one (hdrs, "Cache-Control");
    if (nocache && g_regex_match_simple ("no-cache|no-store", nocache,
                                         G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
    {
        return;
    }

    uri = soup_uri ? soup_uri_to_string (soup_uri, FALSE) : g_strdup ("");
    if (msg->status_code == SOUP_STATUS_NOT_MODIFIED)
    {
        /* g_debug ("loading from cache: %s -> %s", uri, filename); */
        g_signal_handlers_disconnect_by_func (msg,
            web_cache_mesage_got_headers_cb, filename);
        web_cache_message_rewrite (msg, filename);
        g_free (filename);
    }
    else if (msg->status_code == SOUP_STATUS_OK)
    {
        /* g_debug ("updating cache: %s -> %s", uri, filename); */
        if (!web_cache_tmp_prepare (filename))
        {
            g_free (uri);
            return;
        }
        web_cache_save_headers (msg, filename);
        g_signal_connect_data (msg, "got-chunk",
            G_CALLBACK (web_cache_message_got_chunk_cb),
            filename, (GClosureNotify)g_free, 0);
        g_signal_connect (msg, "finished",
            G_CALLBACK (web_cache_message_finished_cb), filename);
    }
    g_free (uri);
}

static void
web_cache_session_request_queued_cb (SoupSession*     session,
                                     SoupMessage*     msg,
                                     MidoriExtension* extension)
{
    SoupURI* soup_uri = soup_message_get_uri (msg);
    gchar* uri = soup_uri_to_string (soup_uri, FALSE);

    if (uri && g_str_has_prefix (uri, "http") && !g_strcmp0 (msg->method, "GET"))
    {
        gchar* filename = web_cache_get_cached_path (extension, uri);
        GHashTable* cache_headers;
        gchar* etag;
        gchar* last_modified;

        cache_headers = web_cache_get_headers (filename);
        if (cache_headers)
        {
            etag = g_hash_table_lookup (cache_headers, "ETag");
            last_modified = g_hash_table_lookup (cache_headers, "Last-Modified");
            if (etag)
                soup_message_headers_replace (msg->request_headers,
                                             "If-None-Match", etag);
            if (last_modified)
                soup_message_headers_replace (msg->request_headers,
                                              "If-Modified-Since", last_modified);
            g_hash_table_destroy (cache_headers);
        }
        g_signal_connect (msg, "got-headers",
                G_CALLBACK (web_cache_mesage_got_headers_cb), filename);

    }
    g_free (uri);
}

#if WEBKIT_CHECK_VERSION (1, 1, 3)
static void
web_cache_add_download_cb (MidoriBrowser*   browser,
                           WebKitDownload*  download,
                           MidoriExtension* extension)
{
    WebKitNetworkRequest* request = webkit_download_get_network_request (download);
    SoupMessage* msg = webkit_network_request_get_message (request);
    if (msg)
        g_object_set_data (G_OBJECT (msg), "midori-web-cache-download",
                           (gpointer)0xdeadbeef);
}
#endif

static void
web_cache_deactivate_cb (MidoriExtension* extension,
                         MidoriBrowser*   browser);

static void
web_cache_app_add_browser_cb (MidoriApp*       app,
                              MidoriBrowser*   browser,
                              MidoriExtension* extension)
{
    #if WEBKIT_CHECK_VERSION (1, 1, 3)
    g_signal_connect (browser, "add-download",
        G_CALLBACK (web_cache_add_download_cb), extension);
    #endif
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (web_cache_deactivate_cb), browser);
}

static void
web_cache_deactivate_cb (MidoriExtension* extension,
                         MidoriBrowser*   browser)
{
    MidoriApp* app = midori_extension_get_app (extension);
    SoupSession* session = webkit_get_default_session ();

    g_signal_handlers_disconnect_by_func (
        session, web_cache_session_request_queued_cb, extension);
    g_signal_handlers_disconnect_by_func (
        extension, web_cache_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, web_cache_app_add_browser_cb, extension);
    #if WEBKIT_CHECK_VERSION (1, 1, 3)
    g_signal_handlers_disconnect_by_func (
        browser, web_cache_add_download_cb, extension);
    #endif
}

static void
web_cache_activate_cb (MidoriExtension* extension,
                       MidoriApp*       app)
{
    const gchar* cache_path = midori_extension_get_string (extension, "path");
    KatzeArray* browsers;
    MidoriBrowser* browser;
    guint i;
    SoupSession* session = webkit_get_default_session ();

    katze_mkdir_with_parents (cache_path, 0700);
    g_signal_connect (session, "request-queued",
                      G_CALLBACK (web_cache_session_request_queued_cb), extension);

    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++)))
        web_cache_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (web_cache_app_add_browser_cb), extension);

    g_object_unref (browsers);
}

MidoriExtension*
extension_init (void)
{
    gchar* cache_path = g_build_filename (g_get_user_cache_dir (),
                                          PACKAGE_NAME, "web", NULL);
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Web Cache"),
        "description", _("Cache HTTP communication on disk"),
        "version", "0.1",
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);
    midori_extension_install_string (extension, "path", cache_path);
    midori_extension_install_integer (extension, "size", 50);

    g_free (cache_path);

    g_signal_connect (extension, "activate",
        G_CALLBACK (web_cache_activate_cb), NULL);

    return extension;
}
