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
#include <glib/gstdio.h>

#include "config.h"
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

static GHashTable* global_keys;
static gchar* jsforms;


static void
formhistory_toggle_state_cb (GtkAction*     action,
                             MidoriBrowser* browser);

static gboolean
formhistory_prepare_js ()
{
   gchar* autosuggest;
   gchar* style;
   guint i;
   gchar* file;

   file = sokoke_find_data_filename ("autosuggestcontrol.js", TRUE);
   if (!g_file_get_contents (file, &autosuggest, NULL, NULL))
   {
       g_free (file);
       return FALSE;
   }
   g_strchomp (autosuggest);

   katze_assign (file, sokoke_find_data_filename ("autosuggestcontrol.css", TRUE));
   if (!g_file_get_contents (file, &style, NULL, NULL))
   {
       g_free (file);
       return FALSE;
   }
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
        "window.addEventListener ('DOMContentLoaded',"
        "function () {"
        "   if (document.getElementById('formhistory'))"
        "       return;"
        "   if (!initSuggestions ())"
        "       return;"
        "   var mystyle = document.createElement('style');"
        "   mystyle.setAttribute('type', 'text/css');"
        "   mystyle.setAttribute('id', 'formhistory');"
        "   mystyle.appendChild(document.createTextNode('%s'));"
        "   var head = document.getElementsByTagName('head')[0];"
        "   if (head) head.appendChild(mystyle);"
        "}, true);",
        autosuggest,
        style);
   g_strstrip (jsforms);
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
    GString* suggestions;
    GHashTableIter iter;
    gpointer key, value;

    suggestions = g_string_new (
        "function FormSuggestions(eid) { "
        "arr = new Array();");
    g_hash_table_iter_init (&iter, global_keys);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        g_string_append_printf (suggestions, " arr[\"%s\"] = [%s]; ",
                                (gchar*)key, (gchar*)value);
    }
    g_string_append (suggestions, "this.suggestions = arr[eid]; }");
    g_string_append (suggestions, jsforms);
    return g_string_free (suggestions, FALSE);
}

