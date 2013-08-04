/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori/midori-session.h"

#include <midori/midori-core.h>
#include "midori-array.h"
#include "midori-extension.h"
#include "sokoke.h"

#include <glib/gi18n-lib.h>
#include <libsoup/soup-cookie-jar-sqlite.h>
#include <libsoup/soup-gnome-features.h>

    #define LIBSOUP_USE_UNSTABLE_REQUEST_API
    #include <libsoup/soup-cache.h>

#ifndef HAVE_WEBKIT2
static void
midori_soup_session_set_proxy_uri (SoupSession* session,
                                   const gchar* uri)
{
    SoupURI* proxy_uri;

    /* soup_uri_new expects a non-NULL string with a protocol */
    gchar* scheme = uri ? g_uri_parse_scheme (uri): NULL;
    if (scheme)
    {
        proxy_uri = soup_uri_new (uri);
        g_free (scheme);
    }
    else if (uri && *uri)
    {
        gchar* fixed_uri = g_strconcat ("http://", uri, NULL);
        proxy_uri = soup_uri_new (fixed_uri);
        g_free (fixed_uri);
    }
    else
        proxy_uri = NULL;
    g_object_set (session, "proxy-uri", proxy_uri, NULL);
    if (proxy_uri)
        soup_uri_free (proxy_uri);
}

static void
soup_session_settings_notify_http_proxy_cb (MidoriWebSettings* settings,
                                            GParamSpec*        pspec,
                                            SoupSession*       session)
{
    MidoriProxy proxy_type = katze_object_get_enum (settings, "proxy-type");
    if (proxy_type == MIDORI_PROXY_AUTOMATIC)
        soup_session_add_feature_by_type (session, SOUP_TYPE_PROXY_RESOLVER_GNOME);
    else if (proxy_type == MIDORI_PROXY_HTTP)
    {
        soup_session_remove_feature_by_type (session, SOUP_TYPE_PROXY_RESOLVER_GNOME);
        gchar* proxy = katze_object_get_string (settings, "http-proxy");
        GString* http_proxy = g_string_new (proxy);
        g_string_append_printf (http_proxy, ":%d", katze_object_get_int (settings, "http-proxy-port"));
        midori_soup_session_set_proxy_uri (session, http_proxy->str);
        g_string_free (http_proxy, TRUE);
        g_free (proxy);
    }
    else
    {
        soup_session_remove_feature_by_type (session, SOUP_TYPE_PROXY_RESOLVER_GNOME);
        midori_soup_session_set_proxy_uri (session, NULL);
    }
}
#endif

#if defined(HAVE_LIBSOUP_2_29_91) && WEBKIT_CHECK_VERSION (1, 1, 21)
static void
soup_session_settings_notify_first_party_cb (MidoriWebSettings* settings,
                                             GParamSpec*        pspec,
                                             gpointer           user_data)
{
    gboolean yes = katze_object_get_boolean (settings, "first-party-cookies-only");
#ifdef HAVE_WEBKIT2
    WebKitWebContext* context = webkit_web_context_get_default ();
    WebKitCookieManager* cookie_manager = webkit_web_context_get_cookie_manager (context);
    webkit_cookie_manager_set_accept_policy (cookie_manager,
        yes ? WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY
            : WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);
#else
    SoupSession* session = webkit_get_default_session ();
    gpointer jar = soup_session_get_feature (session, SOUP_TYPE_COOKIE_JAR);
    g_object_set (jar, "accept-policy",
        yes ? SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY
            : SOUP_COOKIE_JAR_ACCEPT_ALWAYS, NULL);
#endif
}
#endif

#if !defined (HAVE_WEBKIT2) && defined (HAVE_LIBSOUP_2_34_0)
/* Implemented in MidoriLocationAction */
void
midori_map_add_message (SoupMessage* message);

static void
midori_soup_session_request_started_cb (SoupSession* session,
                                        SoupMessage* message,
                                        SoupSocket*  socket,
                                        gpointer     user_data)
{
    midori_map_add_message (message);
}
#endif

#ifndef HAVE_WEBKIT2
const gchar*
midori_web_settings_get_accept_language    (MidoriWebSettings* settings);

