/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "browser.h"

#include "helpers.h"
#include "prefs.h"
#include "sokoke.h"
#include "ui.h"
#include "webView.h"
#include "webSearch.h"
#include "xbel.h"

#include <gdk/gdkkeysyms.h>
#include <string.h>

// -- GTK+ signal handlers begin here

void on_action_window_new_activate(GtkAction* action, CBrowser* browser)
{
    browser_new(NULL);
}

void on_action_tab_new_activate(GtkAction* action, CBrowser* browser)
{
    browser_new(browser);
    update_browser_actions(browser);
}

void on_action_open_activate(GtkAction* action, CBrowser* browser)
{
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Open file"
        , GTK_WINDOW(browser->window)
        , GTK_FILE_CHOOSER_ACTION_OPEN
        , GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL
        , GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT
        , NULL);
     gtk_window_set_icon_name(GTK_WINDOW(dialog), GTK_STOCK_OPEN);
     if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
     {
         gchar* sFilename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
         webView_open(get_nth_webView(-1, browser), sFilename);
         g_free(sFilename);
     }
    gtk_widget_destroy(dialog);
}

void on_action_tab_close_activate(GtkAction* action, CBrowser* browser)
{
    webView_close(get_nth_webView(-1, browser), browser);
}

void on_action_window_close_activate(GtkAction* action, CBrowser* browser)
{
    gtk_widget_destroy(browser->window);
}

void on_action_quit_activate(GtkAction* action, CBrowser* browser)
{
    gtk_main_quit();
}

void on_action_edit_activate(GtkAction* action, CBrowser* browser)
{
    update_edit_items(browser);
}

void on_action_cut_activate(GtkAction* action, CBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus(GTK_WINDOW(browser->window));
    g_signal_emit_by_name(widget, "cut-clipboard");
}

void on_action_copy_activate(GtkAction* action, CBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus(GTK_WINDOW(browser->window));
    g_signal_emit_by_name(widget, "copy-clipboard");
}

void on_action_paste_activate(GtkAction* action, CBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus(GTK_WINDOW(browser->window));
    g_signal_emit_by_name(widget, "paste-clipboard");
}

void on_action_delete_activate(GtkAction* action, CBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus(GTK_WINDOW(browser->window));
    if(WEBKIT_IS_WEB_VIEW(widget))
        ;//webkit_web_view_delete_selection(WEBKIT_WEB_VIEW(widget));
    else if(GTK_IS_EDITABLE(widget))
        gtk_editable_delete_selection(GTK_EDITABLE(widget));
}

void on_action_selectAll_activate(GtkAction* action, CBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus(GTK_WINDOW(browser->window));
    if(GTK_IS_ENTRY(widget))
        gtk_editable_select_region(GTK_EDITABLE(widget), 0, -1);
    else
        g_signal_emit_by_name(widget, "select-all");
}

void on_action_find_activate(GtkAction* action, CBrowser* browser)
{
    if(GTK_WIDGET_VISIBLE(browser->findbox))
        gtk_widget_hide(browser->findbox);
    else
    {
        GtkWidget* icon = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
        sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(browser->findbox_text)
         , SEXY_ICON_ENTRY_PRIMARY, GTK_IMAGE(icon));
        gtk_widget_show(browser->findbox);
        gtk_widget_grab_focus(GTK_WIDGET(browser->findbox_text));
    }
}

void on_action_find_next_activate(GtkAction* action, CBrowser* browser)
{
    /*if(!GTK_WIDGET_VISIBLE(browser->findbox))
        ; // FIXME: What if the findbox is hidden?
    const gchar* text = gtk_entry_get_text(GTK_ENTRY(browser->findbox_text));
    const gboolean caseSensitive = gtk_toggle_tool_button_get_active(
     GTK_TOGGLE_TOOL_BUTTON(browser->findbox_case));
    GtkWidget* webView = get_nth_webView(-1, browser);
    GtkWidget* icon;
    webkit_web_view_unmark_text_matches(WEBKIT_WEB_VIEW(webView));
    if(webkit_web_view_search_text(WEBKIT_WEB_VIEW(webView), text, TRUE, caseSensitive, TRUE))
        icon = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
    else
        icon = gtk_image_new_from_stock(GTK_STOCK_STOP, GTK_ICON_SIZE_MENU);
    sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(browser->findbox_text)
     , SEXY_ICON_ENTRY_PRIMARY, GTK_IMAGE(icon));
    webkit_web_view_mark_text_matches(WEBKIT_WEB_VIEW(webView), text, caseSensitive, 0);
    const gboolean highlight = gtk_toggle_tool_button_get_active(
     GTK_TOGGLE_TOOL_BUTTON(browser->findbox_highlight));
    webkit_web_view_set_highlight_text_matches(WEBKIT_WEB_VIEW(webView), highlight);*/
}

void on_action_find_previous_activate(GtkAction* action, CBrowser* browser)
{
    /*if(!GTK_WIDGET_VISIBLE(browser->findbox))
        ; // FIXME: What if the findbox is hidden?
    const gchar* text = gtk_entry_get_text(GTK_ENTRY(browser->findbox_text));
    const gboolean caseSensitive = gtk_toggle_tool_button_get_active(
     GTK_TOGGLE_TOOL_BUTTON(browser->findbox_case));
    GtkWidget* webView = get_nth_webView(-1, browser);
    webkit_web_view_unmark_text_matches(WEBKIT_WEB_VIEW(webView));
    GtkWidget* icon;
    if(webkit_web_view_search_text(WEBKIT_WEB_VIEW(webView), text, FALSE, caseSensitive, TRUE))
        icon = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
    else
        icon = gtk_image_new_from_stock(GTK_STOCK_STOP, GTK_ICON_SIZE_MENU);
    sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(browser->findbox_text)
     , SEXY_ICON_ENTRY_PRIMARY, GTK_IMAGE(icon));
    webkit_web_view_mark_text_matches(WEBKIT_WEB_VIEW(webView), text, caseSensitive, 0);
    const gboolean highlight = gtk_toggle_tool_button_get_active(
     GTK_TOGGLE_TOOL_BUTTON(browser->findbox_highlight));
    webkit_web_view_set_highlight_text_matches(WEBKIT_WEB_VIEW(webView), highlight);*/
}

void on_findbox_highlight_toggled(GtkToggleToolButton* toolitem, CBrowser* browser)
{
    /*GtkWidget* webView = get_nth_webView(-1, browser);
    const gboolean highlight = gtk_toggle_tool_button_get_active(toolitem);
    webkit_web_view_set_highlight_text_matches(WEBKIT_WEB_VIEW(webView), highlight);*/
}

void on_action_preferences_activate(GtkAction* action, CBrowser* browser)
{
    // Show the preferences dialog. Create it if necessary.
    static GtkWidget* dialog;
    if(GTK_IS_DIALOG(dialog))
        gtk_window_present(GTK_WINDOW(dialog));
    else
    {
        dialog = prefs_preferences_dialog_new(browser);
        gtk_widget_show(dialog);
    }
}

static void on_toolbar_navigation_notify_style(GObject* object, GParamSpec* arg1
 , CBrowser* browser)
{
    if(config->toolbarStyle == CONFIG_TOOLBAR_DEFAULT)
    {
        gtk_toolbar_set_style(GTK_TOOLBAR(browser->navibar)
         , config_to_toolbarstyle(config->toolbarStyle));
    }
}

void on_action_toolbar_navigation_activate(GtkToggleAction* action, CBrowser* browser)
{
    config->toolbarNavigation = gtk_toggle_action_get_active(action);
    sokoke_widget_set_visible(browser->navibar, config->toolbarNavigation);
}

void on_action_toolbar_bookmarks_activate(GtkToggleAction* action, CBrowser* browser)
{
    config->toolbarBookmarks = gtk_toggle_action_get_active(action);
    sokoke_widget_set_visible(browser->bookmarkbar, config->toolbarBookmarks);
}

