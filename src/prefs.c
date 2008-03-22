/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "prefs.h"

#include "helpers.h"
#include "global.h"
#include "sokoke.h"

#include <stdlib.h>
#include <string.h>

static gboolean on_prefs_homepage_focus_out(GtkWidget* widget
 , GdkEventFocus event, CPrefs* prefs)
{
    katze_assign(config->homepage, g_strdup(gtk_entry_get_text(GTK_ENTRY(widget))));
    return FALSE;
}

static void on_prefs_loadonstartup_changed(GtkWidget* widget, CPrefs* prefs)
{
    config->startup = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
}

static void on_prefs_defaultFont_changed(GtkWidget* widget, CPrefs* prefs)
{
    const gchar* font = gtk_font_button_get_font_name(GTK_FONT_BUTTON(widget));
    gchar** components = g_strsplit(font, " ", 0);
    guint i, n = g_strv_length(components) - 1;
    GString* fontName = g_string_new(NULL);
    for(i = 0; i < n; i++)
        g_string_append_printf(fontName, "%s ", components[i]);
    katze_assign(config->defaultFontFamily, g_string_free(fontName, FALSE));
    config->defaultFontSize = atoi(components[n]);
    g_strfreev(components);
    g_object_set(webSettings, "default-font-family", config->defaultFontFamily
     , "default-font-size", config->defaultFontSize, NULL);
}

static void on_prefs_minimumFontSize_changed(GtkWidget* widget, CPrefs* prefs)
{
    config->minimumFontSize = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
    g_object_set(webSettings, "minimum-font-size", config->minimumFontSize, NULL);
}

static void on_prefs_defaultEncoding_changed(GtkWidget* widget, CPrefs* prefs)
{
    gchar* encoding;
    switch(gtk_combo_box_get_active(GTK_COMBO_BOX(widget)))
    {
    case 0:
        encoding = g_strdup("BIG5");
        break;
    case 1:
        encoding = g_strdup("SHIFT_JIS");
        break;
    case 2:
        encoding = g_strdup("KOI8-R");
        break;
    case 3:
        encoding = g_strdup("UTF-8");
        break;
    case 4:
        encoding = g_strdup("ISO-8859-1");
        break;
    default:
        encoding = g_strdup("UTF-8");
        g_warning("Invalid default encoding");
    }
    katze_assign(config->defaultEncoding, encoding);
    g_object_set(webSettings, "default-encoding", config->defaultEncoding, NULL);
}

static void on_prefs_newpages_changed(GtkWidget* widget, CPrefs* prefs)
{
    config->newPages = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
}

void on_prefs_middleClickGoto_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->middleClickGoto = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

void on_prefs_openTabsInTheBackground_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->openTabsInTheBackground = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void on_prefs_openPopupsInTabs_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->openPopupsInTabs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void on_prefs_loadImagesAutomatically_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->autoLoadImages = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    g_object_set(webSettings, "auto-load-images", config->autoLoadImages, NULL);
}

static void on_prefs_shrinkImagesToFit_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->autoShrinkImages = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    g_object_set(webSettings, "auto-shrink-images", config->autoShrinkImages, NULL);
}

static void on_prefs_printBackgrounds_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->printBackgrounds = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    g_object_set(webSettings, "print-backgrounds", config->printBackgrounds, NULL);
}

static void on_prefs_resizableTextAreas_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->resizableTextAreas = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    g_object_set(webSettings, "resizable-text-areas", config->resizableTextAreas, NULL);
}

static void on_prefs_enableJavaScript_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->enableScripts = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    g_object_set(webSettings, "enable-scripts", config->enableScripts, NULL);
}

static void on_prefs_enablePlugins_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->enablePlugins = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    g_object_set(webSettings, "enable-plugins", config->enablePlugins, NULL);
}

static void on_prefs_userStylesheet_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->userStylesheet = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    gtk_widget_set_sensitive(prefs->userStylesheetUri, config->userStylesheet);
    const gchar* uri = config->userStylesheet ? config->userStylesheetUri : "";
    g_object_set(webSettings, "user-stylesheet-uri", uri, NULL);
}

static void on_prefs_userStylesheetUri_file_set(GtkWidget* widget, CPrefs* prefs)
{
    katze_assign(config->userStylesheetUri, g_strdup(gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(widget))));
    g_object_set(webSettings, "user-stylesheet-uri", config->userStylesheetUri, NULL);
}

