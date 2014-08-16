/*
 Copyright (C) 2013 Eder Sosa <eder.sohe@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

typedef struct
{
    gchar* folder;
    gchar* filename;
    gchar* uri;
} tabs2onePriv;

static tabs2onePriv* priv;

tabs2onePriv*
tabs2one_private_new ()
{
    tabs2onePriv* priv = g_slice_new (tabs2onePriv);
    priv->folder = NULL;
    priv->filename = NULL;
    priv->uri = NULL;

    return priv;
}

void tabs2one_private_destroy (tabs2onePriv* priv)
{
    katze_assign (priv->folder, NULL);
    katze_assign (priv->filename, NULL);
    katze_assign (priv->uri, NULL);
    g_slice_free (tabs2onePriv, priv);
}

static void
tabs2one_dom_click_restore_item_cb (WebKitDOMNode  *element,
                                    WebKitDOMEvent *dom_event,
                                    WebKitWebView  *webview);

static void
tabs2one_dom_click_remove_item_cb (WebKitDOMNode *element,
                                   WebKitDOMEvent *dom_event,
                                   WebKitWebView  *webview);

static gchar*
tabs2one_id_generator (void)
{
    GString* id = g_string_new("");
    GRand* generator = g_rand_new();
    gint32 id1 = g_rand_int_range (generator, 1000, 9999);
    gint32 id2 = g_rand_int_range (generator, 1000, 9999);
    g_rand_free(generator);
    g_string_printf(id, "%i-%i", id1, id2);
    return g_string_free(id, FALSE);
}

static void
tabs2one_dom_create_item (WebKitDOMDocument* doc,
                          const gchar* icon,
                          const gchar* uri,
                          const gchar* title)
{
    WebKitDOMElement* body = webkit_dom_document_query_selector(doc, "body", NULL);
    WebKitDOMElement* item = webkit_dom_document_create_element(doc, "div", NULL);
    WebKitDOMElement* favicon = webkit_dom_document_create_element(doc, "img", NULL);
    WebKitDOMElement* link = webkit_dom_document_create_element(doc, "a", NULL);
    WebKitDOMElement* close_link = webkit_dom_document_create_element(doc, "a", NULL);
    WebKitDOMElement* close_button = webkit_dom_document_create_element (doc, "img", NULL);
    WebKitDOMElement* br = webkit_dom_document_create_element(doc, "br", NULL);
    WebKitDOMText* content = webkit_dom_document_create_text_node(doc, title);

    webkit_dom_element_set_attribute(item, "id", tabs2one_id_generator(), NULL);
    webkit_dom_element_set_attribute(item, "class", "item", NULL);
    webkit_dom_element_set_attribute(item, "style", "padding: 5px;", NULL);

    webkit_dom_element_set_attribute(close_link, "class", "close-link", NULL);
    webkit_dom_element_set_attribute(close_link, "target", "_blank", NULL);
    webkit_dom_element_set_attribute(close_link, "href", "about:blank", NULL);
    webkit_dom_element_set_attribute(close_button, "src", "res://close.png", NULL);
    webkit_dom_element_set_attribute(close_button, "width", "16px", NULL);
    webkit_dom_element_set_attribute(close_button, "height", "16px", NULL);

    webkit_dom_element_set_attribute(favicon, "src", icon, NULL);
    webkit_dom_element_set_attribute(favicon, "width", "16px", NULL);
    webkit_dom_element_set_attribute(favicon, "height", "16px", NULL);
    webkit_dom_element_set_attribute(favicon, "style", "padding-left: 5px;", NULL);

    webkit_dom_element_set_attribute(link, "class", "restore-link",NULL);
    webkit_dom_element_set_attribute(link, "href", uri, NULL);
    webkit_dom_element_set_attribute(link, "style", "padding-left: 5px;", NULL);
    webkit_dom_element_set_attribute(link, "target", "_blank", NULL);

    webkit_dom_node_append_child(WEBKIT_DOM_NODE(link), WEBKIT_DOM_NODE(content), NULL);
    webkit_dom_node_append_child(WEBKIT_DOM_NODE(item), WEBKIT_DOM_NODE(close_link), NULL);
    webkit_dom_node_append_child(WEBKIT_DOM_NODE(close_link), WEBKIT_DOM_NODE(close_button), NULL);
    webkit_dom_node_append_child(WEBKIT_DOM_NODE(item), WEBKIT_DOM_NODE(favicon), NULL);
    webkit_dom_node_append_child(WEBKIT_DOM_NODE(item), WEBKIT_DOM_NODE(link), NULL);
    webkit_dom_node_append_child(WEBKIT_DOM_NODE(item), WEBKIT_DOM_NODE(br), NULL);
    webkit_dom_node_append_child(WEBKIT_DOM_NODE(body), WEBKIT_DOM_NODE(item), NULL);
}

static void
tabs2one_cache_create_dir (void)
{
    midori_paths_mkdir_with_parents (priv->folder, 0700);
}

static gchar*
tabs2one_cache_get_filename (void)
{
    return priv->filename;
}

static gchar*
tabs2one_cache_get_uri (void)
{
    return priv->uri;
}

static bool
tabs2one_cache_exist (void)
{
    return g_file_test (priv->filename, G_FILE_TEST_EXISTS);
}

static gboolean
tabs2one_is_uri_tabs2one_tab (const gchar* uri)
{
    return g_str_equal (uri, tabs2one_cache_get_uri ());
}

static void
tabs2one_dom_add_click_listeners (WebKitDOMDocument* doc,
                                  WebKitWebView* webview)
{
    WebKitDOMNodeList *elements = webkit_dom_document_query_selector_all(doc, ".restore-link", NULL);

    int i;

    for (i = 0; i < webkit_dom_node_list_get_length(elements); i++)
    {
        WebKitDOMNode *element = webkit_dom_node_list_item(elements, i);
        webkit_dom_event_target_add_event_listener(
            WEBKIT_DOM_EVENT_TARGET(element), "click",
            G_CALLBACK (tabs2one_dom_click_restore_item_cb), TRUE, webview);
    }

    elements = webkit_dom_document_query_selector_all(doc, ".close-link", NULL);
    for (i = 0; i < webkit_dom_node_list_get_length(elements); i++)
    {
        WebKitDOMNode *element = webkit_dom_node_list_item(elements, i);
        webkit_dom_event_target_add_event_listener(
            WEBKIT_DOM_EVENT_TARGET(element), "click",
            G_CALLBACK (tabs2one_dom_click_remove_item_cb), TRUE, webview);
    }
}

static bool
tabs2one_cache_write_file (WebKitWebView* webview)
{
    WebKitDOMDocument* doc = webkit_web_view_get_dom_document(webview);
    WebKitDOMHTMLDocument* dochtml = (WebKitDOMHTMLDocument*)doc;
    WebKitDOMHTMLElement* elementhtml = (WebKitDOMHTMLElement*)dochtml;
    const gchar* content = webkit_dom_html_element_get_inner_html(elementhtml);
    if (content == NULL)
        return FALSE;

    tabs2one_dom_add_click_listeners (doc, webview);

    return g_file_set_contents(tabs2one_cache_get_filename (), content, -1, NULL);
}

static void
tabs2one_onload_create_items_cb(WebKitWebView*  webview,
                                WebKitWebFrame* webframe,
                                MidoriBrowser*  browser)
{
    WebKitDOMDocument* doc = webkit_web_view_get_dom_document(webview);

    const gchar* icon;
    const gchar* title;
    const gchar* uri;

    GList* tabs = midori_browser_get_tabs (browser);
    for (; tabs; tabs = g_list_next (tabs))
    {
        icon = midori_view_get_icon_uri (tabs->data);
        if (icon == NULL) icon = "";
        title = midori_view_get_display_title (tabs->data);
        uri = midori_view_get_display_uri (tabs->data);

        if (!tabs2one_is_uri_tabs2one_tab (uri))
        {
            if (!midori_uri_is_blank (uri))
                tabs2one_dom_create_item(doc, icon, uri, title);
        }
    }


    if (tabs2one_cache_write_file (webview))
    {
        tabs = midori_browser_get_tabs (browser);
        while ((tabs = g_list_next (tabs)))
            midori_browser_close_tab (browser, tabs->data);

        tabs2one_dom_add_click_listeners (doc, webview);
        midori_browser_set_current_uri (browser, tabs2one_cache_get_uri ());
    }

    g_list_free(tabs);
}

static void
tabs2one_reload_connected_events_cb(WebKitWebView*  webview,
                                    WebKitWebFrame* frame,
                                    MidoriView*     view)
{
    const gchar* uri = midori_view_get_display_uri(view);

    if (tabs2one_is_uri_tabs2one_tab (uri))
    {
        WebKitDOMDocument* doc = webkit_web_view_get_dom_document(webview);
        tabs2one_dom_add_click_listeners (doc, webview);
    }
}


static void
tabs2one_add_tab_cb (MidoriBrowser*   browser,
                     MidoriView*      view,
                     MidoriExtension* extension)
{
    WebKitWebView* webview = WEBKIT_WEB_VIEW (midori_view_get_web_view(view));
    g_signal_connect (webview, "document-load-finished",
        G_CALLBACK (tabs2one_reload_connected_events_cb), view);
}

static void
tabs2one_dom_click_restore_item_cb (WebKitDOMNode  *element,
                                    WebKitDOMEvent *dom_event,
                                    WebKitWebView  *webview)
{
    webkit_dom_event_prevent_default (dom_event);
    MidoriView* view = midori_view_get_for_widget (GTK_WIDGET (webview));
    MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (webview));
    WebKitDOMNode* item = webkit_dom_node_get_parent_node (element);
    WebKitDOMNode* body = webkit_dom_node_get_parent_node (item);
    const gchar* uri = webkit_dom_element_get_attribute(WEBKIT_DOM_ELEMENT(element), "href");
    midori_browser_add_uri (browser, uri);

    WebKitDOMDocument* doc = webkit_web_view_get_dom_document (webview);
    webkit_dom_node_remove_child(body, item, NULL);
    tabs2one_cache_write_file (webview);

    WebKitDOMNodeList *elements = webkit_dom_document_query_selector_all(doc, ".restore-link", NULL);
    if (webkit_dom_node_list_get_length(elements) <= 0){
        midori_browser_close_tab(browser, GTK_WIDGET(view));
    }
}

static void
tabs2one_dom_click_remove_item_cb (WebKitDOMNode  *element,
                                   WebKitDOMEvent *dom_event,
                                   WebKitWebView  *webview)
{
    webkit_dom_event_prevent_default (dom_event);
    MidoriView* view = midori_view_get_for_widget (GTK_WIDGET (webview));
    MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (webview));
    WebKitDOMNode* item = webkit_dom_node_get_parent_node (element);
    WebKitDOMNode* body = webkit_dom_node_get_parent_node (item);

    WebKitDOMDocument* doc = webkit_web_view_get_dom_document (webview);
    webkit_dom_node_remove_child(body, item, NULL);
    tabs2one_cache_write_file (webview);

    WebKitDOMNodeList *elements = webkit_dom_document_query_selector_all(doc, ".close-link", NULL);
    if (webkit_dom_node_list_get_length(elements) <= 0){
        midori_browser_close_tab(browser, GTK_WIDGET(view));
    }
}

static void
tabs2one_apply_cb (GtkWidget*     menuitem,
                   MidoriBrowser* browser)
{
    GtkWidget* tab = NULL;

    if (!tabs2one_cache_exist ())
    {
        GString* tpl = g_string_new ("<html>\n<title>");

        g_string_append_printf (tpl, "%s", _("Tabs to One"));
        g_string_append (tpl, "</title>\n<head><meta charset=\"utf-8\"></head><body>\n");
        g_string_append_printf (tpl, "<h2>%s</h2>\n", _("Tabs you collected so far"));
        g_string_append_printf (tpl, "<div><span>%s</span></div>\n", _("Clicking an item restores a tab."));
        g_string_append (tpl, "</body>\n</html>\n");

        g_file_set_contents(tabs2one_cache_get_filename (), g_string_free (tpl, FALSE), -1, NULL);
    }

    tab = midori_browser_add_uri (browser, tabs2one_cache_get_uri ());

    WebKitWebView* webview = WEBKIT_WEB_VIEW (midori_view_get_web_view(MIDORI_VIEW (tab)));
    midori_browser_set_current_tab (browser, tab);

     g_signal_connect (webview, "document-load-finished",
        G_CALLBACK (tabs2one_onload_create_items_cb), browser);
}

static void
tabs2one_app_add_browser_cb (MidoriApp*       app,
                             MidoriBrowser*   browser,
                             MidoriExtension* extension);

static void
tabs2one_browser_populate_tool_menu_cb (MidoriBrowser*   browser,
                                        GtkWidget*       menu,
                                        MidoriExtension* extension)
{
    GtkWidget* menuitem = gtk_menu_item_new_with_mnemonic (_("Tabs to _One"));

    g_signal_connect (menuitem, "activate",
        G_CALLBACK (tabs2one_apply_cb), browser);
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
}

static void
tabs2one_deactivate_cb (MidoriExtension* extension,
                        MidoriBrowser*   browser)
{
    MidoriApp* app = midori_extension_get_app (extension);

    g_signal_handlers_disconnect_by_func (
        browser, tabs2one_browser_populate_tool_menu_cb, extension);
    g_signal_handlers_disconnect_by_func (
        extension, tabs2one_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, tabs2one_app_add_browser_cb, extension);
    g_signal_handlers_disconnect_by_func (
        browser, tabs2one_add_tab_cb, extension);

    tabs2one_private_destroy (priv);
}

static void
tabs2one_app_add_browser_cb (MidoriApp*       app,
                             MidoriBrowser*   browser,
                             MidoriExtension* extension)
{
    g_signal_connect (browser, "populate-tool-menu",
        G_CALLBACK (tabs2one_browser_populate_tool_menu_cb), extension);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (tabs2one_deactivate_cb), browser);
    g_signal_connect_after (browser, "add-tab",
        G_CALLBACK (tabs2one_add_tab_cb), extension);

    tabs2one_cache_create_dir();
}

static void
tabs2one_activate_cb (MidoriExtension* extension,
                      MidoriApp*       app)
{
    const gchar* config_dir = midori_extension_get_config_dir (extension);
    priv = tabs2one_private_new ();
    KatzeArray* browsers;
    MidoriBrowser* browser;
    gchar* filename, *uri;

    filename = g_build_filename (config_dir, "tabs2one.html", NULL);
    uri = g_filename_to_uri (filename, NULL, NULL);

    priv->filename = g_strdup (filename);
    priv->uri = g_strdup (uri);
    priv->folder = g_strdup (config_dir);

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        tabs2one_app_add_browser_cb (app, browser, extension);
    g_object_unref (browsers);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (tabs2one_app_add_browser_cb), extension);

    g_free (filename);
    g_free (uri);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Tabs to One"),
        "description", _("Closes all open tabs and creates new tab with tabs links"),
        "version", "0.1" MIDORI_VERSION_SUFFIX,
        "authors", "Eder Sosa <eder.sohe@gmail.com>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (tabs2one_activate_cb), NULL);

    return extension;
}