void on_action_toolbar_downloads_activate(GtkToggleAction* action, CBrowser* browser)
{
    /*config->toolbarDownloads = gtk_toggle_action_get_active(action);
    sokoke_widget_set_visible(browser->downloadbar, config->toolbarDownloads);*/
}

void on_action_toolbar_status_activate(GtkToggleAction* action, CBrowser* browser)
{
    config->toolbarStatus = gtk_toggle_action_get_active(action);
    sokoke_widget_set_visible(browser->statusbar, config->toolbarStatus);
}

void on_action_refresh_activate(GtkAction* action, CBrowser* browser)
{
    /*GdkModifierType state = (GdkModifierType)0;
    gint x, y; gdk_window_get_pointer(NULL, &x, &y, &state);
    gboolean fromCache = state & GDK_SHIFT_MASK;*/
    webkit_web_view_reload(WEBKIT_WEB_VIEW(get_nth_webView(-1, browser)));
}

void on_action_refresh_stop_activate(GtkAction* action, CBrowser* browser)
{
    gchar* stockId; g_object_get(action, "stock-id", &stockId, NULL);
    // Refresh or stop, depending on the stock id
    if(!strcmp(stockId, GTK_STOCK_REFRESH))
    {
        /*GdkModifierType state = (GdkModifierType)0;
        gint x, y; gdk_window_get_pointer(NULL, &x, &y, &state);
        gboolean fromCache = state & GDK_SHIFT_MASK;*/
        webkit_web_view_reload(WEBKIT_WEB_VIEW(get_nth_webView(-1, browser)));
    }
    else
        webkit_web_view_stop_loading(WEBKIT_WEB_VIEW(get_nth_webView(-1, browser)));
    g_free(stockId);
}

void on_action_stop_activate(GtkAction* action, CBrowser* browser)
{
    webkit_web_view_stop_loading(WEBKIT_WEB_VIEW(get_nth_webView(-1, browser)));
}

void on_action_zoom_in_activate(GtkAction* action, CBrowser* browser)
{
    /*GtkWidget* webView = get_nth_webView(-1, browser);
    const gfloat zoom = webkit_web_view_get_text_multiplier(WEBKIT_WEB_VIEW(webView));
    webkit_web_view_set_text_multiplier(WEBKIT_WEB_VIEW(webView), zoom + 0.1);*/
}

void on_action_zoom_out_activate(GtkAction* action, CBrowser* browser)
{
    /*GtkWidget* webView = get_nth_webView(-1, browser);
    const gfloat zoom = webView_get_text_size(WEBKIT_WEB_VIEW(webView));
    webkit_web_view_set_text_multiplier(WEBKIT_WEB_VIEW(webView), zoom - 0.1);*/
}

void on_action_zoom_normal_activate(GtkAction* action, CBrowser* browser)
{
    //webkit_web_view_set_text_multiplier(WEBKIT_WEB_VIEW(get_nth_webView(-1, browser)), 1);
}

void on_action_source_view_activate(GtkAction* action, CBrowser* browser)
{
    /*GtkWidget* webView = get_nth_webView(-1, browser);
    gchar* source = webkit_web_view_copy_source(WEBKIT_WEB_VIEW(webView));
    webkit_web_view_load_html_string(WEBKIT_WEB_VIEW(webView), source, "");
    g_free(source);*/
}

void on_action_back_activate(GtkAction* action, CBrowser* browser)
{
    webkit_web_view_go_backward(WEBKIT_WEB_VIEW(get_nth_webView(-1, browser)));
}

void on_action_forward_activate(GtkAction* action, CBrowser* browser)
{
    webkit_web_view_go_forward(WEBKIT_WEB_VIEW(get_nth_webView(-1, browser)));
}

void on_action_home_activate(GtkAction* action, CBrowser* browser)
{
    webView_open(get_nth_webView(-1, browser), config->homepage);
}

void on_action_location_activate(GtkAction* action, CBrowser* browser)
{
    if(GTK_WIDGET_VISIBLE(browser->navibar))
        gtk_widget_grab_focus(browser->location);
    else
    {
        // TODO: We should offer all of the toolbar location's features here
        GtkWidget* dialog;
        dialog = gtk_dialog_new_with_buttons("Open location"
            , GTK_WINDOW(browser->window)
            , GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR
            , GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL
            , GTK_STOCK_JUMP_TO, GTK_RESPONSE_ACCEPT
            , NULL);
        gtk_window_set_icon_name(GTK_WINDOW(dialog), GTK_STOCK_JUMP_TO);
        gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
        gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 5);
        GtkWidget* hbox = gtk_hbox_new(FALSE, 8);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
        GtkWidget* label = gtk_label_new_with_mnemonic("_Location:");
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        GtkWidget* entry = gtk_entry_new();
        gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
        gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);
        gtk_widget_show_all(hbox);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
        if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
        {
            gtk_entry_set_text(GTK_ENTRY(browser->location)
             , gtk_entry_get_text(GTK_ENTRY(entry)));
            GdkEventKey event;
            event.keyval = GDK_Return;
            on_location_key_down(browser->location, &event, browser);
        }
        gtk_widget_destroy(dialog);
    }
}

void on_action_webSearch_activate(GtkAction* action, CBrowser* browser)
{
    if(GTK_WIDGET_VISIBLE(browser->webSearch)
     && GTK_WIDGET_VISIBLE(browser->navibar))
        gtk_widget_grab_focus(browser->webSearch);
    else
    {
        // TODO: We should offer all of the toolbar search's features here
        GtkWidget* dialog = gtk_dialog_new_with_buttons("Web search"
            , GTK_WINDOW(browser->window)
            , GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR
            , GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL
            , GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT
            , NULL);
        gtk_window_set_icon_name(GTK_WINDOW(dialog), GTK_STOCK_FIND);
        gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
        gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 5);
        GtkWidget* hbox = gtk_hbox_new(FALSE, 8);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
        GtkWidget* label = gtk_label_new_with_mnemonic("_Location:");
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        GtkWidget* entry = gtk_entry_new();
        gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
        gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);
        gtk_widget_show_all(hbox);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
        if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
        {
            gtk_entry_set_text(GTK_ENTRY(browser->webSearch)
             , gtk_entry_get_text(GTK_ENTRY(entry)));
            on_webSearch_activate(browser->webSearch, browser);
        }
        gtk_widget_destroy(dialog);
    }
}

void on_menu_tabsClosed_activate(GtkWidget* widget, CBrowser* browser)
{
    GtkWidget* menu = gtk_menu_new();
    guint n = xbel_folder_get_n_items(tabtrash);
    GtkWidget* menuitem;
    guint i;
    for(i = 0; i < n; i++)
    {
        XbelItem* item = xbel_folder_get_nth_item(tabtrash, i);
        const gchar* title = xbel_item_get_title(item);
        const gchar* uri = xbel_bookmark_get_href(item);
        menuitem = gtk_image_menu_item_new_with_label(title ? title : uri);
        // FIXME: Get the real icon
        GtkWidget* icon = gtk_image_new_from_stock(GTK_STOCK_FILE, GTK_ICON_SIZE_MENU);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), icon);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
        g_object_set_data(G_OBJECT(menuitem), "XbelItem", item);
        g_signal_connect(menuitem, "activate", G_CALLBACK(on_menu_tabsClosed_item_activate), browser);
        gtk_widget_show(menuitem);
    }

    menuitem = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    gtk_widget_show(menuitem);
    GtkAction* action = gtk_action_group_get_action(
     browser->actiongroup, "TabsClosedClear");
    menuitem = gtk_action_create_menu_item(action);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    gtk_widget_show(menuitem);
    sokoke_widget_popup(widget, GTK_MENU(menu), NULL);
}

void on_menu_tabsClosed_item_activate(GtkWidget* menuitem, CBrowser* browser)
{
    // Create a new webView with an uri which has been closed before
    XbelItem* item = g_object_get_data(G_OBJECT(menuitem), "XbelItem");
    const gchar* uri = xbel_bookmark_get_href(item);
    CBrowser* curBrowser = browser_new(browser);
    webView_open(curBrowser->webView, uri);
    xbel_folder_remove_item(tabtrash, item);
    xbel_item_free(item);
    update_browser_actions(curBrowser);
}

