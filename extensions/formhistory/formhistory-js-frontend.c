/*
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>
 Copyright (C) 2009-2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
*/
#include "formhistory-frontend.h"
#ifdef FORMHISTORY_USE_JS

FormHistoryPriv*
formhistory_private_new ()
{
    FormHistoryPriv* priv;

    priv = g_slice_new (FormHistoryPriv);
    return priv;
}

gboolean
formhistory_construct_popup_gui (FormHistoryPriv* priv)
{
    gchar* autosuggest;
    gchar* style;
    guint i;
    gchar* file;

    file = midori_app_find_res_filename ("autosuggestcontrol.js");
    if (!g_file_get_contents (file, &autosuggest, NULL, NULL))
    {
        g_free (file);
        return FALSE;
    }
    g_strchomp (autosuggest);

    katze_assign (file, midori_app_find_res_filename ("autosuggestcontrol.css"));
    if (!g_file_get_contents (file, &style, NULL, NULL))
    {
        g_free (file);
        return FALSE;
    }
    g_strchomp (style);
    g_free (file);

    i = 0;
    while (style[i])
    {
        if (style[i] == '\n')
            style[i] = ' ';
        i++;
    }

    priv->jsforms = g_strdup_printf (
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
    g_strstrip (priv->jsforms);
    g_free (style);
    g_free (autosuggest);
    return TRUE;
}

void
formhistory_setup_suggestions (WebKitWebView*   web_view,
                               JSContextRef     js_context,
                               MidoriExtension* extension)
{
    GString* suggestions;
    FormHistoryPriv* priv;
    static sqlite3_stmt* stmt;
    const char* sqlcmd;
    const unsigned char* key;
    const unsigned char* value;

    gint result, pos;

    priv = g_object_get_data (G_OBJECT (extension), "priv");
    if (!priv->db)
        return;

    if (!stmt)
    {
        sqlcmd = "SELECT DISTINCT group_concat(value,'\",\"'), field FROM forms \
                         GROUP BY field ORDER BY field";
        sqlite3_prepare_v2 (priv->db, sqlcmd, strlen (sqlcmd) + 1, &stmt, NULL);
    }
    result = sqlite3_step (stmt);
    if (result != SQLITE_ROW)
    {
        if (result == SQLITE_ERROR)
            g_print (_("Failed to select suggestions\n"));
        sqlite3_reset (stmt);
        return;
    }
    suggestions = g_string_new (
        "function FormSuggestions(eid) { "
        "arr = new Array();");

    while (result == SQLITE_ROW)
    {
        pos++;
        value = sqlite3_column_text (stmt, 0);
        key = sqlite3_column_text (stmt, 1);
        if (value)
        {
            g_string_append_printf (suggestions, " arr[\"%s\"] = [\"%s\"]; ",
                                (gchar*)key, (gchar*)value);
        }
        result = sqlite3_step (stmt);
    }
    g_string_append (suggestions, "this.suggestions = arr[eid]; }");
    g_string_append (suggestions, priv->jsforms);
    sokoke_js_script_eval (js_context, suggestions->str, NULL);
    g_string_free (suggestions, TRUE);
}

void
formhistory_private_destroy (FormHistoryPriv *priv)
{
    if (priv->db)
    {
        sqlite3_close (priv->db);
        priv->db = NULL;
    }
    katze_assign (priv->jsforms, NULL);
    g_slice_free (FormHistoryPriv, priv);
}
#endif
