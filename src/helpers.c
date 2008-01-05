/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "helpers.h"

#include "search.h"
#include "sokoke.h"
#include "../katze/katze.h"

#include <string.h>
#include <webkit.h>

GtkIconTheme* get_icon_theme(GtkWidget* widget)
{
    return gtk_icon_theme_get_for_screen(gtk_widget_get_screen(widget));
}

GtkWidget* menu_item_new(const gchar* text, const gchar* icon
 , GCallback signal, gboolean sensitive, gpointer userdata)
{
    GtkWidget* menuitem;
    if(text)
        menuitem = gtk_image_menu_item_new_with_mnemonic(text);
    else
        menuitem = gtk_image_menu_item_new_from_stock(icon, NULL);
    if(icon)
    {
        GtkWidget* image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU);
        if(gtk_image_get_storage_type(GTK_IMAGE(image)) == GTK_IMAGE_EMPTY)
            image = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_MENU);
        if(gtk_image_get_storage_type(GTK_IMAGE(image)) != GTK_IMAGE_EMPTY)
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
        else
            g_print("Note: The icon %s is not available.", icon);
    }
    if(signal)
        g_signal_connect(menuitem, "activate", signal, userdata);
    gtk_widget_set_sensitive(GTK_WIDGET(menuitem), sensitive && signal);
    return menuitem;
}

GtkToolItem* tool_button_new(const gchar* text, const gchar* icon
 , gboolean important, gboolean sensitive, GCallback signal
 , const gchar* tooltip, gpointer userdata)
{
    GtkToolItem* toolbutton = gtk_tool_button_new(NULL, NULL);
    GtkStockItem stockItem;
    if(gtk_stock_lookup(icon, &stockItem))
        toolbutton = gtk_tool_button_new_from_stock(icon);
    else
    {
        GtkIconTheme* iconTheme = get_icon_theme(GTK_WIDGET(toolbutton));
        if(gtk_icon_theme_has_icon(iconTheme, icon))
            gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(toolbutton), icon);
        else
            gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(toolbutton), GTK_STOCK_MISSING_IMAGE);
    }
    if(text)
        gtk_tool_button_set_label(GTK_TOOL_BUTTON(toolbutton), text);
    if(important)
        gtk_tool_item_set_is_important(toolbutton, TRUE);
    if(signal)
        g_signal_connect(toolbutton, "clicked", signal, userdata);
    gtk_widget_set_sensitive(GTK_WIDGET(toolbutton), sensitive && signal);
    if(tooltip)
        sokoke_tool_item_set_tooltip_text(toolbutton, tooltip);
    return toolbutton;
}

GtkWidget* check_menu_item_new(const gchar* text
 , GCallback signal, gboolean sensitive, gboolean active, CBrowser* browser)
{
    GtkWidget* menuitem = gtk_check_menu_item_new_with_mnemonic(text);
    gtk_widget_set_sensitive(menuitem, sensitive && signal);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), active);
    if(signal)
        g_signal_connect(menuitem, "activate", signal, browser);
    return menuitem;
}

GtkWidget* radio_button_new(GtkRadioButton* radio_button, const gchar* label)
{
    return gtk_radio_button_new_with_mnemonic_from_widget(radio_button, label);
}

void show_error(const gchar* text, const gchar* text2, CBrowser* browser)
{
    GtkWidget* dialog = gtk_message_dialog_new(
     browser ? GTK_WINDOW(browser->window) : NULL
      , 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, text);
    if(text2)
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), text2);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