void on_action_tabsClosed_undo_activate(GtkAction* action, CBrowser* browser)
{
    // Open the most recent tabtrash item
    XbelItem* item = xbel_folder_get_nth_item(tabtrash, 0);
    const gchar* uri = xbel_bookmark_get_href(item);
    CBrowser* curBrowser = browser_new(browser);
    webView_open(curBrowser->webView, uri);
    xbel_folder_remove_item(tabtrash, item);
    xbel_item_free(item);
    update_browser_actions(curBrowser);
}

void on_action_tabsClosed_clear_activate(GtkAction* action, CBrowser* browser)
{
    // Clear the closed tabs list
    xbel_item_free(tabtrash);
    tabtrash = xbel_folder_new();
    update_browser_actions(browser);
}

void on_action_link_tab_new_activate(GtkAction* action, CBrowser* browser)
{
    CBrowser* curBrowser = browser_new(browser);
    webView_open(curBrowser->webView, browser->elementUri);
}

void on_action_link_tab_current_activate(GtkAction* action, CBrowser* browser)
{
    webView_open(get_nth_webView(-1, browser), browser->elementUri);
}

void on_action_link_window_new_activate(GtkAction* action, CBrowser* browser)
{
    CBrowser* curBrowser = browser_new(NULL);
    webView_open(curBrowser->webView, browser->elementUri);
}

void on_action_link_saveWith_activate(GtkAction* action, CBrowser* browser)
{
    spawn_protocol_command("download", browser->elementUri);
}

void on_action_link_copy_activate(GtkAction* action, CBrowser* browser)
{
    GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, browser->elementUri, -1);
}

static void browser_editBookmark_dialog_new(XbelItem* bookmark, CBrowser* browser)
{
    gboolean newBookmark = !bookmark;
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        newBookmark ? "New bookmark" : "Edit bookmark"
        , GTK_WINDOW(browser->window)
        , GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR
        , GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL
        , newBookmark ? GTK_STOCK_ADD : GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT
        , NULL);
    gtk_window_set_icon_name(GTK_WINDOW(dialog)
     , newBookmark ? GTK_STOCK_ADD : GTK_STOCK_REMOVE);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
    gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 5);
    GtkSizeGroup* sizegroup =  gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    if(newBookmark)
        bookmark = xbel_bookmark_new();

    GtkWidget* hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    GtkWidget* label = gtk_label_new_with_mnemonic("_Title:");
    gtk_size_group_add_widget(sizegroup, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    GtkWidget* entry_title = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry_title), TRUE);
    if(!newBookmark)
        gtk_entry_set_text(GTK_ENTRY(entry_title), xbel_item_get_title(bookmark));
    gtk_box_pack_start(GTK_BOX(hbox), entry_title, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);
    gtk_widget_show_all(hbox);
    
    hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    label = gtk_label_new_with_mnemonic("_Description:");
    gtk_size_group_add_widget(sizegroup, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    GtkWidget* entry_desc = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry_desc), TRUE);
    if(!newBookmark)
        gtk_entry_set_text(GTK_ENTRY(entry_desc), xbel_item_get_desc(bookmark));
    gtk_box_pack_start(GTK_BOX(hbox), entry_desc, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);
    gtk_widget_show_all(hbox);
    
    hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    label = gtk_label_new_with_mnemonic("_Uri:");
    gtk_size_group_add_widget(sizegroup, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    GtkWidget* entry_uri = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry_uri), TRUE);
    if(!newBookmark)
        gtk_entry_set_text(GTK_ENTRY(entry_uri), xbel_bookmark_get_href(bookmark));
    gtk_box_pack_start(GTK_BOX(hbox), entry_uri, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);
    gtk_widget_show_all(hbox);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        xbel_item_set_title(bookmark, gtk_entry_get_text(GTK_ENTRY(entry_title)));
        xbel_item_set_desc(bookmark, gtk_entry_get_text(GTK_ENTRY(entry_desc)));
        xbel_bookmark_set_href(bookmark, gtk_entry_get_text(GTK_ENTRY(entry_uri)));

        // FIXME: We want to choose a folder
        if(newBookmark)
            xbel_folder_append_item(bookmarks, bookmark);
    }
    gtk_widget_destroy(dialog);
}

static void create_bookmark_menu(XbelItem*, GtkWidget*, CBrowser*);

static void on_bookmark_menu_folder_activate(GtkWidget* menuitem, CBrowser* browser)
{
    GtkWidget* menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menuitem));
    gtk_container_foreach(GTK_CONTAINER(menu), (GtkCallback)gtk_widget_destroy, NULL);//...
    XbelItem* folder = (XbelItem*)g_object_get_data(G_OBJECT(menuitem), "XbelItem");
    create_bookmark_menu(folder, menu, browser);
    // Remove all menuitems when the menu is hidden.
    // FIXME: We really *want* the line below, but it won't work like that
    //g_signal_connect_after(menu, "hide", G_CALLBACK(gtk_container_foreach), gtk_widget_destroy);
    gtk_widget_show(menuitem);
}

static void on_bookmark_toolbar_folder_activate(GtkToolItem* toolitem, CBrowser* browser)
{
    GtkWidget* menu = gtk_menu_new();
    XbelItem* folder = (XbelItem*)g_object_get_data(G_OBJECT(toolitem), "XbelItem");
    create_bookmark_menu(folder, menu, browser);
    // Remove all menuitems when the menu is hidden.
    // FIXME: We really *should* run the line below, but it won't work like that
    //g_signal_connect(menu, "hide", G_CALLBACK(gtk_container_foreach), gtk_widget_destroy);
    sokoke_widget_popup(GTK_WIDGET(toolitem), GTK_MENU(menu), NULL);
}

void on_menu_bookmarks_item_activate(GtkWidget* widget, CBrowser* browser)
{
    XbelItem* item = (XbelItem*)g_object_get_data(G_OBJECT(widget), "XbelItem");
    webView_open(get_nth_webView(-1, browser), xbel_bookmark_get_href(item));
}

static void create_bookmark_menu(XbelItem* folder, GtkWidget* menu, CBrowser* browser)
{
    guint n = xbel_folder_get_n_items(folder);
    guint i;
    for(i = 0; i < n; i++)
    {
        XbelItem* item = xbel_folder_get_nth_item(folder, i);
        const gchar* title = xbel_item_is_separator(item) ? "" : xbel_item_get_title(item);
        //const gchar* desc = xbel_item_is_separator(item) ? "" : xbel_item_get_desc(item);
        GtkWidget* menuitem = NULL;
        switch(xbel_item_get_kind(item))
        {
        case XBEL_ITEM_FOLDER:
            // FIXME: what about xbel_folder_is_folded?
            menuitem = gtk_image_menu_item_new_with_label(title);
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem)
             , gtk_image_new_from_stock(GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU));
            GtkWidget* _menu = gtk_menu_new();
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), _menu);
            g_signal_connect(menuitem, "activate"
             , G_CALLBACK(on_bookmark_menu_folder_activate), browser);
            g_object_set_data(G_OBJECT(menuitem), "XbelItem", item);
            break;
        case XBEL_ITEM_BOOKMARK:
            menuitem = menu_item_new(title, STOCK_BOOKMARK
             , G_CALLBACK(on_menu_bookmarks_item_activate), TRUE, browser);
            g_object_set_data(G_OBJECT(menuitem), "XbelItem", item);
            break;
        case XBEL_ITEM_SEPARATOR:
            menuitem = gtk_separator_menu_item_new();
            break;
        default:
            g_warning("Unknown xbel item kind");
         }
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
         gtk_widget_show(menuitem);
    }
}

void on_action_bookmark_new_activate(GtkAction* action, CBrowser* browser)
{
    browser_editBookmark_dialog_new(NULL, browser);
}

