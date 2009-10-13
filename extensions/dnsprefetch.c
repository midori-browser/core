/*
 Copyright (C) 2009 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
*/

#include <midori/midori.h>

#include "config.h"

#include <glib/gstdio.h>
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#define MAXHOSTS 50
static gchar* hosts = NULL;
static int host_count;

static void
dnsprefetch_do_prefetch (WebKitWebView* web_view,
                         const gchar*   title,
                         const char*    uri,
                         gpointer       user_data)
{
     SoupURI* s_uri;

     if (!uri)
        return;
     s_uri = soup_uri_new (uri);
     if (!s_uri)
         return;

     #if GLIB_CHECK_VERSION (2, 22, 0)
     if (g_hostname_is_ip_address (s_uri->host))
     #else
     if (g_ascii_isdigit (s_uri->host[0]) && g_strstr_len (s_uri->host, 4, "."))
     #endif
     {
         soup_uri_free (s_uri);
         return;
     }
     if (!g_str_has_prefix (uri, "http"))
     {
         soup_uri_free (s_uri);
         return;
     }

     if (!g_regex_match_simple (s_uri->host, hosts,
                                G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
     {
         SoupAddress* address;
         gchar* new_hosts;

         address = soup_address_new (s_uri->host, SOUP_ADDRESS_ANY_PORT);
         soup_address_resolve_async (address, 0, 0, 0, 0);
         g_object_unref (address);

         if (host_count > MAXHOSTS)
         {
             katze_assign (hosts, g_strdup (""));
             host_count = 0;
         }
         host_count++;
         new_hosts = g_strdup_printf ("%s|%s", hosts, s_uri->host);
         katze_assign (hosts, new_hosts);
     }
     soup_uri_free (s_uri);
}

static void
dnsprefetch_add_tab_cb (MidoriBrowser* browser,
                        MidoriView*    view)
{
    GtkWidget* web_view = gtk_bin_get_child (GTK_BIN (view));
    g_signal_connect (web_view, "hovering-over-link",
            G_CALLBACK (dnsprefetch_do_prefetch), 0);
}

static void
dnsprefetch_deactivate_cb (MidoriExtension* extension,
                           MidoriBrowser*   browser);

static void
dnsprefetch_add_tab_foreach_cb (MidoriView*    view,
                                MidoriBrowser* browser)
{
    dnsprefetch_add_tab_cb (browser, view);
}

static void
dnsprefetch_app_add_browser_cb (MidoriApp*       app,
                                MidoriBrowser*   browser,
                                MidoriExtension* extension)
{
    midori_browser_foreach (browser,
        (GtkCallback)dnsprefetch_add_tab_foreach_cb, browser);
    g_signal_connect (browser, "add-tab",
        G_CALLBACK (dnsprefetch_add_tab_cb), 0);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (dnsprefetch_deactivate_cb), browser);
}

static void
dnsprefetch_deactivate_tabs (MidoriView*    view,
                             MidoriBrowser* browser)
{
    GtkWidget* web_view = gtk_bin_get_child (GTK_BIN (view));
    g_signal_handlers_disconnect_by_func (
       browser, dnsprefetch_add_tab_cb, 0);
    g_signal_handlers_disconnect_by_func (
       web_view, dnsprefetch_do_prefetch, 0);
}

static void
dnsprefetch_deactivate_cb (MidoriExtension* extension,
                       MidoriBrowser*   browser)
{
    MidoriApp* app = midori_extension_get_app (extension);

    katze_assign (hosts, g_strdup (""));
    host_count = 0;

    g_signal_handlers_disconnect_by_func (
        extension, dnsprefetch_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, dnsprefetch_app_add_browser_cb, extension);
    midori_browser_foreach (browser, (GtkCallback)dnsprefetch_deactivate_tabs, browser);

}

static void
dnsprefetch_activate_cb (MidoriExtension* extension,
                         MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;
    guint i;

    katze_assign (hosts, g_strdup (""));
    host_count = 0;

    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++)))
        dnsprefetch_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (dnsprefetch_app_add_browser_cb), extension);

    g_object_unref (browsers);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("DNS prefetching"),
        "description", _("Prefetch IP addresses of hovered links"),
        "version", "0.1",
        "authors", "Alexander V. Butenko <a.butenka@gmail.com>",
        NULL);
    g_signal_connect (extension, "activate",
         G_CALLBACK (dnsprefetch_activate_cb), NULL);

    return extension;
}