gboolean spawn_protocol_command(const gchar* protocol, const gchar* res)
{
    const gchar* command = g_datalist_get_data(&config->protocols_commands, protocol);
    if(!command)
        return FALSE;

    // Create an argument vector
    gchar* uriEscaped = g_shell_quote(res);
    gchar* commandReady;
    if(strstr(command, "%s"))
        commandReady = g_strdup_printf(command, uriEscaped);
    else
        commandReady = g_strconcat(command, " ", uriEscaped, NULL);
    gchar** argv; GError* error = NULL;
    if(!g_shell_parse_argv(commandReady, NULL, &argv, &error))
    {
        // FIXME: Should we have a more specific message?
        show_error("Could not run external program.", error->message, NULL);
        g_error_free(error);
        g_free(commandReady); g_free(uriEscaped);
        return FALSE;
    }

    // Try to run the command
    error = NULL;
    gboolean success = g_spawn_async(NULL, argv, NULL
     , (GSpawnFlags)G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD
     , NULL, NULL, NULL, &error);
    g_strfreev(argv);

    if(!success)
    {
        // FIXME: Should we have a more specific message?
        show_error("Could not run external program.", error->message, NULL);
        g_error_free(error);
    }
    g_free(commandReady); g_free(uriEscaped);
    return TRUE;
}

GdkPixbuf* load_web_icon(const gchar* icon, GtkIconSize size, GtkWidget* widget)
{
    g_return_val_if_fail(GTK_IS_WIDGET(widget), NULL);
    GdkPixbuf* pixbuf = NULL;
    if(icon && *icon)
    {
        // TODO: We want to allow http as well, maybe also base64?
        const gchar* iconReady = g_str_has_prefix(icon, "file://") ? &icon[7] : icon;
        GtkStockItem stockItem;
        if(gtk_stock_lookup(icon, &stockItem))
            pixbuf = gtk_widget_render_icon(widget, iconReady, size, NULL);
        else
        {
            gint width; gint height;
            gtk_icon_size_lookup(size, &width, &height);
            pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default()
             , icon, MAX(width, height), GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
        }
        if(!pixbuf)
            pixbuf = gdk_pixbuf_new_from_file_at_size(iconReady, 16, 16, NULL);
    }
    if(!pixbuf)
        pixbuf = gtk_widget_render_icon(widget, GTK_STOCK_FIND, size, NULL);
    return pixbuf;
}

void entry_setup_completion(GtkEntry* entry)
{
    /* TODO: The current behavior works only with the beginning of strings
             But we want to match "localhost" with "loc" and "hos" */
    GtkEntryCompletion* completion = gtk_entry_completion_new();
    gtk_entry_completion_set_model(completion
     , GTK_TREE_MODEL(gtk_list_store_new(1, G_TYPE_STRING)));
    gtk_entry_completion_set_text_column(completion, 0);
    gtk_entry_completion_set_minimum_key_length(completion, 3);
    gtk_entry_set_completion(entry, completion);
    gtk_entry_completion_set_popup_completion(completion, FALSE); //...
}

void entry_completion_append(GtkEntry* entry, const gchar* text)
{
    GtkEntryCompletion* completion = gtk_entry_get_completion(entry);
    GtkTreeModel* completion_store = gtk_entry_completion_get_model(completion);
    GtkTreeIter iter;
    gtk_list_store_insert(GTK_LIST_STORE(completion_store), &iter, 0);
    gtk_list_store_set(GTK_LIST_STORE(completion_store), &iter, 0, text, -1);
}

GtkWidget* get_nth_webView(gint n, CBrowser* browser)
{
    if(n < 0)
        n = gtk_notebook_get_current_page(GTK_NOTEBOOK(browser->webViews));
    GtkWidget* scrolled = gtk_notebook_get_nth_page(GTK_NOTEBOOK(browser->webViews), n);
    return gtk_bin_get_child(GTK_BIN(scrolled));
}

gint get_webView_index(GtkWidget* webView, CBrowser* browser)
{
    GtkWidget* scrolled = gtk_widget_get_parent(webView);
    return gtk_notebook_page_num(GTK_NOTEBOOK(browser->webViews), scrolled);
}

CBrowser* get_browser_from_webView(GtkWidget* webView)
{
    // FIXME: g_list_first
    CBrowser* browser = NULL; GList* item = g_list_first(browsers);
    do
    {
        browser = (CBrowser*)item->data;
        if(browser->webView == webView)
            return browser;
    }
    while((item = g_list_next(item)));
    return NULL;
}