void on_action_manageSearchEngines_activate(GtkAction* action, CBrowser* browser)
{
    // Show the Manage search engines dialog. Create it if necessary.
    static GtkWidget* dialog;
    if(GTK_IS_DIALOG(dialog))
        gtk_window_present(GTK_WINDOW(dialog));
    else
    {
        dialog = webSearch_manageSearchEngines_dialog_new(browser);
        gtk_widget_show(dialog);
    }
}

void on_action_tab_previous_activate(GtkAction* action, CBrowser* browser)
{
    gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(browser->webViews));
    gtk_notebook_set_current_page(GTK_NOTEBOOK(browser->webViews), page - 1);
}

void on_action_tab_next_activate(GtkAction* action, CBrowser* browser)
{
    // Advance one tab or jump to the first one if we are at the last one
    gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(browser->webViews));
    if(page == gtk_notebook_get_n_pages(GTK_NOTEBOOK(browser->webViews)) - 1)
        page = -1;
    gtk_notebook_set_current_page(GTK_NOTEBOOK(browser->webViews), page + 1);
}

void on_window_menu_item_activate(GtkImageMenuItem* widget, CBrowser* browser)
{
    gint page = get_webView_index(browser->webView, browser);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(browser->webViews), page);
}

void on_action_about_activate(GtkAction* action, CBrowser* browser)
{
    gtk_show_about_dialog(GTK_WINDOW(browser->window)
        , "logo-icon-name", gtk_window_get_icon_name(GTK_WINDOW(browser->window))
        , "name", PACKAGE_NAME
        , "version", PACKAGE_VERSION
        , "comments", "A lightweight web browser."
        , "copyright", "Copyright © 2007 Christian Dywan"
        , "website", "http://software.twotoasts.de"
        , "authors", credits_authors
        , "documenters", credits_documenters
        , "artists", credits_artists
        , "license", license
        , "wrap-license", TRUE
        //, "translator-credits", _("translator-credits")
        , NULL);
}

gboolean on_location_key_down(GtkWidget* widget, GdkEventKey* event, CBrowser* browser)
{
    switch(event->keyval)
    {
    case GDK_Return:
    {
        const gchar* uri = gtk_entry_get_text(GTK_ENTRY(widget));
        if(uri)
        {
            gchar* newUri = magic_uri(uri, TRUE);
            // TODO: Use newUrl intermediately when completion is better
            /* TODO Completion should be generated from history, that is
                    the uri as well as the title. */
            entry_completion_append(GTK_ENTRY(widget), uri);
            webView_open(get_nth_webView(-1, browser), newUri);
            g_free(newUri);
        }
        return TRUE;
    }
    case GDK_Escape:
    {
        GtkWidget* webView = get_nth_webView(-1, browser);
        WebKitWebFrame* frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(webView));
        const gchar* uri = webkit_web_frame_get_location(frame);
        if(uri && *uri)
            gtk_entry_set_text(GTK_ENTRY(widget), uri);
        return TRUE;
    }
    }
    return FALSE;
}

void on_location_changed(GtkWidget* widget, CBrowser* browser)
{
    // Preserve changes to the uri
    /*const gchar* newUri = gtk_entry_get_text(GTK_ENTRY(widget));
    xbel_bookmark_set_href(browser->sessionItem, newUri);*/
    // FIXME: If we want this feature, this is the wrong approach
}

void on_action_panels_activate(GtkToggleAction* action, CBrowser* browser)
{
    config->panelShow = gtk_toggle_action_get_active(action);
    sokoke_widget_set_visible(browser->panels, config->panelShow);
}

void on_action_panel_item_activate(GtkRadioAction* action
 , GtkRadioAction* currentAction, CBrowser* browser)
{
    g_return_if_fail(GTK_IS_ACTION(action));
    // TODO: Activating again should hide the contents; how?
    //gint iValue; gint iCurrentValue;
    //g_object_get(G_OBJECT(action), "value", &iValue, NULL);
    //g_object_get(G_OBJECT(currentAction), "value", &iCurrentValue, NULL);
    //GtkWidget* parent = gtk_widget_get_parent(browser->panels_notebook);
    //sokoke_widget_set_visible(parent, iCurrentValue == iValue);
    /*gtk_paned_set_position(GTK_PANED(gtk_widget_get_parent(browser->panels))
     , iCurrentValue == iValue ? config->iPanelPos : 0);*/
    config->panelActive = gtk_radio_action_get_current_value(action);
    gint page = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(currentAction), "iPage"));
    gtk_notebook_set_current_page(GTK_NOTEBOOK(browser->panels_notebook), page);
    // This is a special case where activation was not user requested.
    if(!GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), "once-silent")))
    {
        config->panelShow = TRUE;
        gtk_widget_show(browser->panels);
    }
    else
        g_object_set_data(G_OBJECT(action), "once-silent", NULL);
}

void on_action_openInPanel_activate(GtkAction* action, CBrowser* browser)
{
    GtkWidget* webView = get_nth_webView(-1, browser);
    g_free(config->panelPageholder);
    WebKitWebFrame* frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(webView));
    const gchar* uri = webkit_web_frame_get_location(frame);
    config->panelPageholder = g_strdup(uri);
    GtkAction* action_pageholder =
     gtk_action_group_get_action(browser->actiongroup, "PanelPageholder");
    gint value; g_object_get(G_OBJECT(action_pageholder), "value", &value, NULL);
    sokoke_radio_action_set_current_value(GTK_RADIO_ACTION(action_pageholder), value);
    gtk_widget_show(browser->panels);
    webView_open(browser->panel_pageholder, config->panelPageholder);
}


static void on_panels_notify_position(GObject* object, GParamSpec* arg1
 , CBrowser* browser)
{
    config->winPanelPos = gtk_paned_get_position(GTK_PANED(object));
}

void on_panels_button_close_clicked(GtkWidget* widget, CBrowser* browser)
{
    config->panelShow = FALSE;
    gtk_widget_hide(browser->panels);
}

gboolean on_notebook_tab_mouse_up(GtkWidget* widget, GdkEventButton* event
 , CBrowser* browser)
{
    if(event->button == 1 && event->type == GDK_2BUTTON_PRESS)
    {
        // Toggle the label visibility on double click
        GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
        GList* children = gtk_container_get_children(GTK_CONTAINER(child));
        child = (GtkWidget*)g_list_nth_data(children, 1);
        gboolean visible = gtk_widget_get_child_visible(GTK_WIDGET(child));
        gtk_widget_set_child_visible(GTK_WIDGET(child), !visible);
        gint a, b; sokoke_widget_get_text_size(browser->webView_name, "M", &a, &b);
        gtk_widget_set_size_request(child, !visible
         ? a * config->tabSize : 0, !visible ? -1 : 0);
        g_list_free(children);
        return TRUE;
    }
    else if(event->button == 2)
    {
        // Close the webView on middle click
        webView_close(browser->webView, browser);
        return TRUE;
    }

    return FALSE;
}

gboolean on_notebook_tab_close_clicked(GtkWidget* widget, CBrowser* browser)
{
    webView_close(browser->webView, browser);
    return TRUE;
}

void on_notebook_switch_page(GtkWidget* widget, GtkNotebookPage* page
 , guint page_num, CBrowser* browser)
{
    GtkWidget* webView = get_nth_webView(page_num, browser);
    browser = get_browser_from_webView(webView);
    const gchar* uri = xbel_bookmark_get_href(browser->sessionItem);
    gtk_entry_set_text(GTK_ENTRY(browser->location), uri);
    const gchar* title = xbel_item_get_title(browser->sessionItem);
    const gchar* effectiveTitle = title ? title : uri;
    gchar* windowTitle = g_strconcat(effectiveTitle, " - ", PACKAGE_NAME, NULL);
    gtk_window_set_title(GTK_WINDOW(browser->window), windowTitle);
    g_free(windowTitle);
    update_favicon(browser);
    update_security(browser);
    update_gui_state(browser);
    update_statusbar_text(browser);
    update_feeds(browser);
    update_search_engines(browser);
}