static void
formhistory_update_database (gpointer     db,
                             const gchar* key,
                             const gchar* value)
{
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

    formhistory_fixup_value (key);
    formhistory_fixup_value (value);
    if ((tmp = g_hash_table_lookup (global_keys, (gpointer)key)))
    {
        gchar* rvalue = g_strdup_printf ("\"%s\"",value);
        gchar* patt = g_regex_escape_string (rvalue, -1);
        if (!g_regex_match_simple (patt, tmp,
                                   G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
        {
            gchar* new_value = g_strdup_printf ("%s%s,", tmp, rvalue);
            g_hash_table_insert (global_keys, g_strdup (key), new_value);
            g_free (rvalue);
            g_free (patt);
        }
        else
        {
            g_free (rvalue);
            g_free (patt);
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

static gboolean
formhistory_navigation_decision_cb (WebKitWebView*             web_view,
                                    WebKitWebFrame*            web_frame,
                                    WebKitNetworkRequest*      request,
                                    WebKitWebNavigationAction* action,
                                    WebKitWebPolicyDecision*   decision,
                                    MidoriExtension*           extension)
{
    /* The script returns form data in the form "field_name|,|value|,|field_type".
       We are handling only input fields with 'text' or 'password' type.
       The field separator is "|||" */
    const gchar* script = "function dumpForm (inputs) {"
                 "  var out = '';"
                 "  for (i=0;i<inputs.length;i++) {"
                 "    if (inputs[i].getAttribute('autocomplete') == 'off')"
                 "        continue;"
                 "    if (inputs[i].value && (inputs[i].type == 'text' || inputs[i].type == 'password')) {"
                 "        var ename = inputs[i].getAttribute('name');"
                 "        var eid = inputs[i].getAttribute('id');"
                 "        if (!ename && eid)"
                 "            ename=eid;"
                 "        if (inputs[i].getAttribute('autocomplete') != 'off')"
                 "            out += ename+'|,|'+inputs[i].value +'|,|'+inputs[i].type +'|||';"
                 "    }"
                 "  }"
                 "  return out;"
                 "}"
                 "dumpForm (document.getElementsByTagName('input'))";

    if (webkit_web_navigation_action_get_reason (action) == WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED)
    {
        JSContextRef js_context = webkit_web_frame_get_global_context (web_frame);
        gchar* value = sokoke_js_script_eval (js_context, script, NULL);
        if (value && *value)
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

static void
formhistory_window_object_cleared_cb (WebKitWebView*  web_view,
                                      WebKitWebFrame* web_frame,
                                      JSContextRef    js_context,
                                      JSObjectRef     js_window)
{
    gchar* script;
    const gchar* page_uri;

    page_uri = webkit_web_frame_get_uri (web_frame);
    if (!midori_uri_is_http (page_uri))
        return;

    script = formhistory_build_js ();
    sokoke_js_script_eval (js_context, script, NULL);
    g_free (script);
}

static void
formhistory_add_tab_cb (MidoriBrowser*   browser,
                        MidoriView*      view,
                        MidoriExtension* extension)
{
    GtkWidget* web_view = midori_view_get_web_view (view);
    g_signal_connect (web_view, "window-object-cleared",
            G_CALLBACK (formhistory_window_object_cleared_cb), NULL);
    g_signal_connect (web_view, "navigation-policy-decision-requested",
        G_CALLBACK (formhistory_navigation_decision_cb), extension);
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
    GtkAccelGroup* acg = gtk_accel_group_new ();
    GtkActionGroup* action_group = midori_browser_get_action_group (browser);
    GtkAction* action = gtk_action_new ("FormHistoryToggleState",
        _("Toggle form history state"),
        _("Activate or deactivate form history for the current tab."), NULL);
    gtk_window_add_accel_group (GTK_WINDOW (browser), acg);

    g_object_set_data (G_OBJECT (browser), "FormHistoryExtension", extension);

    g_signal_connect (action, "activate",
        G_CALLBACK (formhistory_toggle_state_cb), browser);

    gtk_action_group_add_action_with_accel (action_group, action, "<Ctrl><Shift>F");
    gtk_action_set_accel_group (action, acg);
    gtk_action_connect_accelerator (action);

    if (midori_extension_get_boolean (extension, "always-load"))
    {
        midori_browser_foreach (browser,
            (GtkCallback)formhistory_add_tab_foreach_cb, extension);
        g_signal_connect (browser, "add-tab",
            G_CALLBACK (formhistory_add_tab_cb), extension);
    }
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (formhistory_deactivate_cb), browser);
}

static void
formhistory_deactivate_tabs (MidoriView*      view,
                             MidoriBrowser*   browser,
                             MidoriExtension* extension)
{
    GtkWidget* web_view = midori_view_get_web_view (view);
    g_signal_handlers_disconnect_by_func (
       web_view, formhistory_window_object_cleared_cb, NULL);
    g_signal_handlers_disconnect_by_func (
       web_view, formhistory_navigation_decision_cb, extension);
}

static void
formhistory_deactivate_cb (MidoriExtension* extension,
                           MidoriBrowser*   browser)
{
    MidoriApp* app = midori_extension_get_app (extension);
    sqlite3* db;

    GtkActionGroup* action_group = midori_browser_get_action_group (browser);
    GtkAction* action;

    g_signal_handlers_disconnect_by_func (
       browser, formhistory_add_tab_cb, extension);
    g_signal_handlers_disconnect_by_func (
        extension, formhistory_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, formhistory_app_add_browser_cb, extension);
    midori_browser_foreach (browser,
        (GtkCallback)formhistory_deactivate_tabs, extension);

    g_object_set_data (G_OBJECT (browser), "FormHistoryExtension", NULL);
    action = gtk_action_group_get_action ( action_group, "FormHistoryToggleState");
    if (action != NULL)
    {
        gtk_action_group_remove_action (action_group, action);
        g_object_unref (action);
    }

    katze_assign (jsforms, NULL);
    if (global_keys)
        g_hash_table_destroy (global_keys);

    if ((db = g_object_get_data (G_OBJECT (extension), "formhistory-db")))
        sqlite3_close (db);
}

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

static void
formhistory_activate_cb (MidoriExtension* extension,
                         MidoriApp*       app)
{
    const gchar* config_dir;
    gchar* filename;
    sqlite3* db;
    char* errmsg = NULL, *errmsg2 = NULL;
    KatzeArray* browsers;
    MidoriBrowser* browser;

    global_keys = g_hash_table_new_full (g_str_hash, g_str_equal,
                               (GDestroyNotify)g_free,
                               (GDestroyNotify)g_free);
    if(!jsforms)
        formhistory_prepare_js ();
    config_dir = midori_extension_get_config_dir (extension);
    katze_mkdir_with_parents (config_dir, 0700);
    filename = g_build_filename (config_dir, "forms.db", NULL);
    if (sqlite3_open (filename, &db) != SQLITE_OK)
    {
        /* If the folder is /, this is a test run, thus no error */
        if (!g_str_equal (midori_extension_get_config_dir (extension), "/"))
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

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        formhistory_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (formhistory_app_add_browser_cb), extension);

    g_object_unref (browsers);
}

static void
formhistory_preferences_response_cb (GtkWidget*       dialog,
                                     gint             response_id,
                                     MidoriExtension* extension)
{
    GtkWidget* checkbox;
    gboolean old_state;
    gboolean new_state;
    MidoriApp* app;
    KatzeArray* browsers;
    MidoriBrowser* browser;

    if (response_id == GTK_RESPONSE_APPLY)
    {
        checkbox = g_object_get_data (G_OBJECT (dialog), "always-load-checkbox");
        new_state = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));
        old_state = midori_extension_get_boolean (extension, "always-load");

        if (old_state != new_state)
        {
            midori_extension_set_boolean (extension, "always-load", new_state);

            app = midori_extension_get_app (extension);
            browsers = katze_object_get_object (app, "browsers");
            KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
            {
                midori_browser_foreach (browser,
                    (GtkCallback)formhistory_deactivate_tabs, extension);
                g_signal_handlers_disconnect_by_func (
                    browser, formhistory_add_tab_cb, extension);

                if (new_state)
                {
                    midori_browser_foreach (browser,
                        (GtkCallback)formhistory_add_tab_foreach_cb, extension);
                    g_signal_connect (browser, "add-tab",
                        G_CALLBACK (formhistory_add_tab_cb), extension);
                }
            }
        }
    }
    gtk_widget_destroy (dialog);
}

static void
formhistory_preferences_cb (MidoriExtension* extension)
{
    GtkWidget* dialog;
    GtkWidget* content_area;
    GtkWidget* checkbox;

    dialog = gtk_dialog_new ();

    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

    gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_APPLY, GTK_RESPONSE_APPLY);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    checkbox = gtk_check_button_new_with_label (_("only activate form history via hotkey (Ctrl+Shift+F) per tab"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox),
        !midori_extension_get_boolean (extension, "always-load"));
    g_object_set_data (G_OBJECT (dialog), "always-load-checkbox", checkbox);
    gtk_container_add (GTK_CONTAINER (content_area), checkbox);

    g_signal_connect (dialog,
            "response",
            G_CALLBACK (formhistory_preferences_response_cb),
            extension);
    gtk_widget_show_all (dialog);
}

static void
formhistory_toggle_state_cb (GtkAction*     action,
                             MidoriBrowser* browser)
{
    MidoriView* view = MIDORI_VIEW (midori_browser_get_current_tab (browser));
    MidoriExtension* extension = g_object_get_data (G_OBJECT (browser), "FormHistoryExtension");
    GtkWidget* web_view = midori_view_get_web_view (view);

    if (g_signal_handler_find (web_view, G_SIGNAL_MATCH_FUNC,
        g_signal_lookup ("window-object-cleared", MIDORI_TYPE_VIEW), 0, NULL,
        formhistory_window_object_cleared_cb, extension))
    {
        formhistory_deactivate_tabs (view, browser, extension);
    } else {
        formhistory_add_tab_cb (browser, view, extension);
    }
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
        ver = "1.0" MIDORI_VERSION_SUFFIX;
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
    {
        midori_extension_install_boolean (extension, "always-load", TRUE);
        g_signal_connect (extension, "activate",
            G_CALLBACK (formhistory_activate_cb), NULL);
        g_signal_connect (extension, "open-preferences",
            G_CALLBACK (formhistory_preferences_cb), NULL);
    }

    return extension;
}
