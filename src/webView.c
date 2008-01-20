/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "webView.h"

#include "helpers.h"
#include "sokoke.h"
#include "../katze/katze.h"

#include <string.h>

WebKitNavigationResponse on_webView_navigation_requested(GtkWidget* webView
 , WebKitWebFrame* frame, WebKitNetworkRequest* networkRequest)
{
    WebKitNavigationResponse response = WEBKIT_NAVIGATION_RESPONSE_ACCEPT;
    // TODO: Ask webkit wether it knows the protocol for "unknown protcol"
    // TODO: This isn't the place for uri scheme handling
    const gchar* uri = webkit_network_request_get_uri(networkRequest);
    gchar* protocol = strtok(g_strdup(uri), ":");
    if(spawn_protocol_command(protocol, uri))
        response = WEBKIT_NAVIGATION_RESPONSE_IGNORE;
    g_free(protocol);
    return response;
}

void on_webView_title_changed(GtkWidget* webView, WebKitWebFrame* frame
 , const gchar* title, CBrowser* browser)
{
    const gchar* newTitle;
    if(title)
        newTitle = title;
    else
        newTitle = webkit_web_frame_get_uri(frame);
    katze_xbel_item_set_title(browser->sessionItem, newTitle);
    gtk_label_set_text(GTK_LABEL(browser->webView_name), newTitle);
    sokoke_widget_set_tooltip_text(gtk_widget_get_parent(
     gtk_widget_get_parent(browser->webView_name)), newTitle);
    gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(
     browser->webView_menu))), newTitle);
    if(webView == get_nth_webView(-1, browser))
    {
        gchar* windowTitle = g_strconcat(newTitle, " - ", PACKAGE_NAME, NULL);
        gtk_window_set_title(GTK_WINDOW(browser->window), windowTitle);
        g_free(windowTitle);
    }
}

void on_webView_icon_changed(GtkWidget* webView, WebKitWebFrame* widget
 , CBrowser* browser)
{
    // TODO: Implement icon updates; currently this isn't ever called anyway
    const gchar* icon = NULL;
    UNIMPLEMENTED
    if(icon)
    {
        gtk_label_set_text(GTK_LABEL(browser->webView_name), "icon");
        gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(
         browser->webView_menu))), "icon");
        if(webView == get_nth_webView(-1, browser))
        {
            gchar* windowTitle = g_strconcat("icon", " - ", PACKAGE_NAME, NULL);
            gtk_window_set_title(GTK_WINDOW(browser->window), windowTitle);
            g_free(windowTitle);
        }
    }
}

void on_webView_load_started(GtkWidget* webView, WebKitWebFrame* widget
 , CBrowser* browser)
{
    browser->loadedPercent = 0;
    update_favicon(browser);
    if(webView == get_nth_webView(-1, browser))
    {
        update_gui_state(browser);
        update_statusbar(browser);
    }
}

void on_webView_load_committed(GtkWidget* webView, WebKitWebFrame* frame
 , CBrowser* browser)
{
    const gchar* uri = webkit_web_frame_get_uri(frame);
    gchar* newUri = g_strdup(uri ? uri : "");
    katze_xbel_bookmark_set_href(browser->sessionItem, newUri);
    if(webView == get_nth_webView(-1, browser))
    {
        gtk_entry_set_text(GTK_ENTRY(browser->location), newUri);
        gtk_label_set_text(GTK_LABEL(browser->webView_name), newUri);
        katze_assign(browser->statusMessage, NULL);
    }
}

void on_webView_load_changed(GtkWidget* webView, gint progress, CBrowser* browser)
{
    browser->loadedPercent = progress;
    if(webView == get_nth_webView(-1, browser))
        update_statusbar(browser);
}

void on_webView_load_finished(GtkWidget* webView, WebKitWebFrame* widget
 , CBrowser* browser)
{
    browser->loadedPercent = -1;
    update_favicon(browser);
    if(webView == get_nth_webView(-1, browser))
        update_gui_state(browser);
}

void on_webView_status_message(GtkWidget* webView, const gchar* text, CBrowser* browser)
{
    katze_assign(browser->statusMessage, g_strdup(text));
    update_statusbar(browser);
}

void on_webView_selection_changed(GtkWidget* webView, CBrowser* browser)
{
    UNIMPLEMENTED
}

gboolean on_webView_console_message(GtkWidget* webView
 , const gchar* message, gint line, const gchar* sourceId, CBrowser* browser)
{
    return FALSE;
}

