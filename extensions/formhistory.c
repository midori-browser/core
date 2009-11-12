/*
 Copyright (C) 2009 Alexander Butenko <a.butenka@gmail.com>
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
*/

#define MAXCHARS 60
#define MINCHARS 2

#include <midori/midori.h>

#include "config.h"
#include "midori/sokoke.h"

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
formhistory_fixup_value (char* value)
{
    guint i = 0;
    g_strchomp (value);
    while (value[i])
    {
        if (value[i] == '\n')
            value[i] = ' ';
        else if (value[i] == '"')
            value[i] = '\'';
        i++;
    }
    return value;
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
    gchar* sqlcmd;
    gchar* errmsg;
    gint success;

    sqlcmd = sqlite3_mprintf ("INSERT INTO forms VALUES"
                              "('%q', '%q', '%q')",
                              NULL, key, value);
    success = sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg);
    sqlite3_free (sqlcmd);
    if (success != SQLITE_OK)
    {
        g_printerr (_("Failed to add form value: %s\n"), errmsg);
        g_free (errmsg);
        return;
    }
    #endif
}

static gboolean
formhistory_update_main_hash (gchar* key,
                              gchar* value)
{
    guint length;
    gchar* tmp;

    if (!(value && *value))
        return FALSE;
    length = strlen (value);
    if (length > MAXCHARS || length < MINCHARS)
        return FALSE;

    formhistory_fixup_value (value);
    if ((tmp = g_hash_table_lookup (global_keys, (gpointer)key)))
    {
        gchar* rvalue = g_strdup_printf ("\"%s\"",value);
        if (!g_regex_match_simple (rvalue, tmp,
                                   G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
        {
            gchar* new_value = g_strdup_printf ("%s%s,", tmp, rvalue);
            g_hash_table_insert (global_keys, g_strdup (key), new_value);
            g_free (rvalue);
        }
        else
        {
            g_free (rvalue);
            return FALSE;
        }
    }
    else
    {
        gchar* new_value = g_strdup_printf ("\"%s\",",value);
        g_hash_table_replace (global_keys, g_strdup (key), new_value);
    }
    return TRUE;
}

#if WEBKIT_CHECK_VERSION (1, 1, 4)
static gboolean
formhistory_navigation_decision_cb (WebKitWebView*             web_view,
                                    WebKitWebFrame*            web_frame,
                                    WebKitNetworkRequest*      request,
                                    WebKitWebNavigationAction* action,
                                    WebKitWebPolicyDecision*   decision,
                                    MidoriExtension*           extension)
{
    JSContextRef js_context = webkit_web_frame_get_global_context (web_frame);
    /* The script returns form data in the form "field_name|,|value|,|field_type".
       We are handling only input fields with 'text' or 'password' type.
       The field separator is "|||" */
    const gchar* script = "function dumpForm (inputs) {"
                 "  var out = '';"
                 "  for (i=0;i<inputs.length;i++) {"
                 "    if (inputs[i].value && (inputs[i].type == 'text' || inputs[i].type == 'password')) {"
                 "        var ename = inputs[i].getAttribute('name');"
                 "        var eid = inputs[i].getAttribute('id');"
                 "        if (!ename && eid)"
                 "            ename=eid;"
                 "        out += ename+'|,|'+inputs[i].value +'|,|'+inputs[i].type +'|||';"
                 "    }"
                 "  }"
                 "  return out;"
                 "}"
                 "dumpForm (document.getElementsByTagName('input'))";

    if (webkit_web_navigation_action_get_reason (action) == WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED
     || webkit_web_navigation_action_get_reason (action) == WEBKIT_WEB_NAVIGATION_REASON_FORM_RESUBMITTED)
    {
        gchar* value = sokoke_js_script_eval (js_context, script, NULL);
        if (value)
        {
            gpointer db = g_object_get_data (G_OBJECT (extension), "formhistory-db");
            gchar** inputs = g_strsplit (value, "|||", 0);
            guint i = 0;
            while (inputs[i] != NULL)
            {
                gchar** parts = g_strsplit (inputs[i], "|,|", 3);
                if (parts && parts[0] && parts[1] && parts[2])
                {
                    /* FIXME: We need to handle passwords */
                    if (strcmp (parts[2], "password"))
                    {
                        if (formhistory_update_main_hash (parts[0], parts[1]))
                            formhistory_update_database (db, parts[0], parts[1]);
                    }
                }
                g_strfreev (parts);
                i++;
            }
            g_strfreev (inputs);
            g_free (value);
        }
    }
    return FALSE;
}
#else
static void
formhistory_feed_keys (GHashTable* keys,
                       gpointer    db)
{
    GHashTableIter iter;
    gchar* key;
    gchar* value;

    g_hash_table_iter_init (&iter, keys);
    while (g_hash_table_iter_next (&iter, (gpointer)&key, (gpointer)&value))
    {
        if (formhistory_update_main_hash (key, value))
            formhistory_update_database (db, key, value);
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
            formhistory_feed_keys (keys, db);
            soup_buffer_free (buffer);
            g_hash_table_destroy (keys);
        }
    }
    g_free (method);
}
#endif

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
    g_signal_connect (web_view, "window-object-cleared",
            G_CALLBACK (formhistory_window_object_cleared_cb), NULL);
    #if WEBKIT_CHECK_VERSION (1, 1, 4)
    g_signal_connect (web_view, "navigation-policy-decision-requested",
        G_CALLBACK (formhistory_navigation_decision_cb), extension);
    #else
    g_signal_connect (webkit_get_default_session (), "request-queued",
        G_CALLBACK (formhistory_session_request_queued_cb), extension);
    #endif
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
    g_signal_handlers_disconnect_by_func (
       browser, formhistory_add_tab_cb, extension);
    g_signal_handlers_disconnect_by_func (
       web_view, formhistory_window_object_cleared_cb, NULL);
    #if WEBKIT_CHECK_VERSION (1, 1, 4)
    g_signal_handlers_disconnect_by_func (
       web_view, formhistory_navigation_decision_cb, extension);
    #else
    g_signal_handlers_disconnect_by_func (
       webkit_get_default_session (), formhistory_session_request_queued_cb, extension);
    #endif
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

#if HAVE_SQLITE
static int
formhistory_add_field (gpointer  data,
                       int       argc,
                       char**    argv,
                       char**    colname)
{
    gint i;
    gint ncols = 3;

    /* Test whether have the right number of columns */
    g_return_val_if_fail (argc % ncols == 0, 1);

    for (i = 0; i < (argc - ncols) + 1; i++)
    {
        if (argv[i])
        {
            if (colname[i] && !g_ascii_strcasecmp (colname[i], "domain")
             && colname[i + 1] && !g_ascii_strcasecmp (colname[i + 1], "field")
             && colname[i + 2] && !g_ascii_strcasecmp (colname[i + 2], "value"))
            {
                gchar* key = argv[i + 1];
                formhistory_update_main_hash (g_strdup (key), g_strdup (argv[i + 2]));
            }
        }
    }
    return 0;
}
#endif

static void
formhistory_activate_cb (MidoriExtension* extension,
                         MidoriApp*       app)
{
    #if HAVE_SQLITE
    const gchar* config_dir;
    gchar* filename;
    sqlite3* db;
    char* errmsg = NULL, *errmsg2 = NULL;
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
    if ((sqlite3_exec (db, "CREATE TABLE IF NOT EXISTS "
                           "forms (domain text, field text, value text)",
                           NULL, NULL, &errmsg) == SQLITE_OK)
        && (sqlite3_exec (db, "SELECT domain, field, value FROM forms ",
                          formhistory_add_field,
                          NULL, &errmsg2) == SQLITE_OK))
        g_object_set_data (G_OBJECT (extension), "formhistory-db", db);
    else
    {
        if (errmsg)
        {
            g_critical (_("Failed to execute database statement: %s\n"), errmsg);
            sqlite3_free (errmsg);
            if (errmsg2)
            {
                g_critical (_("Failed to execute database statement: %s\n"), errmsg2);
                sqlite3_free (errmsg2);
            }
        }
        sqlite3_close (db);
    }
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