static void on_prefs_toolbarstyle_changed(GtkWidget* widget, CPrefs* prefs)
{
    config->toolbarStyle = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    /*gtk_toolbar_set_style(GTK_TOOLBAR(prefs->browser->navibar)
     , config_to_toolbarstyle(config->toolbarStyle));*/
}

static void on_prefs_toolbarSmall_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->toolbarSmall = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    /*gtk_toolbar_set_icon_size(GTK_TOOLBAR(prefs->browser->navibar)
     , config_to_toolbariconsize(config->toolbarSmall));*/
}

static void on_prefs_tabClose_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->tabClose = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    g_object_set(webSettings, "close-button", config->tabClose, NULL);
}

static void on_prefs_tabSize_changed(GtkWidget* widget, CPrefs* prefs)
{
    config->tabSize = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
    g_object_set(webSettings, "tab-label-size", config->tabSize, NULL);
}

static void on_prefs_toolbarWebSearch_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->toolbarWebSearch = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    //sokoke_widget_set_visible(prefs->browser->webSearch, config->toolbarWebSearch);
}

static void on_prefs_toolbarNewTab_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->toolbarNewTab = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    //sokoke_widget_set_visible(prefs->browser->newTab, config->toolbarNewTab);
}

static void on_prefs_toolbarClosedTabs_toggled(GtkWidget* widget, CPrefs* prefs)
{
    config->toolbarClosedTabs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    //sokoke_widget_set_visible(prefs->browser->closedTabs, config->toolbarClosedTabs);
}

static gboolean on_prefs_locationsearch_focus_out(GtkWidget* widget
 , GdkEventFocus event, CPrefs* prefs)
{
    katze_assign(config->locationSearch, g_strdup(gtk_entry_get_text(GTK_ENTRY(widget))));
    return FALSE;
}

static void on_prefs_protocols_render_icon(GtkTreeViewColumn* column
 , GtkCellRenderer* renderer, GtkTreeModel* model, GtkTreeIter* iter, CPrefs* prefs)
{
    gchar* command;
    gtk_tree_model_get(model, iter, PROTOCOLS_COL_COMMAND, &command, -1);

    // TODO: Would it be better, to not do this on every redraw?
    // Determine the actual binary to be able to display an icon
    gchar* binary = NULL;
    if(command)
        binary = strtok(command, " ");
    if(binary)
    {
        gchar* path;
        if((path = g_find_program_in_path(binary)))
        {
            GdkScreen* screen = gtk_widget_get_screen(prefs->treeview);
            if(!screen)
                screen = gdk_screen_get_default();
            GtkIconTheme* icon_theme = gtk_icon_theme_get_for_screen(screen);
            if(g_path_is_absolute(binary))
            {
                g_free(path); path = g_path_get_basename(binary);
            }
            // TODO: Is it good to just display nothing if there is no icon?
            if(!gtk_icon_theme_has_icon(icon_theme, binary))
                binary = NULL;
            #if GTK_CHECK_VERSION(2, 10, 0)
            g_object_set(renderer, "icon-name", binary, NULL);
            #else
            GdkPixbuf* icon = binary != NULL
             ? gtk_icon_theme_load_icon(gtk_icon_theme_get_default()
             , binary, GTK_ICON_SIZE_MENU, 0, NULL) : NULL;
            g_object_set(renderer, "pixbuf", icon, NULL);
            if(icon)
                g_object_unref(icon);
            #endif
            g_free(path);
        }
        else
        {
            #if GTK_CHECK_VERSION(2, 10, 0)
            g_object_set(renderer, "icon-name", NULL, NULL);
            #endif
            g_object_set(renderer, "stock-id", GTK_STOCK_DIALOG_ERROR, NULL);
        }
    }
    else
    {
        // We need to reset the icon
        #if GTK_CHECK_VERSION(2, 10, 0)
        g_object_set(renderer, "icon-name", NULL, NULL);
        #else
        g_object_set(renderer, "stock-id", NULL, NULL);
        #endif
    }
    g_free(command);
}

static void on_prefs_protocols_edited(GtkCellRendererText* renderer
 , gchar* path, gchar* textNew, CPrefs* prefs)
{
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(prefs->treeview));
    GtkTreeIter iter;
    gtk_tree_model_get_iter_from_string(model, &iter, path);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter
     , PROTOCOLS_COL_COMMAND, textNew, -1);
    gchar* protocol;
    gtk_tree_model_get(model, &iter, PROTOCOLS_COL_NAME, &protocol, -1);
    g_datalist_set_data_full(&config->protocols_commands
     , protocol, g_strdup(textNew), g_free);
}

