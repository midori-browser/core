/*
 Copyright (C) 2008-2010 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2011 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "katze-http-cookies-sqlite.h"

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
    #include <unistd.h>
#endif
#include <glib/gi18n.h>
#include <libsoup/soup.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <sqlite3.h>

#define QUERY_ALL "SELECT id, name, value, host, path, expiry, lastAccessed, isSecure, isHttpOnly FROM moz_cookies;"
#define CREATE_TABLE "CREATE TABLE IF NOT EXISTS moz_cookies (id INTEGER PRIMARY KEY, name TEXT, value TEXT, host TEXT, path TEXT,expiry INTEGER, lastAccessed INTEGER, isSecure INTEGER, isHttpOnly INTEGER)"
#define QUERY_INSERT "INSERT INTO moz_cookies VALUES(NULL, %Q, %Q, %Q, %Q, %d, NULL, %d, %d);"
#define QUERY_DELETE "DELETE FROM moz_cookies WHERE name=%Q AND host=%Q;"

enum {
    COL_ID,
    COL_NAME,
    COL_VALUE,
    COL_HOST,
    COL_PATH,
    COL_EXPIRY,
    COL_LAST_ACCESS,
    COL_SECURE,
    COL_HTTP_ONLY,
    N_COL,
};

struct _KatzeHttpCookiesSqlite
{
    GObject parent_instance;
    gchar* filename;
    SoupCookieJar* jar;
    sqlite3 *db;
    guint counter;
};

struct _KatzeHttpCookiesSqliteClass
{
    GObjectClass parent_class;
};

static void
katze_http_cookies_sqlite_session_feature_iface_init (SoupSessionFeatureInterface *iface,
                                                      gpointer                     data);

G_DEFINE_TYPE_WITH_CODE (KatzeHttpCookiesSqlite, katze_http_cookies_sqlite, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SOUP_TYPE_SESSION_FEATURE,
                         katze_http_cookies_sqlite_session_feature_iface_init));

/* Cookie jar saving into sqlite database
   Copyright (C) 2008 Diego Escalante Urrelo
   Copyright (C) 2009 Collabora Ltd.
   Mostly copied from libSoup 2.30, coding style retained */

/* Follows sqlite3 convention; returns TRUE on error */
static gboolean
katze_http_cookies_sqlite_open_db (KatzeHttpCookiesSqlite* http_cookies)
{
    char *error = NULL;

   if (sqlite3_open (http_cookies->filename, &http_cookies->db)) {
        sqlite3_close (http_cookies->db);
        g_warning ("Can't open %s", http_cookies->filename);
        return TRUE;
    }

    if (sqlite3_exec (http_cookies->db, CREATE_TABLE, NULL, NULL, &error)) {
        g_warning ("Failed to execute query: %s", error);
        sqlite3_free (error);
    }

    if (sqlite3_exec (http_cookies->db, "PRAGMA secure_delete = 1;",
        NULL, NULL, &error)) {
        g_warning ("Failed to execute query: %s", error);
        sqlite3_free (error);
    }

    sqlite3_exec (http_cookies->db,
        /* Arguably cookies are like a cache, so performance over integrity */
        "PRAGMA synchronous = OFF; PRAGMA temp_store = MEMORY;"
        "PRAGMA count_changes = OFF; PRAGMA journal_mode = TRUNCATE;",
        NULL, NULL, &error);

    return FALSE;
}

