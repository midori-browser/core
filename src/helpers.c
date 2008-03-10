/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "helpers.h"

#include "global.h"
#include "search.h"
#include "sokoke.h"

#include "midori-webview.h"
#include <katze/katze.h>

#include <string.h>
#include <webkit/webkit.h>

GtkWidget* check_menu_item_new(const gchar* text
 , GCallback signal, gboolean sensitive, gboolean active, gpointer userdata)
{
    GtkWidget* menuitem = gtk_check_menu_item_new_with_mnemonic(text);
    gtk_widget_set_sensitive(menuitem, sensitive && signal);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), active);
    if(signal)
        g_signal_connect(menuitem, "activate", signal, userdata);
    return menuitem;
}

GtkWidget* radio_button_new(GtkRadioButton* radio_button, const gchar* label)
{
    return gtk_radio_button_new_with_mnemonic_from_widget(radio_button, label);
}

void show_error(const gchar* text, const gchar* text2, MidoriBrowser* browser)
{
    GtkWidget* dialog = gtk_message_dialog_new(
     browser ? GTK_WINDOW(browser) : NULL
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
    g_free(commandReady);
    g_free(uriEscaped);
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

gchar* magic_uri(const gchar* uri, gboolean search)
{
    // Add file:// if we have a local path
    if(g_path_is_absolute(uri))
        return g_strconcat("file://", uri, NULL);
    // Do we need to add a protocol?
    if(!strstr(uri, "://"))
    {
        // Do we have a domain, ip address or localhost?
        if(strchr(uri, '.') != NULL || !strcmp(uri, "localhost"))
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
