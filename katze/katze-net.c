/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "katze-net.h"

#if HAVE_LIBSOUP
    #include <libsoup/soup.h>
#endif

struct _KatzeNet
{
    GObject parent_instance;

    GHashTable* memory;
    gchar* cache_path;
    guint cache_size;

    #if HAVE_LIBSOUP
    SoupSession* session;
    #endif
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
katze_net_object_maybe_unref (gpointer object)
{
    if (object)
        g_object_unref (object);
}

static void
katze_net_init (KatzeNet* net)
{
    net->memory = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, katze_net_object_maybe_unref);
    net->cache_path = g_build_filename (g_get_user_cache_dir (),
                                        PACKAGE_NAME, NULL);

    #if HAVE_LIBSOUP
    net->session = soup_session_async_new ();
    #endif
}

static void
katze_net_finalize (GObject* object)
{
    KatzeNet* net = KATZE_NET (object);

    g_hash_table_destroy (net->memory);
    katze_assign (net->cache_path, NULL);

    G_OBJECT_CLASS (katze_net_parent_class)->finalize (object);
}

/**
 * katze_net_new:
 *
 * Instantiates a new #KatzeNet singleton.
 *
 * Subsequent calls will ref the initial instance.
 *
 * Return value: a new #KatzeNet
 **/
KatzeNet*
katze_net_new (void)
{
    KatzeNet* net = g_object_new (KATZE_TYPE_NET,
                                  NULL);

    return net;
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

static gchar*
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
    g_mkdir_with_parents (cache_path, 0755);
    #if GLIB_CHECK_VERSION (2, 16, 0)
    checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
    #else
    checksum = g_strdup_printf ("%u", g_str_hash (uri));
    #endif

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

#if HAVE_LIBSOUP
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
#endif

gboolean
katze_net_local_cb (KatzeNetPriv* priv)
{
    KatzeNetRequest* request;
    gchar* filename;
    gchar* contents;
    gsize length;

    request = priv->request;
    filename = g_filename_from_uri (request->uri, NULL, NULL);

    if (!filename || !g_file_test (filename, G_FILE_TEST_EXISTS))
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

gboolean
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
    #if HAVE_LIBSOUP
    SoupMessage* msg;
    #endif

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

    #if HAVE_LIBSOUP
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
    #endif

    if (g_str_has_prefix (uri, "file://"))
    {
        g_idle_add ((GSourceFunc)katze_net_local_cb, priv);
        return;
    }

    g_idle_add ((GSourceFunc)katze_net_default_cb, priv);
}

typedef struct
{
    KatzeNet* net;
    gchar* icon_file;
    KatzeNetIconCb icon_cb;
    GtkWidget* widget;
    gpointer user_data;
} KatzeNetIconPriv;

static void
katze_net_icon_priv_free (KatzeNetIconPriv* priv)
{
    g_free (priv->icon_file);
    g_object_unref (priv->widget);
    g_free (priv);
}

static gboolean
katze_net_icon_status_cb (KatzeNetRequest*  request,
                          KatzeNetIconPriv* priv)
{
    switch (request->status)
    {
    case KATZE_NET_VERIFIED:
        if (request->mime_type &&
            !g_str_has_prefix (request->mime_type, "image/"))
        {
            katze_net_icon_priv_free (priv);
            return FALSE;
        }
        break;
    case KATZE_NET_MOVED:
        break;
    default:
        katze_net_icon_priv_free (priv);
        return FALSE;
    }

    return TRUE;
}

static void
katze_net_icon_transfer_cb (KatzeNetRequest*  request,
                            KatzeNetIconPriv* priv)
{
    GdkPixbuf* pixbuf;
    FILE* fp;
    GdkPixbuf* pixbuf_scaled;
    gint icon_width, icon_height;

    if (request->status == KATZE_NET_MOVED)
        return;

    pixbuf = NULL;
    if (request->data)
    {
        if ((fp = fopen (priv->icon_file, "wb")))
        {
            fwrite (request->data, 1, request->length, fp);
            fclose (fp);
            pixbuf = gdk_pixbuf_new_from_file (priv->icon_file, NULL);
        }
        else
            pixbuf = katze_pixbuf_new_from_buffer ((guchar*)request->data,
                request->length, request->mime_type, NULL);
        if (pixbuf)
            g_object_ref (pixbuf);
        g_hash_table_insert (priv->net->memory,
            g_strdup (priv->icon_file), pixbuf);
    }

    if (!priv->icon_cb)
    {
        katze_net_icon_priv_free (priv);
        return;
    }

    if (!pixbuf)
        pixbuf = gtk_widget_render_icon (priv->widget,
            GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);
    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, &icon_height);
    pixbuf_scaled = gdk_pixbuf_scale_simple (pixbuf, icon_width, icon_height,
                                             GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);

    priv->icon_cb (pixbuf_scaled, priv->user_data);
    katze_net_icon_priv_free (priv);
}

