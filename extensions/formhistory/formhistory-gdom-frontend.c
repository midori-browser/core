/*
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>
 Copyright (C) 2009-2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
*/
#include "formhistory-frontend.h"
#define COMPLETION_DELAY 200

FormHistoryPriv*
formhistory_private_new ()
{
    FormHistoryPriv* priv = g_slice_new (FormHistoryPriv);
    priv->oldkeyword = g_strdup ("");
    priv->selection_index = -1;
    return priv;
}

void
formhistory_suggestions_hide_cb (WebKitDOMElement* element,
                                 WebKitDOMEvent*   dom_event,
                                 FormHistoryPriv*  priv)
{
    if (gtk_widget_get_visible (priv->popup))
        gtk_widget_hide (priv->popup);
    priv->selection_index = -1;
}

static void
formhistory_suggestion_set (GtkTreePath*     path,
                            FormHistoryPriv* priv)
{
    GtkTreeIter iter;
    gchar* value;

    if (!gtk_tree_model_get_iter (priv->completion_model, &iter, path))
        return;

    gtk_tree_model_get (priv->completion_model, &iter, 0, &value, -1);
    g_object_set (priv->element, "value", value, NULL);
    g_free (value);
}

static gboolean
formhistory_suggestion_selected_cb (GtkWidget*       treeview,
                                    GdkEventButton*  event,
                                    FormHistoryPriv* priv)

{
    GtkTreePath* path;

    if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
        event->x, event->y, &path, NULL, NULL, NULL))
    {
        formhistory_suggestion_set (path, priv);
        formhistory_suggestions_hide_cb (NULL, NULL, priv);
        gtk_tree_path_free (path);
        return TRUE;
    }
    return FALSE;
}
static void
formhistory_suggestion_remove (GtkTreePath*     path,
                               FormHistoryPriv* priv)
{
    GtkTreeIter iter;
    gchar* sqlcmd;
    char* errmsg = NULL;
    gchar* name;
    gchar* value;

    if (!gtk_tree_model_get_iter (priv->completion_model, &iter, path))
        return;

    if (!priv->db)
        return;

    gtk_tree_model_get (priv->completion_model, &iter, 0, &value, -1);
    g_object_get (priv->element, "name", &name, NULL);
    gtk_list_store_remove (GTK_LIST_STORE (priv->completion_model), &iter);

    sqlcmd = sqlite3_mprintf ("DELETE FROM forms WHERE field = '%q' AND value = '%q'",
                              name, value);
    g_free (name);
    g_free (value);
    sqlite3_exec (priv->db, sqlcmd, NULL, NULL, &errmsg);
    sqlite3_free (sqlcmd);
}

static void
get_absolute_offset_for_element (WebKitDOMElement*  element,
                                 WebKitDOMDocument* element_document,
                                 WebKitDOMNodeList* frames,
                                 glong*             x,
                                 glong*             y,
                                 gboolean           ismainframe)
{
    WebKitDOMElement* offset_parent;
    gint offset_top = 0, offset_left = 0;
    gulong i;

    g_object_get (element, "offset-left", &offset_left,
                           "offset-top", &offset_top,
                           "offset-parent", &offset_parent,
                           NULL);
    *x += offset_left;
    *y += offset_top;
    /* To avoid deadlock check only first element of the mainframe parent */
    if (ismainframe == TRUE)
        return;
    if (offset_parent)
        goto finish;

    /* Element havent returned any parents. Thats mean or there is no parents or we are inside the frame
       Loop over all frames we have to find frame == element_document which is a root for our element
       and get its offsets */
    for (i = 0; i < webkit_dom_node_list_get_length (frames); i++)
    {
        WebKitDOMDocument *fdoc;
        WebKitDOMNode *frame = webkit_dom_node_list_item (frames, i);

        if (WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (frame))
            fdoc = webkit_dom_html_iframe_element_get_content_document (WEBKIT_DOM_HTML_IFRAME_ELEMENT (frame));
        else
            fdoc = webkit_dom_html_frame_element_get_content_document (WEBKIT_DOM_HTML_FRAME_ELEMENT (frame));
        if (fdoc == element_document)
        {
            offset_parent = WEBKIT_DOM_ELEMENT (frame);
            ismainframe = TRUE;
            /* Add extra 4px to ~cover size of borders  */
            *y += 4;
            break;
        }
    }
finish:
    if (offset_parent)
        get_absolute_offset_for_element (offset_parent, element_document, frames, x, y, ismainframe);
}