static void on_prefs_protocols_add_clicked(GtkWidget* widget, CPrefs* prefs)
{
    gchar* protocol = gtk_combo_box_get_active_text(GTK_COMBO_BOX(prefs->combobox));
    GtkTreeModel* liststore = gtk_tree_view_get_model(GTK_TREE_VIEW(prefs->treeview));
    gtk_list_store_insert_with_values(GTK_LIST_STORE(liststore), NULL, G_MAXINT
        , PROTOCOLS_COL_NAME, protocol
        , PROTOCOLS_COL_COMMAND, "", -1);
    g_ptr_array_add(config->protocols_names, (gpointer)protocol);
    g_datalist_set_data_full(&config->protocols_commands
     , protocol, g_strdup(""), g_free);
    gtk_widget_set_sensitive(prefs->add, FALSE);
}

static void on_prefs_protocols_combobox_changed(GtkWidget* widget, CPrefs* prefs)
{
    gchar* protocol = gtk_combo_box_get_active_text(GTK_COMBO_BOX(widget));
    gchar* command = (gchar*)g_datalist_get_data(&config->protocols_commands, protocol);
    g_free(protocol);
    gtk_widget_set_sensitive(prefs->add, command == NULL);
}

GtkWidget* prefs_preferences_dialog_new(MidoriBrowser* browser)
{
    gchar* dialogTitle = g_strdup_printf(_("%s Preferences"), g_get_application_name());
    GtkWidget* dialog = gtk_dialog_new_with_buttons(dialogTitle
        , GTK_WINDOW(browser)
        , GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR
        , GTK_STOCK_HELP, GTK_RESPONSE_HELP
        , GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE
        , NULL);
    gtk_window_set_icon_name(GTK_WINDOW(dialog), GTK_STOCK_PREFERENCES);
    // TODO: Implement some kind of help function
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_HELP, FALSE); //...
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);

    CPrefs* prefs = g_new0(CPrefs, 1);
    prefs->browser = browser;
    g_signal_connect(dialog, "response", G_CALLBACK(g_free), prefs);

    // TODO: Do we want tooltips for explainations or can we omit that?
    // TODO: We need mnemonics
    // TODO: Take multiple windows into account when applying changes
    GtkWidget* xfce_heading;
    if((xfce_heading = sokoke_xfce_header_new(
     gtk_window_get_icon_name(GTK_WINDOW(browser)), dialogTitle)))
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox)
         , xfce_heading, FALSE, FALSE, 0);
    g_free(dialogTitle);
    GtkWidget* notebook = gtk_notebook_new();
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 6);
    GtkSizeGroup* sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    GtkSizeGroup* sizegroup2 = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    GtkWidget* page; GtkWidget* frame; GtkWidget* table; GtkWidget* align;
    GtkWidget* button; GtkWidget* checkbutton; GtkWidget* colorbutton;
    GtkWidget* combobox; GtkWidget* entry; GtkWidget* hbox; GtkWidget* spinbutton;
    #define PAGE_NEW(__label) page = gtk_vbox_new(FALSE, 0);\
     gtk_container_set_border_width(GTK_CONTAINER(page), 5);\
     gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, gtk_label_new(__label))
    #define FRAME_NEW(__label) frame = sokoke_hig_frame_new(__label);\
     gtk_container_set_border_width(GTK_CONTAINER(frame), 5);\
     gtk_box_pack_start(GTK_BOX(page), frame, FALSE, FALSE, 0);
    #define TABLE_NEW(__rows, __cols) table = gtk_table_new(__rows, __cols, FALSE);\
     gtk_container_set_border_width(GTK_CONTAINER(table), 5);\
     gtk_container_add(GTK_CONTAINER(frame), table);
    #define WIDGET_ADD(__widget, __left, __right, __top, __bottom)\
     gtk_table_attach(GTK_TABLE(table), __widget\
      , __left, __right, __top, __bottom\
      , GTK_FILL, GTK_FILL, 8, 2)
    #define FILLED_ADD(__widget, __left, __right, __top, __bottom)\
     gtk_table_attach(GTK_TABLE(table), __widget\
      , __left, __right, __top, __bottom\
      , GTK_EXPAND | GTK_FILL, GTK_FILL, 8, 2)
    #define INDENTED_ADD(__widget, __left, __right, __top, __bottom)\
     align = gtk_alignment_new(0, 0.5, 0, 0);\
     gtk_container_add(GTK_CONTAINER(align), __widget);\
     gtk_size_group_add_widget(sizegroup, align);\
     WIDGET_ADD(align, __left, __right, __top, __bottom)
    #define SEMI_INDENTED_ADD(__widget, __left, __right, __top, __bottom)\
     align = gtk_alignment_new(0, 0.5, 0, 0);\
     gtk_container_add(GTK_CONTAINER(align), __widget);\
     gtk_size_group_add_widget(sizegroup2, align);\
     WIDGET_ADD(align, __left, __right, __top, __bottom)
    #define SPANNED_ADD(__widget, __left, __right, __top, __bottom)\
     align = gtk_alignment_new(0, 0.5, 0, 0);\
     gtk_container_add(GTK_CONTAINER(align), __widget);\
     FILLED_ADD(align, __left, __right, __top, __bottom)
    // Page "General"
    PAGE_NEW(_("General"));
    FRAME_NEW(_("Startup"));
    TABLE_NEW(2, 2);
    INDENTED_ADD(gtk_label_new(_("Load on startup")), 0, 1, 0, 1);
    combobox = gtk_combo_box_new_text();
    sokoke_combo_box_add_strings(GTK_COMBO_BOX(combobox)
     , _("Blank page"), _("Homepage"), _("Last open pages"), NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), config->startup);
    g_signal_connect(combobox, "changed"
     , G_CALLBACK(on_prefs_loadonstartup_changed), prefs);
    FILLED_ADD(combobox, 1, 2, 0, 1);
    INDENTED_ADD(gtk_label_new(_("Homepage")), 0, 1, 1, 2);
    entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), config->homepage);
    g_signal_connect(entry, "focus-out-event"
    , G_CALLBACK(on_prefs_homepage_focus_out), prefs);
    FILLED_ADD(entry, 1, 2, 1, 2);
    // TODO: We need something like "use current website"
    FRAME_NEW(_("Transfers"));
    TABLE_NEW(1, 2);
    INDENTED_ADD(gtk_label_new(_("Download folder")), 0, 1, 0, 1);
    GtkWidget* filebutton = gtk_file_chooser_button_new(
     _("Choose downloaded files folder"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    // FIXME: The default should probably be ~/Desktop
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filebutton)
     , g_get_home_dir()); //...
    gtk_widget_set_sensitive(filebutton, FALSE); //...
    FILLED_ADD(filebutton, 1, 2, 0, 1);
    checkbutton = gtk_check_button_new_with_mnemonic
     (_("Show a notification window for finished transfers"));
    gtk_widget_set_sensitive(checkbutton, FALSE); //...
    SPANNED_ADD(checkbutton, 0, 2, 1, 2);
    FRAME_NEW(_("Languages"));
    TABLE_NEW(1, 2);
    INDENTED_ADD(gtk_label_new(_("Preferred languages")), 0, 1, 0, 1);
    entry = gtk_entry_new();
    // TODO: Make sth like get_browser_languages_default filtering encodings and C out
    // TODO: Provide a real ui with real language names (iso-codes)
    const gchar* const* sLanguages = g_get_language_names();
    gchar* sLanguagesPreferred = g_strjoinv(",", (gchar**)sLanguages);
    gtk_entry_set_text(GTK_ENTRY(entry), sLanguagesPreferred/*config->sLanguagesPreferred*/);
    g_free(sLanguagesPreferred);
    gtk_widget_set_sensitive(entry, FALSE); //...
    FILLED_ADD(entry, 1, 2, 0, 1);

    // Page "Appearance"
    PAGE_NEW(_("Appearance"));
    FRAME_NEW(_("Font settings"));
    TABLE_NEW(5, 2);
    INDENTED_ADD(gtk_label_new_with_mnemonic(_("Default _font")), 0, 1, 0, 1);
    gchar* defaultFont = g_strdup_printf("%s %d"
     , config->defaultFontFamily, config->defaultFontSize);
    button = gtk_font_button_new_with_font(defaultFont);
    g_free(defaultFont);
    g_signal_connect(button, "font-set", G_CALLBACK(on_prefs_defaultFont_changed), prefs);
    FILLED_ADD(button, 1, 2, 0, 1);
    INDENTED_ADD(gtk_label_new_with_mnemonic(_("_Minimum font size")), 0, 1, 1, 2);
    hbox = gtk_hbox_new(FALSE, 4);
    spinbutton = gtk_spin_button_new_with_range(1, G_MAXINT, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton), config->minimumFontSize);
    g_signal_connect(spinbutton, "value-changed"
     , G_CALLBACK(on_prefs_minimumFontSize_changed), prefs);
    gtk_box_pack_start(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 0);
    button = gtk_button_new_with_mnemonic(_("_Advanced"));
    gtk_widget_set_sensitive(button, FALSE); //...
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 4);
    FILLED_ADD(hbox, 1, 2, 1, 2);
    INDENTED_ADD(gtk_label_new_with_mnemonic(_("Default _encoding")), 0, 1, 2, 3);
    combobox = gtk_combo_box_new_text();
    sokoke_combo_box_add_strings(GTK_COMBO_BOX(combobox)
     , _("Chinese (BIG5)"), _("Japanese (SHIFT_JIS)"), _("Russian (KOI8-R)")
     , _("Unicode (UTF-8)"), _("Western (ISO-8859-1)"), NULL);
    if(!strcmp(config->defaultEncoding, "BIG5"))
        gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 0);
    else if(!strcmp(config->defaultEncoding, "SHIFT_JIS"))
        gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 1);
    else if(!strcmp(config->defaultEncoding, "KOI8-R"))
        gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 2);
    else if(!strcmp(config->defaultEncoding, "UTF-8"))
        gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 3);
    else if(!strcmp(config->defaultEncoding, "ISO-8859-1"))
        gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 4);
    // FIXME: Provide a 'Custom' item
    g_signal_connect(combobox, "changed"
     , G_CALLBACK(on_prefs_defaultEncoding_changed), prefs);
    FILLED_ADD(combobox, 1, 2, 2, 3);
    button = gtk_button_new_with_label(_("Advanced settings"));
    gtk_widget_set_sensitive(button, FALSE); //...
    WIDGET_ADD(button, 1, 2, 2, 3);
    FRAME_NEW(_("Default colors"));
    TABLE_NEW(2, 4);
    SEMI_INDENTED_ADD(gtk_label_new(_("Text color")), 0, 1, 0, 1);
    colorbutton = gtk_color_button_new();
    gtk_widget_set_sensitive(colorbutton, FALSE); //...
    WIDGET_ADD(colorbutton, 1, 2, 0, 1);
    SEMI_INDENTED_ADD(gtk_label_new(_("Background color")), 2, 3, 0, 1);
    colorbutton = gtk_color_button_new();
    gtk_widget_set_sensitive(colorbutton, FALSE); //...
    WIDGET_ADD(colorbutton, 3, 4, 0, 1);
    SEMI_INDENTED_ADD(gtk_label_new(_("Link color")), 0, 1, 1, 2);
    colorbutton = gtk_color_button_new();
    gtk_widget_set_sensitive(colorbutton, FALSE); //...
    WIDGET_ADD(colorbutton, 1, 2, 1, 2);
    SEMI_INDENTED_ADD(gtk_label_new(_("Visited link color")), 2, 3, 1, 2);
    colorbutton = gtk_color_button_new();
    gtk_widget_set_sensitive(colorbutton, FALSE); //...
    WIDGET_ADD(colorbutton, 3, 4, 1, 2);

    // Page "Behavior"
    PAGE_NEW(_("Behavior"));
    FRAME_NEW(_("Browsing"));
    TABLE_NEW(3, 2);
    INDENTED_ADD(gtk_label_new_with_mnemonic(_("Open _new pages in")), 0, 1, 0, 1);
    combobox = gtk_combo_box_new_text();
    sokoke_combo_box_add_strings(GTK_COMBO_BOX(combobox)
     , _("New tab"), _("New window"), _("Current tab"), NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), config->newPages);
    g_signal_connect(combobox, "changed", G_CALLBACK(on_prefs_newpages_changed), prefs);
    gtk_widget_set_sensitive(combobox, FALSE); //...
    FILLED_ADD(combobox, 1, 2, 0, 1);
    checkbutton = gtk_check_button_new_with_mnemonic(_("_Middle click opens selection"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->middleClickGoto);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_middleClickGoto_toggled), prefs);
    INDENTED_ADD(checkbutton, 0, 1, 1, 2);
    checkbutton = gtk_check_button_new_with_mnemonic(_("Open tabs in the _background"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->openTabsInTheBackground);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_openTabsInTheBackground_toggled), prefs);
    SPANNED_ADD(checkbutton, 1, 2, 1, 2);
    checkbutton = gtk_check_button_new_with_mnemonic(_("Open popups in _tabs"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->openPopupsInTabs);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_openPopupsInTabs_toggled), prefs);
    gtk_widget_set_sensitive(checkbutton, FALSE); //...
    SPANNED_ADD(checkbutton, 0, 2, 2, 3);
    FRAME_NEW(_("Features"));
    TABLE_NEW(4, 2);
    checkbutton = gtk_check_button_new_with_mnemonic(_("Load _images"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->autoLoadImages);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_loadImagesAutomatically_toggled), prefs);
    INDENTED_ADD(checkbutton, 0, 1, 0, 1);
    checkbutton = gtk_check_button_new_with_mnemonic(_("_Shrink images to fit"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->autoShrinkImages);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_shrinkImagesToFit_toggled), prefs);
    SPANNED_ADD(checkbutton, 1, 2, 0, 1);
    checkbutton = gtk_check_button_new_with_mnemonic(_("Print _backgrounds"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->printBackgrounds);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_printBackgrounds_toggled), prefs);
    INDENTED_ADD(checkbutton, 0, 1, 1, 2);
    checkbutton = gtk_check_button_new_with_mnemonic(_("_Resizable textareas"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->resizableTextAreas);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_resizableTextAreas_toggled), prefs);
    SPANNED_ADD(checkbutton, 1, 2, 1, 2);
    checkbutton = gtk_check_button_new_with_mnemonic(_("Enable _scripts"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->enableScripts);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_enableJavaScript_toggled), prefs);
    INDENTED_ADD(checkbutton, 0, 1, 2, 3);
    checkbutton = gtk_check_button_new_with_mnemonic(_("Enable _plugins"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->enablePlugins);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_enablePlugins_toggled), prefs);
    SPANNED_ADD(checkbutton, 1, 2, 2, 3);
    checkbutton = gtk_check_button_new_with_mnemonic(_("_User Stylesheet"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->userStylesheet);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_userStylesheet_toggled), prefs);
    INDENTED_ADD(checkbutton, 0, 1, 3, 4);
    filebutton = gtk_file_chooser_button_new(
     _("Choose user stylesheet"), GTK_FILE_CHOOSER_ACTION_OPEN);
    prefs->userStylesheetUri = filebutton;
    gtk_file_chooser_set_uri(GTK_FILE_CHOOSER(filebutton), config->userStylesheetUri);
    g_signal_connect(filebutton, "file-set"
     , G_CALLBACK(on_prefs_userStylesheetUri_file_set), prefs);
    gtk_widget_set_sensitive(filebutton, config->userStylesheet);
    FILLED_ADD(filebutton, 1, 2, 3, 4);

    // Page "Interface"
    PAGE_NEW(_("Interface"));
    FRAME_NEW(_("Navigationbar"));
    TABLE_NEW(3, 2);
    INDENTED_ADD(gtk_label_new_with_mnemonic(_("_Toolbar style")), 0, 1, 0, 1);
    combobox = gtk_combo_box_new_text();
    sokoke_combo_box_add_strings(GTK_COMBO_BOX(combobox)
     , _("Default"), _("Icons"), _("Text"), _("Both"), _("Both horizontal"), NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), config->toolbarStyle);
    g_signal_connect(combobox, "changed"
     , G_CALLBACK(on_prefs_toolbarstyle_changed), prefs);
    FILLED_ADD(combobox, 1, 2, 0, 1);
    checkbutton = gtk_check_button_new_with_mnemonic(_("Show small _icons"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->toolbarSmall);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_toolbarSmall_toggled), prefs);
    INDENTED_ADD(checkbutton, 0, 1, 1, 2);
    checkbutton = gtk_check_button_new_with_mnemonic(_("Show Web_search"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->toolbarWebSearch);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_toolbarWebSearch_toggled), prefs);
    SPANNED_ADD(checkbutton, 1, 2, 1, 2);
    checkbutton = gtk_check_button_new_with_mnemonic(_("Show _New Tab"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->toolbarNewTab);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_toolbarNewTab_toggled), prefs);
    INDENTED_ADD(checkbutton, 0, 1, 2, 3);
    checkbutton = gtk_check_button_new_with_mnemonic(_("Show _Trash"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->toolbarClosedTabs);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_toolbarClosedTabs_toggled), prefs);
    SPANNED_ADD(checkbutton, 1, 2, 2, 3);
    FRAME_NEW(_("Miscellaneous"));
    TABLE_NEW(2, 2);
    checkbutton = gtk_check_button_new_with_mnemonic(_("Close _buttons on tabs"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), config->tabClose);
    g_signal_connect(checkbutton, "toggled"
     , G_CALLBACK(on_prefs_tabClose_toggled), prefs);
    INDENTED_ADD(checkbutton, 0, 1, 0, 1);
    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox)
     , gtk_label_new_with_mnemonic(_("Tab Si_ze")), FALSE, FALSE, 4);
    spinbutton = gtk_spin_button_new_with_range(0, 36, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton), config->tabSize);
    g_signal_connect(spinbutton, "changed"
     , G_CALLBACK(on_prefs_tabSize_changed), prefs);
    gtk_box_pack_start(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 0);
    FILLED_ADD(hbox, 1, 2, 0, 1);
    INDENTED_ADD(gtk_label_new_with_mnemonic(_("_Location search engine")), 0, 1, 1, 2);
    entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), config->locationSearch);
    g_signal_connect(entry, "focus-out-event"
     , G_CALLBACK(on_prefs_locationsearch_focus_out), prefs);
    FILLED_ADD(entry, 1, 2, 1, 2);

    // Page "Network"
    PAGE_NEW(_("Network"));
    FRAME_NEW(_("Proxy Server"));
    TABLE_NEW(5, 2);
    checkbutton = gtk_check_button_new_with_mnemonic(_("_Custom proxy server"));
    gtk_widget_set_sensitive(checkbutton, FALSE); //...
    SPANNED_ADD(checkbutton, 0, 2, 0, 1);
    hbox = gtk_hbox_new(FALSE, 4);
    INDENTED_ADD(gtk_label_new_with_mnemonic(_("_Host/ Port")), 0, 1, 1, 2);
    entry = gtk_entry_new();
    gtk_widget_set_sensitive(entry, FALSE); //...
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    spinbutton = gtk_spin_button_new_with_range(0, 65535, 1);
    gtk_widget_set_sensitive(spinbutton, FALSE); //...
    gtk_box_pack_start(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 0);
    FILLED_ADD(hbox, 1, 2, 1, 2);
    checkbutton = gtk_check_button_new_with_mnemonic
     (_("Proxy requires authentication"));
    gtk_widget_set_sensitive(checkbutton, FALSE); //...
    // TODO: The proxy user and pass need to be indented further
    SPANNED_ADD(checkbutton, 0, 2, 2, 3);
    INDENTED_ADD(gtk_label_new(_("Username")), 0, 1, 3, 4);
    entry = gtk_entry_new();
    gtk_widget_set_sensitive(entry, FALSE); //...
    FILLED_ADD(entry, 1, 2, 3, 4);
    INDENTED_ADD(gtk_label_new(_("Password")), 0, 1, 4, 5);
    entry = gtk_entry_new();
    gtk_widget_set_sensitive(entry, FALSE); //...
    FILLED_ADD(entry, 1, 2, 4, 5);
    FRAME_NEW(_("Cache"));
    TABLE_NEW(1, 2);
    INDENTED_ADD(gtk_label_new(_("Cache size")), 0, 1, 0, 1);
    hbox = gtk_hbox_new(FALSE, 4);
    spinbutton = gtk_spin_button_new_with_range(0, 10000, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton), 100/*config->iCacheSize*/);
    gtk_widget_set_sensitive(spinbutton, FALSE); //...
    gtk_box_pack_start(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("MB")), FALSE, FALSE, 0);
    button = gtk_button_new_with_label(_("Clear cache"));
    gtk_widget_set_sensitive(button, FALSE); //...
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 4);
    FILLED_ADD(hbox, 1, 2, 0, 1);

    // Page "Privacy"
    PAGE_NEW(_("Privacy"));
    FRAME_NEW(_("Cookies"));
    TABLE_NEW(3, 2);
    INDENTED_ADD(gtk_label_new(_("Accept cookies")), 0, 1, 0, 1);
    combobox = gtk_combo_box_new_text();
    sokoke_combo_box_add_strings(GTK_COMBO_BOX(combobox)
     , _("All cookies"), _("Session cookies"), _("None"), NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 0); //...
    gtk_widget_set_sensitive(combobox, FALSE); //...
    FILLED_ADD(combobox, 1, 2, 0, 1);
    checkbutton = gtk_check_button_new_with_mnemonic
     (_("Allow cookies from the original website only"));
    gtk_widget_set_sensitive(checkbutton, FALSE); //...
    SPANNED_ADD(checkbutton, 0, 2, 1, 2);
    INDENTED_ADD(gtk_label_new(_("Maximum cookie age")), 0, 1, 2, 3);
    hbox = gtk_hbox_new(FALSE, 4);
    spinbutton = gtk_spin_button_new_with_range(0, 360, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton), 30/*config->iCookieAgeMax*/);
    gtk_widget_set_sensitive(spinbutton, FALSE); //...
    gtk_box_pack_start(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("days")), FALSE, FALSE, 0);
    button = gtk_button_new_with_label(_("View cookies"));
    gtk_widget_set_sensitive(button, FALSE); //...
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 4);
    FILLED_ADD(hbox, 1, 2, 2, 3);
    FRAME_NEW(_("History"));
    TABLE_NEW(3, 2);
    checkbutton = gtk_check_button_new_with_mnemonic(_("Remember my visited pages"));
    gtk_widget_set_sensitive(checkbutton, FALSE); //...
    SPANNED_ADD(checkbutton, 0, 1, 0, 1);
    hbox = gtk_hbox_new(FALSE, 4);
    spinbutton = gtk_spin_button_new_with_range(0, 360, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton), 30/*config->iHistoryAgeMax*/);
    gtk_widget_set_sensitive(spinbutton, FALSE); //...
    gtk_box_pack_start(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("days")), FALSE, FALSE, 0);
    SPANNED_ADD(hbox, 1, 2, 0, 1);
    checkbutton = gtk_check_button_new_with_mnemonic
     (_("Remember my form inputs"));
    gtk_widget_set_sensitive(checkbutton, FALSE); //...
    SPANNED_ADD(checkbutton, 0, 2, 1, 2);
    checkbutton = gtk_check_button_new_with_mnemonic
     (_("Remember my downloaded files"));
    gtk_widget_set_sensitive(checkbutton, FALSE); //...
    SPANNED_ADD(checkbutton, 0, 2, 2, 3);

    // Page "Programs"
    PAGE_NEW(_("Programs"));
    FRAME_NEW(_("External programs"));
    TABLE_NEW(3, 2);
    GtkWidget* treeview; GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text; GtkCellRenderer* renderer_pixbuf;
    GtkListStore* liststore = gtk_list_store_new(PROTOCOLS_COL_N
     , G_TYPE_STRING, G_TYPE_STRING);
    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(liststore));
    prefs->treeview = treeview;
    renderer_text = gtk_cell_renderer_text_new();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes(
     _("Protocol"), renderer_text, "text", PROTOCOLS_COL_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("Command"));
    gtk_tree_view_column_pack_start(column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func(column, renderer_pixbuf
     , (GtkTreeCellDataFunc)on_prefs_protocols_render_icon, prefs, NULL);
    renderer_text = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer_text), "editable", TRUE, NULL);
    g_signal_connect(renderer_text, "edited"
     , G_CALLBACK(on_prefs_protocols_edited), prefs);
    gtk_tree_view_column_pack_start(column, renderer_text, TRUE);
    gtk_tree_view_column_add_attribute(column, renderer_text, "text", PROTOCOLS_COL_COMMAND);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled)
     , GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), treeview);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 5);
    guint i;
    for(i = 0; i < config->protocols_names->len; i++)
    {
        gchar* protocol = (gchar*)g_ptr_array_index(config->protocols_names, i);
        // TODO: We might want to determine 'default' programs somehow
        // TODO: Any way to make it easier to add eg. only a mail client? O_o
        const gchar* command = g_datalist_get_data(&config->protocols_commands, protocol);
        gtk_list_store_insert_with_values(GTK_LIST_STORE(liststore), NULL, i
         , PROTOCOLS_COL_NAME   , protocol
         , PROTOCOLS_COL_COMMAND, command
         , -1);
    }
    g_object_unref(liststore);
    GtkWidget* vbox = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);
    combobox = gtk_combo_box_new_text();
    prefs->combobox = combobox;
    sokoke_combo_box_add_strings(GTK_COMBO_BOX(combobox)
     , "download", "ed2k", "feed", "ftp", "irc", "mailto"
     , "news", "tel", "torrent", "view-source", NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 1);
    gtk_box_pack_start(GTK_BOX(vbox), combobox, FALSE, FALSE, 0);
    button = gtk_button_new_from_stock(GTK_STOCK_ADD);
    prefs->add = button;
    g_signal_connect(combobox, "changed"
     , G_CALLBACK(on_prefs_protocols_combobox_changed), prefs);
    g_signal_connect(button, "clicked"
     , G_CALLBACK(on_prefs_protocols_add_clicked), prefs);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
    button = gtk_label_new(""); // This is an invisible separator
    gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 12);
    button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
    gtk_widget_set_sensitive(button, FALSE); //...
    gtk_box_pack_end(GTK_BOX(vbox), button, FALSE, FALSE, 0);
    FILLED_ADD(hbox, 0, 2, 0, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox)
     , notebook, FALSE, FALSE, 4);
    gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);
    return dialog;
}
