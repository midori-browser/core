/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#include "katze-net.h"
#include "midori-core.h"

#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <webkit/webkit.h>

struct _KatzeNet
{
    GObject parent_instance;
};

struct _KatzeNetClass
{
    GObjectClass parent_class;
};

G_DEFINE_TYPE (KatzeNet, katze_net, G_TYPE_OBJECT)

static void
katze_net_finalize (GObject* object);

static void
katze_net_class_init (KatzeNetClass* class)
{
    GObjectClass* gobject_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = katze_net_finalize;
}

static void
katze_net_init (KatzeNet* net)
{
}

static void
katze_net_finalize (GObject* object)
{
    G_OBJECT_CLASS (katze_net_parent_class)->finalize (object);
}

typedef struct
{
    KatzeNetStatusCb status_cb;
    KatzeNetTransferCb transfer_cb;
    gpointer user_data;
    KatzeNetRequest* request;
} KatzeNetPriv;

static void
katze_net_priv_free (KatzeNetPriv* priv)
{
    KatzeNetRequest* request = priv->request;
    g_free (request->uri);
    g_free (request->mime_type);
    g_free (request->data);
    g_slice_free (KatzeNetRequest, request);
    g_slice_free (KatzeNetPriv, priv);
}

gchar*
katze_net_get_cached_path (KatzeNet*    net,
                           const gchar* uri,
                           const gchar* subfolder)
{
    gchar* checksum;
    gchar* extension;
    gchar* cached_filename;
    gchar* cached_path;

    checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
    extension = g_strrstr (uri, ".");
    cached_filename = g_strdup_printf ("%s%s", checksum,
                                       extension ? extension : "");
    g_free (checksum);

    if (subfolder)
    {
        gchar* cache_path = g_build_filename (midori_paths_get_cache_dir (), subfolder, NULL);
        katze_mkdir_with_parents (cache_path, 0700);
        cached_path = g_build_filename (cache_path, cached_filename, NULL);
        g_free (cache_path);
    }
    else
        cached_path = g_build_filename (midori_paths_get_cache_dir (), cached_filename, NULL);

    g_free (cached_filename);
    return cached_path;
}

static void
katze_net_got_body_cb (SoupMessage*  msg,
                       KatzeNetPriv* priv);

static void
katze_net_got_headers_cb (SoupMessage*  msg,
                          KatzeNetPriv* priv)
{
    KatzeNetRequest* request = priv->request;

    switch (msg->status_code)
    {
    case 200:
        request->status = KATZE_NET_VERIFIED;
        break;
    case 301:
        request->status = KATZE_NET_MOVED;
        break;
    default:
        request->status = KATZE_NET_NOT_FOUND;
    }

    if (!priv->status_cb (request, priv->user_data))
    {
        g_signal_handlers_disconnect_by_func (msg, katze_net_got_headers_cb, priv);
        g_signal_handlers_disconnect_by_func (msg, katze_net_got_body_cb, priv);
        soup_session_cancel_message (webkit_get_default_session (), msg, 1);
    }
}

static void
katze_net_got_body_cb (SoupMessage*  msg,
                       KatzeNetPriv* priv)
{
    KatzeNetRequest* request = priv->request;

    if (msg->response_body->length > 0)
    {
        request->data = g_memdup (msg->response_body->data,
                                  msg->response_body->length);
        request->length = msg->response_body->length;
    }

    priv->transfer_cb (request, priv->user_data);
}

static void
katze_net_finished_cb (SoupMessage*  msg,
                       KatzeNetPriv* priv)
{
    katze_net_priv_free (priv);
}

static gboolean
katze_net_local_cb (KatzeNetPriv* priv)
{
    KatzeNetRequest* request = priv->request;
    gchar* filename = g_filename_from_uri (request->uri, NULL, NULL);

    if (!filename || g_access (filename, F_OK) != 0)
    {
        request->status = KATZE_NET_NOT_FOUND;
        if (priv->status_cb)
            priv->status_cb (request, priv->user_data);
    }
    else if (!(priv->status_cb && !priv->status_cb (request, priv->user_data))
           &&  priv->transfer_cb)
    {
        gchar* contents = NULL;
        gsize length;

        request->status = KATZE_NET_VERIFIED;
        if (!g_file_get_contents (filename, &contents, &length, NULL))
        {
            request->status = KATZE_NET_FAILED;
        }
        else
        {
            request->status = KATZE_NET_DONE;
            request->data = contents;
            request->length = length;
        }
        priv->transfer_cb (request, priv->user_data);
    }
    g_free (filename);
    katze_net_priv_free (priv);
    return FALSE;
}

static gboolean
katze_net_default_cb (KatzeNetPriv* priv)
{
    KatzeNetRequest* request;

    request = priv->request;
    request->status = KATZE_NET_NOT_FOUND;
    if (priv->status_cb)
        priv->status_cb (request, priv->user_data);
    katze_net_priv_free (priv);
    return FALSE;
}

/**
 * katze_net_load_uri:
 * @net: a #KatzeNet, or %NULL
 * @uri: an URI string
 * @status_cb: function to call for status information
 * @transfer_cb: function to call upon transfer
 * @user_data: data to pass to the callback
 *
 * Requests a transfer of @uri.
 *
 * @status_cb will be called when the status of @uri
 * is verified. The specified callback may be called
 * multiple times unless cancelled.
 *
 * @transfer_cb will be called when the data @uri is
 * pointing to was transferred. Note that even a failed
 * transfer may transfer data.
 *
 * @status_cb will always to be called at least once.
 **/
void
katze_net_load_uri (KatzeNet*          net,
                    const gchar*       uri,
                    KatzeNetStatusCb   status_cb,
                    KatzeNetTransferCb transfer_cb,
                    gpointer           user_data)
{
    KatzeNetRequest* request;
    KatzeNetPriv* priv;
    SoupMessage* msg;

    g_return_if_fail (uri != NULL);

    if (!status_cb && !transfer_cb)
        return;

    request = g_slice_new (KatzeNetRequest);
    request->uri = g_strdup (uri);
    request->mime_type = NULL;
    request->data = NULL;

    priv = g_slice_new (KatzeNetPriv);
    priv->status_cb = status_cb;
    priv->transfer_cb = transfer_cb;
    priv->user_data = user_data;
    priv->request = request;

    if (midori_uri_is_http (uri))
    {
        msg = soup_message_new ("GET", uri);
        if (status_cb)
            g_signal_connect (msg, "got-headers",
                G_CALLBACK (katze_net_got_headers_cb), priv);
        if (transfer_cb)
            g_signal_connect (msg, "got-body",
                G_CALLBACK (katze_net_got_body_cb), priv);
        g_signal_connect (msg, "finished",
            G_CALLBACK (katze_net_finished_cb), priv);
        soup_session_queue_message (webkit_get_default_session (), msg, NULL, NULL);
        return;
    }

    if (g_str_has_prefix (uri, "file://"))
        g_idle_add ((GSourceFunc)katze_net_local_cb, priv);
    else
        g_idle_add ((GSourceFunc)katze_net_default_cb, priv);
}