void update_favicon(CBrowser* browser)
{
    if(browser->loadedPercent == -1)
    {
        if(0) //browser->favicon // Has favicon?
        {
            // TODO: use custom icon
            // gtk_image_set_from_file(GTK_IMAGE(browser->icon_page), "image");
        }
        else if(0) // Known mime-type?
        {
            // TODO: Retrieve mime type and load icon; don't forget ftp listings
        }
        else
            katze_throbber_set_static_stock_id(KATZE_THROBBER(browser->webView_icon)
             , GTK_STOCK_FILE);
    }
    katze_throbber_set_animated(KATZE_THROBBER(browser->webView_icon)
     , browser->loadedPercent != -1);
}

void update_security(CBrowser* browser)
{
    const gchar* uri = xbel_bookmark_get_href(browser->sessionItem);
    // TODO: This check is bogus, until webkit tells us how secure a page is
    if(g_str_has_prefix(uri, "https://"))
    {
        // TODO: highlighted entry indicates security, find an alternative
        gtk_widget_modify_base(browser->location, GTK_STATE_NORMAL
         , &browser->location->style->base[GTK_STATE_SELECTED]);
        gtk_widget_modify_text(browser->location, GTK_STATE_NORMAL
         , &browser->location->style->text[GTK_STATE_SELECTED]);
        gtk_image_set_from_stock(GTK_IMAGE(browser->icon_security)
         , GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_MENU);
    }
    else
    {
        gtk_widget_modify_base(browser->location, GTK_STATE_NORMAL, NULL);
        gtk_widget_modify_text(browser->location, GTK_STATE_NORMAL, NULL);
        gtk_image_set_from_stock(GTK_IMAGE(browser->icon_security)
         , GTK_STOCK_INFO, GTK_ICON_SIZE_MENU);
    }
}

void update_visibility(CBrowser* browser, gboolean visibility)
{
    // A tabbed window shouldn't be manipulatable
    if(gtk_notebook_get_n_pages(GTK_NOTEBOOK(browser->webViews)) > 1)
        return;

    // SHOULD SCRIPTS BE ABLE TO HIDE WINDOWS AT ALL?
    if(0 && !visibility)
    {
        gtk_widget_hide(browser->window);
        return;
    }
    else if(!visibility)
        g_print("Window was not hidden.\n");

    sokoke_widget_set_visible(browser->menubar, browser->hasMenubar);
    sokoke_widget_set_visible(browser->navibar, browser->hasToolbar);
    sokoke_widget_set_visible(browser->location, browser->hasLocation);
    sokoke_widget_set_visible(browser->webSearch, browser->hasLocation);
    sokoke_widget_set_visible(browser->statusbar, browser->hasStatusbar);
}

void action_set_active(const gchar* name, gboolean active, CBrowser* browser)
{
    // This shortcut toggles activity state by an action name
    GtkAction* action = gtk_action_group_get_action(browser->actiongroup, name);
    g_return_if_fail(GTK_IS_ACTION(action));
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), active);
}

void action_set_sensitive(const gchar* name, gboolean sensitive, CBrowser* browser)
{
    // This shortcut toggles sensitivity by an action name
    GtkAction* action = gtk_action_group_get_action(browser->actiongroup, name);
    g_return_if_fail(GTK_IS_ACTION(action));
    gtk_action_set_sensitive(action, sensitive);
}

void action_set_visible(const gchar* name, gboolean visible, CBrowser* browser)
{
    // This shortcut toggles visibility by an action name
    GtkAction* action = gtk_action_group_get_action(browser->actiongroup, name);
    g_return_if_fail(GTK_IS_ACTION(action));
    gtk_action_set_visible(action, visible);
}

