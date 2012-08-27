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
#include <glib/gstdio.h>
#include <stdlib.h>

#include "config.h"
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#if !WEBKIT_CHECK_VERSION (1, 3, 11)

#define MAXLENGTH 1024 * 1024

static gchar*
web_cache_get_cache_dir (void)
{
    static gchar* cache_dir = NULL;
    if (!cache_dir)
        cache_dir = g_build_filename (midori_paths_get_cache_dir (), "web", NULL);
    return cache_dir;
}

static gchar*
web_cache_get_cached_path (MidoriExtension* extension,
                           const gchar*     uri)
{
    gchar* checksum;
    gchar* folder;
    gchar* sub_path;
    gchar* encoded;
    gchar* ext;
    gchar* cached_filename;
    gchar* cached_path;

    checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
    folder = g_strdup_printf ("%c%c", checksum[0], checksum[1]);
    sub_path = g_build_path (G_DIR_SEPARATOR_S,
                             web_cache_get_cache_dir (), folder, NULL);
    katze_mkdir_with_parents (sub_path, 0700);
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

static gboolean
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
      g_free (dsc_filename);
      if (!dscfd)
          return FALSE;

      while (soup_message_headers_iter_next (&iter, &name, &value))
          g_fprintf (dscfd, "%s: %s\n", name, value);
      fclose (dscfd);

      return TRUE;
}

static GHashTable*
web_cache_get_headers (gchar* filename)
{
    GHashTable* headers;
    FILE* file;
    gchar* dsc_filename;
    gchar line[128];

    if (!filename)
        return NULL;

    /* use g_access() instead of g_file_test for better performance */
    if (g_access (filename, F_OK) != 0)
        return NULL;

    dsc_filename = g_strdup_printf ("%s.dsc", filename);
    headers = g_hash_table_new_full (g_str_hash, g_str_equal,
                               (GDestroyNotify)g_free,
                               (GDestroyNotify)g_free);

    if (!(file = g_fopen (dsc_filename, "r")))
    {
        g_hash_table_destroy (headers);
        g_free (dsc_filename);
        return NULL;
    }
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

static GFile*
web_cache_tmp_prepare (gchar* filename)
{
    GFile *file;

    gchar* tmp_filename = g_strdup_printf ("%s.tmp", filename);
    if (g_access (tmp_filename, F_OK) == 0)
    {
        g_free (tmp_filename);
        return NULL;
    }
    file = g_file_new_for_path (tmp_filename);
    g_free (tmp_filename);

    return file;
}

static void
web_cache_set_content_type (SoupMessage* msg,
                            SoupBuffer*  buffer)
{
    gchar* sniffed_type;
    SoupContentSniffer* sniffer = soup_content_sniffer_new ();
    if ((sniffed_type = soup_content_sniffer_sniff (sniffer, msg, buffer, NULL)))
    {
        g_signal_emit_by_name (msg, "content-sniffed", sniffed_type, NULL);
        g_free (sniffed_type);
    }
    else
    {
        const gchar* content_type = soup_message_headers_get_one (
            msg->response_headers, "Content-Type");
        g_signal_emit_by_name (msg, "content-sniffed", content_type, NULL);
    }
}

static void
web_cache_message_finished_cb (SoupMessage*   msg,
                               GOutputStream* stream)
{
    gchar* headers;
    gchar* tmp_headers;
    gchar* tmp_data;
    gchar* filename;

    filename = g_object_get_data (G_OBJECT (stream), "filename");
    headers = g_strdup_printf ("%s.dsc", filename);
    tmp_headers = g_strdup_printf ("%s.dsc.tmp", filename);
    tmp_data = g_strdup_printf ("%s.tmp", filename);
    g_output_stream_close (stream, NULL, NULL);

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

    g_object_unref (stream);
    g_free (headers);
    g_free (tmp_headers);
    g_free (tmp_data);
}

static void web_cache_pause_message (SoupMessage* msg)
{
    SoupSession* session;
    session = g_object_get_data (G_OBJECT (msg), "session");
    soup_session_pause_message (session, msg);
}

static void web_cache_unpause_message (SoupMessage* msg)
{
    SoupSession* session;
    session = g_object_get_data (G_OBJECT (msg), "session");
    soup_session_unpause_message (session, msg);
}

static void
web_cache_message_got_chunk_cb (SoupMessage* msg,
                                SoupBuffer*  chunk,
                                GOutputStream* stream)
{
    if (!chunk->data || !chunk->length)
        return;
    /* FIXME g_output_stream_write_async (stream, chunk->data, chunk->length,
        G_PRIORITY_DEFAULT, NULL, NULL, (gpointer)chunk->length); */
    g_output_stream_write (stream, chunk->data, chunk->length, NULL, NULL);
}

static void
web_cache_message_rewrite_async_cb (GFile *file,
                                    GAsyncResult* res,
                                    SoupMessage*  msg)
{
    SoupBuffer *buffer;
    char *data;
    gsize length;
    GError *error = NULL;

    if (g_file_load_contents_finish (file, res, &data, &length, NULL, &error))
    {
        buffer = soup_buffer_new (SOUP_MEMORY_TEMPORARY, data, length);
        web_cache_set_content_type (msg, buffer);
        soup_message_body_append_buffer (msg->response_body, buffer);
        /* FIXME? */
        web_cache_unpause_message (msg);
        g_signal_emit_by_name (msg, "got-chunk", buffer, NULL);
        soup_buffer_free (buffer);
        g_free (data);
        soup_message_got_body (msg);
        soup_message_finished (msg);
    }
    g_object_unref (file);
    g_object_unref (msg);
}

static void
web_cache_message_rewrite (SoupMessage*  msg,
                           gchar*        filename)
{
    GHashTableIter iter;
    gpointer key, value;
    GFile *file;

    GHashTable* cache_headers = web_cache_get_headers (filename);
    if (!cache_headers)
        return;

    soup_message_set_status (msg, SOUP_STATUS_OK);
    g_hash_table_iter_init (&iter, cache_headers);
    while (g_hash_table_iter_next (&iter, &key, &value))
        soup_message_headers_replace (msg->response_headers, key, value);
    g_signal_emit_by_name (msg, "got-headers", NULL);
    g_hash_table_destroy (cache_headers);

    /* FIXME? It seems libsoup already said "goodbye" by the time
       the asynchronous function is starting to send data */
    web_cache_pause_message (msg);
    file = g_file_new_for_path (filename);
    g_free (filename);
    g_object_ref (msg);
    g_file_load_contents_async (file, NULL,
        (GAsyncReadyCallback)web_cache_message_rewrite_async_cb, msg);
}

static void
web_cache_mesage_got_headers_cb (SoupMessage* msg,
                                 gchar*       filename)
{
    const gchar* nocache;
    SoupMessageHeaders *hdrs = msg->response_headers;
    const char* cl;

    /* Skip files downloaded by the user */
    if (g_object_get_data (G_OBJECT (msg), "midori-web-cache-download"))
        return;

    /* Skip big files */
    cl = soup_message_headers_get_one (hdrs, "Content-Length");
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

    if (msg->status_code == SOUP_STATUS_NOT_MODIFIED)
    {
        /* g_debug ("loading from cache: %s", filename); */
        g_signal_handlers_disconnect_by_func (msg,
            web_cache_mesage_got_headers_cb, filename);
        web_cache_message_rewrite (msg, filename);
    }
    else if (msg->status_code == SOUP_STATUS_OK)
    {
        GFile* file;
        GOutputStream* ostream;

        /* g_debug ("updating cache: %s", filename); */
        if (!(file = web_cache_tmp_prepare (filename)))
            return;
        if (!web_cache_save_headers (msg, filename))
            return;

        ostream = (GOutputStream*)g_file_append_to (file,
            G_FILE_CREATE_PRIVATE | G_FILE_CREATE_REPLACE_DESTINATION, NULL, NULL);
        g_object_unref (file);

        if (!ostream)
            return;

        g_object_set_data_full (G_OBJECT (ostream), "filename",
                                filename, (GDestroyNotify)g_free);
        g_signal_connect (msg, "got-chunk",
            G_CALLBACK (web_cache_message_got_chunk_cb), ostream);
        g_signal_connect (msg, "finished",
            G_CALLBACK (web_cache_message_finished_cb), ostream);
    }
}

static void
web_cache_session_request_queued_cb (SoupSession*     session,
                                     SoupMessage*     msg,
                                     MidoriExtension* extension)
{
    SoupURI* soup_uri = soup_message_get_uri (msg);
    gchar* uri = soup_uri_to_string (soup_uri, FALSE);

    if (midori_uri_is_http (uri) && !g_strcmp0 (msg->method, "GET"))
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
        g_object_set_data (G_OBJECT (msg), "session", session);
        g_signal_connect (msg, "got-headers",
                G_CALLBACK (web_cache_mesage_got_headers_cb), filename);

    }
    g_free (uri);
}

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