void on_findbox_button_close_clicked(GtkWidget* widget, CBrowser* browser)
{
    gtk_widget_hide(browser->findbox);
}

static gboolean on_window_configure(GtkWidget* widget, GdkEventConfigure* event
 , CBrowser* browser)
{
    gtk_window_get_position(GTK_WINDOW(browser->window)
     , &config->winLeft, &config->winTop);
    return FALSE;
}

static void on_window_size_allocate(GtkWidget* widget, GtkAllocation* allocation
 , CBrowser* browser)
{
    config->winWidth = allocation->width;
    config->winHeight = allocation->height;
}

gboolean on_window_destroy(GtkWidget* widget, GdkEvent* event, CBrowser* browser)
{
    gboolean proceed = TRUE;
    // TODO: What if there are multiple windows?
    // TODO: Smart dialog, à la 'Session?: Save, Discard, Cancel'
    // TODO: Pref startup: session, ask, homepage, blank <-- ask
    // TODO: Pref quit: session, ask, none <-- ask

    if(0 /*g_list_length(browser_list) > 1*/)
    {
        GtkDialog* dialog;
        dialog = GTK_DIALOG(gtk_message_dialog_new(GTK_WINDOW(browser->window)
         , GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO
         , "There is more than one tab open. Do you want to close anyway?"));
        gtk_window_set_title(GTK_WINDOW(dialog), PACKAGE_NAME);
        gtk_dialog_set_default_response(dialog, GTK_RESPONSE_YES);
        proceed = gtk_dialog_run(dialog) == GTK_RESPONSE_YES;
        gtk_widget_destroy(GTK_WIDGET(dialog));
    }
    return !proceed;
}

// -- Browser creation begins here

