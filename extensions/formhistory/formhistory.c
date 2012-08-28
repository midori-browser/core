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
#define GTK_RESPONSE_IGNORE 99
#include "formhistory-frontend.h"

static void
formhistory_toggle_state_cb (GtkAction*     action,
                             MidoriBrowser* browser);

static void
formhistory_update_database (gpointer     db,
                             const gchar* host,
                             const gchar* key,
                             const gchar* value)
{
    gchar* sqlcmd;
    gchar* errmsg;
    gint success;

    if (!(value && *value))
        return;

    sqlcmd = sqlite3_mprintf ("INSERT INTO forms VALUES"
                              "('%q', '%q', '%q')",
                              host, key, value);
    success = sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg);
    sqlite3_free (sqlcmd);
    if (success != SQLITE_OK)
    {
        g_printerr (_("Failed to add form value: %s\n"), errmsg);
        g_free (errmsg);
        return;
    }
}

static gchar*
formhistory_get_login_data (gpointer     db,
                            const gchar* domain)
{
    static sqlite3_stmt* stmt;
    const char* sqlcmd;
    gint result;
    gchar* value = NULL;

    if (!stmt)
    {
        sqlcmd = "SELECT value FROM forms WHERE domain = ?1 and field = 'MidoriPasswordManager' limit 1";
        sqlite3_prepare_v2 (db, sqlcmd, strlen (sqlcmd) + 1, &stmt, NULL);
    }
    sqlite3_bind_text (stmt, 1, domain, -1, NULL);
    result = sqlite3_step (stmt);
    if (result == SQLITE_ROW)
        value = g_strdup ((gchar*)sqlite3_column_text (stmt, 0));
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    return value;
}

static gboolean
formhistory_check_master_password (GtkWidget*       parent,
                                   FormHistoryPriv* priv)
{
    GtkWidget* dialog;
    GtkWidget* content_area;
    GtkWidget* hbox;
    GtkWidget* image;
    GtkWidget* label;
    GtkWidget* entry;
    const gchar* title;
    static int alive;
    gboolean ret = FALSE;

    /* Password is set */
    if (priv->master_password && *priv->master_password)
        return TRUE;

    /* Other prompt is active */
    if (alive == 1)
        return FALSE;

    /* Prompt was cancelled */
    if (priv->master_password_canceled == 1)
        return FALSE;

    alive = 1;
    title = _("Form history");
    dialog = gtk_dialog_new_with_buttons (title, GTK_WINDOW (parent),
            GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OK, GTK_RESPONSE_OK,
            NULL);
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_DIALOG_AUTHENTICATION);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION,
                                      GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

    label = gtk_label_new (_("Master password required\n"
                             "to open password database"));
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (content_area), hbox);

    entry = gtk_entry_new ();
    g_object_set (entry, "truncate-multiline", TRUE, NULL);
    gtk_entry_set_visibility(GTK_ENTRY (entry),FALSE);
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_container_add (GTK_CONTAINER (content_area), entry);

    gtk_widget_show_all (entry);
    gtk_widget_show_all (hbox);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
        /* FIXME: add password verification */
        katze_assign (priv->master_password,
            g_strdup (gtk_entry_get_text (GTK_ENTRY (entry))));
        ret = TRUE;
    }
    else
        priv->master_password_canceled = 1;

    gtk_widget_destroy (dialog);
    alive = 0;

    return ret;
}

static gchar*
formhistory_encrypt (const gchar* data,
                     const gchar* password)
{
    /* TODO: Implement persistent storage/ keyring support */
    return NULL;
}