void on_webView_link_hover(GtkWidget* webView, const gchar* tooltip
 , const gchar* uri, CBrowser* browser)
{
    katze_assign(browser->statusMessage, g_strdup(uri));
    update_statusbar(browser);
    katze_assign(browser->elementUri, g_strdup(uri));
}

/*
GtkWidget* on_webView_window_open(GtkWidget* webView, const gchar* sUri
 , CBrowser* browser)
{
    // A window is created
    // TODO: Respect config->iNewPages
    // TODO: Find out if this comes from a script or a click
    // TODO: Block scripted popups, return NULL and show status icon
    CBrowser* newBrowser = browser_new(config->openPopupsInTabs ? browser : NULL);
    return newBrowser->webView;
}
*/

void webView_popup(GtkWidget* webView, GdkEventButton* event, CBrowser* browser)
{
    gboolean isLink = browser->elementUri != NULL; // Did we right-click a link?
    gboolean haveLinks = FALSE; // TODO: Are several links selected?
    gboolean isImage = FALSE; // TODO: Did we right-click an image?
    gboolean isEditable = webkit_web_view_can_paste_clipboard(WEBKIT_WEB_VIEW(webView));
    gboolean hasSelection = webkit_web_view_has_selection(WEBKIT_WEB_VIEW(webView));

    update_edit_items(browser);

    // Download manager available?
    const gchar* downloadManager = g_datalist_get_data(&config->protocols_commands, "download");
    gboolean canDownload = downloadManager && *downloadManager;

    action_set_visible("LinkTabNew", isLink, browser);
    action_set_visible("LinkTabCurrent", isLink, browser);
    action_set_visible("LinkWindowNew", isLink, browser);

    action_set_visible("LinkSaveAs", isLink, browser);
    action_set_visible("LinkSaveWith", isLink && canDownload, browser);
    action_set_visible("LinkCopy", isLink, browser);
    action_set_visible("LinkBookmarkNew", isLink, browser);

    action_set_visible("SelectionLinksNewTabs", haveLinks && hasSelection, browser);
    action_set_visible("SelectionTextTabNew", haveLinks && hasSelection, browser);
    action_set_visible("SelectionTextTabCurrent", haveLinks && hasSelection, browser);
    action_set_visible("SelectionTextWindowNew", haveLinks && hasSelection, browser);

    action_set_visible("ImageViewTabNew", isImage, browser);
    action_set_visible("ImageViewTabCurrent", isImage, browser);
    action_set_visible("ImageSaveAs", isImage, browser);
    action_set_visible("ImageSaveWith", isImage && canDownload, browser);
    action_set_visible("ImageCopy", isImage, browser);

    action_set_visible("Copy_", hasSelection || isEditable, browser);
    action_set_visible("SelectionSearch", hasSelection, browser);
    action_set_visible("SelectionSearchWith", hasSelection, browser);
    action_set_visible("SelectionSourceView", hasSelection, browser);

    action_set_visible("SourceView", !hasSelection, browser);

    if(isEditable)
        sokoke_widget_popup(webView, GTK_MENU(browser->popup_editable), event);
    else if(isLink || isImage || hasSelection)
        sokoke_widget_popup(webView, GTK_MENU(browser->popup_element), event);
    else
        sokoke_widget_popup(webView, GTK_MENU(browser->popup_webView), event);
}

gboolean on_webView_button_press(GtkWidget* webView, GdkEventButton* event
 , CBrowser* browser)
{
    GdkModifierType state = (GdkModifierType)0;
    gint x, y;
    gdk_window_get_pointer(NULL, &x, &y, &state);
    switch(event->button)
    {
    case 1:
        if(!browser->elementUri)
            return FALSE;
        if(state & GDK_SHIFT_MASK)
        {
            // Open link in new window
            CBrowser* curBrowser = browser_new(NULL);
            webkit_web_view_open(WEBKIT_WEB_VIEW(curBrowser->webView), browser->elementUri);
            return TRUE;
        }
        else if(state & GDK_MOD1_MASK)
        {
            // Open link in new tab
            CBrowser* curBrowser = browser_new(browser);
            webkit_web_view_open(WEBKIT_WEB_VIEW(curBrowser->webView), browser->elementUri);
            return TRUE;
        }
        break;
    case 2:
        if(state & GDK_CONTROL_MASK)
        {
            //webkit_web_view_set_text_size(WEBKIT_WEB_VIEW(webView), 1);
            return TRUE;
        }
        else
        {
            if(!browser->elementUri)
                return FALSE;
            // Open link in new tab
            CBrowser* curBrowser = browser_new(browser);
            webkit_web_view_open(WEBKIT_WEB_VIEW(curBrowser->webView), browser->elementUri);
            return TRUE;
        }
        break;
    case 3:
        webView_popup(webView, event, browser);
        return TRUE;
    }
    return FALSE;
}

