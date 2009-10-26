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
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

static gboolean offline_mode = FALSE;
#define HAVE_WEBKIT_RESOURCE_REQUEST WEBKIT_CHECK_VERSION (1, 1, 14)
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

static gboolean
web_cache_replace_frame_uri (MidoriExtension* extension,
                             const gchar*     uri,
                             WebKitWebFrame*  web_frame)
{
    gchar* filename;
    gboolean handled = FALSE;

    filename = web_cache_get_cached_path (extension, uri);

    /* g_debug ("cache lookup: %s => %s", uri, filename); */

    if (g_file_test (filename, G_FILE_TEST_EXISTS))
    {
        gchar* data;
        g_file_get_contents (filename, &data, NULL, NULL);
        webkit_web_frame_load_alternate_string (web_frame, data, NULL, uri);
        g_free (data);
        handled = TRUE;
    }

    g_free (filename);
    return handled;
}

static gboolean
web_cache_navigation_decision_cb (WebKitWebView*             web_view,
                                  WebKitWebFrame*            web_frame,
                                  WebKitNetworkRequest*      request,
                                  WebKitWebNavigationAction* action,
                                  WebKitWebPolicyDecision*   decision,
                                  MidoriExtension*           extension)
{
    const gchar* uri = webkit_network_request_get_uri (request);
    if (!(uri && g_str_has_prefix (uri, "http://")))
        return FALSE;
    if (offline_mode == FALSE)
        return FALSE;

    return web_cache_replace_frame_uri (extension, uri, web_frame);
}

#if WEBKIT_CHECK_VERSION (1, 1, 6)
static gboolean
web_cache_load_error_cb (WebKitWebView*   web_view,
                         WebKitWebFrame*  web_frame,
                         const gchar*     uri,
                         GError*          error,
                         MidoriExtension* extension)
{
    if (offline_mode == FALSE)
        return FALSE;
    if (!(uri && g_str_has_prefix (uri, "http://")))
        return FALSE;

    return web_cache_replace_frame_uri (extension, uri, web_frame);
}
#endif

static void
web_cache_save_headers (SoupMessage* msg,
                        gchar*       filename)
{
      gchar* dsc_filename = g_strdup_printf ("%s.dsc", filename);
      SoupMessageHeaders* hdrs = msg->response_headers;
      SoupMessageHeadersIter iter;
      const gchar* name, *value;
      FILE* dscfd;

      soup_message_headers_iter_init (&iter, hdrs);
      dscfd = g_fopen (dsc_filename,"w+");
      while (soup_message_headers_iter_next (&iter, &name, &value))
          g_fprintf (dscfd, "%s: %s\n", name, value);
      fclose (dscfd);
}

GHashTable*
web_cache_get_headers (gchar* filename)
{
    GHashTable* headers;
    FILE* file;
    gchar* dsc_filename;

    headers = g_hash_table_new_full (g_str_hash, g_str_equal,
                               (GDestroyNotify)g_free,
                               (GDestroyNotify)g_free);

    if (!filename)
        return headers;
    if (!g_file_test (filename, G_FILE_TEST_EXISTS))
        return headers;

    dsc_filename = g_strdup_printf ("%s.dsc", filename);
    if (!g_file_test (dsc_filename, G_FILE_TEST_EXISTS))
    {
        g_free (dsc_filename);
        return headers;
    }
    if ((file = g_fopen (dsc_filename, "r")))
    {
        gchar line[128];
        while (fgets (line, 128, file))
        {
            if (line==NULL)
                continue;
            g_strchomp (line);
            gchar** data;
            data = g_strsplit (line, ":", 2);
            if (data[0] && data[1])
                g_hash_table_insert (headers, g_strdup (data[0]),
                                     g_strdup (g_strchug (data[1])));
            g_strfreev (data);
        }
    }
    fclose (file);
    /* g_hash_table_destroy (headers); */
    g_free (dsc_filename);
    return headers;
}

