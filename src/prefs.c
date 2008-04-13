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
#include "sokoke.h"

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

static void
clear_button_clicked_cb (GtkWidget* button, GtkWidget* file_chooser)
{
    gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (file_chooser), "");
    // Emit "file-set" manually for Gtk doesn't emit it otherwise
    g_signal_emit_by_name (file_chooser, "file-set");
}

GtkWidget* prefs_preferences_dialog_new (GtkWindow* window,
                                         MidoriWebSettings* settings)
{
    gchar* dialogTitle = g_strdup_printf(_("%s Preferences"), g_get_application_name());
    GtkWidget* dialog = gtk_dialog_new_with_buttons(dialogTitle
        , window
        , GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR
        , GTK_STOCK_HELP, GTK_RESPONSE_HELP
        , GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE
        , NULL);
    gtk_window_set_icon_name(GTK_WINDOW(dialog), GTK_STOCK_PREFERENCES);
    // TODO: Implement some kind of help function
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_HELP, FALSE); //...
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);

    CPrefs* prefs = g_new0(CPrefs, 1);
    g_signal_connect(dialog, "response", G_CALLBACK(g_free), prefs);

    // TODO: Do we want tooltips for explainations or can we omit that?
    // TODO: We need mnemonics
    // TODO: Take multiple windows into account when applying changes
    GtkWidget* xfce_heading;
    if((xfce_heading = sokoke_xfce_header_new(
     gtk_window_get_icon_name(window), dialogTitle)))
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox)
         , xfce_heading, FALSE, FALSE, 0);
    g_free(dialogTitle);
    GtkWidget* notebook = gtk_notebook_new();
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 6);
    GtkSizeGroup* sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    GtkWidget* page; GtkWidget* frame; GtkWidget* table; GtkWidget* align;
    GtkWidget* label; GtkWidget* button;
    GtkWidget* entry; GtkWidget* hbox;
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
    #define SPANNED_ADD(__widget, __left, __right, __top, __bottom)\
     align = gtk_alignment_new(0, 0.5, 0, 0);\
     gtk_container_add(GTK_CONTAINER(align), __widget);\
     FILLED_ADD(align, __left, __right, __top, __bottom)
    // Page "General"
    PAGE_NEW (_("General"));
    FRAME_NEW (_("Startup"));
    TABLE_NEW (2, 2);
    label = katze_property_label (settings, "load-on-startup");
    INDENTED_ADD (label, 0, 1, 0, 1);
    button = katze_property_proxy (settings, "load-on-startup", NULL);
    FILLED_ADD (button, 1, 2, 0, 1);
    label = katze_property_label (settings, "homepage");
    INDENTED_ADD (label, 0, 1, 1, 2);
    entry = katze_property_proxy (settings, "homepage", NULL);
    FILLED_ADD (entry, 1, 2, 1, 2);
    // TODO: We need something like "use current website"
    FRAME_NEW (_("Transfers"));
    TABLE_NEW (1, 2);
    label = katze_property_label (settings, "download-folder");
    INDENTED_ADD (label, 0, 1, 0, 1);
    button = katze_property_proxy (settings, "download-folder", "folder");
    FILLED_ADD (button, 1, 2, 0, 1);
    button = katze_property_proxy (settings, "show-download-notification", "blurb");
    SPANNED_ADD (button, 0, 2, 1, 2);

    // Page "Appearance"
    PAGE_NEW (_("Appearance"));
    FRAME_NEW (_("Font settings"));
    TABLE_NEW (5, 2);
    label = katze_property_label (settings, "default-font-family");
    INDENTED_ADD (label, 0, 1, 0, 1);
    hbox = gtk_hbox_new (FALSE, 4);
    button = katze_property_proxy (settings, "default-font-family", NULL);
    gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
    entry = katze_property_proxy (settings, "default-font-size", NULL);
    gtk_box_pack_end (GTK_BOX (hbox), entry, FALSE, FALSE, 4);
    FILLED_ADD (hbox, 1, 2, 0, 1);
    label = katze_property_label (settings, "minimum-font-size");
    INDENTED_ADD (label, 0, 1, 1, 2);
    hbox = gtk_hbox_new (FALSE, 4);
    entry = katze_property_proxy (settings, "minimum-font-size", NULL);
    INDENTED_ADD (entry, 1, 2, 1, 2);
    label = katze_property_label (settings, "preferred-encoding");
    INDENTED_ADD (label, 0, 1, 2, 3);
    button = katze_property_proxy (settings, "preferred-encoding", NULL);
    FILLED_ADD (button, 1, 2, 2, 3);

    // Page "Behavior"
    PAGE_NEW (_("Behavior"));
    FRAME_NEW (_("Features"));
    TABLE_NEW (5, 2);
    button = katze_property_proxy (settings, "auto-load-images", NULL);
    INDENTED_ADD (button, 0, 1, 0, 1);
    button = katze_property_proxy (settings, "auto-shrink-images", NULL);
    SPANNED_ADD (button, 1, 2, 0, 1);
    button = katze_property_proxy (settings, "print-backgrounds", NULL);
    INDENTED_ADD (button, 0, 1, 1, 2);
    button = katze_property_proxy (settings, "resizable-text-areas", NULL);
    SPANNED_ADD (button, 1, 2, 1, 2);
    button = katze_property_proxy (settings, "enable-scripts", NULL);
    INDENTED_ADD (button, 0, 1, 2, 3);
    button = katze_property_proxy (settings, "enable-plugins", NULL);
    SPANNED_ADD(button, 1, 2, 2, 3);
    label = katze_property_label (settings, "user-stylesheet-uri");
    INDENTED_ADD (label, 0, 1, 3, 4);
    hbox = gtk_hbox_new (FALSE, 4);
    entry = katze_property_proxy (settings, "user-stylesheet-uri", "uri");
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
    button = gtk_button_new ();
    gtk_container_add (GTK_CONTAINER (button),
        gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU));
    g_signal_connect (button, "clicked",
                      G_CALLBACK (clear_button_clicked_cb), entry);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 4);
    FILLED_ADD (hbox, 1, 2, 3, 4);
    label = katze_property_label (settings, "location-entry-search");
    INDENTED_ADD (label, 0, 1, 4, 5);
    entry = katze_property_proxy (settings, "location-entry-search", NULL);
    FILLED_ADD (entry, 1, 2, 4, 5);

    // Page "Interface"
    PAGE_NEW (_("Interface"));
    FRAME_NEW (_("Navigationbar"));
    TABLE_NEW (3, 2);
    INDENTED_ADD (katze_property_label (settings, "toolbar-style"), 0, 1, 0, 1);
    button = katze_property_proxy (settings, "toolbar-style", NULL);
    FILLED_ADD(button, 1, 2, 0, 1);
    button = katze_property_proxy (settings, "small-toolbar", NULL);
    INDENTED_ADD (button, 0, 1, 1, 2);
    button = katze_property_proxy (settings, "show-web-search", NULL);
    SPANNED_ADD (button, 1, 2, 1, 2);
    button = katze_property_proxy (settings, "show-new-tab", NULL);
    INDENTED_ADD (button, 0, 1, 2, 3);
    button = katze_property_proxy (settings, "show-trash", NULL);
    SPANNED_ADD (button, 1, 2, 2, 3);
    FRAME_NEW(_("Browsing"));
    TABLE_NEW (3, 2);
    label = katze_property_label (settings, "open-new-pages-in");
    INDENTED_ADD (label, 0, 1, 0, 1);
    button = katze_property_proxy (settings, "open-new-pages-in", NULL);
    FILLED_ADD (button, 1, 2, 0, 1);
    button = katze_property_proxy (settings, "middle-click-opens-selection", NULL);
    INDENTED_ADD (button, 0, 1, 1, 2);
    button = katze_property_proxy (settings, "open-tabs-in-the-background", NULL);
    SPANNED_ADD (button, 1, 2, 1, 2);
    button = katze_property_proxy (settings, "open-popups-in-tabs", NULL);
    SPANNED_ADD (button, 0, 1, 2, 3);
    button = katze_property_proxy (settings, "close-buttons-on-tabs", NULL);
    SPANNED_ADD (button, 1, 2, 2, 3);

    // Page "Network"
    PAGE_NEW (_("Network"));
    FRAME_NEW (_("Network"));
    TABLE_NEW (2, 2);
    label = katze_property_label (settings, "http-proxy");
    INDENTED_ADD (label, 0, 1, 0, 1);
    button = katze_property_proxy (settings, "http-proxy", NULL);
    FILLED_ADD (button, 1, 2, 0, 1);
    label = katze_property_label (settings, "cache-size");
    INDENTED_ADD (label, 0, 1, 1, 2);
    hbox = gtk_hbox_new (FALSE, 4);
    entry = katze_property_proxy (settings, "cache-size", NULL);
    gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("MB")),
                        FALSE, FALSE, 0);
    FILLED_ADD (hbox, 1, 2, 1, 2);

    // Page "Privacy"
    PAGE_NEW (_("Privacy"));
    FRAME_NEW (_("Cookies"));
    TABLE_NEW (3, 2);
    label = katze_property_label (settings, "accept-cookies");
    INDENTED_ADD (label, 0, 1, 0, 1);
    button = katze_property_proxy (settings, "accept-cookies", NULL);
    FILLED_ADD (button, 1, 2, 0, 1);
    button = katze_property_proxy (settings, "original-cookies-only", "blurb");
    SPANNED_ADD (button, 0, 2, 1, 2);
    label = katze_property_label (settings, "maximum-cookie-age");
    INDENTED_ADD (label, 0, 1, 2, 3);
    hbox = gtk_hbox_new (FALSE, 4);
    entry = katze_property_proxy (settings, "maximum-cookie-age", NULL);
    gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("days")),
                        FALSE, FALSE, 0);
    FILLED_ADD (hbox, 1, 2, 2, 3);
    FRAME_NEW (_("History"));
    TABLE_NEW (3, 2);
    button = katze_property_proxy (settings, "remember-last-visited-pages", NULL);
    SPANNED_ADD (button, 0, 1, 0, 1);
    hbox = gtk_hbox_new (FALSE, 4);
    button = katze_property_proxy (settings, "maximum-history-age", NULL);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("days")),
                        FALSE, FALSE, 0);
    SPANNED_ADD (hbox, 1, 2, 0, 1);
    button = katze_property_proxy (settings, "remember-last-form-inputs", NULL);
    SPANNED_ADD (button, 0, 2, 1, 2);
    button = katze_property_proxy (settings, "remember-last-downloaded-files", NULL);
    SPANNED_ADD (button, 0, 2, 2, 3);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox)
     , notebook, FALSE, FALSE, 4);
    gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);
    return dialog;
}