void update_statusbar(CBrowser* browser)
{
    gtk_statusbar_pop(GTK_STATUSBAR(browser->statusbar), 1);
    gtk_statusbar_push(GTK_STATUSBAR(browser->statusbar), 1
     , browser->statusMessage ? browser->statusMessage : "");
    if(browser->loadedPercent > -1)
    {
        if(browser->loadedPercent > -1)
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(browser->progress)
             , browser->loadedPercent ? browser->loadedPercent / 100.0 : 0);
        else
            gtk_progress_bar_pulse(GTK_PROGRESS_BAR(browser->progress));
        gchar* message = g_strdup_printf("%d%% loaded", browser->loadedPercent);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(browser->progress), message);
        g_free(message);
    }
}

void update_edit_items(CBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus(GTK_WINDOW(browser->window));
    gboolean canCut = FALSE; gboolean canCopy = FALSE; gboolean canPaste = FALSE;
    if(widget && (WEBKIT_IS_WEB_VIEW(widget) || GTK_IS_EDITABLE(widget)))
    {
        gboolean hasSelection = WEBKIT_IS_WEB_VIEW(widget)
         ? webkit_web_view_has_selection(WEBKIT_WEB_VIEW(widget))
         : gtk_editable_get_selection_bounds(GTK_EDITABLE(widget), NULL, NULL);
        canCut = WEBKIT_IS_WEB_VIEW(widget)
         ? webkit_web_view_can_cut_clipboard(WEBKIT_WEB_VIEW(widget))
         : hasSelection && gtk_editable_get_editable(GTK_EDITABLE(widget));
        canCopy = WEBKIT_IS_WEB_VIEW(widget)
         ? webkit_web_view_can_copy_clipboard(WEBKIT_WEB_VIEW(widget))
         : hasSelection;
        canPaste = WEBKIT_IS_WEB_VIEW(widget)
         ? webkit_web_view_can_paste_clipboard(WEBKIT_WEB_VIEW(widget))
         : gtk_editable_get_editable(GTK_EDITABLE(widget));
        action_set_sensitive("SelectAll", TRUE, browser);
    }
    else
        action_set_sensitive("SelectAll", FALSE, browser);
    action_set_sensitive("Cut", canCut, browser);
    action_set_sensitive("Copy", canCopy, browser);
    action_set_sensitive("Paste", canPaste, browser);
    action_set_sensitive("Delete", canCut, browser);
}

void update_gui_state(CBrowser* browser)
{
    GtkWidget* webView = get_nth_webView(-1, browser);
    action_set_sensitive("ZoomIn", FALSE, browser);//webkit_web_view_can_increase_text_size(WEBKIT_WEB_VIEW(webView), browser);
    action_set_sensitive("ZoomOut", FALSE, browser);//webkit_web_view_can_decrease_text_size(WEBKIT_WEB_VIEW(webView)), browser);
    action_set_sensitive("ZoomNormal", FALSE, browser);//webkit_web_view_get_text_size(WEBKIT_WEB_VIEW(webView)) != 1, browser);
    action_set_sensitive("Back", webkit_web_view_can_go_backward(WEBKIT_WEB_VIEW(webView)), browser);
    action_set_sensitive("Forward", webkit_web_view_can_go_forward(WEBKIT_WEB_VIEW(webView)), browser);
    action_set_sensitive("Refresh", browser->loadedPercent == -1, browser);
    action_set_sensitive("Stop", browser->loadedPercent != -1, browser);

    GtkAction* action = gtk_action_group_get_action(browser->actiongroup, "RefreshStop");
    if(browser->loadedPercent == -1)
    {
        gtk_widget_set_sensitive(browser->throbber, FALSE);
        g_object_set(action, "stock-id", GTK_STOCK_REFRESH, NULL);
        g_object_set(action, "tooltip", "Refresh the current page", NULL);
        gtk_widget_hide(browser->progress);
    }
    else
    {
        gtk_widget_set_sensitive(browser->throbber, TRUE);
        g_object_set(action, "stock-id", GTK_STOCK_STOP, NULL);
        g_object_set(action, "tooltip", "Stop loading the current page", NULL);
        gtk_widget_show(browser->progress);
    }
    katze_throbber_set_animated(KATZE_THROBBER(browser->throbber)
     , browser->loadedPercent != -1);

    gtk_image_set_from_stock(GTK_IMAGE(browser->location_icon), GTK_STOCK_FILE
     , GTK_ICON_SIZE_MENU);
}