static void
katze_http_cookies_sqlite_load (KatzeHttpCookiesSqlite* http_cookies)
{
    const char *name, *value, *host, *path;
    sqlite3_stmt* stmt;
    SoupCookie *cookie = NULL;
    gint64 expire_time;
    time_t now;
    int max_age;
    gboolean http_only = FALSE, secure = FALSE;
    char *query;
    int result;

    if (http_cookies->db == NULL) {
        if (katze_http_cookies_sqlite_open_db (http_cookies))
            return;
    }

    sqlite3_prepare_v2 (http_cookies->db, QUERY_ALL, strlen (QUERY_ALL) + 1, &stmt, NULL);
    result = sqlite3_step (stmt);
    if (result != SQLITE_ROW)
    {
        if (result == SQLITE_ERROR)
            g_print (_("Failed to load cookies\n"));
        sqlite3_reset (stmt);
        return;
    }

    while (result == SQLITE_ROW)
    {
        now = time (NULL);
        name = (const char*)sqlite3_column_text (stmt, COL_NAME);
        value = (const char*)sqlite3_column_text (stmt, COL_VALUE);
        host = (const char*)sqlite3_column_text (stmt, COL_HOST);
        path = (const char*)sqlite3_column_text (stmt, COL_PATH);
        expire_time = sqlite3_column_int64 (stmt,COL_EXPIRY);
        secure = sqlite3_column_int (stmt, COL_SECURE);
        http_only = sqlite3_column_int (stmt, COL_HTTP_ONLY);

        if (now >= expire_time)
        {
            /* Cookie expired, remove it from database */
            query = sqlite3_mprintf (QUERY_DELETE, name, host);
            sqlite3_exec (http_cookies->db, QUERY_DELETE, NULL, NULL, NULL);
            sqlite3_free (query);
            result = sqlite3_step (stmt);
            continue;
        }
        max_age = (expire_time - now <= G_MAXINT ? expire_time - now : G_MAXINT);
        cookie = soup_cookie_new (name, value, host, path, max_age);

        if (secure)
            soup_cookie_set_secure (cookie, TRUE);
        if (http_only)
            soup_cookie_set_http_only (cookie, TRUE);

        soup_cookie_jar_add_cookie (http_cookies->jar, cookie);
        result = sqlite3_step (stmt);
    }

    if (stmt)
    {
        sqlite3_reset (stmt);
        sqlite3_clear_bindings (stmt);
    }
}
static void
katze_http_cookies_sqlite_jar_changed_cb (SoupCookieJar*    jar,
                                          SoupCookie*       old_cookie,
                                          SoupCookie*       new_cookie,
                                          KatzeHttpCookiesSqlite* http_cookies)
{
    GObject* settings;
    char *query;
    time_t expires = 0; /* Avoid warning */

    if (http_cookies->db == NULL) {
        if (katze_http_cookies_sqlite_open_db (http_cookies))
            return;
    }

    if (new_cookie && new_cookie->expires)
    {
        gint age;

        expires = soup_date_to_time_t (new_cookie->expires);
        settings = g_object_get_data (G_OBJECT (jar), "midori-settings");
        age = katze_object_get_int (settings, "maximum-cookie-age");
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

    if (!g_strcmp0 (g_getenv ("MIDORI_DEBUG"), "cookies"))
        http_cookies->counter++;

    if (old_cookie) {
        query = sqlite3_mprintf (QUERY_DELETE,
                     old_cookie->name,
                     old_cookie->domain);
        sqlite3_exec (http_cookies->db, query, NULL, NULL, NULL);
        sqlite3_free (query);
    }

    if (new_cookie && new_cookie->expires) {

        query = sqlite3_mprintf (QUERY_INSERT,
                     new_cookie->name,
                     new_cookie->value,
                     new_cookie->domain,
                     new_cookie->path,
                     expires,
                     new_cookie->secure,
                     new_cookie->http_only);
        sqlite3_exec (http_cookies->db, query, NULL, NULL, NULL);
        sqlite3_free (query);
    }
}

static void
katze_http_cookies_sqlite_attach (SoupSessionFeature* feature,
                                  SoupSession*        session)
{
    KatzeHttpCookiesSqlite* http_cookies = (KatzeHttpCookiesSqlite*)feature;
    const gchar* filename = g_object_get_data (G_OBJECT (feature), "filename");
    SoupSessionFeature* jar = soup_session_get_feature (session, SOUP_TYPE_COOKIE_JAR);
    g_return_if_fail (jar != NULL);
    g_return_if_fail (filename != NULL);
    katze_assign (http_cookies->filename, g_strdup (filename));
    http_cookies->jar = g_object_ref (jar);
    katze_http_cookies_sqlite_open_db (http_cookies);
    katze_http_cookies_sqlite_load (http_cookies);
    g_signal_connect (jar, "changed",
        G_CALLBACK (katze_http_cookies_sqlite_jar_changed_cb), feature);

}

static void
katze_http_cookies_sqlite_detach (SoupSessionFeature* feature,
                                  SoupSession*        session)
{
    KatzeHttpCookiesSqlite* http_cookies = (KatzeHttpCookiesSqlite*)feature;
    katze_assign (http_cookies->filename, NULL);
    katze_object_assign (http_cookies->jar, NULL);
    sqlite3_close (http_cookies->db);
}

static void
katze_http_cookies_sqlite_session_feature_iface_init (SoupSessionFeatureInterface *iface,
                                                      gpointer                     data)
{
    iface->attach = katze_http_cookies_sqlite_attach;
    iface->detach = katze_http_cookies_sqlite_detach;
}

static void
katze_http_cookies_sqlite_finalize (GObject* object)
{
    katze_http_cookies_sqlite_detach ((SoupSessionFeature*)object, NULL);
}

static void
katze_http_cookies_sqlite_class_init (KatzeHttpCookiesSqliteClass* class)
{
    GObjectClass* gobject_class = (GObjectClass*)class;
    gobject_class->finalize = katze_http_cookies_sqlite_finalize;
}

static void
katze_http_cookies_sqlite_init (KatzeHttpCookiesSqlite* http_cookies)
{
    http_cookies->filename = NULL;
    http_cookies->jar = NULL;
    http_cookies->db = NULL;
    http_cookies->counter = 0;
}
