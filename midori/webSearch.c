/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "webSearch.h"

#include "search.h"

#include "main.h"
#include "sokoke.h"

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

static GdkPixbuf*
load_web_icon (const gchar* icon, GtkIconSize size, GtkWidget* widget)
{
    g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
    GdkPixbuf* pixbuf = NULL;
    if (icon && *icon)
    {
        // TODO: We want to allow http as well, maybe also base64?
        const gchar* icon_ready = g_str_has_prefix (icon, "file://")
            ? &icon[7] : icon;
        GtkStockItem stock_id;
        if (gtk_stock_lookup (icon, &stock_id))
            pixbuf = gtk_widget_render_icon (widget, icon_ready, size, NULL);
        else
        {
            gint width, height;
            gtk_icon_size_lookup (size, &width, &height);
            if (gtk_widget_has_screen (widget))
            {
                GdkScreen* screen = gtk_widget_get_screen (widget);
                pixbuf = gtk_icon_theme_load_icon (
                    gtk_icon_theme_get_for_screen (screen), icon,
                    MAX (width, height), GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
            }
        }
        if (!pixbuf)
            pixbuf = gdk_pixbuf_new_from_file_at_size (icon_ready, 16, 16, NULL);
    }
    if (!pixbuf)
        pixbuf = gtk_widget_render_icon (widget, GTK_STOCK_FIND, size, NULL);
    return pixbuf;
}

void update_searchEngine(guint index, GtkWidget* search)
{
    guint n = g_list_length(searchEngines);
    // Display a default icon in case we have no engines
    if(!n)
        sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(search), SEXY_ICON_ENTRY_PRIMARY
         , GTK_IMAGE(gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU)));
    // Change the icon and default text according to the chosen engine
    else
    {
        // Reset in case the index is out of range
        if(index >= n)
            index = 0;
        SearchEngine* engine = (SearchEngine*)g_list_nth_data(searchEngines, index);
        GdkPixbuf* pixbuf = load_web_icon(search_engine_get_icon(engine)
         , GTK_ICON_SIZE_MENU, search);
        sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(search)
         , SEXY_ICON_ENTRY_PRIMARY, GTK_IMAGE(gtk_image_new_from_pixbuf(pixbuf)));
        g_object_unref(pixbuf);
        sokoke_entry_set_default_text(GTK_ENTRY(search)
         , search_engine_get_short_name(engine));
        // config->searchEngine = index;
    }
}

void on_webSearch_engine_activate(GtkWidget* widget, MidoriBrowser* browser)
{
    guint index = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(widget), "engine"));
    update_searchEngine(index, widget);
}

void on_webSearch_icon_released(GtkWidget* widget, SexyIconEntryPosition* pos
 , gint button, MidoriBrowser* browser)
{
    GtkWidget* menu = gtk_menu_new();
    guint n = g_list_length(searchEngines);
    GtkWidget* menuitem;
    if(n)
    {
        guint i;
        for(i = 0; i < n; i++)
        {
            SearchEngine* engine = (SearchEngine*)g_list_nth_data(searchEngines, i);
            menuitem = gtk_image_menu_item_new_with_label(
             search_engine_get_short_name(engine));
            GdkPixbuf* pixbuf = load_web_icon(search_engine_get_icon(engine)
             , GTK_ICON_SIZE_MENU, menuitem);
            GtkWidget* icon = gtk_image_new_from_pixbuf(pixbuf);
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), icon);
            g_object_unref(pixbuf);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
            g_object_set_data(G_OBJECT(menuitem), "engine", GUINT_TO_POINTER(i));
            g_signal_connect(menuitem, "activate"
             , G_CALLBACK(on_webSearch_engine_activate), browser);
            gtk_widget_show(menuitem);
        }
    }
    else
    {
        menuitem = gtk_image_menu_item_new_with_label(_("Empty"));
        gtk_widget_set_sensitive(menuitem, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
        gtk_widget_show(menuitem);
    }

    /*menuitem = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    gtk_widget_show(menuitem);
    GtkAction* action = gtk_action_group_get_action(
     browser->actiongroup, "ManageSearchEngines");
    menuitem = gtk_action_create_menu_item(action);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    gtk_widget_show(menuitem);*/
    sokoke_widget_popup(widget, GTK_MENU(menu),
		        NULL, SOKOKE_MENU_POSITION_LEFT);
}

