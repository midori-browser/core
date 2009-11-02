/*
 Copyright (C) 2009 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
*/

#define MAXCHARS 20
#define MINCHARS 2

#include <midori/midori.h>

#include "config.h"

#include <glib/gstdio.h>
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#if HAVE_SQLITE
    #include <sqlite3.h>
#endif

static GHashTable* global_keys;
static gchar* jsforms;

static gboolean
formhistory_prepare_js ()
{
   gchar* autosuggest;
   gchar* style;
   guint i;
   gchar* file;

   gchar* data_path = g_build_filename (MDATADIR, PACKAGE_NAME, "res", NULL);
   file = g_build_filename (data_path,"/autosuggestcontrol.js",NULL);
   if (!g_file_test (file, G_FILE_TEST_EXISTS))
       return FALSE;
   g_file_get_contents (file, &autosuggest, NULL, NULL);
   g_strchomp (autosuggest);

   file = g_build_filename (data_path,"/autosuggestcontrol.css",NULL);
   if (!g_file_test (file, G_FILE_TEST_EXISTS))
       return FALSE;
   g_file_get_contents (file, &style, NULL, NULL);
   g_strchomp (style);
   i = 0;
   while (style[i])
   {
       if (style[i] == '\n')
           style[i] = ' ';
       i++;
   }

   jsforms = g_strdup_printf (
        "%s"
        "window.addEventListener (\"load\", function () { initSuggestions (); }, true);"
        "window.addEventListener ('DOMContentLoaded',"
        "function () {"
        "var mystyle = document.createElement(\"style\");"
        "mystyle.setAttribute(\"type\", \"text/css\");"
        "mystyle.appendChild(document.createTextNode(\"%s\"));"
        "var head = document.getElementsByTagName(\"head\")[0];"
        "if (head) head.appendChild(mystyle);"
        "else document.documentElement.insertBefore(mystyle, document.documentElement.firstChild);"
        "}, true);",
        autosuggest,
        style);
   g_strstrip (jsforms);
   g_free (data_path);
   g_free (file);
   g_free (style);
   g_free (autosuggest);
   return TRUE;
}

static gchar*
formhistory_build_js ()
{
    gchar* suggestions = g_strdup ("");
    GHashTableIter iter;
    gpointer key, value;
    gchar* script;

    g_hash_table_iter_init (&iter, global_keys);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
       gchar* _suggestions = g_strdup_printf ("%s arr[\"%s\"] = [%s]; ",
                                              suggestions, (char*)key, (char*)value);
       katze_assign (suggestions, _suggestions);
    }
    script = g_strdup_printf ("function FormSuggestions(eid) { "
                              "arr = new Array();"
                              "%s"
                              "this.suggestions = arr[eid]; }"
                              "%s",
                              suggestions,
                              jsforms);
    g_free (suggestions);
   return script;
}

static void
formhistory_update_database (gpointer     db,
                             const gchar* key,
                             const gchar* value)
{
    #if HAVE_SQLITE
    /* FIXME: Write keys to the database */
    /* g_print (":: %s= %s\n", key, value); */
    #endif
}