static void
web_cache_message_got_chunk_cb (SoupMessage* msg,
                                SoupBuffer*  chunk,
                                gchar*       filename)
{
    GFile *file;
    GOutputStream *stream;

    if (!chunk->data || !chunk->length)
        return;

    if (!(g_file_test (filename, G_FILE_TEST_EXISTS)))
    {
        /* FIXME: Is there are better ways to create a file? like touch */
        FILE* cffd;
        cffd = g_fopen (filename,"w");
        fclose (cffd);
    }
    file = g_file_new_for_path (filename);
    stream = (GOutputStream*)g_file_append_to (file, 0, NULL, NULL);
    g_output_stream_write (stream, chunk->data, chunk->length, NULL, NULL);
    g_output_stream_close (stream, NULL, NULL);
    g_object_unref (file);
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

    /* FIXME: Seems to open image in a new tab we need to set content-type separately */
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
        soup_message_body_append_buffer (msg->response_body, buffer);
        g_signal_emit_by_name (msg, "got-chunk", buffer, NULL);
        soup_buffer_free (buffer);
    }
    soup_message_got_body (msg);
    g_free (data);

    #if 0
    if (offline_mode == TRUE)
    {
       /* Workaroung for offline mode
       FIXME: libsoup-CRITICAL **: queue_message: assertion `item != NULL' failed */
       SoupSession *session = webkit_get_default_session ();
       soup_session_requeue_message (session, msg);
    }
    soup_message_finished (msg);
    #endif
}