static void on_webSearch_engines_render_icon(GtkTreeViewColumn* column
 , GtkCellRenderer* renderer, GtkTreeModel* model, GtkTreeIter* iter
 , GtkWidget* treeview)
{
    SearchEngine* searchEngine;
    gtk_tree_model_get(model, iter, ENGINES_COL_ENGINE, &searchEngine, -1);

    // TODO: Would it be better to not do this on every redraw?
    const gchar* icon = search_engine_get_icon(searchEngine);
    if(icon)
    {
        GdkPixbuf* pixbuf = load_web_icon(icon, GTK_ICON_SIZE_DND, treeview);
        g_object_set(renderer, "pixbuf", pixbuf, NULL);
        if(pixbuf)
            g_object_unref(pixbuf);
    }
    else
        g_object_set(renderer, "pixbuf", NULL, NULL);
}

static void on_webSearch_engines_render_text(GtkTreeViewColumn* column
 , GtkCellRenderer* renderer, GtkTreeModel* model, GtkTreeIter* iter
 , GtkWidget* treeview)
{
    SearchEngine* searchEngine;
    gtk_tree_model_get(model, iter, ENGINES_COL_ENGINE, &searchEngine, -1);
    const gchar* name = search_engine_get_short_name(searchEngine);
    const gchar* description = search_engine_get_description(searchEngine);
    gchar* markup = g_markup_printf_escaped("<b>%s</b>\n%s", name, description);
    g_object_set(renderer, "markup", markup, NULL);
    g_free(markup);
}

static void webSearch_toggle_edit_buttons(gboolean sensitive, CWebSearch* webSearch)
{
    gtk_widget_set_sensitive(webSearch->edit, sensitive);
    gtk_widget_set_sensitive(webSearch->remove, sensitive);
}

static void on_webSearch_shortName_changed(GtkWidget* widget, GtkWidget* dialog)
{
    const gchar* text = gtk_entry_get_text(GTK_ENTRY(widget));
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog)
     , GTK_RESPONSE_ACCEPT, text && *text);
}

const gchar* STR_NON_NULL(const gchar* string)
{
    return string ? string : "";
}

static void webSearch_editEngine_dialog_new(gboolean newEngine, CWebSearch* webSearch)
{
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        newEngine ? _("Add search engine") : _("Edit search engine")
        , GTK_WINDOW(webSearch->window)
        , GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR
        , GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL
        , newEngine ? GTK_STOCK_ADD : GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT
        , NULL);
    gtk_window_set_icon_name(GTK_WINDOW(dialog)
     , newEngine ? GTK_STOCK_ADD : GTK_STOCK_REMOVE);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
    gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 5);
    GtkSizeGroup* sizegroup =  gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    SearchEngine* searchEngine;
    GtkTreeModel* liststore;
    GtkTreeIter iter;
    if(newEngine)
    {
        searchEngine = search_engine_new();
        gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog)
         , GTK_RESPONSE_ACCEPT, FALSE);
    }
    else
    {
        GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(webSearch->treeview));
        gtk_tree_selection_get_selected(selection, &liststore, &iter);
        gtk_tree_model_get(liststore, &iter, ENGINES_COL_ENGINE, &searchEngine, -1);
    }

    GtkWidget* hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    GtkWidget* label = gtk_label_new_with_mnemonic(_("_Name:"));
    gtk_size_group_add_widget(sizegroup, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    GtkWidget* entry_shortName = gtk_entry_new();
    g_signal_connect(entry_shortName, "changed"
     , G_CALLBACK(on_webSearch_shortName_changed), dialog);
    gtk_entry_set_activates_default(GTK_ENTRY(entry_shortName), TRUE);
    if(!newEngine)
        gtk_entry_set_text(GTK_ENTRY(entry_shortName)
         , search_engine_get_short_name(searchEngine));
    gtk_box_pack_start(GTK_BOX(hbox), entry_shortName, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);
    gtk_widget_show_all(hbox);
    
    hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    label = gtk_label_new_with_mnemonic(_("_Description:"));
    gtk_size_group_add_widget(sizegroup, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    GtkWidget* entry_description = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry_description), TRUE);
    if(!newEngine)
        gtk_entry_set_text(GTK_ENTRY(entry_description)
         , STR_NON_NULL(search_engine_get_description(searchEngine)));
    gtk_box_pack_start(GTK_BOX(hbox), entry_description, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);
    gtk_widget_show_all(hbox);
    
    hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    label = gtk_label_new_with_mnemonic(_("_URL:"));
    gtk_size_group_add_widget(sizegroup, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    GtkWidget* entry_url = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry_url), TRUE);
    if(!newEngine)
        gtk_entry_set_text(GTK_ENTRY(entry_url)
         , STR_NON_NULL(search_engine_get_url(searchEngine)));
    gtk_box_pack_start(GTK_BOX(hbox), entry_url, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);
    gtk_widget_show_all(hbox);
    
    hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    label = gtk_label_new_with_mnemonic(_("_Icon (name or file):"));
    gtk_size_group_add_widget(sizegroup, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    GtkWidget* entry_icon = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry_icon), TRUE);
    if(!newEngine)
        gtk_entry_set_text(GTK_ENTRY(entry_icon)
         , STR_NON_NULL(search_engine_get_icon(searchEngine)));
    gtk_box_pack_start(GTK_BOX(hbox), entry_icon, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);
    gtk_widget_show_all(hbox);
    
    hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    label = gtk_label_new_with_mnemonic(_("_Keyword:"));
    gtk_size_group_add_widget(sizegroup, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    GtkWidget* entry_keyword = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry_keyword), TRUE);
    if(!newEngine)
        gtk_entry_set_text(GTK_ENTRY(entry_keyword)
         , STR_NON_NULL(search_engine_get_keyword(searchEngine)));
    gtk_box_pack_start(GTK_BOX(hbox), entry_keyword, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);
    gtk_widget_show_all(hbox);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        search_engine_set_short_name(searchEngine
         , gtk_entry_get_text(GTK_ENTRY(entry_shortName)));
        search_engine_set_description(searchEngine
         , gtk_entry_get_text(GTK_ENTRY(entry_description)));
        search_engine_set_url(searchEngine
         , gtk_entry_get_text(GTK_ENTRY(entry_url)));
        /*search_engine_set_input_encoding(searchEngine
         , gtk_entry_get_text(GTK_ENTRY(entry_inputEncoding)));*/
        search_engine_set_icon(searchEngine
         , gtk_entry_get_text(GTK_ENTRY(entry_icon)));
        search_engine_set_keyword(searchEngine
         , gtk_entry_get_text(GTK_ENTRY(entry_keyword)));

        if(newEngine)
        {
            searchEngines = g_list_append(searchEngines, searchEngine);
            liststore = gtk_tree_view_get_model(GTK_TREE_VIEW(webSearch->treeview));
            gtk_list_store_append(GTK_LIST_STORE(liststore), &iter);
        }
        gtk_list_store_set(GTK_LIST_STORE(liststore), &iter
             , ENGINES_COL_ENGINE, searchEngine, -1);
        webSearch_toggle_edit_buttons(TRUE, webSearch);
    }
    gtk_widget_destroy(dialog);
}