static void
formhistory_update_main_hash (GHashTable* keys,
                              gpointer    db)
{
    GHashTableIter iter;
    gchar* key;
    gchar* value;

    g_hash_table_iter_init (&iter, keys);
    while (g_hash_table_iter_next (&iter, (gpointer)&key, (gpointer)&value))
    {
        guint length;
        gchar* tmp;

        if (!(value && *value))
            continue;
        length = strlen (value);
        if (length > MAXCHARS || length < MINCHARS)
            continue;

        if ((tmp = g_hash_table_lookup (global_keys, (gpointer)key)))
        {
            gchar* rvalue = g_strdup_printf ("\"%s\"",value);
            if (!g_regex_match_simple (rvalue, tmp,
                                       G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
            {
                gchar* new_value = g_strdup_printf ("%s%s,", tmp, rvalue);
                g_hash_table_replace (global_keys, key, new_value);
                formhistory_update_database (db, key, value);
            }
            g_free (rvalue);
        }
        else
        {
            gchar* new_value = g_strdup_printf ("\"%s\",",value);
            g_hash_table_insert (global_keys, key, new_value);
            formhistory_update_database (db, key, value);
        }
    }
}

static void
formhistory_session_request_queued_cb (SoupSession*     session,
                                       SoupMessage*     msg,
                                       MidoriExtension* extension)
{
    gchar* method = katze_object_get_string (msg, "method");
    if (method && !strncmp (method, "POST", 4))
    {
        SoupMessageBody* body = msg->request_body;
        if (soup_message_body_get_accumulate (body))
        {
            SoupBuffer* buffer;
            GHashTable* keys;
            gpointer db;

            buffer = soup_message_body_flatten (body);
            keys = soup_form_decode (body->data);

            db = g_object_get_data (G_OBJECT (extension), "formhistory-db");
            formhistory_update_main_hash (keys, db);
            soup_buffer_free (buffer);
            g_hash_table_destroy (keys);
        }
    }
    g_free (method);
}


static void
formhistory_window_object_cleared_cb (GtkWidget*      web_view,
                                      WebKitWebFrame* web_frame,
                                      JSContextRef    js_context,
                                      JSObjectRef     js_window)
{
    webkit_web_view_execute_script (WEBKIT_WEB_VIEW (web_view),
                                    formhistory_build_js ());
}

static void
formhistory_add_tab_cb (MidoriBrowser*   browser,
                        MidoriView*      view,
                        MidoriExtension* extension)
{
    GtkWidget* web_view = gtk_bin_get_child (GTK_BIN (view));
    SoupSession *session = webkit_get_default_session ();
    g_signal_connect (web_view, "window-object-cleared",
            G_CALLBACK (formhistory_window_object_cleared_cb), NULL);
    g_signal_connect (session, "request-queued",
       G_CALLBACK (formhistory_session_request_queued_cb), extension);
}

static void
formhistory_deactivate_cb (MidoriExtension* extension,
                           MidoriBrowser*   browser);

static void
formhistory_add_tab_foreach_cb (MidoriView*     view,
                                MidoriBrowser*  browser,
                                MidoriExtension* extension)
{
    formhistory_add_tab_cb (browser, view, extension);
}

static void
formhistory_app_add_browser_cb (MidoriApp*       app,
                                MidoriBrowser*   browser,
                                MidoriExtension* extension)
{
    midori_browser_foreach (browser,
        (GtkCallback)formhistory_add_tab_foreach_cb, extension);
    g_signal_connect (browser, "add-tab",
        G_CALLBACK (formhistory_add_tab_cb), extension);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (formhistory_deactivate_cb), browser);
}

static void
formhistory_deactivate_tabs (MidoriView*      view,
                             MidoriBrowser*   browser,
                             MidoriExtension* extension)
{
    GtkWidget* web_view = gtk_bin_get_child (GTK_BIN (view));
    SoupSession *session = webkit_get_default_session ();
    g_signal_handlers_disconnect_by_func (
       browser, formhistory_add_tab_cb, extension);
    g_signal_handlers_disconnect_by_func (
       web_view, formhistory_window_object_cleared_cb, NULL);
    g_signal_handlers_disconnect_by_func (
       session, formhistory_session_request_queued_cb, extension);
}

static void
formhistory_deactivate_cb (MidoriExtension* extension,
                       MidoriBrowser*   browser)
{
    MidoriApp* app = midori_extension_get_app (extension);
    #if HAVE_SQLITE
    sqlite3* db;
    #endif

    g_signal_handlers_disconnect_by_func (
        extension, formhistory_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, formhistory_app_add_browser_cb, extension);
    midori_browser_foreach (browser,
        (GtkCallback)formhistory_deactivate_tabs, extension);

    jsforms = "";
    if (global_keys)
        g_hash_table_destroy (global_keys);

    #if HAVE_SQLITE
    if ((db = g_object_get_data (G_OBJECT (extension), "formhistory-db")))
        sqlite3_close (db);
    #endif
}

static void
formhistory_activate_cb (MidoriExtension* extension,
                         MidoriApp*       app)
{
    #if HAVE_SQLITE
    const gchar* config_dir;
    gchar* filename;
    sqlite3* db;
    #endif
    KatzeArray* browsers;
    MidoriBrowser* browser;
    guint i;

    global_keys = g_hash_table_new_full (g_str_hash, g_str_equal,
                               (GDestroyNotify)g_free,
                               (GDestroyNotify)g_free);
    #if HAVE_SQLITE
    config_dir = midori_extension_get_config_dir (extension);
    katze_mkdir_with_parents (config_dir, 0700);
    filename = g_build_filename (config_dir, "forms.db", NULL);
    if (sqlite3_open (filename, &db))
    {
        g_warning (_("Failed to open database: %s\n"), sqlite3_errmsg (db));
        sqlite3_close (db);
    }
    g_free (filename);
    /* FIXME: Load keys from the database */

    g_object_set_data (G_OBJECT (extension), "formhistory-db", db);
    #endif

    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++)))
        formhistory_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (formhistory_app_add_browser_cb), extension);

    g_object_unref (browsers);
}

#if G_ENABLE_DEBUG
/*
<html>
    <head>
        <title>autosuggest testcase</title>
    </head>
    <body>
        <form method=post>
        <p><input type="text" id="txt1" /></p>
        <p><input type="text" name="txt2" /></p>
        <input type=submit>
        </form>
    </body>
</html> */
#endif

MidoriExtension*
extension_init (void)
{
    gboolean should_init = TRUE;
    const gchar* ver;
    gchar* desc;
    MidoriExtension* extension;

    if (formhistory_prepare_js ())
    {
        ver = "0.1";
        desc = g_strdup (_("Stores history of entered form data"));
    }
    else
    {
        desc = g_strdup_printf (_("Not available: %s"),
                                _("Resource files not installed"));
        ver = NULL;
        should_init = FALSE;
    }

    extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Form history filler"),
        "description", desc,
        "version", ver,
        "authors", "Alexander V. Butenko <a.butenka@gmail.com>",
        NULL);
    g_free (desc);

    if (should_init)
        g_signal_connect (extension, "activate",
            G_CALLBACK (formhistory_activate_cb), NULL);

    return extension;
}
