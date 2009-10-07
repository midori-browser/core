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

static GHashTable* global_keys;
static gchar* jsforms;

static gboolean
formhistory_prepare_js ()
{
   gchar* autosuggest;
   gchar* style;
   guint i;
   gchar* file;

   gchar* data_path = g_build_filename (MDATADIR, PACKAGE_NAME, NULL);
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
formhistory_update_main_hash (GHashTable* keys)
{
    GHashTableIter iter;
    gchar* tmp = "";
    gchar* new_value = "";
    gchar* key = "";
    gchar* value = "";
    g_hash_table_iter_init (&iter, keys);
    while (g_hash_table_iter_next (&iter, (gpointer)&key, (gpointer)&value))
    {
        if (value && *value && (strlen (value) > MAXCHARS || strlen (value) < MINCHARS))
            continue;

        tmp = g_hash_table_lookup (global_keys, (gpointer)key);
        if (tmp)
        {
            gchar* rvalue = g_strdup_printf ("\"%s\"",value);
            if (!g_regex_match_simple (rvalue, tmp,
                                       G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
            {
                new_value = g_strdup_printf ("%s%s,", tmp, rvalue);
                g_hash_table_replace (global_keys, key, new_value);
            }
            g_free (rvalue);
        }
        else
        {
            new_value = g_strdup_printf ("\"%s\",",value);
            g_hash_table_insert (global_keys, key, new_value);
        }
    }
}

static void
formhistory_session_request_queued_cb (SoupSession* session,
                                       SoupMessage* msg)
{
    gchar* method = katze_object_get_string (msg, "method");
    if (method[0] == 'P' && method[1] == 'O' && method[2] == 'S')
    {
        SoupMessageHeaders* hdrs = msg->request_headers;
        /* FIXME: Need a permanent storage implementation */
        const char* referer = soup_message_headers_get_one (hdrs, "Referer");
        SoupMessageBody* body = msg->request_body;
        soup_buffer_free (soup_message_body_flatten (body));
        GHashTable* keys = soup_form_decode (body->data);
        formhistory_update_main_hash (keys);
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
formhistory_add_tab_cb (MidoriBrowser* browser,
                        MidoriView*    view)
{
    GtkWidget* web_view = gtk_bin_get_child (GTK_BIN (view));
    SoupSession *session = webkit_get_default_session ();
    g_signal_connect (web_view, "window-object-cleared",
            G_CALLBACK (formhistory_window_object_cleared_cb), 0);
    g_signal_connect (session, "request-queued",
       G_CALLBACK (formhistory_session_request_queued_cb), 0);
}

static void
formhistory_deactivate_cb (MidoriExtension* extension,
                           MidoriBrowser*   browser);

static void
formhistory_add_tab_foreach_cb (MidoriView*    view,
                                MidoriBrowser* browser)
{
    formhistory_add_tab_cb (browser, view);
}

static void
formhistory_app_add_browser_cb (MidoriApp*       app,
                                MidoriBrowser*   browser,
                                MidoriExtension* extension)
{
    midori_browser_foreach (browser,
        (GtkCallback)formhistory_add_tab_foreach_cb, browser);
    g_signal_connect (browser, "add-tab", G_CALLBACK (formhistory_add_tab_cb), 0);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (formhistory_deactivate_cb), browser);
}

static void
formhistory_deactivate_tabs (MidoriView*    view,
                             MidoriBrowser* browser)
{
    GtkWidget* web_view = gtk_bin_get_child (GTK_BIN (view));
    SoupSession *session = webkit_get_default_session ();
    g_signal_handlers_disconnect_by_func (
       browser, formhistory_add_tab_cb, 0);
    g_signal_handlers_disconnect_by_func (
       web_view, formhistory_window_object_cleared_cb, 0);
    g_signal_handlers_disconnect_by_func (
       session, formhistory_session_request_queued_cb, 0);
}

static void
formhistory_deactivate_cb (MidoriExtension* extension,
                       MidoriBrowser*   browser)
{
    MidoriApp* app = midori_extension_get_app (extension);

    g_signal_handlers_disconnect_by_func (
        extension, formhistory_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, formhistory_app_add_browser_cb, extension);
    midori_browser_foreach (browser, (GtkCallback)formhistory_deactivate_tabs, browser);

    jsforms = "";
    if (global_keys)
        g_hash_table_destroy (global_keys);
}

static void
formhistory_activate_cb (MidoriExtension* extension,
                         MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;
    guint i;

    global_keys = g_hash_table_new_full (g_str_hash, g_str_equal,
                               (GDestroyNotify)g_free,
                               (GDestroyNotify)g_free);
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
    gchar* ver;
    gchar* desc;
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
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
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