static void
midori_soup_session_settings_accept_language_cb (SoupSession*       session,
                                                 SoupMessage*       msg,
                                                 MidoriWebSettings* settings)
{
    const gchar* accept = midori_web_settings_get_accept_language (settings);
    soup_message_headers_append (msg->request_headers, "Accept-Language", accept);

    if (katze_object_get_boolean (settings, "strip-referer"))
    {
        const gchar* referer
            = soup_message_headers_get_one (msg->request_headers, "Referer");
        SoupURI* destination = soup_message_get_uri (msg);
        SoupURI* stripped_uri;
        if (referer && destination && !strstr (referer, destination->host)
                    && (stripped_uri = soup_uri_new (referer)))
        {
            gchar* stripped_referer;
            soup_uri_set_path (stripped_uri, "");
            soup_uri_set_query (stripped_uri, NULL);
            stripped_referer = soup_uri_to_string (stripped_uri, FALSE);
            soup_uri_free (stripped_uri);
            if (strcmp (stripped_referer, referer))
            {
                if (midori_debug ("referer"))
                    g_message ("Referer '%s' stripped to '%s'", referer, stripped_referer);
                soup_message_headers_replace (msg->request_headers, "Referer",
                                              stripped_referer);
            }
            g_free (stripped_referer);
        }

        /* With HTTP, Host is optional. Strip to outsmart some filter proxies */
        if (destination && destination->scheme == SOUP_URI_SCHEME_HTTP)
            soup_message_headers_remove (msg->request_headers, "Host");
    }
}
#endif

gboolean
midori_load_soup_session (gpointer settings)
{
    #if defined(HAVE_LIBSOUP_2_29_91) && WEBKIT_CHECK_VERSION (1, 1, 21)
    g_signal_connect (settings, "notify::first-party-cookies-only",
        G_CALLBACK (soup_session_settings_notify_first_party_cb), NULL);
    #endif

#ifndef HAVE_WEBKIT2
    SoupSession* session = webkit_get_default_session ();

    #ifndef G_OS_WIN32
    #if defined (HAVE_LIBSOUP_2_37_1)
    g_object_set (session,
                  "ssl-use-system-ca-file", TRUE,
                  "ssl-strict", FALSE,
                  NULL);
    #elif defined (HAVE_LIBSOUP_2_29_91)
    const gchar* certificate_files[] =
    {
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/ssl/certs/ca-bundle.crt",
        "/usr/local/share/certs/ca-root-nss.crt", /* FreeBSD */
        "/var/lib/ca-certificates/ca-bundle.pem", /* openSUSE */
        NULL
    };
    guint i;

    for (i = 0; i < G_N_ELEMENTS (certificate_files); i++)
        if (g_access (certificate_files[i], F_OK) == 0)
        {
            g_object_set (session,
                "ssl-ca-file", certificate_files[i],
                "ssl-strict", FALSE,
                NULL);
            break;
        }
    if (i == G_N_ELEMENTS (certificate_files))
        g_warning (_("No root certificate file is available. "
                     "SSL certificates cannot be verified."));
    #endif
    #else /* G_OS_WIN32 */
    /* We cannot use "ssl-use-system-ca-file" on Windows
     * some GTLS backend pieces are missing currently.
     * Instead we specify the bundle we ship ourselves */
    gchar* certificate_file = midori_paths_get_res_filename ("ca-bundle.crt");
    g_object_set (session,
                  "ssl-ca-file", certificate_file,
                  "ssl-strict", FALSE,
                  NULL);
    g_free (certificate_file);
    #endif

    g_object_set_data (G_OBJECT (session), "midori-settings", settings);
    soup_session_settings_notify_http_proxy_cb (settings, NULL, session);
    g_signal_connect (settings, "notify::http-proxy",
        G_CALLBACK (soup_session_settings_notify_http_proxy_cb), session);
    g_signal_connect (settings, "notify::proxy-type",
        G_CALLBACK (soup_session_settings_notify_http_proxy_cb), session);

    #if defined (HAVE_LIBSOUP_2_34_0)
    g_signal_connect (session, "request-started",
        G_CALLBACK (midori_soup_session_request_started_cb), session);
    #endif
    g_signal_connect (session, "request-queued",
        G_CALLBACK (midori_soup_session_settings_accept_language_cb), settings);

    soup_session_add_feature (session, SOUP_SESSION_FEATURE (midori_hsts_new ()));

    if (midori_debug ("headers"))
    {
        SoupLogger* logger = soup_logger_new (SOUP_LOGGER_LOG_HEADERS, -1);
        soup_logger_attach (logger, session);
        g_object_unref (logger);
    }
    else if (midori_debug ("body"))
    {
        SoupLogger* logger = soup_logger_new (SOUP_LOGGER_LOG_BODY, -1);
        soup_logger_attach (logger, session);
        g_object_unref (logger);
    }

    g_object_set_data (G_OBJECT (session), "midori-session-initialized", (void*)1);
#endif
    return FALSE;
}

