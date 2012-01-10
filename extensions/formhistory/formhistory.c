/*
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>
 Copyright (C) 2009-2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
*/
#define MAXCHARS 60
#define MINCHARS 2
#include "formhistory-frontend.h"

static void
formhistory_toggle_state_cb (GtkAction*     action,
                             MidoriBrowser* browser);

static void
formhistory_update_database (gpointer     db,
                             const gchar* key,
                             const gchar* value)
{
    gchar* sqlcmd;
    gchar* errmsg;
    gint success;
    guint length;

    if (!(value && *value))
        return;
    length = strlen (value);
    if (length > MAXCHARS || length < MINCHARS)
        return;

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
formhistory_navigation_decision_cb (WebKitWebView*             web_view,
                                    WebKitWebFrame*            web_frame,
                                    WebKitNetworkRequest*      request,
                                    WebKitWebNavigationAction* action,
                                    WebKitWebPolicyDecision*   decision,
                                    MidoriExtension*           extension)
{
    FormHistoryPriv* priv;
    JSContextRef js_context;
    gchar* value;

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
                 "        out += ename+'|,|'+inputs[i].value +'|,|'+inputs[i].type +'|||';"
                 "    }"
                 "  }"
                 "  return out;"
                 "}"
                 "dumpForm (document.getElementsByTagName('input'))";

    if (webkit_web_navigation_action_get_reason (action) != WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED)
        return FALSE;

    priv = g_object_get_data (G_OBJECT (extension), "priv");
    js_context = webkit_web_frame_get_global_context (web_frame);
    value = sokoke_js_script_eval (js_context, script, NULL);

    formhistory_suggestions_hide_cb (NULL, NULL, priv);
    if (value && *value)
    {
        gchar** inputs = g_strsplit (value, "|||", 0);
        guint i = 0;
        while (inputs[i] != NULL)
        {
            gchar** parts = g_strsplit (inputs[i], "|,|", 3);
            if (parts && parts[0] && parts[1] && parts[2])
            {
                /* FIXME: We need to handle passwords */
                if (strcmp (parts[2], "password"))
                    formhistory_update_database (priv->db, parts[0], parts[1]);
            }
            g_strfreev (parts);
            i++;
        }
        g_strfreev (inputs);
        g_free (value);
    }
    return FALSE;
}

static void
formhistory_window_object_cleared_cb (WebKitWebView*   web_view,
                                      WebKitWebFrame*  web_frame,
                                      JSContextRef     js_context,
                                      JSObjectRef      js_window,
                                      MidoriExtension* extension)
{
    const gchar* page_uri;

    page_uri = webkit_web_frame_get_uri (web_frame);
    if (!page_uri)
        return;

    if (!midori_uri_is_http (page_uri) && !g_str_has_prefix (page_uri, "file"))
        return;

    formhistory_setup_suggestions (web_view, js_context, extension);
}

static void
formhistory_deactivate_cb (MidoriExtension* extension,
                           MidoriBrowser*   browser);

static void
formhistory_add_tab_cb (MidoriBrowser*   browser,
                        MidoriView*      view,
                        MidoriExtension* extension)
{
    GtkWidget* web_view = midori_view_get_web_view (view);

    g_signal_connect (web_view, "window-object-cleared",
        G_CALLBACK (formhistory_window_object_cleared_cb), extension);
    g_signal_connect (web_view, "navigation-policy-decision-requested",
        G_CALLBACK (formhistory_navigation_decision_cb), extension);
}

static void
formhistory_add_tab_foreach_cb (MidoriView*      view,
                                MidoriExtension* extension)
{
    formhistory_add_tab_cb (NULL, view, extension);
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
formhistory_deactivate_tab (MidoriView*      view,
                            MidoriExtension* extension)
{
    GtkWidget* web_view = midori_view_get_web_view (view);

    g_signal_handlers_disconnect_by_func (
       web_view, formhistory_window_object_cleared_cb, extension);
    g_signal_handlers_disconnect_by_func (
       web_view, formhistory_navigation_decision_cb, extension);
}

static void
formhistory_deactivate_cb (MidoriExtension* extension,
                           MidoriBrowser*   browser)
{
    MidoriApp* app = midori_extension_get_app (extension);
    FormHistoryPriv* priv = g_object_get_data (G_OBJECT (extension), "priv");

    GtkActionGroup* action_group = midori_browser_get_action_group (browser);
    GtkAction* action;

    g_signal_handlers_disconnect_by_func (
       browser, formhistory_add_tab_cb, extension);
    g_signal_handlers_disconnect_by_func (
        extension, formhistory_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, formhistory_app_add_browser_cb, extension);
    midori_browser_foreach (browser,
        (GtkCallback)formhistory_deactivate_tab, extension);

    g_object_set_data (G_OBJECT (browser), "FormHistoryExtension", NULL);
    action = gtk_action_group_get_action ( action_group, "FormHistoryToggleState");
    if (action != NULL)
    {
        gtk_action_group_remove_action (action_group, action);
        g_object_unref (action);
    }

    formhistory_private_destroy (priv);
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
    FormHistoryPriv* priv;

    priv = formhistory_private_new ();
    formhistory_construct_popup_gui (priv);

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
                           NULL, NULL, &errmsg) == SQLITE_OK))
    {
        priv->db = db;
    }
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

    g_object_set_data (G_OBJECT (extension), "priv", priv);
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
                    (GtkCallback)formhistory_deactivate_tab, extension);
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
        formhistory_deactivate_tab (view, extension);
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
    const gchar* ver;
    gchar* desc;
    MidoriExtension* extension;

    ver = "2.0" MIDORI_VERSION_SUFFIX;
    desc = g_strdup (_("Stores history of entered form data"));

    extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Form history filler"),
        "description", desc,
        "version", ver,
        "authors", "Alexander V. Butenko <a.butenka@gmail.com>",
        NULL);

    g_free (desc);

    midori_extension_install_boolean (extension, "always-load", TRUE);
    g_signal_connect (extension, "activate",
        G_CALLBACK (formhistory_activate_cb), NULL);
    g_signal_connect (extension, "open-preferences",
        G_CALLBACK (formhistory_preferences_cb), NULL);

    return extension;
}