static void
web_cache_mesage_got_headers_cb (SoupMessage*     msg,
                                 MidoriExtension* extension)
{
    SoupURI* soup_uri = soup_message_get_uri (msg);
    gchar* uri = soup_uri ? soup_uri_to_string (soup_uri, FALSE) : g_strdup ("");
    gchar* filename = web_cache_get_cached_path (extension, uri);
    const gchar* nocache;
    SoupMessageHeaders *hdrs = msg->response_headers;
    gint length;

    /* Skip big files */
    length = GPOINTER_TO_INT (soup_message_headers_get_one (hdrs, "Content-Length"));
    if (length > MAXLENGTH)
        return;

    nocache = soup_message_headers_get_one (hdrs, "Pragma");
    if (nocache == NULL)
        nocache = soup_message_headers_get_one (hdrs, "Cache-Control");
    if (nocache)
    {
        if (g_regex_match_simple ("no-cache|no-store", nocache,
                                  G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
        {
            g_free (uri);
            return;
        }
    }

    if (msg->status_code == SOUP_STATUS_NOT_MODIFIED)
    {
        /* g_debug ("loading from cache: %s -> %s", uri, filename); */
        g_signal_handlers_disconnect_by_func (msg,
            web_cache_mesage_got_headers_cb, extension);
        web_cache_message_rewrite (msg, filename);
    }
    else if (msg->status_code == SOUP_STATUS_OK)
    {
        /* g_debug ("updating cache: %s -> %s", uri, filename); */
        web_cache_save_headers (msg, filename);
        /* FIXME: Do we need to disconnect signal after we are in unqueue? */
        g_signal_connect (msg, "got-chunk",
            G_CALLBACK (web_cache_message_got_chunk_cb), filename);
    }
    /* FIXME: how to free this?
      g_free (filename); */
    g_free (uri);
}

#if HAVE_WEBKIT_RESOURCE_REQUEST
static void
web_cache_resource_request_starting_cb (WebKitWebView*         web_view,
                                        WebKitWebFrame*        web_frame,
                                        WebKitWebResource*     web_resource,
                                        WebKitNetworkRequest*  request,
                                        WebKitNetworkResponse* response,
                                        MidoriExtension*       extension)
{
    const gchar* uri;
    gchar* filename;
    /* TODO: Good place to check are we offline */
    uri = webkit_network_request_get_uri (request);
    if (!(uri && g_str_has_prefix (uri, "http://")))
        return;

    if (offline_mode == FALSE)
       return;

    filename = web_cache_get_cached_path (extension, uri);
    /* g_debug ("loading %s -> %s",uri, filename); */
    if (!g_file_test (filename, G_FILE_TEST_EXISTS))
    {
        g_free (filename);
        return;
    }

    if (!(g_strcmp0 (uri, webkit_web_frame_get_uri (web_frame))
        && g_strcmp0 (webkit_web_data_source_get_unreachable_uri (webkit_web_frame_get_data_source (web_frame)), uri)))
    {
        web_cache_replace_frame_uri (extension, uri, web_frame);
        g_free (filename);
        return;
    }

    gchar* file_uri = g_filename_to_uri (filename, NULL, NULL);
    webkit_network_request_set_uri (request, file_uri);

    g_free (file_uri);
    g_free (filename);
}
#endif

static void
web_cache_session_request_queued_cb (SoupSession*     session,
                                     SoupMessage*     msg,
                                     MidoriExtension* extension)
{
    /*FIXME: Should we need to free soupuri? */
    SoupURI* soup_uri = soup_message_get_uri (msg);
    gchar* uri = soup_uri ? soup_uri_to_string (soup_uri, FALSE) : g_strdup ("");

    /* For now we are handling only online mode here */
    if (offline_mode == TRUE)
        return;

    if (g_str_has_prefix (uri, "http") && !g_strcmp0 (msg->method, "GET"))
    {
        gchar* filename = web_cache_get_cached_path (extension, uri);
        if (offline_mode == FALSE)
        {
            GHashTable* cache_headers;
            gchar* etag;
            gchar* last_modified;

            cache_headers = web_cache_get_headers (filename);
            etag = g_hash_table_lookup (cache_headers, "ETag");
            last_modified = g_hash_table_lookup (cache_headers, "Last-Modified");
            if (etag)
                soup_message_headers_append (msg->request_headers,
                                             "If-None-Match", etag);
            if (last_modified)
                soup_message_headers_append (msg->request_headers,
                                             "If-Modified-Since", last_modified);
            /* FIXME: Do we need to disconnect signal after we are in unqueue? */
            g_signal_connect (msg, "got-headers",
                G_CALLBACK (web_cache_mesage_got_headers_cb), extension);

            g_free (etag);
            g_free (last_modified);
            g_free (filename);
            /* FIXME: uncoment this is leading to a crash
              g_hash_table_destroy (cache_headers); */
            return;
        }
/*
        else
        {
            g_debug("queued in offline mode: %s -> %s", uri, filename);
            if (g_file_test (filename, G_FILE_TEST_EXISTS))
            {
                 soup_message_set_status (msg, SOUP_STATUS_NOT_MODIFIED);
                 web_cache_message_rewrite (msg, filename);
            }
        }
*/
        g_free (filename);
    }
    g_free (uri);
}

static void
web_cache_add_tab_cb (MidoriBrowser*   browser,
                      MidoriView*      view,
                      MidoriExtension* extension)
{
    GtkWidget* web_view = gtk_bin_get_child (GTK_BIN (view));
    g_signal_connect (web_view, "navigation-policy-decision-requested",
        G_CALLBACK (web_cache_navigation_decision_cb), extension);

    #if WEBKIT_CHECK_VERSION (1, 1, 6)
    g_signal_connect (web_view, "load-error",
        G_CALLBACK (web_cache_load_error_cb), extension);
    #endif

    #if HAVE_WEBKIT_RESOURCE_REQUEST
    g_signal_connect (web_view, "resource-request-starting",
        G_CALLBACK (web_cache_resource_request_starting_cb), extension);
    #endif
}

static void
web_cache_deactivate_cb (MidoriExtension* extension,
                         MidoriBrowser*   browser);

static void
web_cache_add_tab_foreach_cb (MidoriView*      view,
                              MidoriBrowser*   browser,
                              MidoriExtension* extension)
{
    web_cache_add_tab_cb (browser, view, extension);
}

static void
web_cache_app_add_browser_cb (MidoriApp*       app,
                              MidoriBrowser*   browser,
                              MidoriExtension* extension)
{
    midori_browser_foreach (browser,
          (GtkCallback)web_cache_add_tab_foreach_cb, extension);
    g_signal_connect (browser, "add-tab",
        G_CALLBACK (web_cache_add_tab_cb), extension);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (web_cache_deactivate_cb), browser);
}

static void
web_cache_deactivate_tabs (MidoriView*      view,
                           MidoriExtension* extension)
{
    GtkWidget* web_view = gtk_bin_get_child (GTK_BIN (view));
    MidoriBrowser* browser = midori_browser_get_for_widget (web_view);

    g_signal_handlers_disconnect_by_func (
       browser, web_cache_add_tab_cb, 0);
    #if HAVE_WEBKIT_RESOURCE_REQUEST
    g_signal_handlers_disconnect_by_func (
       web_view, web_cache_resource_request_starting_cb, extension);
    #endif
    g_signal_handlers_disconnect_by_func (
       webkit_get_default_session (), web_cache_session_request_queued_cb, extension);
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
    midori_browser_foreach (browser, (GtkCallback)web_cache_deactivate_tabs, extension);
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
