/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

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

#define HAVE_WEBKIT_RESOURCE_REQUEST 0 /* WEBKIT_CHECK_VERSION (1, 1, 14) */

static gchar*
web_cache_get_cached_path (const gchar* cache_path,
                           const gchar* uri)
{
    gchar* checksum;
    gchar* folder;
    gchar* sub_path;
    gchar* extension;
    gchar* cached_filename;
    gchar* cached_path;

    checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
    folder = g_strdup_printf ("%c%c", checksum[0], checksum[1]);
    sub_path = g_build_path (G_DIR_SEPARATOR_S, cache_path, folder, NULL);
    g_mkdir (sub_path, 0700);
    g_free (folder);

    extension = g_strrstr (uri, ".");
    cached_filename = g_strdup_printf ("%s%s", checksum,
                                       extension ? extension : "");
    g_free (checksum);
    cached_path = g_build_filename (sub_path, cached_filename, NULL);
    g_free (cached_filename);
    return cached_path;
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
    const gchar* cache_path;
    gchar* filename;

    uri = webkit_network_request_get_uri (request);
    if (!(uri && g_str_has_prefix (uri, "http://")))
        return;

    cache_path = midori_extension_get_string (extension, "path");
    filename = web_cache_get_cached_path (cache_path, uri);
    /* g_debug ("cache lookup: %s => %s", uri, filename); */

    g_free (filename);
}
#else
static void
web_cache_mesage_got_headers_cb (SoupMessage*     msg,
                                 MidoriExtension* extension)
{
    const gchar* cache_path = midori_extension_get_string (extension, "path");
    SoupURI* soup_uri = soup_message_get_uri (msg);
    gchar* uri = soup_uri ? soup_uri_to_string (soup_uri, FALSE) : g_strdup ("");
    gchar* filename = web_cache_get_cached_path (cache_path, uri);
    gchar* data;
    gsize length;

    g_debug ("cache serve: %s", filename);

    /* FIXME: Inspect headers and decide whether we need to update the cache */

    g_file_get_contents (filename, &data, &length, NULL);
    /* soup_message_body_append (msg->response_body, SOUP_MEMORY_TAKE, data, length); */
    /* FIXME: MIME type */
    soup_message_set_response (msg, "image/jpeg", SOUP_MEMORY_TAKE, data, length);
    soup_message_body_complete (msg->response_body);
    soup_message_finished (msg);
    /* soup_message_cancel (msg); */

    g_free (filename);
    g_free (uri);
}

static void
web_cache_mesage_got_chunk_cb (SoupMessage*     msg,
                               SoupBuffer*      chunk,
                               MidoriExtension* extension)
{
    /* Fill the body in manually for later use, even if WebKitGTK+
        disables accumulation. We should probably do this differently.  */
    if (!soup_message_body_get_accumulate (msg->response_body))
        soup_message_body_append_buffer (msg->response_body, chunk);
}

static void
web_cache_session_request_queued_cb (SoupSession*     session,
                                     SoupMessage*     msg,
                                     MidoriExtension* extension)
{
    SoupURI* soup_uri = soup_message_get_uri (msg);
    gchar* uri = soup_uri ? soup_uri_to_string (soup_uri, FALSE) : g_strdup ("");

    if (g_str_has_prefix (uri, "http"))
    {
        const gchar* cache_path = midori_extension_get_string (extension, "path");
        gchar* filename = web_cache_get_cached_path (cache_path, uri);

        /* g_debug ("cache lookup: %s => %s", uri, filename); */

        if (g_file_test (filename, G_FILE_TEST_EXISTS))
            g_signal_connect (msg, "got-headers",
                G_CALLBACK (web_cache_mesage_got_chunk_cb), extension);
        else
            g_signal_connect (msg, "got-chunk",
                G_CALLBACK (web_cache_mesage_got_chunk_cb), extension);

        g_free (filename);
    }

    g_free (uri);
}
#endif

static void
web_cache_session_request_unqueued_cb (SoupSession*     session,
                                       SoupMessage*     msg,
                                       MidoriExtension* extension)
{
    SoupURI* soup_uri = soup_message_get_uri (msg);
    gchar* uri = soup_uri ? soup_uri_to_string (soup_uri, FALSE) : NULL;

    /* g_debug ("request unqueued: %d", msg->status_code); */

    if (uri && msg->status_code != SOUP_STATUS_OK && g_str_has_prefix (uri, "http"))
    {
        SoupMessageHeaders* hdrs = msg->response_headers;
        const gchar* mime_type = soup_message_headers_get_content_type (hdrs, NULL);

        /* Only images are cached */
        if (mime_type && g_str_has_prefix (mime_type, "image/"))
        {
            const gchar* cache_path = midori_extension_get_string (extension, "path");
            gchar* filename = web_cache_get_cached_path (cache_path, uri);
            SoupMessageBody* body = msg->response_body;
            SoupBuffer* buffer = NULL;

            /* We fed the buffer manually before, so this actually works. */
            soup_message_body_set_accumulate (body, TRUE);
            buffer = soup_message_body_flatten (body);

            /* g_debug ("cache store: %s => %s", uri, filename); */

            /* FIXME: Update sensibly */
            if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                g_file_set_contents (filename, body->data, body->length, NULL);

            if (buffer)
                soup_buffer_free (buffer);
        }
        /* else
            g_debug ("cache skip: %s", mime_type); */
    }

    g_free (uri);
}

static void
web_cache_add_tab_cb (MidoriBrowser*   browser,
                      MidoriView*      view,
                      MidoriExtension* extension)
{
    GtkWidget* web_view = gtk_bin_get_child (GTK_BIN (view));
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
    #else
    g_signal_handlers_disconnect_by_func (
       webkit_get_default_session (), web_cache_session_request_queued_cb, extension);
    #endif
}

static void
web_cache_deactivate_cb (MidoriExtension* extension,
                         MidoriBrowser*   browser)
{
    MidoriApp* app = midori_extension_get_app (extension);

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
    KatzeArray* browsers;
    MidoriBrowser* browser;
    guint i;
    SoupSession* session = webkit_get_default_session ();

    #if !HAVE_WEBKIT_RESOURCE_REQUEST
    g_signal_connect (session, "request-queued",
                      G_CALLBACK (web_cache_session_request_queued_cb), extension);
    #endif
    g_signal_connect (session, "request-unqueued",
                      G_CALLBACK (web_cache_session_request_unqueued_cb), extension);

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