static void
formhistory_reposition_popup (FormHistoryPriv* priv)
{
    WebKitDOMDocument* element_document;
    WebKitDOMNodeList* frames;
    GtkWidget* view;
    GdkWindow* window;
    GtkWidget* toplevel;
    gint rx, ry;
    gint wx, wy;
    glong x = 0, y = 0;
    glong height;

    view = g_object_get_data (G_OBJECT (priv->element), "webview");
    toplevel = gtk_widget_get_toplevel (view);
    /* Position of a root window */
    window = gtk_widget_get_window (toplevel);
    gdk_window_get_position (window, &rx, &ry);

    /* Postion of webview in root window */
    window = gtk_widget_get_window (view);
    gdk_window_get_position (window, &wx, &wy);

    /* Position of editbox on the webview */
    frames = g_object_get_data (G_OBJECT (priv->element), "framelist");
    element_document = g_object_get_data (G_OBJECT (priv->element), "doc");
    get_absolute_offset_for_element (priv->element, element_document, frames, &x, &y, FALSE);
    /* Add height as menu should start under editbox, now on top of it */
    g_object_get (priv->element, "client-height", &height, NULL);
    y += height + 1;
    gtk_window_move (GTK_WINDOW (priv->popup),  rx + wx + x, ry +wy + y);

    /* Window configuration */
    gtk_window_set_screen (GTK_WINDOW (priv->popup), gtk_widget_get_screen (view));
    gtk_window_set_transient_for (GTK_WINDOW (priv->popup), GTK_WINDOW (toplevel));
    gtk_tree_view_columns_autosize (GTK_TREE_VIEW (priv->treeview));
    /* FIXME: Adjust size according to treeview width and some reasonable height */
    gtk_window_resize (GTK_WINDOW (priv->popup), 50, 80);
}

static gboolean
formhistory_suggestions_show (FormHistoryPriv* priv)
{
    GtkListStore* store;
    static sqlite3_stmt* stmt;
    gchar* value, * name;
    const char* sqlcmd;
    gint result;
    gchar* likedvalue;
    int pos = 0;

    g_return_val_if_fail (priv->element, FALSE);

    g_object_get (priv->element,
                  "name", &name,
                  "value", &value,
                  NULL);

    katze_assign (priv->oldkeyword, g_strdup (value));
    if (!priv->popup)
        formhistory_construct_popup_gui (priv);

    if (!stmt)
    {
        if (!priv->db)
            goto free_data;

        sqlcmd = "SELECT DISTINCT value FROM forms WHERE field = ?1 and value like ?2";
        sqlite3_prepare_v2 (priv->db, sqlcmd, strlen (sqlcmd) + 1, &stmt, NULL);
    }

    likedvalue = g_strdup_printf ("%s%%", value);
    sqlite3_bind_text (stmt, 1, name, -1, NULL);
    sqlite3_bind_text (stmt, 2, likedvalue, -1, g_free);
    result = sqlite3_step (stmt);

    if (result != SQLITE_ROW)
    {
        if (result == SQLITE_ERROR)
            g_print (_("Failed to select suggestions\n"));
        sqlite3_reset (stmt);
        sqlite3_clear_bindings (stmt);
        formhistory_suggestions_hide_cb (NULL, NULL, priv);
        goto free_data;
    }

    store = GTK_LIST_STORE (priv->completion_model);
    gtk_list_store_clear (store);

    while (result == SQLITE_ROW)
    {
        const unsigned char* text = sqlite3_column_text (stmt, 0);
        pos++;
        gtk_list_store_insert_with_values (store, NULL, pos, 0, text, -1);
        result = sqlite3_step (stmt);
    }
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);

    if (!gtk_widget_get_visible (priv->popup))
    {
        formhistory_reposition_popup (priv);
        gtk_widget_show_all (priv->popup);
    }

free_data:
    g_free (name);
    g_free (value);
    return FALSE;
}