void update_feeds(CBrowser* browser)
{
    // TODO: Look for available feeds, requires dom access
}

void update_search_engines(CBrowser* browser)
{
    // TODO: Look for available search engines, requires dom access
}

void update_browser_actions(CBrowser* browser)
{
    gboolean active = gtk_notebook_get_n_pages(GTK_NOTEBOOK(browser->webViews)) > 1;
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(browser->webViews), active);
    action_set_sensitive("TabClose", active, browser);
    action_set_sensitive("TabPrevious", active, browser);
    action_set_sensitive("TabNext", active, browser);

    gboolean tabtrashEmpty = xbel_folder_is_empty(tabtrash);
    action_set_sensitive("UndoTabClose", !tabtrashEmpty, browser);
    action_set_sensitive("TabsClosed", !tabtrashEmpty, browser);
}

gchar* magic_uri(const gchar* uri, gboolean search)
{
    // Add file:// if we have a local path
    if(g_path_is_absolute(uri))
        return g_strconcat("file://", uri, NULL);
    // Do we need to add a protocol?
    if(!strstr(uri, "://"))
    {
        // Do we have a domain, ip address or localhost?
        if(strstr(uri, ".") != NULL || !strcmp(uri, "localhost"))
            return g_strconcat("http://", uri, NULL);
        // We don't want to search? So return early.
        if(!search)
            return g_strdup(uri);
        gchar search[256];
        const gchar* searchUrl = NULL;
        // Do we have a keyword and a string?
        gchar** parts = g_strsplit(uri, " ", 2);
        if(parts[0] && parts[1])
        {
            guint n = g_list_length(searchEngines);
            guint i;
            for(i = 0; i < n; i++)
            {
                SearchEngine* searchEngine = (SearchEngine*)g_list_nth_data(searchEngines, i);
                if(!strcmp(search_engine_get_keyword(searchEngine), parts[0]))
                    searchUrl = searchEngine->url;
            }
        if(searchUrl != NULL)
            g_snprintf(search, 255, searchUrl, parts[1]);
        }
        //g_strfreev(sParts);
        // We only have a word or there is no matching keyowrd, so search for it
        if(searchUrl == NULL)
            g_snprintf(search, 255, config->locationSearch, uri);
        return g_strdup(search);
    }
    return g_strdup(uri);
}

gchar* get_default_font(void)
{
    GtkSettings* gtksettings = gtk_settings_get_default();
    gchar* defaultFont;
    g_object_get(gtksettings, "gtk-font-name", &defaultFont, NULL);
    return defaultFont;
}

GtkToolbarStyle config_to_toolbarstyle(guint toolbarStyle)
{
    switch(toolbarStyle)
    {
    case CONFIG_TOOLBAR_ICONS:
        return GTK_TOOLBAR_ICONS;
    case CONFIG_TOOLBAR_TEXT:
        return GTK_TOOLBAR_TEXT;
    case CONFIG_TOOLBAR_BOTH:
        return GTK_TOOLBAR_BOTH;
    case CONFIG_TOOLBAR_BOTH_HORIZ:
        return GTK_TOOLBAR_BOTH_HORIZ;
    }
    GtkSettings* gtkSettings = gtk_settings_get_default();
    g_object_get(gtkSettings, "gtk-toolbar-style", &toolbarStyle, NULL);
    return toolbarStyle;
}

GtkToolbarStyle config_to_toolbariconsize(gboolean toolbarSmall)
{
    return toolbarSmall ? GTK_ICON_SIZE_SMALL_TOOLBAR
     : GTK_ICON_SIZE_LARGE_TOOLBAR;
}
