/*
 Copyright (C) 2008-2010 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "katze-http-cookies.h"

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
    #include <unistd.h>
#endif
#include <glib/gi18n.h>
#include <libsoup/soup.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>

struct _KatzeHttpCookies
{
    GObject parent_instance;
    gchar* filename;
    SoupCookieJar* jar;
    guint timeout;
    guint counter;
};

struct _KatzeHttpCookiesClass
{
    GObjectClass parent_class;
};

static void
katze_http_cookies_session_feature_iface_init (SoupSessionFeatureInterface *iface,
                                               gpointer                     data);

G_DEFINE_TYPE_WITH_CODE (KatzeHttpCookies, katze_http_cookies, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SOUP_TYPE_SESSION_FEATURE,
                         katze_http_cookies_session_feature_iface_init));

/* Cookie jar saving to Mozilla format
   Copyright (C) 2008 Xan Lopez <xan@gnome.org>
   Copyright (C) 2008 Dan Winship <danw@gnome.org>
   Mostly copied from libSoup 2.24, coding style adjusted */
static SoupCookie*
parse_cookie (gchar* line,
              time_t now)
{
    gchar** result;
    SoupCookie *cookie = NULL;
    gboolean http_only;
    time_t max_age;
    gchar* host/*, *is_domain*/, *path, *secure, *expires, *name, *value;

    if (g_str_has_prefix (line, "#HttpOnly_"))
    {
        http_only = TRUE;
        line += strlen ("#HttpOnly_");
    }
    else if (*line == '#' || g_ascii_isspace (*line))
        return cookie;
    else
        http_only = FALSE;

    result = g_strsplit (line, "\t", -1);
    if (g_strv_length (result) != 7)
        goto out;

    /* Check this first */
    expires = result[4];
    max_age = strtoul (expires, NULL, 10) - now;
    if (max_age <= 0)
        goto out;

    host = result[0];
    /* is_domain = result[1]; */
    path = result[2];
    secure = result[3];

    name = result[5];
    value = result[6];

    cookie = soup_cookie_new (name, value, host, path, max_age);

    if (strcmp (secure, "FALSE"))
        soup_cookie_set_secure (cookie, TRUE);
    if (http_only)
        soup_cookie_set_http_only (cookie, TRUE);

    out:
        g_strfreev (result);

    return cookie;
}

/* Cookie jar saving to Mozilla format
   Copyright (C) 2008 Xan Lopez <xan@gnome.org>
   Copyright (C) 2008 Dan Winship <danw@gnome.org>
   Mostly copied from libSoup 2.24, coding style adjusted */
static void
parse_line (SoupCookieJar* jar,
            gchar*         line,
            time_t         now)
{
    SoupCookie* cookie;

    if ((cookie = parse_cookie (line, now)))
        soup_cookie_jar_add_cookie (jar, cookie);
}

/* Cookie jar saving to Mozilla format
   Copyright (C) 2008 Xan Lopez <xan@gnome.org>
   Copyright (C) 2008 Dan Winship <danw@gnome.org>
   Mostly copied from libSoup 2.24, coding style adjusted */
static void
cookie_jar_load (SoupCookieJar* jar,
                 const gchar*   filename)
{
    char* contents = NULL;
    gchar* line;
    gchar* p;
    gsize length = 0;
    time_t now;

    if (!g_file_get_contents (filename, &contents, &length, NULL))
        return;

    now = time (NULL);
    line = contents;
    for (p = contents; *p; p++)
    {
        /* \r\n comes out as an extra empty line and gets ignored */
        if (*p == '\r' || *p == '\n')
        {
            *p = '\0';
            parse_line (jar, line, now);
            line = p + 1;
        }
    }
    parse_line (jar, line, now);

    g_free (contents);
}

/* Cookie jar saving to Mozilla format
   Copyright (C) 2008 Xan Lopez <xan@gnome.org>
   Copyright (C) 2008 Dan Winship <danw@gnome.org>
   Copied from libSoup 2.24, coding style preserved */
static gboolean
write_cookie (FILE *out, SoupCookie *cookie)
{
	if (fprintf (out, "%s%s\t%s\t%s\t%s\t%lu\t%s\t%s\n",
		 cookie->http_only ? "#HttpOnly_" : "",
		 cookie->domain,
		 *cookie->domain == '.' ? "TRUE" : "FALSE",
		 cookie->path,
		 cookie->secure ? "TRUE" : "FALSE",
		 (gulong)soup_date_to_time_t (cookie->expires),
		 cookie->name,
		 cookie->value) < 0)
            return FALSE;
        return TRUE;
}