static void
formhistory_editbox_key_pressed_cb (WebKitDOMElement* element,
                                    WebKitDOMEvent*   dom_event,
                                    FormHistoryPriv*  priv)
{
    glong key;
    GtkTreePath* path;
    gchar* keyword;
    gint matches;

    /* FIXME: Priv is still set after module is disabled */
    g_return_if_fail (priv);
    g_return_if_fail (element);

    if (priv->completion_timeout > 0)
        g_source_remove (priv->completion_timeout);

    katze_object_assign (priv->element, g_object_ref (element));

    key = webkit_dom_ui_event_get_key_code (WEBKIT_DOM_UI_EVENT (dom_event));
    switch (key)
    {
        /* ESC key*/
        case 27:
        case 35:
        case 36:
        /* Left key*/
        case 37:
        /* Right key*/
        case 39:
        /* Enter key*/
        case 13:
            if (key == 27)
                g_object_set (element, "value", priv->oldkeyword, NULL);
            formhistory_suggestions_hide_cb (element, dom_event, priv);
            return;
            break;
        /* Del key */
        case 46:
        /* Up key */
        case 38:
        /* Down key */
        case 40:

            if (!gtk_widget_get_visible (priv->popup))
            {
                formhistory_suggestions_show (priv);
                return;
            }
            matches = gtk_tree_model_iter_n_children (priv->completion_model, NULL);
            if (key == 38)
            {
                if (priv->selection_index <= 0)
                    priv->selection_index = matches - 1;
                else
                    priv->selection_index = MAX (priv->selection_index - 1, 0);
            }
            else if (key == 40)
            {
                if (priv->selection_index == matches - 1)
                    priv->selection_index = 0;
                else
                    priv->selection_index = MIN (priv->selection_index + 1, matches -1);
            }
            if (priv->selection_index == -1)
            {
                /* No element is selected */
                return;
            }

            path = gtk_tree_path_new_from_indices (priv->selection_index, -1);
            if (key == 46)
            {
                g_object_set (element, "value", priv->oldkeyword, NULL);
                formhistory_suggestion_remove (path, priv);
                matches--;
            }

            if (matches == 0)
                formhistory_suggestions_hide_cb (element, dom_event, priv);
            else
            {
                gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->treeview), path, NULL, FALSE);
                formhistory_suggestion_set (path, priv);
            }
            gtk_tree_path_free (path);
            return;
            break;
        /* PgUp, PgDn, Ins */
        case 33:
        case 34:
        case 45:
        /* Shift, Ctrl, Alt, Tab, Caps Lock*/
        case 16:
        case 17:
        case 18:
        case 20:
        case 9:
            return;
            break;
    }

    g_object_get (element, "value", &keyword, NULL);
    if (!(keyword && *keyword && *keyword != ' '))
    {
        formhistory_suggestions_hide_cb (element, dom_event, priv);
        goto free_data;
    }

    /* If the same keyword is submitted there's no need to regenerate suggestions */
    if (gtk_widget_get_visible (priv->popup) &&
        !g_strcmp0 (keyword, priv->oldkeyword))
        goto free_data;
    priv->completion_timeout = midori_timeout_add (COMPLETION_DELAY,
        (GSourceFunc)formhistory_suggestions_show, priv, NULL);
free_data:
    g_free (keyword);
}

static void
formhistory_DOMContentLoaded_cb (WebKitDOMElement* window,
                                 WebKitDOMEvent*   dom_event,
                                 FormHistoryPriv*  priv)
{
    gulong i;
    WebKitDOMDocument* doc;
    WebKitDOMNodeList* inputs;
    WebKitDOMNodeList* frames;
    GtkWidget* web_view;

    if (WEBKIT_DOM_IS_DOCUMENT (window))
        doc = WEBKIT_DOM_DOCUMENT (window);
    else
        doc = webkit_dom_dom_window_get_document (WEBKIT_DOM_DOM_WINDOW (window));
    inputs = webkit_dom_document_query_selector_all (doc, "input[type='text']", NULL);
    frames = g_object_get_data (G_OBJECT (window), "framelist");
    web_view = g_object_get_data (G_OBJECT (window), "webview");

    for (i = 0; i < webkit_dom_node_list_get_length (inputs); i++)
    {
        WebKitDOMNode* element = webkit_dom_node_list_item (inputs, i);
        gchar* autocomplete = webkit_dom_html_input_element_get_autocomplete (
            WEBKIT_DOM_HTML_INPUT_ELEMENT (element));
        gboolean off = !g_strcmp0 (autocomplete, "off");
        g_free (autocomplete);
        if (off)
            continue;

        g_object_set_data (G_OBJECT (element), "doc", doc);
        g_object_set_data (G_OBJECT (element), "webview", web_view);
        g_object_set_data (G_OBJECT (element), "framelist", frames);
        /* Add dblclick? */
        webkit_dom_event_target_add_event_listener (
                      WEBKIT_DOM_EVENT_TARGET (element), "keyup",
                      G_CALLBACK (formhistory_editbox_key_pressed_cb), false,
                      priv);
        webkit_dom_event_target_add_event_listener (
                      WEBKIT_DOM_EVENT_TARGET (element), "blur",
                      G_CALLBACK (formhistory_suggestions_hide_cb), false,
                      priv);
    }
}

