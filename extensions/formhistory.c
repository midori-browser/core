/*
 Copyright (C) 2009 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

*/

#include <midori/midori.h>

#include <midori/sokoke.h>
#include "config.h"

#include <glib/gstdio.h>
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

static gchar* jsforms;

static void
formhistory_prepare_js ()
{
   gchar* autosuggest;
   gchar* style;
   guint i;

   /* FIXME: Don't hardcode paths */
   g_file_get_contents ("/usr/local/share/midori/autosuggestcontrol.js", &autosuggest, NULL, NULL);
   g_strchomp (autosuggest);

   g_file_get_contents ("/usr/local/share/midori/autosuggestcontrol.css", &style, NULL, NULL);
   g_strchomp (style);
   i = 0;
   while (style[i])
   {
       if (style[i] == '\n')
           style[i] = ' ';
       i++;
   }
   g_print ("%s\n", style);

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
   g_free (style);
   g_free (autosuggest);
}

static gchar*
formhistory_build_js ()
{
    const gchar* suggestions = "arr[\"txt1\"] = [\"Alabama\", \"Alaska\", \"Arizona\", \"Arkansas\"];"
                               "arr[\"txt2\"] = [\"Alabama\", \"Alaska\", \"Arizona\", \"Arkansas\"];";
    gchar* script = g_strdup_printf ("function FormSuggestions(eid) { "
                                     "arr = new Array();"
                                     "%s"
                                     "this.suggestions = arr[eid]; }"
                                     "%s",
                                     suggestions,
                                     jsforms);
   return script;
}

static void
formhistory_session_request_queued_cb (SoupSession* session,
                                       SoupMessage* msg)
{
    gchar* method = katze_object_get_string (msg, "method");
    if (method[0] == 'P' && method[1] == 'O' && method[2] == 'S')
    {
        SoupMessageHeaders* hdrs = msg->request_headers;
        SoupMessageHeadersIter iter;
        const gchar* name, *value;
        SoupMessageBody* body = msg->request_body;

        soup_message_headers_iter_init (&iter, hdrs);
        while (soup_message_headers_iter_next (&iter, &name, &value))
        {
            g_warning ("%s=%s\n", name, value);
        }
        soup_buffer_free (soup_message_body_flatten (body));
        g_warning ("BODY: %s\n", body->data);
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
    /* FIXME: Deactivate request-queued on unload */
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
    g_signal_handlers_disconnect_by_func (
       browser, formhistory_add_tab_cb, 0);
    g_signal_handlers_disconnect_by_func (
       web_view, formhistory_window_object_cleared_cb, 0);
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
}

static void
formhistory_activate_cb (MidoriExtension* extension,
                         MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;
    guint i;

    formhistory_prepare_js ();
    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++)))
        formhistory_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (formhistory_app_add_browser_cb), extension);

    g_object_unref (browsers);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Form history filler"),
        "description", _("Stores history of entered form data"),
        "version", "0.1",
        "authors", "Alexander V. Butenko <a.butenka@gmail.com>",
        NULL);
    g_signal_connect (extension, "activate",
        G_CALLBACK (formhistory_activate_cb), NULL);

    return extension;
}