/**
 * katze_net_load_icon:
 * @net: a #KatzeNet
 * @uri: an URI string, or %NULL
 * @icon_cb: function to call upon completion
 * @widget: a related #GtkWidget
 * @user_data: data to pass to the callback
 *
 * Requests a transfer of an icon for @uri. This is
 * implemented by looking for a favicon.ico, an
 * image according to the file type or even a
 * generated icon. The provided icon is intended
 * for user interfaces and not guaranteed to be
 * the same over multiple requests, plus it may
 * be scaled to fit the menu icon size.
 *
 * The @widget is needed for theming information.
 *
 * The caller is expected to use the returned icon
 * and update it if @icon_cb is called.
 *
 * Depending on whether the icon was previously
 * cached or @uri is a local resource, the returned
 * icon may already be the final one.
 *
 * Note that both the returned #GdkPixbuf and the
 * icon passed to @icon_cb are newly allocated and
 * the caller owns the reference.
 **/
GdkPixbuf*
katze_net_load_icon (KatzeNet*      net,
                     const gchar*   uri,
                     KatzeNetIconCb icon_cb,
                     GtkWidget*     widget,
                     gpointer       user_data)
{
    guint i;
    KatzeNetIconPriv* priv;
    gchar* icon_uri;
    gchar* icon_file;
    GdkPixbuf* pixbuf;
    gint icon_width, icon_height;
    GdkPixbuf* pixbuf_scaled;

    g_return_val_if_fail (KATZE_IS_NET (net), NULL);
    g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

    pixbuf = NULL;
    if (uri && g_str_has_prefix (uri, "http://"))
    {
        i = 8;
        while (uri[i] != '\0' && uri[i] != '/')
            i++;
        if (uri[i] == '/')
        {
            icon_uri = g_strdup (uri);
            icon_uri[i] = '\0';
            icon_uri = g_strdup_printf ("%s/favicon.ico", icon_uri);
        }
        else
            icon_uri = g_strdup_printf ("%s/favicon.ico", uri);

        icon_file = katze_net_get_cached_path (net, icon_uri, "icons");

        if (g_hash_table_lookup_extended (net->memory,
                                          icon_file, NULL, (gpointer)&pixbuf))
        {
            if (pixbuf)
                g_object_ref (pixbuf);
        }
        else if (g_file_test (icon_file, G_FILE_TEST_EXISTS))
            pixbuf = gdk_pixbuf_new_from_file (icon_file, NULL);
        else
        {
            priv = g_new0 (KatzeNetIconPriv, 1);
            priv->net = net;
            priv->icon_file = icon_file;
            priv->icon_cb = icon_cb;
            priv->widget = g_object_ref (widget);
            priv->user_data = user_data;

            katze_net_load_uri (net, icon_uri,
                (KatzeNetStatusCb)katze_net_icon_status_cb,
                (KatzeNetTransferCb)katze_net_icon_transfer_cb, priv);
        }
        g_free (icon_uri);
    }

    if (!pixbuf)
        pixbuf = gtk_widget_render_icon (widget,
            GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);
    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, &icon_height);
    pixbuf_scaled = gdk_pixbuf_scale_simple (pixbuf, icon_width, icon_height,
                                             GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);

    return pixbuf_scaled;
}