void on_webView_popup(GtkWidget* webView, CBrowser* browser)
{
    webView_popup(webView, NULL, browser);
}

gboolean on_webView_scroll(GtkWidget* webView, GdkEventScroll* event
 , CBrowser* browser)
{
    GdkModifierType state = (GdkModifierType)0;
    gint x, y;
    gdk_window_get_pointer(NULL, &x, &y, &state);
    if(state & GDK_CONTROL_MASK)
    {
        /*const gfloat size = webkit_web_view_get_text_size(WEBKIT_WEB_VIEW(webView));
        if(event->direction == GDK_SCROLL_DOWN)
            webkit_web_view_set_text_size(WEBKIT_WEB_VIEW(webView), size + 0.1);
        else if(event->direction == GDK_SCROLL_UP)
            webView_set_text_size(WEBKIT_WEB_VIEW(webView), size - 0.1);*/
        return TRUE;
    }
    else
        return FALSE;
}

gboolean on_webView_leave(GtkWidget* webView, GdkEventCrossing* event, CBrowser* browser)
{
    katze_assign(browser->statusMessage, NULL);
    update_statusbar(browser);
    return TRUE;
}

void on_webView_destroy(GtkWidget* widget, CBrowser* browser)
{
    // Update browser list, free memory and possibly quit
    GList* tmp = g_list_find(browsers, browser);
    browsers = g_list_delete_link(browsers, tmp);
    g_free(browser->elementUri);
    g_free(browser->statusMessage);
    // FIXME: Multiple windows are not taken into account
    if(!g_list_nth(browsers, 0))
    {
        g_object_unref(browser->actiongroup);
        g_object_unref(browser->popup_bookmark);
        g_object_unref(browser->popup_webView);
        g_object_unref(browser->popup_element);
        g_object_unref(browser->popup_editable);
        guint i;
        guint n = katze_xbel_folder_get_n_items(bookmarks);
        for(i = 0; i < n; i++)
            katze_xbel_item_unref(katze_xbel_folder_get_nth_item(bookmarks, i));
        gtk_main_quit();
    }
}

// webView actions begin here

GtkWidget* webView_new(GtkWidget** scrolled)
{
    *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(*scrolled)
     , GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    GTK_WIDGET_SET_FLAGS(*scrolled, GTK_CAN_FOCUS);
    GtkWidget* webView = webkit_web_view_new();
    gtk_container_add(GTK_CONTAINER(*scrolled), webView);
    return webView;
}

void webView_open(GtkWidget* webView, const gchar* uri)
{
    webkit_web_view_open(WEBKIT_WEB_VIEW(webView), uri);
    // We need to check the browser first
    // No browser means this is a panel
    CBrowser* browser = get_browser_from_webView(webView);
    if(browser)
    {
        katze_xbel_bookmark_set_href(browser->sessionItem, uri);
        katze_xbel_item_set_title(browser->sessionItem, "");
    }
}

void webView_close(GtkWidget* webView, CBrowser* browser)
{
    browser = get_browser_from_webView(webView);
    const gchar* uri = katze_xbel_bookmark_get_href(browser->sessionItem);
    katze_xbel_folder_remove_item(session, browser->sessionItem);
    if(uri && *uri)
    {
        katze_xbel_folder_prepend_item(tabtrash, browser->sessionItem);
        guint n = katze_xbel_folder_get_n_items(tabtrash);
        if(n > 10)
        {
            KatzeXbelItem* item = katze_xbel_folder_get_nth_item(tabtrash, n - 1);
            katze_xbel_item_unref(item);
        }
    }
    else
        katze_xbel_item_unref(browser->sessionItem);
    gtk_widget_destroy(browser->webView_menu);
    gtk_notebook_remove_page(GTK_NOTEBOOK(browser->webViews)
     , get_webView_index(webView, browser));
    update_browser_actions(browser);
}