void
formhistory_setup_suggestions (WebKitWebView*   web_view,
                               JSContextRef     js_context,
                               MidoriExtension* extension)
{
    WebKitDOMDocument* doc;
    WebKitDOMNodeList* frames;
    gulong i;

    FormHistoryPriv* priv = g_object_get_data (G_OBJECT (extension), "priv");
    doc = webkit_web_view_get_dom_document (web_view);
    frames = webkit_dom_document_query_selector_all (doc, "iframe, frame", NULL);
    g_object_set_data (G_OBJECT (doc), "framelist", frames);
    g_object_set_data (G_OBJECT (doc), "webview", web_view);
    /* Connect to DOMContentLoaded of the main frame */
    webkit_dom_event_target_add_event_listener(
                      WEBKIT_DOM_EVENT_TARGET (doc), "DOMContentLoaded",
                      G_CALLBACK (formhistory_DOMContentLoaded_cb), false,
                      priv);

    /* Connect to DOMContentLoaded of frames */
    for (i = 0; i < webkit_dom_node_list_get_length (frames); i++)
    {
        WebKitDOMDOMWindow* framewin;

        WebKitDOMNode* frame = webkit_dom_node_list_item (frames, i);
        if (WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (frame))
            framewin = webkit_dom_html_iframe_element_get_content_window (WEBKIT_DOM_HTML_IFRAME_ELEMENT (frame));
        else
            framewin = webkit_dom_html_frame_element_get_content_window (WEBKIT_DOM_HTML_FRAME_ELEMENT (frame));
        g_object_set_data (G_OBJECT (framewin), "framelist", frames);
        g_object_set_data (G_OBJECT (framewin), "webview", (GtkWidget*)web_view);
        webkit_dom_event_target_add_event_listener (
                      WEBKIT_DOM_EVENT_TARGET (framewin), "DOMContentLoaded",
                      G_CALLBACK (formhistory_DOMContentLoaded_cb), false,
                      priv);
    }
    formhistory_suggestions_hide_cb (NULL, NULL, priv);
}

void
formhistory_private_destroy (FormHistoryPriv *priv)
{
    katze_object_assign (priv->database, NULL);
    katze_assign (priv->oldkeyword, NULL);
    gtk_widget_destroy (priv->popup);
    priv->popup = NULL;
    katze_object_assign (priv->element, NULL);
    g_slice_free (FormHistoryPriv, priv);
}

gboolean
formhistory_construct_popup_gui (FormHistoryPriv* priv)
{
    GtkTreeModel* model = NULL;
    GtkWidget* popup;
    GtkWidget* popup_frame;
    GtkWidget* scrolled;
    GtkWidget* treeview;
    GtkCellRenderer* renderer;
    GtkTreeViewColumn* column;

    model = (GtkTreeModel*) gtk_list_store_new (1, G_TYPE_STRING);
    priv->completion_model = model;
    popup = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_window_set_type_hint (GTK_WINDOW (popup), GDK_WINDOW_TYPE_HINT_COMBO);
    popup_frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (popup_frame), GTK_SHADOW_ETCHED_IN);
    gtk_container_add (GTK_CONTAINER (popup), popup_frame);
    scrolled = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
        "hscrollbar-policy", GTK_POLICY_NEVER,
        "vscrollbar-policy", GTK_POLICY_AUTOMATIC, NULL);
    gtk_container_add (GTK_CONTAINER (popup_frame), scrolled);
    treeview = gtk_tree_view_new_with_model (model);
    priv->treeview = treeview;
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (treeview), TRUE);
    gtk_container_add (GTK_CONTAINER (scrolled), treeview);
    gtk_widget_set_size_request (gtk_scrolled_window_get_vscrollbar (
        GTK_SCROLLED_WINDOW (scrolled)), -1, 0);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("suggestions", renderer, "text", 0, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    priv->popup = popup;
    priv->element = NULL;

    g_signal_connect (treeview, "button-press-event",
        G_CALLBACK (formhistory_suggestion_selected_cb), priv);
    return TRUE;
}