static gboolean
katze_http_cookies_update_jar (KatzeHttpCookies* http_cookies)
{
    gint fn = 0;
    FILE* f = NULL;
    gchar* temporary_filename = NULL;
    GSList* cookies;

    if (http_cookies->timeout > 0)
    {
        g_source_remove (http_cookies->timeout);
        http_cookies->timeout = 0;
    }

    temporary_filename = g_strconcat (http_cookies->filename, ".XXXXXX", NULL);
    if ((fn = g_mkstemp (temporary_filename)) == -1)
        goto failed;
    if (!((f = fdopen (fn, "wb"))))
        goto failed;

    cookies = soup_cookie_jar_all_cookies (http_cookies->jar);
    for (; cookies != NULL; cookies = g_slist_next (cookies))
    {
        SoupCookie* cookie = cookies->data;
        if (cookie->expires && !soup_date_is_past (cookie->expires))
            write_cookie (f, cookie);
        soup_cookie_free (cookie);
    }
    g_slist_free (cookies);

    if (fclose (f) != 0)
    {
        f = NULL;
        goto failed;
    }
    f = NULL;

    if (g_rename (temporary_filename, http_cookies->filename) == -1)
        goto failed;
    g_free (temporary_filename);

    if (!g_strcmp0 (g_getenv ("MIDORI_DEBUG"), "cookies"))
    {
        g_print ("KatzeHttpCookies: %d cookies changed\n", http_cookies->counter);
        http_cookies->counter = 0;
    }
    return FALSE;

failed:
    if (f)
        fclose (f);
    g_unlink (temporary_filename);
    g_free (temporary_filename);
    if (!g_strcmp0 (g_getenv ("MIDORI_DEBUG"), "cookies"))
        g_print ("KatzeHttpCookies: Failed to write '%s'\n",
                 http_cookies->filename);
    return FALSE;
}

static void
katze_http_cookies_jar_changed_cb (SoupCookieJar*    jar,
                                   SoupCookie*       old_cookie,
                                   SoupCookie*       new_cookie,
                                   KatzeHttpCookies* http_cookies)
{
    GObject* settings;

    if (old_cookie)
        soup_cookie_set_max_age (old_cookie, 0);

    if (new_cookie)
    {
        settings = g_object_get_data (G_OBJECT (jar), "midori-settings");
        if (new_cookie->expires)
        {
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
    }

    if (!g_strcmp0 (g_getenv ("MIDORI_DEBUG"), "cookies"))
        http_cookies->counter++;

    if (!http_cookies->timeout && (old_cookie || new_cookie->expires))
        http_cookies->timeout = g_timeout_add_seconds (5,
            (GSourceFunc)katze_http_cookies_update_jar, http_cookies);
}

static void
katze_http_cookies_attach (SoupSessionFeature* feature,
                           SoupSession*        session)
{
    KatzeHttpCookies* http_cookies = (KatzeHttpCookies*)feature;
    const gchar* filename = g_object_get_data (G_OBJECT (feature), "filename");
    SoupSessionFeature* jar = soup_session_get_feature (session, SOUP_TYPE_COOKIE_JAR);
    g_return_if_fail (jar != NULL);
    g_return_if_fail (filename != NULL);
    katze_assign (http_cookies->filename, g_strdup (filename));
    http_cookies->jar = g_object_ref (jar);
    cookie_jar_load (http_cookies->jar, http_cookies->filename);
    g_signal_connect (jar, "changed",
        G_CALLBACK (katze_http_cookies_jar_changed_cb), feature);

}

static void
katze_http_cookies_detach (SoupSessionFeature* feature,
                           SoupSession*        session)
{
    KatzeHttpCookies* http_cookies = (KatzeHttpCookies*)feature;
    if (http_cookies->timeout > 0)
        katze_http_cookies_update_jar (http_cookies);
    katze_assign (http_cookies->filename, NULL);
    katze_object_assign (http_cookies->jar, NULL);
}

static void
katze_http_cookies_session_feature_iface_init (SoupSessionFeatureInterface *iface,
                                               gpointer                     data)
{
    iface->attach = katze_http_cookies_attach;
    iface->detach = katze_http_cookies_detach;
}

static void
katze_http_cookies_finalize (GObject* object)
{
    katze_http_cookies_detach ((SoupSessionFeature*)object, NULL);
}

static void
katze_http_cookies_class_init (KatzeHttpCookiesClass* class)
{
    GObjectClass* gobject_class = (GObjectClass*)class;
    gobject_class->finalize = katze_http_cookies_finalize;
}

static void
katze_http_cookies_init (KatzeHttpCookies* http_cookies)
{
    http_cookies->filename = NULL;
    http_cookies->jar = NULL;
    http_cookies->timeout = 0;
    http_cookies->counter = 0;
}