static void
formhistory_remember_password_response (GtkWidget*                infobar,
                                        gint                      response_id,
                                        FormhistoryPasswordEntry* entry)
{
    gchar* encrypted_form;

    if (response_id == GTK_RESPONSE_IGNORE)
        goto cleanup;

    if (formhistory_check_master_password (NULL, entry->priv))
    {
        if (response_id != GTK_RESPONSE_ACCEPT)
            katze_assign (entry->form_data, g_strdup ("never"));

        if ((encrypted_form = formhistory_encrypt (entry->form_data,
            entry->priv->master_password)))
        formhistory_update_database (entry->priv->db, entry->domain, "MidoriPasswordManager", encrypted_form);
        g_free (encrypted_form);
    }

cleanup:
    g_free (entry->form_data);
    g_free (entry->domain);
    g_slice_free (FormhistoryPasswordEntry, entry);
    gtk_widget_destroy (infobar);
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
                 "  for (var i = 0; i < inputs.length; i++) {"
                 "    if (inputs[i].getAttribute('autocomplete') == 'off' && "
                 "        inputs[i].type == 'text')"
                 "        continue;"
                 "    if (inputs[i].value && (inputs[i].type == 'text' || inputs[i].type == 'password')) {"
                 "        var ename = inputs[i].getAttribute('name');"
                 "        var eid = inputs[i].getAttribute('id');"
                 "        if (!eid && ename)"
                 "            eid=ename;"
                 "        out += eid+'|,|'+inputs[i].value +'|,|'+inputs[i].type +'|||';"
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

#ifdef FORMHISTORY_USE_GDOM
    formhistory_suggestions_hide_cb (NULL, NULL, priv);
#endif
    if (value && *value)
    {
        gchar** inputs = g_strsplit (value, "|||", 0);
        guint i = 0;
        while (inputs[i] != NULL)
        {
            gchar** parts = g_strsplit (inputs[i], "|,|", 3);
            if (parts && parts[0] && parts[1] && parts[2])
            {
                if (strcmp (parts[2], "password"))
                    formhistory_update_database (priv->db, NULL, parts[0], parts[1]);
                #if WEBKIT_CHECK_VERSION (1, 3, 8)
                else
                {
                    gchar* data;
                    gchar* domain;
                    #if 0
                    FormhistoryPasswordEntry* entry;
                    #endif

                    domain = midori_uri_parse_hostname (webkit_web_frame_get_uri (web_frame), NULL);
                    data = formhistory_get_login_data (priv->db, domain);
                    if (data)
                    {
                        g_free (data);
                        g_free (domain);
                        break;
                    }
                    #if 0
                    entry = g_slice_new (FormhistoryPasswordEntry);
                    /* Domain and form data are freed from infopanel callback*/
                    entry->form_data = g_strdup (value);
                    entry->domain = domain;
                    entry->priv = priv;
                    g_object_set_data (G_OBJECT (web_view), "FormHistoryPasswordEntry", entry);
                    #endif
                }
                #endif
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
    FormhistoryPasswordEntry* entry;
    GtkWidget* view;

    page_uri = webkit_web_frame_get_uri (web_frame);
    if (!page_uri)
        return;

    if (!midori_uri_is_http (page_uri) && !g_str_has_prefix (page_uri, "file"))
        return;

    formhistory_setup_suggestions (web_view, js_context, extension);

    #if WEBKIT_CHECK_VERSION (1, 3, 8)
    entry = g_object_get_data (G_OBJECT (web_view), "FormHistoryPasswordEntry");
    if (entry)
    {
        const gchar* message = _("Remember password on this page?");
        view = midori_browser_get_current_tab (midori_app_get_browser (
                                               midori_extension_get_app (extension)));
        midori_view_add_info_bar (MIDORI_VIEW (view), GTK_MESSAGE_QUESTION, message,
                                  G_CALLBACK (formhistory_remember_password_response), entry,
                                  _("Remember"), GTK_RESPONSE_ACCEPT,
                                  _("Not now"), GTK_RESPONSE_IGNORE,
                                  _("Never for this page"), GTK_RESPONSE_CANCEL, NULL);
        g_object_set_data (G_OBJECT (web_view), "FormHistoryPasswordEntry", NULL);
    }
    #endif
}

#if WEBKIT_CHECK_VERSION (1, 3, 8)
static gchar*
formhistory_decrypt (const gchar* data,
                     const gchar* password)
{
    /* TODO: Implement persistent storage/ keyring support */
    return NULL;
}

static void
formhistory_fill_login_data (JSContextRef js_context,
                             FormHistoryPriv* priv,
                             const gchar* data)
{
    gchar* decrypted_data = NULL;
    guint i = 0;
    GString *script;
    gchar** inputs;

    /* Handle case that user dont want to store password */
    if (!strncmp (data, "never", 5))
        return;

    #if 0
    if (!formhistory_check_master_password (NULL, priv))
        return;
    #endif

    if (!(decrypted_data = formhistory_decrypt (data, priv->master_password)))
        return;

    script = g_string_new ("");
    inputs = g_strsplit (decrypted_data, "|||", 0);
    while (inputs[i] != NULL)
    {
        gchar** parts = g_strsplit (inputs[i], "|,|", 3);
        if (parts && parts[0] && parts[1] && parts[2])
        {
            g_string_append_printf (script, "node = null;"
                                            "node = document.getElementById ('%s');"
                                            "if (!node) { node = document.getElementsByName ('%s')[0]; }"
                                            "if (node && node.type == '%s') { node.value = '%s'; }",
                                            parts[0], parts[0], parts[2], parts[1]);
        }
        g_strfreev (parts);
        i++;
    }
    g_free (decrypted_data);
    g_strfreev (inputs);
    g_free (sokoke_js_script_eval (js_context, script->str, NULL));
    g_string_free (script, TRUE);
}

static void
formhistory_frame_loaded_cb (WebKitWebView*   web_view,
                             WebKitWebFrame*  web_frame,
                             MidoriExtension* extension)
{
    const gchar* page_uri;
    const gchar* count_request;
    FormHistoryPriv* priv;
    JSContextRef js_context;
    gchar* data;
    gchar* domain;
    gchar* count;

    page_uri = webkit_web_frame_get_uri (web_frame);
    if (!page_uri)
        return;

    count_request = "document.querySelectorAll('input[type=password]').length";
    js_context = webkit_web_frame_get_global_context (web_frame);
    count = sokoke_js_script_eval (js_context, count_request, NULL);
    if (count && count[0] == '0')
    {
        g_free (count);
        return;
    }
    g_free (count);

    priv = g_object_get_data (G_OBJECT (extension), "priv");
    domain = midori_uri_parse_hostname (webkit_web_frame_get_uri (web_frame), NULL);
    data = formhistory_get_login_data (priv->db, domain);
    g_free (domain);

    if (!data)
        return;
    formhistory_fill_login_data (js_context, priv, data);
    g_free (data);
}
#endif

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

    #if WEBKIT_CHECK_VERSION (1, 3, 8)
    g_signal_connect (web_view, "onload-event",
        G_CALLBACK (formhistory_frame_loaded_cb), extension);
    #endif
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
    #if WEBKIT_CHECK_VERSION (1, 3, 8)
    g_signal_handlers_disconnect_by_func (
       web_view, formhistory_frame_loaded_cb, extension);
    #endif
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
    action = gtk_action_group_get_action (action_group, "FormHistoryToggleState");
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
    priv->master_password = NULL;
    priv->master_password_canceled = 0;
    formhistory_construct_popup_gui (priv);

    config_dir = midori_extension_get_config_dir (extension);
    if (config_dir != NULL)
        katze_mkdir_with_parents (config_dir, 0700);
    filename = g_build_filename (config_dir, "forms.db", NULL);
    if (sqlite3_open (filename, &db) != SQLITE_OK)
    {
        if (config_dir != NULL)
            g_warning (_("Failed to open database: %s\n"), sqlite3_errmsg (db));
        sqlite3_close (db);
    }
    g_free (filename);
    if ((sqlite3_exec (db, "CREATE TABLE IF NOT EXISTS "
                           "forms (domain text, field text, value text)",
                           NULL, NULL, &errmsg) == SQLITE_OK))
    {
        sqlite3_exec (db,
            /* "PRAGMA synchronous = OFF; PRAGMA temp_store = MEMORY" */
            "PRAGMA count_changes = OFF; PRAGMA journal_mode = TRUNCATE;",
            NULL, NULL, &errmsg);
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
    checkbox = gtk_check_button_new_with_label (_("Only activate form history via hotkey (Ctrl+Shift+F) per tab"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox),
        !midori_extension_get_boolean (extension, "always-load"));
    g_object_set_data (G_OBJECT (dialog), "always-load-checkbox", checkbox);
    gtk_container_add (GTK_CONTAINER (content_area), checkbox);
    /* FIXME: Add pref to disable password manager */

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
    }
    else
        formhistory_add_tab_cb (browser, view, extension);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension;


    extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Form history filler"),
        "description", _("Stores history of entered form data"),
        "version", "2.0" MIDORI_VERSION_SUFFIX,
        "authors", "Alexander V. Butenko <a.butenka@gmail.com>",
        NULL);

    midori_extension_install_boolean (extension, "always-load", TRUE);
    g_signal_connect (extension, "activate",
        G_CALLBACK (formhistory_activate_cb), NULL);
    g_signal_connect (extension, "open-preferences",
        G_CALLBACK (formhistory_preferences_cb), NULL);

    return extension;
}