#ifndef HAVE_WEBKIT2
static void
midori_session_cookie_jar_changed_cb (SoupCookieJar*     jar,
                                      SoupCookie*        old_cookie,
                                      SoupCookie*        new_cookie,
                                      MidoriWebSettings* settings)
{
    if (new_cookie && new_cookie->expires)
    {
        time_t expires = soup_date_to_time_t (new_cookie->expires);
        gint age = katze_object_get_int (settings, "maximum-cookie-age");
        if (age > 0)
        {
            SoupDate* max_date = soup_date_new_from_now (
                   age * SOUP_COOKIE_MAX_AGE_ONE_DAY);
            if (soup_date_to_time_t (new_cookie->expires)
                > soup_date_to_time_t (max_date))
                   soup_cookie_set_expires (new_cookie, max_date);
        }
        else
        {
            /* An age of 0 to SoupCookie means already-expired
            A user choosing 0 days probably expects 1 hour. */
            soup_cookie_set_max_age (new_cookie, SOUP_COOKIE_MAX_AGE_ONE_HOUR);
        }
    }

    if (midori_debug ("cookies"))
        g_print ("cookie changed: old %p new %p\n", old_cookie, new_cookie);
}
#endif

gboolean
midori_load_soup_session_full (gpointer settings)
{
    #ifndef HAVE_WEBKIT2
    SoupSession* session = webkit_get_default_session ();
    SoupCookieJar* jar;
    gchar* config_file;
    SoupSessionFeature* feature;
    gboolean have_new_cookies;
    SoupSessionFeature* feature_import;

    midori_load_soup_session (settings);

    config_file = midori_paths_get_config_filename_for_writing ("logins");
    feature = g_object_new (KATZE_TYPE_HTTP_AUTH, "filename", config_file, NULL);
    soup_session_add_feature (session, feature);
    g_object_unref (feature);

    katze_assign (config_file, midori_paths_get_config_filename_for_writing ("cookies.db"));
    jar = soup_cookie_jar_sqlite_new (config_file, FALSE);
    soup_session_add_feature (session, SOUP_SESSION_FEATURE (jar));
    g_signal_connect (jar, "changed",
                      G_CALLBACK (midori_session_cookie_jar_changed_cb), settings);
    g_object_unref (jar);

    katze_assign (config_file, g_build_filename (midori_paths_get_cache_dir (), "web", NULL));
    feature = SOUP_SESSION_FEATURE (soup_cache_new (config_file, 0));
    soup_session_add_feature (session, feature);
    soup_cache_set_max_size (SOUP_CACHE (feature),
        katze_object_get_int (settings, "maximum-cache-size") * 1024 * 1024);
    soup_cache_load (SOUP_CACHE (feature));
    g_free (config_file);
    #endif
    return FALSE;
}

static void
extensions_update_cb (KatzeArray* extensions,
                      MidoriApp*  app)
{
    MidoriWebSettings* settings = katze_object_get_object (app, "settings");
    g_object_notify (G_OBJECT (settings), "load-on-startup");
    g_object_unref (settings);
}

