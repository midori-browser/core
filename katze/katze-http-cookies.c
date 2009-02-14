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

#include "katze-http-cookies.h"

#if HAVE_LIBSOUP
    #include <libsoup/soup.h>
#endif

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

struct _KatzeHttpCookies
{
    GObject parent_instance;
};

struct _KatzeHttpCookiesClass
{
    GObjectClass parent_class;
};

#if HAVE_LIBSOUP_2_25_2
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
    time_t now = time (NULL);

    if (!g_file_get_contents (filename, &contents, &length, NULL))
        return;

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
static void
write_cookie (FILE *out, SoupCookie *cookie)
{
	fseek (out, 0, SEEK_END);

	fprintf (out, "%s%s\t%s\t%s\t%s\t%lu\t%s\t%s\n",
		 cookie->http_only ? "#HttpOnly_" : "",
		 cookie->domain,
		 *cookie->domain == '.' ? "TRUE" : "FALSE",
		 cookie->path,
		 cookie->secure ? "TRUE" : "FALSE",
		 (gulong)soup_date_to_time_t (cookie->expires),
		 cookie->name,
		 cookie->value);
}

/* Cookie jar saving to Mozilla format
   Copyright (C) 2008 Xan Lopez <xan@gnome.org>
   Copyright (C) 2008 Dan Winship <danw@gnome.org>
   Copied from libSoup 2.24, coding style preserved */
static void
delete_cookie (const char *filename, SoupCookie *cookie)
{
	char *contents = NULL, *line, *p;
	gsize length = 0;
	FILE *f;
	SoupCookie *c;
	time_t now = time (NULL);

	if (!g_file_get_contents (filename, &contents, &length, NULL))
		return;

	f = fopen (filename, "w");
	if (!f) {
		g_free (contents);
		return;
	}

	line = contents;
	for (p = contents; *p; p++) {
		/* \r\n comes out as an extra empty line and gets ignored */
		if (*p == '\r' || *p == '\n') {
			*p = '\0';
			c = parse_cookie (line, now);
			if (!c)
				continue;
			if (!soup_cookie_equal (cookie, c))
				write_cookie (f, c);
			line = p + 1;
			soup_cookie_free (c);
		}
	}
	c = parse_cookie (line, now);
	if (c) {
		if (!soup_cookie_equal (cookie, c))
			write_cookie (f, c);
		soup_cookie_free (c);
	}

	g_free (contents);
	fclose (f);
}

/* Cookie jar saving to Mozilla format
   Copyright (C) 2008 Xan Lopez <xan@gnome.org>
   Copyright (C) 2008 Dan Winship <danw@gnome.org>
   Mostly copied from libSoup 2.24, coding style adjusted */
static void
cookie_jar_changed_cb (SoupCookieJar* jar,
                       SoupCookie*    old_cookie,
                       SoupCookie*    new_cookie,
                       gchar*         filename)
{
    GObject* app;
    GObject* settings;
    guint accept_cookies;

    if (old_cookie)
        delete_cookie (filename, old_cookie);

    if (new_cookie)
    {
        FILE *out;

        /* FIXME: This is really a hack */
        app = g_type_get_qdata (SOUP_TYPE_SESSION,
            g_quark_from_static_string ("midori-app"));
        settings = katze_object_get_object (G_OBJECT (app), "settings");
        accept_cookies = katze_object_get_enum (settings, "accept-cookies");
        if (accept_cookies == 2 /* MIDORI_ACCEPT_COOKIES_NONE */)
        {
            soup_cookie_jar_delete_cookie (jar, new_cookie);
        }
        else if (accept_cookies == 1 /* MIDORI_ACCEPT_COOKIES_SESSION */
            && new_cookie->expires)
        {
            soup_cookie_jar_delete_cookie (jar, new_cookie);
        }
        else if (new_cookie->expires)
        {
            gint age = katze_object_get_int (settings, "maximum-cookie-age");
            soup_cookie_set_max_age (new_cookie,
                age * SOUP_COOKIE_MAX_AGE_ONE_DAY);

            if (!(out = fopen (filename, "a")))
                return;
            write_cookie (out, new_cookie);
            if (fclose (out) != 0)
                return;
        }
    }
}

static void
katze_http_cookies_session_request_queued_cb (SoupSession*        session,
                                              SoupMessage*        msg,
                                              SoupSessionFeature* feature)
{
    const gchar* filename;
    SoupSessionFeature* cookie_jar;

    filename = g_object_get_data (G_OBJECT (feature), "filename");
    if (!filename)
        return;

    cookie_jar = soup_session_get_feature (session, SOUP_TYPE_COOKIE_JAR);
    if (!cookie_jar)
        return;

    g_type_set_qdata (SOUP_TYPE_COOKIE_JAR,
        g_quark_from_static_string ("midori-has-jar"), (void*)1);
    cookie_jar_load (SOUP_COOKIE_JAR (cookie_jar), filename);
    g_signal_connect_data (cookie_jar, "changed",
        G_CALLBACK (cookie_jar_changed_cb), g_strdup (filename),
        (GClosureNotify)g_free, 0);
    g_signal_handlers_disconnect_by_func (session,
        katze_http_cookies_session_request_queued_cb, feature);
}

static void
katze_http_cookies_attach (SoupSessionFeature* feature,
                           SoupSession*        session)
{
    g_signal_connect (session, "request-queued",
        G_CALLBACK (katze_http_cookies_session_request_queued_cb), feature);
}

static void
katze_http_cookies_detach (SoupSessionFeature* feature,
                           SoupSession*        session)
{
    g_signal_handlers_disconnect_by_func (session,
        katze_http_cookies_session_request_queued_cb, feature);
}

static void
katze_http_cookies_session_feature_iface_init (SoupSessionFeatureInterface *iface,
                                               gpointer                     data)
{
    iface->attach = katze_http_cookies_attach;
    iface->detach = katze_http_cookies_detach;
}
#else
G_DEFINE_TYPE (KatzeHttpCookies, katze_http_cookies, G_TYPE_OBJECT)
#endif

static void
katze_http_cookies_class_init (KatzeHttpCookiesClass* class)
{
    /* Nothing to do. */
}

static void
katze_http_cookies_init (KatzeHttpCookies* http_cookies)
{
    /* Nothing to do. */
}