static void on_webSearch_add(GtkWidget* widget, CWebSearch* webSearch)
{
    webSearch_editEngine_dialog_new(TRUE, webSearch);
}

static void on_webSearch_edit(GtkWidget* widget, CWebSearch* webSearch)
{
    webSearch_editEngine_dialog_new(FALSE, webSearch);
}

static void on_webSearch_remove(GtkWidget* widget, CWebSearch* webSearch)
{
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(webSearch->treeview));
    GtkTreeModel* liststore;
    GtkTreeIter iter;
    gtk_tree_selection_get_selected(selection, &liststore, &iter);
    SearchEngine* searchEngine;
    gtk_tree_model_get(liststore, &iter, ENGINES_COL_ENGINE, &searchEngine, -1);
    gtk_list_store_remove(GTK_LIST_STORE(liststore), &iter);
    search_engine_free(searchEngine);
    searchEngines = g_list_remove(searchEngines, searchEngine);
    //update_searchEngine(config->searchEngine, webSearch->browser);
    webSearch_toggle_edit_buttons(g_list_nth(searchEngines, 0) != NULL, webSearch);
    // FIXME: we want to allow undo of some kind
}

GtkWidget* webSearch_manageSearchEngines_dialog_new(MidoriBrowser* browser)
{
    const gchar* dialogTitle = _("Manage search engines");
    GtkWidget* dialog = gtk_dialog_new_with_buttons(dialogTitle
        , GTK_WINDOW(browser)
        , GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR
        , GTK_STOCK_HELP
        , GTK_RESPONSE_HELP
        , GTK_STOCK_CLOSE
        , GTK_RESPONSE_CLOSE
        , NULL);
    gtk_window_set_icon_name(GTK_WINDOW(dialog), GTK_STOCK_PROPERTIES);
    // TODO: Implement some kind of help function
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog)
     , GTK_RESPONSE_HELP, FALSE); //...
    gint iWidth, iHeight;
    sokoke_widget_get_text_size(dialog, "M", &iWidth, &iHeight);
    gtk_window_set_default_size(GTK_WINDOW(dialog), iWidth * 45, -1);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
    // TODO: Do we want tooltips for explainations or can we omit that?
    // TODO: We need mnemonics
    // TODO: Take multiple windows into account when applying changes
    GtkWidget* xfce_heading;
    if((xfce_heading = sokoke_xfce_header_new(
     gtk_window_get_icon_name(GTK_WINDOW(dialog)), dialogTitle)))
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), xfce_heading, FALSE, FALSE, 0);
    GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 12);
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text; GtkCellRenderer* renderer_pixbuf;
    GtkListStore* liststore = gtk_list_store_new(ENGINES_COL_N
     , G_TYPE_SEARCH_ENGINE);
    GtkWidget* treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(liststore));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    column = gtk_tree_view_column_new();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func(column, renderer_pixbuf
    , (GtkTreeCellDataFunc)on_webSearch_engines_render_icon, treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer_text, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer_text
    , (GtkTreeCellDataFunc)on_webSearch_engines_render_text, treeview, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled)
    , GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), treeview);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 5);
    guint n = g_list_length(searchEngines);
    guint i;
    for(i = 0; i < n; i++)
    {
        SearchEngine* searchEngine = (SearchEngine*)g_list_nth_data(searchEngines, i);
        gtk_list_store_insert_with_values(GTK_LIST_STORE(liststore), NULL, i
         , ENGINES_COL_ENGINE, searchEngine, -1);
    }
    g_object_unref(liststore);
    CWebSearch* webSearch = g_new0(CWebSearch, 1);
    webSearch->browser = browser;
    webSearch->window = dialog;
    webSearch->treeview = treeview;
    g_signal_connect(dialog, "response", G_CALLBACK(g_free), webSearch);
    GtkWidget* vbox = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);
    GtkWidget* button = gtk_button_new_from_stock(GTK_STOCK_ADD);
    g_signal_connect(button, "clicked", G_CALLBACK(on_webSearch_add), webSearch);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
    button = gtk_button_new_from_stock(GTK_STOCK_EDIT);
    g_signal_connect(button, "clicked", G_CALLBACK(on_webSearch_edit), webSearch);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
    webSearch->edit = button;
    button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
    g_signal_connect(button, "clicked", G_CALLBACK(on_webSearch_remove), webSearch);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
    webSearch->remove = button;
    button = gtk_label_new(""); // This is an invisible separator
    gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 12);
    button = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
    gtk_widget_set_sensitive(button, FALSE); //...
    gtk_box_pack_end(GTK_BOX(vbox), button, FALSE, FALSE, 0);
    button = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
    gtk_widget_set_sensitive(button, FALSE); //...
    gtk_box_pack_end(GTK_BOX(vbox), button, FALSE, FALSE, 0);
    webSearch_toggle_edit_buttons(n > 0, webSearch);
    gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);
    return dialog;
}