static void
web_cache_deactivate_cb (MidoriExtension* extension,
                         MidoriBrowser*   browser);

static void
web_cache_app_add_browser_cb (MidoriApp*       app,
                              MidoriBrowser*   browser,
                              MidoriExtension* extension)
{
    g_signal_connect (browser, "add-download",
        G_CALLBACK (web_cache_add_download_cb), extension);
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
    g_signal_handlers_disconnect_by_func (
        browser, web_cache_add_download_cb, extension);
}

static void
web_cache_activate_cb (MidoriExtension* extension,
                       MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;
    SoupSession* session = webkit_get_default_session ();

    katze_mkdir_with_parents (web_cache_get_cache_dir (), 0700);
    g_signal_connect (session, "request-queued",
                      G_CALLBACK (web_cache_session_request_queued_cb), extension);

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        web_cache_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (web_cache_app_add_browser_cb), extension);

    g_object_unref (browsers);
}

static void
web_cache_clear_cache_cb (void)
{
    sokoke_remove_path (web_cache_get_cache_dir (), TRUE);
}
#endif

MidoriExtension*
extension_init (void)
{
    #if WEBKIT_CHECK_VERSION (1, 3, 11)
    return NULL;
    #else
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Web Cache"),
        "description", _("Cache HTTP communication on disk"),
        "version", "0.1" MIDORI_VERSION_SUFFIX,
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (web_cache_activate_cb), NULL);

    sokoke_register_privacy_item ("web-cache", _("Web Cache"),
        G_CALLBACK (web_cache_clear_cache_cb));

    return extension;
    #endif
}
