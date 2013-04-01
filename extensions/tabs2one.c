/*
 Copyright (C) 2013 Eder Sosa <eder.sohe@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

static void 
tabs2one_dom_click_remove_item_cb (WebKitDOMNode  *element, 
                                   WebKitDOMEvent *dom_event, 
                                   WebKitWebView  *webview);

static gchar*
tabs2one_cache_get_dir (void){
    return g_build_filename (midori_paths_get_cache_dir (), "tabs2one", NULL);
}

static void
tabs2one_cache_create_dir (void){
    midori_paths_mkdir_with_parents (tabs2one_cache_get_dir (), 0700);
}

static gchar*
tabs2one_cache_get_filename (void){
    return g_build_filename (tabs2one_cache_get_dir (), "tabs2one.html", NULL);
}

static gchar*
tabs2one_cache_get_uri (void){
    return g_strconcat ("file://", tabs2one_cache_get_filename (), NULL);
}

static bool
tabs2one_cache_exist (void){
    return g_file_test (tabs2one_cache_get_filename (), G_FILE_TEST_EXISTS);
}

static void
tabs2one_dom_click_items(WebKitDOMDocument* doc,
                         WebKitWebView* webview)
{
    WebKitDOMNodeList *elements = webkit_dom_document_query_selector_all(doc, ".item a", NULL);

    int i;

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
    tabs2one_dom_click_items(doc, webview);
    const gchar* content = webkit_dom_html_element_get_inner_html(elementhtml);
    return g_file_set_contents(tabs2one_cache_get_filename (), content, -1, NULL);
}

static void
tabs2one_load_finished_cb(WebKitWebView*  webview,
                          WebKitWebFrame* webframe,
                          MidoriView*     view)
{
    const gchar* title = midori_view_get_display_title(view);
    const gchar* uri = midori_view_get_display_uri(view);

    if (!strcmp(uri, tabs2one_cache_get_uri ())) {
        WebKitDOMDocument* doc = webkit_web_view_get_dom_document(webview);
        tabs2one_dom_click_items(doc, webview);
    }
}

static void
tabs2one_add_tab_cb (MidoriBrowser*   browser,
                     MidoriView*      view,
                     MidoriExtension* extension)
{
    WebKitWebView* webview = WEBKIT_WEB_VIEW (midori_view_get_web_view(view));
    g_signal_connect (webview, "document-load-finished",
        G_CALLBACK (tabs2one_load_finished_cb), view);
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
    const gchar* uri = webkit_dom_element_get_attribute(WEBKIT_DOM_ELEMENT(element), "href");
    midori_browser_add_uri (browser, uri);
    
    WebKitDOMDocument* doc = webkit_web_view_get_dom_document (webview);
    webkit_dom_node_remove_child(body, item, NULL);
    tabs2one_cache_write_file (webview);

    WebKitDOMNodeList *elements = webkit_dom_document_query_selector_all(doc, ".item a", NULL);
    if (webkit_dom_node_list_get_length(elements) <= 0){
        midori_browser_close_tab(browser, GTK_WIDGET(view));
    }
}

static void
tabs2one_apply_cb (GtkWidget*     menuitem,
                   MidoriBrowser* browser)
{
    bool exist = FALSE;
    GtkWidget* tab = NULL;
    GList* tabs = midori_browser_get_tabs (browser);

    for (; tabs; tabs = g_list_next (tabs))
    {
        if (!strcmp(midori_view_get_display_uri (tabs->data), tabs2one_cache_get_uri ())){
            exist = TRUE;
            tab = tabs->data;
            break;
        }
    }

    g_list_free(tabs);

    if (!exist && tabs2one_cache_exist ()){
        tab = midori_browser_add_uri (browser, tabs2one_cache_get_uri ());
    }

    if (!exist && !tabs2one_cache_exist ()){

        const gchar* tpl = "<html><title>Tabs to One</title><head><meta charset=\"utf-8\"><script>\n"
                           "    function id() {\n"
                           "        return Math.floor((1 + Math.random()) * 0x10000).toString(16).substring(1); }\n"
                           "    function add(title,icon,uri) {\n"
                           "        d=document.createElement(\"div\");\n"
                           "        i=document.createElement(\"img\");\n"
                           "        a=document.createElement(\"a\");\n"
                           "        b=document.createElement(\"br\");\n"
                           "        t=document.createTextNode(title);\n"
                           "        var _id=id() + '-' + id(); d.id=_id; d.className=\"item\";\n"
                           "        i.src=icon; a.href=uri; a.target=\"_blank\"; a.appendChild(t);\n"
                           "        i.width=16; i.height=16; d.style.padding=5; i.style.paddingLeft=5; a.style.paddingLeft=5;\n"
                           "        d.appendChild(i); d.appendChild(a); d.appendChild(b);\n"
                           "        document.body.appendChild(d); }\n"
                           "</script></head><body></body></html>";

        g_file_set_contents(tabs2one_cache_get_filename (), tpl, -1, NULL);
        tab = midori_browser_add_uri (browser, tabs2one_cache_get_uri ());
    }

    WebKitWebView* webview = WEBKIT_WEB_VIEW (midori_view_get_web_view(MIDORI_VIEW (tab)));

    while (gtk_events_pending())
        gtk_main_iteration();
   
    midori_browser_set_current_tab (browser, tab);

    WebKitWebFrame* webframe = webkit_web_view_get_main_frame (webview);
    JSContextRef jscontext = webkit_web_frame_get_global_context (webframe);

    GString* text = g_string_new("");

    const gchar* tpl = "add(\"%s\", \"%s\", \"%s\");";
    const gchar* icon;
    const gchar* title;
    const gchar* uri;
    gchar* data = NULL;
    
    tabs = midori_browser_get_tabs (browser);
    for (; tabs; tabs = g_list_next (tabs))
    {
        icon = midori_view_get_icon_uri (tabs->data);
        title = midori_view_get_display_title (tabs->data);
        uri = midori_view_get_display_uri (tabs->data);
        if (strcmp(uri, tabs2one_cache_get_uri ())){
            g_string_append_printf (text, tpl, title, icon, uri);
            midori_browser_close_tab(browser, tabs->data);
            data = g_string_free(text, FALSE);
            text = g_string_new("");
            sokoke_js_script_eval (jscontext, data, NULL);
        }
    }

    tabs2one_cache_write_file (webview);

    g_string_free(text, TRUE);
    g_free(data);
    g_list_free(tabs);
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
    KatzeArray* browsers;
    MidoriBrowser* browser;

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        tabs2one_app_add_browser_cb (app, browser, extension);
    g_object_unref (browsers);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (tabs2one_app_add_browser_cb), extension);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Tabs to One"),
        "description", _("Closes all open tabs and create new tab with links tabs"),
        "version", "0.1" MIDORI_VERSION_SUFFIX,
        "authors", "Eder Sosa <eder.sohe@gmail.com>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (tabs2one_activate_cb), NULL);

    return extension;
}