CBrowser* browser_new(CBrowser* oldBrowser)
{
    CBrowser* browser = g_new0(CBrowser, 1);
    browsers = g_list_prepend(browsers, browser);
    browser->sessionItem = xbel_bookmark_new();
    xbel_item_set_title(browser->sessionItem, "about:blank");
    xbel_folder_append_item(session, browser->sessionItem);

    GtkWidget* scrolled;

    if(!oldBrowser)
    {

    GtkWidget* label; GtkWidget* hbox;

    // Setup the window metrics
    browser->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GdkScreen* screen = gtk_window_get_screen(GTK_WINDOW(browser->window));
    const gint defaultWidth = (gint)gdk_screen_get_width(screen) / 1.7;
    const gint defaultHeight = (gint)gdk_screen_get_height(screen) / 1.7;
    if(config->rememberWinMetrics)
    {
        if(!config->winWidth && !config->winHeight)
        {
            config->winWidth = defaultWidth;
            config->winHeight = defaultWidth;
        }
        if(config->winLeft && config->winTop)
            gtk_window_move(GTK_WINDOW(browser->window)
             , config->winLeft, config->winTop);
        gtk_window_set_default_size(GTK_WINDOW(browser->window)
         , config->winWidth, config->winHeight);
    }
    else
        gtk_window_set_default_size(GTK_WINDOW(browser->window)
         , defaultWidth, defaultHeight);
    g_signal_connect(browser->window, "configure-event"
     , G_CALLBACK(on_window_configure), browser);
    g_signal_connect(browser->window, "size-allocate"
     , G_CALLBACK(on_window_size_allocate), browser);
    // FIXME: Use custom program icon
    gtk_window_set_icon_name(GTK_WINDOW(browser->window), "web-browser");
    gtk_window_set_title(GTK_WINDOW(browser->window), g_get_application_name());
    gtk_window_add_accel_group(GTK_WINDOW(browser->window), accel_group);
    g_signal_connect(browser->window, "delete-event"
     , G_CALLBACK(on_window_destroy), browser);
    GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(browser->window), vbox);
    gtk_widget_show(vbox);

    // Let us see some ui manager magic
    browser->actiongroup = gtk_action_group_new("Browser");
    gtk_action_group_add_actions(browser->actiongroup, entries, entries_n, browser);
    gtk_action_group_add_toggle_actions(browser->actiongroup
     , toggle_entries, toggle_entries_n, browser);
    gtk_action_group_add_radio_actions(browser->actiongroup
     , refreshevery_entries, refreshevery_entries_n
     , 300, NULL/*G_CALLBACK(activate_refreshevery_period_action)*/, browser);
    gtk_action_group_add_radio_actions(browser->actiongroup
     , panel_entries, panel_entries_n, -1
     , G_CALLBACK(on_action_panel_item_activate), browser);
    GtkUIManager* ui_manager = gtk_ui_manager_new();
    gtk_ui_manager_insert_action_group(ui_manager, browser->actiongroup, 0);
    gtk_window_add_accel_group(GTK_WINDOW(browser->window)
     , gtk_ui_manager_get_accel_group(ui_manager));

    GError* error = NULL;
    if(!gtk_ui_manager_add_ui_from_string(ui_manager, ui_markup, -1, &error))
    {
        // TODO: Should this be a message dialog? When does this happen?
        g_message("User interface couldn't be created: %s", error->message);
        g_error_free(error);
    }

    GtkAction* action;
    // Make all actions except toplevel menus which lack a callback insensitive
    // This will vanish once all actions are implemented
    guint i;
    for(i = 0; i < entries_n; i++)
    {
        action = gtk_action_group_get_action(browser->actiongroup, entries[i].name);
        gtk_action_set_sensitive(action, entries[i].callback || !entries[i].tooltip);
    }
    for(i = 0; i < toggle_entries_n; i++)
    {
        action = gtk_action_group_get_action(browser->actiongroup
         , toggle_entries[i].name);
        gtk_action_set_sensitive(action, toggle_entries[i].callback != NULL);
    }
    for(i = 0; i < refreshevery_entries_n; i++)
    {
        action = gtk_action_group_get_action(browser->actiongroup
         , refreshevery_entries[i].name);
        gtk_action_set_sensitive(action, FALSE);
    }

    //action_set_active("ToolbarDownloads", config->bToolbarDownloads, browser);

    // Create the menubar
    browser->menubar = gtk_ui_manager_get_widget(ui_manager, "/menubar");
    GtkWidget* menuitem = gtk_menu_item_new();
    gtk_widget_show(menuitem);
    browser->throbber = gtk_image_new_from_stock(GTK_STOCK_EXECUTE, GTK_ICON_SIZE_MENU);
    gtk_widget_show(browser->throbber);
    gtk_container_add(GTK_CONTAINER(menuitem), browser->throbber);
    gtk_widget_set_sensitive(menuitem, FALSE);
    gtk_menu_item_set_right_justified(GTK_MENU_ITEM(menuitem), TRUE);
    gtk_menu_shell_append(GTK_MENU_SHELL(browser->menubar), menuitem);
    gtk_box_pack_start(GTK_BOX(vbox), browser->menubar, FALSE, FALSE, 0);
    menuitem = gtk_ui_manager_get_widget(ui_manager, "/menubar/Go/TabsClosed");
    g_signal_connect(menuitem, "activate"
     , G_CALLBACK(on_menu_tabsClosed_activate), browser);
    browser->menu_bookmarks = gtk_menu_item_get_submenu(
     GTK_MENU_ITEM(gtk_ui_manager_get_widget(ui_manager, "/menubar/Bookmarks")));
    menuitem = gtk_separator_menu_item_new();
    gtk_widget_show(menuitem);
    gtk_menu_shell_append(GTK_MENU_SHELL(browser->menu_bookmarks), menuitem);
    browser->menu_window = gtk_menu_item_get_submenu(
     GTK_MENU_ITEM(gtk_ui_manager_get_widget(ui_manager, "/menubar/Window")));
    menuitem = gtk_separator_menu_item_new();
    gtk_widget_show(menuitem);
    gtk_menu_shell_append(GTK_MENU_SHELL(browser->menu_window), menuitem);
    gtk_widget_show(browser->menubar);
    action_set_sensitive("PrivateBrowsing", FALSE, browser); //...
    action_set_sensitive("WorkOffline", FALSE, browser); //...
    browser->popup_webView = gtk_ui_manager_get_widget(ui_manager, "/popup_webView");
    g_object_ref(browser->popup_webView);
    browser->popup_element = gtk_ui_manager_get_widget(ui_manager, "/popup_element");
    g_object_ref(browser->popup_element);
    browser->popup_editable = gtk_ui_manager_get_widget(ui_manager, "/popup_editable");
    g_object_ref(browser->popup_editable);

    // Create the navigation toolbar
    browser->navibar = gtk_ui_manager_get_widget(ui_manager, "/toolbar_navigation");
    gtk_toolbar_set_style(GTK_TOOLBAR(browser->navibar)
     , config_to_toolbarstyle(config->toolbarStyle));
    g_signal_connect(gtk_settings_get_default(), "notify::gtk-toolbar-style"
     , G_CALLBACK(on_toolbar_navigation_notify_style), browser);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(browser->navibar)
     , config_to_toolbariconsize(config->toolbarSmall));
    gtk_toolbar_set_show_arrow(GTK_TOOLBAR(browser->navibar), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), browser->navibar, FALSE, FALSE, 0);
    browser->newTab = gtk_ui_manager_get_widget(ui_manager, "/toolbar_navigation/TabNew");
    action = gtk_action_group_get_action(browser->actiongroup, "Back");
    g_object_set(action, "is-important", TRUE, NULL);

    // Location entry
    browser->location = sexy_icon_entry_new();
    entry_setup_completion(GTK_ENTRY(browser->location));
    sokoke_entry_set_can_undo(GTK_ENTRY(browser->location), TRUE);
    browser->location_icon = gtk_image_new();
    sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(browser->location)
     , SEXY_ICON_ENTRY_PRIMARY, GTK_IMAGE(browser->location_icon));
    sexy_icon_entry_add_clear_button(SEXY_ICON_ENTRY(browser->location));
    g_signal_connect(browser->location, "key-press-event"
     , G_CALLBACK(on_location_key_down), browser);
    g_signal_connect(browser->location, "changed"
     , G_CALLBACK(on_location_changed), browser);
    GtkToolItem* toolitem = gtk_tool_item_new();
    gtk_tool_item_set_expand(GTK_TOOL_ITEM(toolitem), TRUE);
    gtk_container_add(GTK_CONTAINER(toolitem), browser->location);
    gtk_toolbar_insert(GTK_TOOLBAR(browser->navibar), toolitem, -1);

    // Search entry
    browser->webSearch = sexy_icon_entry_new();
    sexy_icon_entry_set_icon_highlight(SEXY_ICON_ENTRY(browser->webSearch)
     , SEXY_ICON_ENTRY_PRIMARY, TRUE);
    // TODO: Make this actively resizable or enlarge to fit contents?
    // FIXME: The interface is somewhat awkward and ought to be rethought
    // TODO: Display "show in context menu" search engines as "completion actions"
    entry_setup_completion(GTK_ENTRY(browser->webSearch));
    sokoke_entry_set_can_undo(GTK_ENTRY(browser->webSearch), TRUE);
    update_searchEngine(config->searchEngine, browser);
    g_signal_connect(browser->webSearch, "icon-released"
     , G_CALLBACK(on_webSearch_icon_released), browser);
    g_signal_connect(browser->webSearch, "key-press-event"
     , G_CALLBACK(on_webSearch_key_down), browser);
    g_signal_connect(browser->webSearch, "scroll-event"
     , G_CALLBACK(on_webSearch_scroll), browser);
    g_signal_connect(browser->webSearch, "activate"
     , G_CALLBACK(on_webSearch_activate), browser);
    toolitem = gtk_tool_item_new();
    gtk_container_add(GTK_CONTAINER(toolitem), browser->webSearch);
    gtk_toolbar_insert(GTK_TOOLBAR(browser->navibar), toolitem, -1);
    action = gtk_action_group_get_action(browser->actiongroup, "TabsClosed");
    browser->closedTabs = gtk_action_create_tool_item(action);
    g_signal_connect(browser->closedTabs, "clicked"
     , G_CALLBACK(on_menu_tabsClosed_activate), browser);
    gtk_toolbar_insert(GTK_TOOLBAR(browser->navibar)
     , GTK_TOOL_ITEM(browser->closedTabs), -1);
    sokoke_container_show_children(GTK_CONTAINER(browser->navibar));
    action_set_active("ToolbarNavigation", config->toolbarNavigation, browser);

    // Bookmarkbar
    browser->bookmarkbar = gtk_toolbar_new();
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(browser->bookmarkbar), GTK_ICON_SIZE_MENU);
    gtk_toolbar_set_style(GTK_TOOLBAR(browser->bookmarkbar), GTK_TOOLBAR_BOTH_HORIZ);
    create_bookmark_menu(bookmarks, browser->menu_bookmarks, browser);
    for(i = 0; i < xbel_folder_get_n_items(bookmarks); i++)
    {
        XbelItem* item = xbel_folder_get_nth_item(bookmarks, i);
        const gchar* title = xbel_item_is_separator(item)
         ? "" : xbel_item_get_title(item);
        const gchar* desc = xbel_item_is_separator(item)
         ? "" : xbel_item_get_desc(item);
        switch(xbel_item_get_kind(item))
        {
        case XBEL_ITEM_FOLDER:
            toolitem = tool_button_new(title, GTK_STOCK_DIRECTORY, TRUE, TRUE
             , G_CALLBACK(on_bookmark_toolbar_folder_activate), desc, browser);
            g_object_set_data(G_OBJECT(toolitem), "XbelItem", item);
            break;
        case XBEL_ITEM_BOOKMARK:
            toolitem = tool_button_new(title, STOCK_BOOKMARK, TRUE, TRUE
             , G_CALLBACK(on_menu_bookmarks_item_activate), desc, browser);
            g_object_set_data(G_OBJECT(toolitem), "XbelItem", item);
            break;
        case XBEL_ITEM_SEPARATOR:
            toolitem = gtk_separator_tool_item_new();
            break;
        default:
            g_warning("Unknown item kind");
        }
        gtk_toolbar_insert(GTK_TOOLBAR(browser->bookmarkbar), toolitem, -1);
    }
    sokoke_container_show_children(GTK_CONTAINER(browser->bookmarkbar));
    gtk_box_pack_start(GTK_BOX(vbox), browser->bookmarkbar, FALSE, FALSE, 0);
    action_set_active("ToolbarBookmarks", config->toolbarBookmarks, browser);

    // Superuser warning
    if((hbox = sokoke_superuser_warning_new()))
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    // Create the panels
    GtkWidget* hpaned = gtk_hpaned_new();
    gtk_paned_set_position(GTK_PANED(hpaned), config->winPanelPos);
    g_signal_connect(hpaned, "notify::position"
     , G_CALLBACK(on_panels_notify_position), browser);
    gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);
    gtk_widget_show(hpaned);

    browser->panels = gtk_hbox_new(FALSE, 0);
    gtk_paned_pack1(GTK_PANED(hpaned), browser->panels, FALSE, FALSE);
    sokoke_widget_set_visible(browser->panels, config->panelShow);

    // Create the panel toolbar
    GtkWidget* panelbar = gtk_ui_manager_get_widget(ui_manager, "/toolbar_panels");
    gtk_toolbar_set_style(GTK_TOOLBAR(panelbar), GTK_TOOLBAR_BOTH);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(panelbar), GTK_ICON_SIZE_BUTTON);
    gtk_toolbar_set_orientation(GTK_TOOLBAR(panelbar), GTK_ORIENTATION_VERTICAL); 
    gtk_box_pack_start(GTK_BOX(browser->panels), panelbar, FALSE, FALSE, 0);
    action_set_active("Panels", config->panelShow, browser);
    g_object_unref(ui_manager);

    GtkWidget* cbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(browser->panels), cbox, TRUE, TRUE, 0);
    gtk_widget_show(cbox);

    // Panels titlebar
    GtkWidget* labelbar = gtk_toolbar_new();
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(labelbar), GTK_ICON_SIZE_MENU);
    gtk_toolbar_set_style(GTK_TOOLBAR(labelbar), GTK_TOOLBAR_ICONS);
    toolitem = gtk_tool_item_new();
    gtk_tool_item_set_expand(toolitem, TRUE);
    label = gtk_label_new_with_mnemonic("_Panels");
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_container_add(GTK_CONTAINER(toolitem), label);
    gtk_container_set_border_width(GTK_CONTAINER(toolitem), 6);
    gtk_toolbar_insert(GTK_TOOLBAR(labelbar), toolitem, -1);
    // TODO: Does 'goto top' actually indicate 'detach'?
    toolitem = tool_button_new(NULL, GTK_STOCK_GOTO_TOP, FALSE, TRUE
     , NULL/*G_CALLBACK(on_panels_button_float_clicked)*/, "Detach panel", browser);
    gtk_toolbar_insert(GTK_TOOLBAR(labelbar), toolitem, -1);
    toolitem = tool_button_new(NULL, GTK_STOCK_CLOSE, FALSE, TRUE
     , G_CALLBACK(on_panels_button_close_clicked), "Close panel", browser);
    gtk_toolbar_insert(GTK_TOOLBAR(labelbar), toolitem, -1);
    gtk_box_pack_start(GTK_BOX(cbox), labelbar, FALSE, FALSE, 0);
    gtk_widget_show_all(labelbar);

    // Notebook, containing all panels
    browser->panels_notebook = gtk_notebook_new();
    gtk_notebook_set_show_border(GTK_NOTEBOOK(browser->panels_notebook), FALSE);
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(browser->panels_notebook), FALSE);
      gint page;
      // Dummy: This is the "fallback" panel for now
      page = gtk_notebook_append_page(GTK_NOTEBOOK(browser->panels_notebook)
       , gtk_label_new("empty"), NULL);
      // Pageholder
      browser->panel_pageholder = webView_new(&scrolled);
      page = gtk_notebook_append_page(GTK_NOTEBOOK(browser->panels_notebook)
       , scrolled, NULL);
      //webView_load_from_uri(browser->panel_pageholder, config->panelPageholder);
      action = gtk_action_group_get_action(browser->actiongroup, "PanelPageholder");
      g_object_set_data(G_OBJECT(action), "iPage", GINT_TO_POINTER(page));
    GtkWidget* frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(frame), browser->panels_notebook);
    gtk_box_pack_start(GTK_BOX(cbox), frame, TRUE, TRUE, 0);
    gtk_widget_show_all(gtk_widget_get_parent(browser->panels_notebook));
    action = gtk_action_group_get_action(browser->actiongroup, "PanelDownloads");
    g_object_set_data(G_OBJECT(action), "once-silent", GINT_TO_POINTER(1));
    sokoke_radio_action_set_current_value(GTK_RADIO_ACTION(action), config->panelActive);
    sokoke_widget_set_visible(browser->panels, config->panelShow);

    // Notebook, containing all webViews
    browser->webViews = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(browser->webViews), TRUE);
    #if GTK_CHECK_VERSION(2, 10, 0)
    //gtk_notebook_set_group_id(GTK_NOTEBOOK(browser->webViews), 0);
    #endif
    gtk_paned_pack2(GTK_PANED(hpaned), browser->webViews, FALSE, FALSE);
    gtk_widget_show(browser->webViews);

    // Incremental findbar
    browser->findbox = gtk_toolbar_new();
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(browser->findbox), GTK_ICON_SIZE_MENU);
    gtk_toolbar_set_style(GTK_TOOLBAR(browser->findbox), GTK_TOOLBAR_BOTH_HORIZ);
    toolitem = gtk_tool_item_new();
    gtk_container_set_border_width(GTK_CONTAINER(toolitem), 6);
    gtk_container_add(GTK_CONTAINER(toolitem)
     , gtk_label_new_with_mnemonic("_Inline find:"));
    gtk_toolbar_insert(GTK_TOOLBAR(browser->findbox), toolitem, -1);
    browser->findbox_text = sexy_icon_entry_new();
    GtkWidget* icon = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
    sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(browser->findbox_text)
     , SEXY_ICON_ENTRY_PRIMARY, GTK_IMAGE(icon));
    sexy_icon_entry_add_clear_button(SEXY_ICON_ENTRY(browser->findbox_text));
    sokoke_entry_set_can_undo(GTK_ENTRY(browser->findbox_text), TRUE);
    g_signal_connect(browser->findbox_text, "activate"
     , G_CALLBACK(on_action_find_next_activate), browser);
    toolitem = gtk_tool_item_new();
    gtk_container_add(GTK_CONTAINER(toolitem), browser->findbox_text);
    gtk_tool_item_set_expand(GTK_TOOL_ITEM(toolitem), TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(browser->findbox), toolitem, -1);
    toolitem = tool_button_new(NULL, GTK_STOCK_GO_BACK, TRUE, TRUE
     , G_CALLBACK(on_action_find_previous_activate), NULL, browser);
    gtk_toolbar_insert(GTK_TOOLBAR(browser->findbox), toolitem, -1);
    toolitem = tool_button_new(NULL, GTK_STOCK_GO_FORWARD, TRUE, TRUE
     , G_CALLBACK(on_action_find_next_activate), NULL, browser);
    gtk_toolbar_insert(GTK_TOOLBAR(browser->findbox), toolitem, -1);
    browser->findbox_case = gtk_toggle_tool_button_new_from_stock(GTK_STOCK_SPELL_CHECK);
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(browser->findbox_case), "Match Case");
    gtk_tool_item_set_is_important(GTK_TOOL_ITEM(browser->findbox_case), TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(browser->findbox), browser->findbox_case, -1);
    browser->findbox_highlight = gtk_toggle_tool_button_new_from_stock(GTK_STOCK_SELECT_ALL);
    g_signal_connect(browser->findbox_highlight, "toggled"
     , G_CALLBACK(on_findbox_highlight_toggled), browser);
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(browser->findbox_highlight), "Highlight Matches");
    gtk_tool_item_set_is_important(GTK_TOOL_ITEM(browser->findbox_highlight), TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(browser->findbox), browser->findbox_highlight, -1);
    toolitem = gtk_separator_tool_item_new();
    gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(toolitem), FALSE);
    gtk_tool_item_set_expand(GTK_TOOL_ITEM(toolitem), TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(browser->findbox), toolitem, -1);
    toolitem = tool_button_new(NULL, GTK_STOCK_CLOSE, FALSE, TRUE
     , G_CALLBACK(on_findbox_button_close_clicked), "Close Findbar", browser);
    gtk_toolbar_insert(GTK_TOOLBAR(browser->findbox), toolitem, -1);
    sokoke_container_show_children(GTK_CONTAINER(browser->findbox));
    gtk_box_pack_start(GTK_BOX(vbox), browser->findbox, FALSE, FALSE, 0);

    // Statusbar
    // TODO: fix children overlapping statusbar border
    browser->statusbar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), browser->statusbar, FALSE, FALSE, 0);
    browser->progress = gtk_progress_bar_new();
    // Setting the progressbar's height to 1 makes it fit in the statusbar
    gtk_widget_set_size_request(browser->progress, -1, 1);
    gtk_box_pack_start(GTK_BOX(browser->statusbar), browser->progress
     , FALSE, FALSE, 3);
    browser->icon_security = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(browser->statusbar)
     , browser->icon_security, FALSE, FALSE, 0);
    gtk_widget_show(browser->icon_security);
    browser->icon_newsfeed = gtk_image_new_from_icon_name(STOCK_NEWSFEED
     , GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(browser->statusbar)
     , browser->icon_newsfeed, FALSE, FALSE, 0);
    action_set_active("ToolbarStatus", config->toolbarStatus, browser);

    }
    else
    {

    browser->window = oldBrowser->window;
    browser->actiongroup = oldBrowser->actiongroup;
    browser->menubar = oldBrowser->menubar;
    browser->menu_bookmarks = oldBrowser->menu_bookmarks;
    browser->menu_window = oldBrowser->menu_window;
    browser->popup_webView = oldBrowser->popup_webView;
    browser->popup_element = oldBrowser->popup_element;
    browser->popup_editable = oldBrowser->popup_editable;
    browser->throbber = oldBrowser->throbber;
    browser->navibar = oldBrowser->navibar;
    browser->newTab = oldBrowser->newTab;
    browser->location_icon = oldBrowser->location_icon;
    browser->location = oldBrowser->location;
    browser->webSearch = oldBrowser->webSearch;
    browser->closedTabs = oldBrowser->closedTabs;
    browser->bookmarkbar = oldBrowser->bookmarkbar;
    browser->panels = oldBrowser->panels;
    browser->panels_notebook = oldBrowser->panels_notebook;
    browser->panel_pageholder = oldBrowser->panel_pageholder;
    browser->webViews = oldBrowser->webViews;
    browser->findbox = oldBrowser->findbox;
    browser->findbox_case = oldBrowser->findbox_case;
    browser->findbox_highlight = oldBrowser->findbox_highlight;
    browser->statusbar = oldBrowser->statusbar;
    browser->progress = oldBrowser->progress;
    browser->icon_security = oldBrowser->icon_security;
    browser->icon_newsfeed = oldBrowser->icon_newsfeed;

    }

    // Define some default values
    browser->hasMenubar = TRUE;
    browser->hasToolbar = TRUE;
    browser->hasLocation = TRUE;
    browser->hasStatusbar = TRUE;
    browser->elementUri = NULL;
    browser->loadedPercent = -1; // initially "not loading"

    // Add a window menu item
    // TODO: Menu items should be ordered like the notebook tabs
    // TODO: Watch tab reordering in >= gtk 2.10
    browser->webView_menu = menu_item_new("about:blank", GTK_STOCK_FILE
     , G_CALLBACK(on_window_menu_item_activate), TRUE, browser);
    gtk_widget_show(browser->webView_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(browser->menu_window), browser->webView_menu);

    // Create a new tab label
    GtkWidget* eventbox = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(eventbox), FALSE);
    g_signal_connect(eventbox, "button-release-event"
     , G_CALLBACK(on_notebook_tab_mouse_up), browser);
    GtkWidget* hbox = gtk_hbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(eventbox), GTK_WIDGET(hbox));
    browser->webView_icon = gtk_image_new_from_stock(GTK_STOCK_FILE, GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(hbox), browser->webView_icon, FALSE, FALSE, 0);
    browser->webView_name = gtk_label_new(xbel_item_get_title(browser->sessionItem));
    gtk_misc_set_alignment(GTK_MISC(browser->webView_name), 0.0, 0.5);
    // TODO: make the tab initially look "unvisited" until it's focused
    // TODO: gtk's tab scrolling is weird?
    gint w, h;
    sokoke_widget_get_text_size(browser->webView_name, "M", &w, &h);
    gtk_widget_set_size_request(GTK_WIDGET(browser->webView_name)
     , w * config->tabSize, -1);
    gtk_label_set_ellipsize(GTK_LABEL(browser->webView_name), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(hbox), browser->webView_name, FALSE, FALSE, 0);
    browser->webView_close = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(browser->webView_close), GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click(GTK_BUTTON(browser->webView_close), FALSE);
    GtkRcStyle* rcstyle = gtk_rc_style_new();
    rcstyle->xthickness = rcstyle->ythickness = 0;
    gtk_widget_modify_style(browser->webView_close, rcstyle);
    GtkWidget* image = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
    gtk_button_set_image(GTK_BUTTON(browser->webView_close), image);
    gtk_box_pack_start(GTK_BOX(hbox), browser->webView_close, FALSE, FALSE, 0);
    GtkSettings* gtksettings = gtk_settings_get_default();
    gint height;
    gtk_icon_size_lookup_for_settings(gtksettings, GTK_ICON_SIZE_BUTTON, 0, &height);
    gtk_widget_set_size_request(browser->webView_close, -1, height);
    gtk_widget_show_all(GTK_WIDGET(eventbox));
    sokoke_widget_set_visible(browser->webView_close, config->tabClose);
    g_signal_connect(browser->webView_close, "clicked"
     , G_CALLBACK(on_notebook_tab_close_clicked), browser);

    // Create a webView inside a scrolled window
    browser->webView = webView_new(&scrolled);
    gtk_widget_show(GTK_WIDGET(scrolled));
    gtk_widget_show(GTK_WIDGET(browser->webView));
    gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(browser->webViews));
    page = gtk_notebook_insert_page(GTK_NOTEBOOK(browser->webViews)
     , scrolled, GTK_WIDGET(eventbox), page + 1);
    g_signal_connect_after(GTK_OBJECT(browser->webViews), "switch-page"
     , G_CALLBACK(on_notebook_switch_page), browser);
    #if GTK_CHECK_VERSION(2, 10, 0)
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(browser->webViews), scrolled, TRUE);
    gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(browser->webViews), scrolled, TRUE);
    #endif

    // Connect signals
    #define DOC_CONNECT(__sig, __func) g_signal_connect \
     (G_OBJECT(browser->webView), __sig, G_CALLBACK(__func), browser);
    DOC_CONNECT  ("navigation-requested"        , on_webView_navigation_requested)
    DOC_CONNECT  ("title-changed"               , on_webView_title_changed)
    DOC_CONNECT  ("icon-loaded"                 , on_webView_icon_changed)
    DOC_CONNECT  ("load-started"                , on_webView_load_started)
    DOC_CONNECT  ("load-progress-changed"       , on_webView_load_changed)
    DOC_CONNECT  ("load-finished"               , on_webView_load_finished)
    DOC_CONNECT  ("status-bar-text-changed"     , on_webView_status_message)
    DOC_CONNECT  ("hovering-over-link"          , on_webView_link_hover)
    DOC_CONNECT  ("console-message"             , on_webView_console_message)

    // For now we check for "plugins-enabled", in case this build has no properties
    if(g_object_class_find_property(G_OBJECT_GET_CLASS(browser->webView), "plugins-enabled"))
        g_object_set(G_OBJECT(browser->webView)
         , "loads-images-automatically"      , config->loadImagesAutomatically
         , "shrinks-standalone-images-to-fit", config->shrinkImagesToFit
         , "text-areas-are-resizable"        , config->resizableTextAreas
         , "java-script-enabled"             , config->enableJavaScript
         , "plugins-enabled"                 , config->enablePlugins
         , NULL);

    DOC_CONNECT  ("button-release-event"        , on_webView_button_release)
    DOC_CONNECT  ("popup-menu"                  , on_webView_popup);
    DOC_CONNECT  ("scroll-event"                , on_webView_scroll);
    DOC_CONNECT  ("leave-notify-event"          , on_webView_leave)
    DOC_CONNECT  ("destroy"                     , on_webView_destroy)
    #undef DOC_CONNECT

    // Eventually pack and display everything
    sokoke_widget_set_visible(browser->navibar, config->toolbarNavigation);
    sokoke_widget_set_visible(browser->newTab, config->toolbarNewTab);
    sokoke_widget_set_visible(browser->webSearch, config->toolbarWebSearch);
    sokoke_widget_set_visible(browser->closedTabs, config->toolbarClosedTabs);
    sokoke_widget_set_visible(browser->bookmarkbar, config->toolbarBookmarks);
    sokoke_widget_set_visible(browser->statusbar, config->toolbarStatus);
    if(!config->openTabsInTheBackground)
        gtk_notebook_set_current_page(GTK_NOTEBOOK(browser->webViews), page);

    update_browser_actions(browser);
    gtk_widget_show(browser->window);
    gtk_widget_grab_focus(GTK_WIDGET(browser->location));

    return browser;
}
