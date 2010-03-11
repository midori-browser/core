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

#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <webkit/webkit.h>

struct _KatzeNet
{
    GObject parent_instance;

    gchar* cache_path;
    guint cache_size;

    SoupSession* session;
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
    net->cache_path = g_build_filename (g_get_user_cache_dir (),
                                        PACKAGE_NAME, NULL);

    net->session = webkit_get_default_session ();
}

static void
katze_net_finalize (GObject* object)
{
    KatzeNet* net = KATZE_NET (object);

    katze_assign (net->cache_path, NULL);

    G_OBJECT_CLASS (katze_net_parent_class)->finalize (object);
}

/**
 * katze_net_new:
 *
 * Instantiates a new #KatzeNet instance.
 *
 * Return value: a new #KatzeNet
 **/
KatzeNet*
katze_net_new (void)
{
    static KatzeNet* net = NULL;

    if (!net)
    {
        net = g_object_new (KATZE_TYPE_NET, NULL);
        /* Since this is a "singleton", keep an extra reference */
        g_object_ref (net);
    }
    else
        g_object_ref (net);

    return net;
}

/**
 * katze_net_get_session:
 *
 * Retrieves the session of the net.
 *
 * Return value: a session, or %NULL
 *
 * Since: 0.1.3
 **/
gpointer
katze_net_get_session (KatzeNet* net)
{
    g_return_val_if_fail (KATZE_IS_NET (net), NULL);

    return net->session;
}

typedef struct
{
    KatzeNet* net;
    KatzeNetStatusCb status_cb;
    KatzeNetTransferCb transfer_cb;
    gpointer user_data;
    KatzeNetRequest* request;
} KatzeNetPriv;

static void
katze_net_priv_free (KatzeNetPriv* priv)
{
    KatzeNetRequest* request;

    request = priv->request;

    g_free (request->uri);
    g_free (request->mime_type);
    g_free (request->data);

    g_free (request);
    g_free (priv);
}

gchar*
katze_net_get_cached_path (KatzeNet*    net,
                           const gchar* uri,
                           const gchar* subfolder)
{
    gchar* cache_path;
    gchar* checksum;
    gchar* extension;
    gchar* cached_filename;
    gchar* cached_path;

    if (subfolder)
        cache_path = g_build_filename (net->cache_path, subfolder, NULL);
    else
        cache_path = net->cache_path;
    katze_mkdir_with_parents (cache_path, 0700);
    checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);

    extension = g_strrstr (uri, ".");
    cached_filename = g_strdup_printf ("%s%s", checksum,
                                       extension ? extension : "");
    g_free (checksum);
    cached_path = g_build_filename (cache_path, cached_filename, NULL);
    g_free (cached_filename);
    if (subfolder)
        g_free (cache_path);
    return cached_path;
}

static void
katze_net_got_body_cb (SoupMessage*  msg,
                       KatzeNetPriv* priv);

static void
katze_net_got_headers_cb (SoupMessage*  msg,
                          KatzeNetPriv* priv)
{
    KatzeNetRequest* request;

    request = priv->request;

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
        soup_session_cancel_message (priv->net->session, msg, 1);
    }
}

static void
katze_net_got_body_cb (SoupMessage*  msg,
                       KatzeNetPriv* priv)
{
    KatzeNetRequest* request;
    #if 0
    gchar* filename;
    FILE* fp;
    #endif

    request = priv->request;

    if (msg->response_body->length > 0)
    {
        #if 0
        /* FIXME: Caching */
        filename = katze_net_get_cached_path (net, request->uri, NULL);
        if ((fp = fopen (filename, "wb")))
        {
            fwrite (msg->response_body->data,
                    1, msg->response_body->length, fp);
            fclose (fp);
        }
        g_free (filename);
        #endif
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
    KatzeNetRequest* request;
    gchar* filename;
    gchar* contents;
    gsize length;

    request = priv->request;
    filename = g_filename_from_uri (request->uri, NULL, NULL);

    if (!filename || g_access (filename, F_OK) != 0)
    {
        request->status = KATZE_NET_NOT_FOUND;
        if (priv->status_cb)
            priv->status_cb (request, priv->user_data);
        katze_net_priv_free (priv);
        return FALSE;
    }
    request->status = KATZE_NET_VERIFIED;
    if (priv->status_cb && !priv->status_cb (request, priv->user_data))
    {
        katze_net_priv_free (priv);
        return FALSE;
    }

    if (!priv->transfer_cb)
    {
        katze_net_priv_free (priv);
        return FALSE;
    }

    contents = NULL;
    if (!g_file_get_contents (filename, &contents, &length, NULL))
    {
        request->status = KATZE_NET_FAILED;
        priv->transfer_cb (request, priv->user_data);
        katze_net_priv_free (priv);
        return FALSE;
    }

    request->status = KATZE_NET_DONE;
    request->data = contents;
    request->length = length;
    priv->transfer_cb (request, priv->user_data);
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
 * @net: a #KatzeNet
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

    g_return_if_fail (KATZE_IS_NET (net));
    g_return_if_fail (uri != NULL);

    if (!status_cb && !transfer_cb)
        return;

    request = g_new0 (KatzeNetRequest, 1);
    request->uri = g_strdup (uri);
    request->mime_type = NULL;
    request->data = NULL;

    priv = g_new0 (KatzeNetPriv, 1);
    priv->net = net;
    priv->status_cb = status_cb;
    priv->transfer_cb = transfer_cb;
    priv->user_data = user_data;
    priv->request = request;

    if (g_str_has_prefix (uri, "http://") || g_str_has_prefix (uri, "https://"))
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
        soup_session_queue_message (net->session, msg, NULL, NULL);
        return;
    }

    if (g_str_has_prefix (uri, "file://"))
    {
        g_idle_add ((GSourceFunc)katze_net_local_cb, priv);
        return;
    }

    g_idle_add ((GSourceFunc)katze_net_default_cb, priv);
}

