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

#define HAVE_WEBKIT_RESOURCE_REQUEST WEBKIT_CHECK_VERSION (1, 1, 14)

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

    /* cache_path = midori_extension_get_string (extension, "path"); */
    if (!cache_path)
        cache_path = g_build_filename (g_get_user_cache_dir (),
                                       PACKAGE_NAME, "web", NULL);
    checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
    folder = g_strdup_printf ("%c%c", checksum[0], checksum[1]);
    sub_path = g_build_path (G_DIR_SEPARATOR_S, cache_path, folder, NULL);
    g_mkdir (sub_path, 0700);
    g_free (folder);

    encoded = soup_uri_encode (uri, "/");
    ext = g_strrstr (encoded, ".");
    cached_filename = g_strdup_printf ("%s%s", checksum, ext ? ext : "");
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
    const gchar* provisional;

    if (!(uri && g_str_has_prefix (uri, "http://")))
        return FALSE;

    return web_cache_replace_frame_uri (extension, uri, web_frame);
}
#endif

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

    uri = webkit_network_request_get_uri (request);
    if (!(uri && g_str_has_prefix (uri, "http://")))
        return;

    if (!(g_strcmp0 (uri, webkit_web_frame_get_uri (web_frame))
        && g_strcmp0 (webkit_web_data_source_get_unreachable_uri (webkit_web_frame_get_data_source (web_frame)), uri)))
    {
        web_cache_replace_frame_uri (extension, uri, web_frame);
        return;
    }

    filename = web_cache_get_cached_path (extension, uri);
    if (g_file_test (filename, G_FILE_TEST_EXISTS))
    {
        gchar* file_uri = g_filename_to_uri (filename, NULL, NULL);
        webkit_network_request_set_uri (request, file_uri);
        g_free (file_uri);
    }
    g_free (filename);
}
#endif

static void
web_cache_mesage_got_headers_cb (SoupMessage*     msg,
                                 MidoriExtension* extension)
{
    SoupURI* soup_uri = soup_message_get_uri (msg);
    gchar* uri = soup_uri ? soup_uri_to_string (soup_uri, FALSE) : g_strdup ("");
    gchar* filename = web_cache_get_cached_path (extension, uri);
    gchar* data;
    gsize length;

    /* g_debug ("cache serve: %s (%s)", uri, filename); */

    /* FIXME: Inspect headers and decide whether we need to update the cache */

    g_file_get_contents (filename, &data, &length, NULL);
    soup_message_set_status (msg, SOUP_STATUS_OK);
    /* FIXME: MIME type */
    soup_message_set_request (msg, "image/jpeg", SOUP_MEMORY_TAKE, data, length);
    g_signal_handlers_disconnect_by_func (msg, web_cache_mesage_got_headers_cb, extension);
    soup_session_requeue_message (g_object_get_data (G_OBJECT (msg), "session"), msg);

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
        gchar* filename = web_cache_get_cached_path (extension, uri);

        /* g_debug ("cache lookup: %d %s => %s", msg->status_code, uri, filename); */

        g_object_set_data (G_OBJECT (msg), "session", session);

        /* Network is unavailable, so we fallback to cache */
        if (msg->status_code == SOUP_STATUS_CANT_RESOLVE)
            web_cache_mesage_got_headers_cb (msg, extension);

        if (g_file_test (filename, G_FILE_TEST_EXISTS))
            g_signal_connect (msg, "got-headers",
                G_CALLBACK (web_cache_mesage_got_headers_cb), extension);
        else
            g_signal_connect (msg, "got-chunk",
                G_CALLBACK (web_cache_mesage_got_chunk_cb), extension);

        g_free (filename);
    }

    g_free (uri);
}

static void
web_cache_session_request_unqueued_cb (SoupSession*     session,
                                       SoupMessage*     msg,
                                       MidoriExtension* extension)
{
    SoupURI* soup_uri = soup_message_get_uri (msg);
    gchar* uri = soup_uri ? soup_uri_to_string (soup_uri, FALSE) : NULL;

    /* g_debug ("request unqueued: %d %s", msg->status_code, uri); */

    #if !HAVE_WEBKIT_RESOURCE_REQUEST
    /* Network is unavailable, so we fallback to cache */
    if (msg->status_code == SOUP_STATUS_CANT_RESOLVE)
        web_cache_mesage_got_headers_cb (msg, extension);
    else
    #endif

    /* FIXME: Only store if this wasn't a cached message already */
    /* FIXME: Don't store files from the res server */
    if (uri && g_str_has_prefix (uri, "http"))
    {
        SoupMessageHeaders* hdrs = msg->response_headers;
        const gchar* mime_type = soup_message_headers_get_content_type (hdrs, NULL);

        /* FIXME: Don't store big files */

        if (mime_type)
        {
            gchar* filename = web_cache_get_cached_path (extension, uri);
            SoupMessageBody* body = msg->response_body;
            SoupBuffer* buffer = NULL;

            /* We fed the buffer manually before, so this actually works. */
            if (!soup_message_body_get_accumulate (body))
            {
                soup_message_body_set_accumulate (body, TRUE);
                buffer = soup_message_body_flatten (body);
            }

            /* FIXME: Update sensibly */
            if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                if (body->length)
                {
                    /* g_debug ("cache store: %s => %s (%d)", uri, filename, body->length); */
                    GError* error = NULL;
                    g_file_set_contents (filename, body->data, body->length, &error);
                    if (error)
                    {
                        g_printf ("%s\n", error->message);
                        g_error_free (error);
                    }
                }
                /* else
                    g_debug ("cache skip empty"); */

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

    g_signal_connect (session, "request-queued",
                      G_CALLBACK (web_cache_session_request_queued_cb), extension);
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