gboolean on_webSearch_key_down(GtkWidget* widget, GdkEventKey* event, MidoriBrowser* browser)
{
    GdkModifierType state = (GdkModifierType)0;
    gint x, y; gdk_window_get_pointer(NULL, &x, &y, &state);
    if(!(state & GDK_CONTROL_MASK))
        return FALSE;
    switch(event->keyval)
    {
    case GDK_Up:
        //update_searchEngine(config->searchEngine - 1, browser);
        return TRUE;
    case GDK_Down:
        //update_searchEngine(config->searchEngine + 1, browser);
        return TRUE;
    }
    return FALSE;
}

gboolean on_webSearch_scroll(GtkWidget* webView, GdkEventScroll* event, MidoriBrowser* browser)
{
    if(event->direction == GDK_SCROLL_DOWN)
        ;//update_searchEngine(config->searchEngine + 1, browser);
    else if(event->direction == GDK_SCROLL_UP)
        ;//update_searchEngine(config->searchEngine - 1, browser);
    return TRUE;
}

void on_webSearch_activate(GtkWidget* widget, MidoriBrowser* browser)
{
    const gchar* keywords = gtk_entry_get_text(GTK_ENTRY(widget));
    gchar* url;
    SearchEngine* searchEngine = (SearchEngine*)g_list_nth_data(searchEngines, 0/*config->searchEngine*/);
    if(searchEngine)
        url = searchEngine->url;
    else // The location search is our fallback
     url = "";//config->locationSearch;
    gchar* search;
    if(strstr(url, "%s"))
     search = g_strdup_printf(url, keywords);
    else
     search = g_strconcat(url, " ", keywords, NULL);
    sokoke_entry_append_completion(GTK_ENTRY(widget), keywords);
    GtkWidget* webView = midori_browser_get_current_web_view(browser);
    webkit_web_view_open(WEBKIT_WEB_VIEW(webView), search);
    g_free(search);
}