gboolean
midori_load_extensions (gpointer data)
{
    MidoriApp* app = MIDORI_APP (data);
    gchar** keys = g_object_get_data (G_OBJECT (app), "extensions");
    KatzeArray* extensions;
    #ifdef G_ENABLE_DEBUG
    gboolean startup_timer = midori_debug ("startup");
    GTimer* timer = startup_timer ? g_timer_new () : NULL;
    #endif

    /* Load extensions */
    extensions = katze_array_new (MIDORI_TYPE_EXTENSION);
    g_signal_connect (extensions, "update", G_CALLBACK (extensions_update_cb), app);
    g_object_set (app, "extensions", extensions, NULL);
    midori_extension_load_from_folder (app, keys, TRUE);

    #ifdef G_ENABLE_DEBUG
    if (startup_timer)
        g_debug ("Extensions:\t%f", g_timer_elapsed (timer, NULL));
    #endif

    return FALSE;
}

static void
settings_notify_cb (MidoriWebSettings* settings,
                    GParamSpec*        pspec,
                    MidoriApp*         app)
{
    GError* error = NULL;
    gchar* config_file;

    /* Skip state related properties to avoid disk I/ O */
    if (pspec && midori_settings_delay_saving (MIDORI_SETTINGS (settings), pspec->name))
        return;

    config_file = midori_paths_get_config_filename_for_writing ("config");
    if (!midori_settings_save_to_file (settings, G_OBJECT (app), config_file, &error))
    {
        g_warning (_("The configuration couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);
}

void
midori_session_persistent_settings (MidoriWebSettings* settings,
                                    MidoriApp*         app)
{
    g_signal_connect_after (settings, "notify", G_CALLBACK (settings_notify_cb), app);
}

static void
midori_browser_action_last_session_activate_cb (GtkAction*     action,
                                                MidoriBrowser* browser)
{
    KatzeArray* old_session = katze_array_new (KATZE_TYPE_ITEM);
    gchar* config_file = midori_paths_get_config_filename_for_reading ("session.old.xbel");
    GError* error = NULL;
    if (midori_array_from_file (old_session, config_file, "xbel-tiny", &error))
    {
        KatzeItem* item;
        KATZE_ARRAY_FOREACH_ITEM (item, old_session)
            midori_browser_add_item (browser, item);
    }
    else
    {
        sokoke_message_dialog (GTK_MESSAGE_ERROR,
            _("The session couldn't be loaded: %s\n"), error->message, FALSE);
        g_error_free (error);
    }
    g_free (config_file);
    gtk_action_set_sensitive (action, FALSE);
    g_signal_handlers_disconnect_by_func (action,
        midori_browser_action_last_session_activate_cb, browser);
}

static void
midori_session_accel_map_changed_cb (GtkAccelMap*    accel_map,
                                     gchar*          accel_path,
                                     guint           accel_key,
                                     GdkModifierType accel_mods)
{
    gchar* config_file = midori_paths_get_config_filename_for_writing ("accels");
    gtk_accel_map_save (config_file);
    g_free (config_file);
}

static guint save_timeout = 0;

static gboolean
midori_session_save_timeout_cb (KatzeArray* session)
{
    gchar* config_file = midori_paths_get_config_filename_for_writing ("session.xbel");
    GError* error = NULL;
    if (!midori_array_to_file (session, config_file, "xbel-tiny", &error))
    {
        g_warning (_("The session couldn't be saved. %s"), error->message);
        g_error_free (error);
    }
    g_free (config_file);

    save_timeout = 0;
    return FALSE;
}

static void
midori_browser_session_cb (MidoriBrowser* browser,
                           gpointer       pspec,
                           KatzeArray*    session)
{
    if (!save_timeout)
        save_timeout = midori_timeout_add_seconds (
            5, (GSourceFunc)midori_session_save_timeout_cb, session, NULL);
}

static void
midori_app_quit_cb (MidoriBrowser* browser,
                    KatzeArray*    session)
{
    midori_session_save_timeout_cb (session);
}

static void
midori_browser_weak_notify_cb (MidoriBrowser* browser,
                               KatzeArray*    session)
{
    g_object_disconnect (browser, "any-signal",
                         G_CALLBACK (midori_browser_session_cb), session, NULL);
}

gboolean
midori_load_session (gpointer data)
{
    KatzeArray* saved_session = KATZE_ARRAY (data);
    MidoriBrowser* browser;
    MidoriApp* app = katze_item_get_parent (KATZE_ITEM (saved_session));
    MidoriWebSettings* settings = katze_object_get_object (app, "settings");
    MidoriStartup load_on_startup;
    gchar* config_file;
    KatzeArray* session;
    KatzeItem* item;
    gint64 current;
    gchar** open_uris = g_object_get_data (G_OBJECT (app), "open-uris");
    gchar** execute_commands = g_object_get_data (G_OBJECT (app), "execute-commands");
    gchar* uri;
    guint i = 0;
    #ifdef G_ENABLE_DEBUG
    gboolean startup_timer = midori_debug ("startup");
    GTimer* timer = startup_timer ? g_timer_new () : NULL;
    #endif

    browser = midori_app_create_browser (app);
    midori_session_persistent_settings (settings, app);

    config_file = midori_paths_get_config_filename_for_reading ("session.old.xbel");
    if (g_access (config_file, F_OK) == 0)
    {
        GtkActionGroup* action_group = midori_browser_get_action_group (browser);
        GtkAction* action = gtk_action_group_get_action (action_group, "LastSession");
        g_signal_connect (action, "activate",
            G_CALLBACK (midori_browser_action_last_session_activate_cb), browser);
        gtk_action_set_visible (action, TRUE);
    }
    midori_app_add_browser (app, browser);
    gtk_widget_show (GTK_WIDGET (browser));

    katze_assign (config_file, midori_paths_get_config_filename_for_reading ("accels"));
    g_signal_connect_after (gtk_accel_map_get (), "changed",
        G_CALLBACK (midori_session_accel_map_changed_cb), NULL);

    load_on_startup = (MidoriStartup)g_object_get_data (G_OBJECT (settings), "load-on-startup");
    if (katze_array_is_empty (saved_session))
    {
        item = katze_item_new ();
        if (open_uris)
        {
            uri = sokoke_magic_uri (open_uris[i], TRUE, TRUE);
            katze_item_set_uri (item, uri);
            g_free (uri);
            i++;
        } else if (load_on_startup == MIDORI_STARTUP_BLANK_PAGE)
            katze_item_set_uri (item, "about:new");
        else
            katze_item_set_uri (item, "about:home");
        katze_array_add_item (saved_session, item);
        g_object_unref (item);
    }

    session = midori_browser_get_proxy_array (browser);
    KATZE_ARRAY_FOREACH_ITEM (item, saved_session)
    {
        katze_item_set_meta_integer (item, "append", 1);
        katze_item_set_meta_integer (item, "dont-write-history", 1);
        if (load_on_startup == MIDORI_STARTUP_DELAYED_PAGES
         || katze_item_get_meta_integer (item, "delay") == MIDORI_DELAY_PENDING_UNDELAY)
            katze_item_set_meta_integer (item, "delay", MIDORI_DELAY_DELAYED);
        midori_browser_add_item (browser, item);
    }

    current = katze_item_get_meta_integer (KATZE_ITEM (saved_session), "current");
    if (!(item = katze_array_get_nth_item (saved_session, current)))
    {
        current = 0;
        item = katze_array_get_nth_item (saved_session, 0);
    }
    midori_browser_set_current_page (browser, current);
    if (midori_uri_is_blank (katze_item_get_uri (item)))
        midori_browser_activate_action (browser, "Location");

    /* `i` also used above; in that case we won't re-add the same URLs here */
    for (; open_uris && open_uris[i]; i++)
    {
        uri = sokoke_magic_uri (open_uris[i], TRUE, TRUE);
        midori_browser_add_uri (browser, uri);
        g_free (uri);
    }

    g_object_unref (settings);
    g_object_unref (saved_session);
    g_free (config_file);

    g_signal_connect_after (browser, "add-tab",
        G_CALLBACK (midori_browser_session_cb), session);
    g_signal_connect_after (browser, "remove-tab",
        G_CALLBACK (midori_browser_session_cb), session);
    g_signal_connect (app, "quit",
        G_CALLBACK (midori_app_quit_cb), session);
    g_object_weak_ref (G_OBJECT (session),
        (GWeakNotify)(midori_browser_weak_notify_cb), browser);

    if (execute_commands != NULL)
        midori_app_send_command (app, execute_commands);

    #ifdef G_ENABLE_DEBUG
    if (startup_timer)
        g_debug ("Session setup:\t%f", g_timer_elapsed (timer, NULL));
    #endif

    return FALSE;
}

