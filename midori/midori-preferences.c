/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-preferences.h"

#include "sokoke.h"

#include <glib/gi18n.h>

struct _MidoriPreferences
{
    GtkDialog parent_instance;

    GtkWidget* notebook;
};

G_DEFINE_TYPE (MidoriPreferences, midori_preferences, GTK_TYPE_DIALOG)

enum
{
    PROP_0,

    PROP_SETTINGS
};

static void
midori_preferences_finalize (GObject* object);

static void
midori_preferences_set_property (GObject*      object,
                                 guint         prop_id,
                                 const GValue* value,
                                 GParamSpec*   pspec);

static void
midori_preferences_get_property (GObject*    object,
                                 guint       prop_id,
                                 GValue*     value,
                                 GParamSpec* pspec);

static void
midori_preferences_class_init (MidoriPreferencesClass* class)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_preferences_finalize;
    gobject_class->set_property = midori_preferences_set_property;
    gobject_class->get_property = midori_preferences_get_property;

    g_object_class_install_property (gobject_class,
                                     PROP_SETTINGS,
                                     g_param_spec_object (
                                     "settings",
                                     _("Settings"),
                                     _("Settings instance to provide properties"),
                                     MIDORI_TYPE_WEB_SETTINGS,
                                     G_PARAM_WRITABLE));
}

static void
midori_preferences_response_cb (MidoriPreferences* preferences,
                                gint               response)
{
    if (response == GTK_RESPONSE_CLOSE)
        gtk_widget_destroy (GTK_WIDGET (preferences));
}

static void
midori_preferences_init (MidoriPreferences* preferences)
{
    gchar* dialog_title;

    preferences->notebook = NULL;

    dialog_title = g_strdup_printf (_("%s Preferences"),
                                    g_get_application_name ());
    g_object_set (preferences,
                  "icon-name", GTK_STOCK_PREFERENCES,
                  "title", dialog_title,
                  "has-separator", FALSE,
                  NULL);
    gtk_dialog_add_buttons (GTK_DIALOG (preferences),
        GTK_STOCK_HELP, GTK_RESPONSE_HELP,
        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
        NULL);
    g_signal_connect (preferences, "response",
                      G_CALLBACK (midori_preferences_response_cb), NULL);

    /* TODO: Do we want tooltips for explainations or can we omit that? */
    g_free (dialog_title);
}

static void
midori_preferences_finalize (GObject* object)
{
    G_OBJECT_CLASS (midori_preferences_parent_class)->finalize (object);
}

static void
midori_preferences_set_property (GObject*      object,
                                 guint         prop_id,
                                 const GValue* value,
                                 GParamSpec*   pspec)
{
    MidoriPreferences* preferences = MIDORI_PREFERENCES (object);

    switch (prop_id)
    {
    case PROP_SETTINGS:
    {
        GtkWidget* xfce_heading;
        GtkWindow* parent;
        g_object_get (object, "transient-for", &parent, NULL);
        if ((xfce_heading = sokoke_xfce_header_new (
            gtk_window_get_icon_name (parent),
            gtk_window_get_title (GTK_WINDOW (object)))))
                gtk_box_pack_start (GTK_BOX (GTK_DIALOG (preferences)->vbox),
                                    xfce_heading, FALSE, FALSE, 0);
        midori_preferences_set_settings (preferences,
                                         g_value_get_object (value));
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_preferences_get_property (GObject*    object,
                                 guint       prop_id,
                                 GValue*     value,
                                 GParamSpec* pspec)
{
    switch (prop_id)
    {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * midori_preferences_new:
 * @parent: the parent window
 * @settings: the settings
 *
 * Creates a new preferences dialog.
 *
 * Return value: a new #MidoriPreferences
 **/
GtkWidget*
midori_preferences_new (GtkWindow*         parent,
                        MidoriWebSettings* settings)
{
    g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
    g_return_val_if_fail (MIDORI_IS_WEB_SETTINGS (settings), NULL);

    MidoriPreferences* preferences = g_object_new (MIDORI_TYPE_PREFERENCES,
                                                   "transient-for", parent,
                                                   "settings", settings,
                                                   NULL);

    return GTK_WIDGET (preferences);
}

static gboolean
proxy_download_manager_icon_cb (GtkWidget*     entry,
                                GdkEventFocus* event,
                                GtkImage*      icon)
{
    const gchar* program;
    gchar* path;

    program = gtk_entry_get_text (GTK_ENTRY (entry));
    path = g_find_program_in_path (program);

    if (path)
    {
        if (gtk_icon_theme_has_icon (gtk_icon_theme_get_for_screen (
            gtk_widget_get_screen (entry)), program))
            gtk_image_set_from_icon_name (icon, program, GTK_ICON_SIZE_MENU);
        else
            gtk_image_set_from_stock (icon, GTK_STOCK_EXECUTE, GTK_ICON_SIZE_MENU);
        g_free (path);
    }
    else if (program && *program)
        gtk_image_set_from_stock (icon, GTK_STOCK_STOP, GTK_ICON_SIZE_MENU);
    else
        gtk_image_clear (icon);

    return FALSE;
}

/**
 * midori_preferences_set_settings:
 * @settings: the settings
 *
 * Assigns a settings instance to a preferences dialog.
 *
 * Note: This must not be called more than once.
 **/
void
midori_preferences_set_settings (MidoriPreferences* preferences,
                                 MidoriWebSettings* settings)
{
    GtkSizeGroup* sizegroup;
    GtkWidget* page;
    GtkWidget* frame;
    GtkWidget* table;
    GtkWidget* align;
    GtkWidget* label;
    GtkWidget* button;
    GtkWidget* entry;
    GtkWidget* hbox;
    gint icon_width, icon_height;

    g_return_if_fail (MIDORI_IS_PREFERENCES (preferences));
    g_return_if_fail (MIDORI_IS_WEB_SETTINGS (settings));

    g_return_if_fail (!preferences->notebook);

    preferences->notebook = gtk_notebook_new ();
    gtk_container_set_border_width (GTK_CONTAINER (preferences->notebook), 6);
    sizegroup = NULL;
    #define PAGE_NEW(__label) page = gtk_vbox_new (FALSE, 0); \
     if (sizegroup) g_object_unref (sizegroup); \
     sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL); \
     gtk_container_set_border_width (GTK_CONTAINER (page), 4); \
     gtk_notebook_append_page (GTK_NOTEBOOK (preferences->notebook), page, \
                               gtk_label_new (__label))
    #define FRAME_NEW(__label) frame = sokoke_hig_frame_new (__label); \
     gtk_container_set_border_width (GTK_CONTAINER (frame), 4); \
     gtk_box_pack_start (GTK_BOX (page), frame, FALSE, FALSE, 0);
    #define TABLE_NEW(__rows, __cols) table = gtk_table_new ( \
                                       __rows, __cols, FALSE); \
     gtk_container_set_border_width (GTK_CONTAINER (table), 4); \
     gtk_container_add (GTK_CONTAINER (frame), table);
    #define WIDGET_ADD(__widget, __left, __right, __top, __bottom) \
     gtk_table_attach (GTK_TABLE (table), __widget \
      , __left, __right, __top, __bottom \
      , GTK_FILL, GTK_FILL, 8, 2)
    #define FILLED_ADD(__widget, __left, __right, __top, __bottom) \
     gtk_table_attach (GTK_TABLE (table), __widget \
      , __left, __right, __top, __bottom\
      , GTK_EXPAND | GTK_FILL, GTK_FILL, 8, 2)
    #define INDENTED_ADD(__widget, __left, __right, __top, __bottom) \
     align = gtk_alignment_new (0, 0.5, 0, 0); \
     gtk_container_add (GTK_CONTAINER (align), __widget); \
     gtk_size_group_add_widget (sizegroup, align); \
     WIDGET_ADD (align, __left, __right, __top, __bottom)
    #define SPANNED_ADD(__widget, __left, __right, __top, __bottom) \
     align = gtk_alignment_new (0, 0.5, 0, 0); \
     gtk_container_add (GTK_CONTAINER (align), __widget); \
     FILLED_ADD (align, __left, __right, __top, __bottom)
    /* Page "General" */
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
    /* TODO: We need something like "use current website" */
    FRAME_NEW (_("Transfers"));
    TABLE_NEW (1, 2);
    label = katze_property_label (settings, "download-folder");
    INDENTED_ADD (label, 0, 1, 0, 1);
    button = katze_property_proxy (settings, "download-folder", "folder");
    FILLED_ADD (button, 1, 2, 0, 1);
    label = katze_property_label (settings, "download-manager");
    INDENTED_ADD (label, 0, 1, 1, 2);
    hbox = gtk_hbox_new (FALSE, 4);
    button = gtk_image_new ();
    gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (button),
        GTK_ICON_SIZE_MENU, &icon_width, &icon_height);
    gtk_widget_set_size_request (button, icon_width, icon_height);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 4);
    entry = katze_property_proxy (settings, "download-manager", NULL);
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
    proxy_download_manager_icon_cb (entry, NULL, GTK_IMAGE (button));
    g_signal_connect (entry, "focus-out-event",
        G_CALLBACK (proxy_download_manager_icon_cb), button);
    FILLED_ADD (hbox, 1, 2, 1, 2);

    /* Page "Appearance" */
    PAGE_NEW (_("Appearance"));
    FRAME_NEW (_("Font settings"));
    TABLE_NEW (5, 2);
    label = katze_property_label (settings, "default-font-family");
    INDENTED_ADD (label, 0, 1, 0, 1);
    hbox = gtk_hbox_new (FALSE, 4);
    button = katze_property_proxy (settings, "default-font-family", "font");
    gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
    entry = katze_property_proxy (settings, "default-font-size", NULL);
    gtk_box_pack_end (GTK_BOX (hbox), entry, FALSE, FALSE, 4);
    FILLED_ADD (hbox, 1, 2, 0, 1);
    label = katze_property_label (settings, "minimum-font-size");
    INDENTED_ADD (label, 0, 1, 1, 2);
    entry = katze_property_proxy (settings, "minimum-font-size", NULL);
    INDENTED_ADD (entry, 1, 2, 1, 2);
    label = katze_property_label (settings, "preferred-encoding");
    INDENTED_ADD (label, 0, 1, 2, 3);
    button = katze_property_proxy (settings, "preferred-encoding", NULL);
    FILLED_ADD (button, 1, 2, 2, 3);

    /* Page "Behavior" */
    PAGE_NEW (_("Behavior"));
    FRAME_NEW (_("Features"));
    TABLE_NEW (6, 2);
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
    SPANNED_ADD (button, 1, 2, 2, 3);
    label = katze_property_label (settings, "location-entry-search");
    INDENTED_ADD (label, 0, 1, 3, 4);
    entry = katze_property_proxy (settings, "location-entry-search", NULL);
    FILLED_ADD (entry, 1, 2, 3, 4);

    /* Page "Interface" */
    PAGE_NEW (_("Interface"));
    FRAME_NEW (_("Navigationbar"));
    TABLE_NEW (3, 2);
    INDENTED_ADD (katze_property_label (settings, "toolbar-style"), 0, 1, 0, 1);
    button = katze_property_proxy (settings, "toolbar-style", NULL);
    FILLED_ADD (button, 1, 2, 0, 1);
    INDENTED_ADD (katze_property_label (settings, "toolbar-items"), 0, 1, 1, 2);
    button = katze_property_proxy (settings, "toolbar-items", NULL);
    FILLED_ADD (button, 1, 2, 1, 2);
    button = katze_property_proxy (settings, "always-show-tabbar", NULL);
    INDENTED_ADD (button, 0, 1, 2, 3);
    FRAME_NEW (_("Browsing"));
    TABLE_NEW (3, 2);
    /* label = katze_property_label (settings, "open-new-pages-in");
    INDENTED_ADD (label, 0, 1, 0, 1);
    button = katze_property_proxy (settings, "open-new-pages-in", NULL);
    FILLED_ADD (button, 1, 2, 0, 1); */
    button = katze_property_proxy (settings, "middle-click-opens-selection", NULL);
    INDENTED_ADD (button, 0, 1, 1, 2);
    button = katze_property_proxy (settings, "open-tabs-in-the-background", NULL);
    WIDGET_ADD (button, 1, 2, 1, 2);
    /* button = katze_property_proxy (settings, "open-popups-in-tabs", NULL);
    SPANNED_ADD (button, 0, 1, 2, 3);*/
    button = katze_property_proxy (settings, "open-tabs-next-to-current", NULL);
    WIDGET_ADD (button, 0, 1, 2, 3);
    button = katze_property_proxy (settings, "close-buttons-on-tabs", NULL);
    WIDGET_ADD (button, 1, 2, 2, 3);

    /* Page "Network" */
    /*PAGE_NEW (_("Network"));
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
    FILLED_ADD (hbox, 1, 2, 1, 2);*/

    /* Page "Privacy" */
    PAGE_NEW (_("Privacy"));
    /*FRAME_NEW (_("Web Cookies"));
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
    FILLED_ADD (hbox, 1, 2, 2, 3);*/
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

    g_object_unref (sizegroup);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (preferences)->vbox),
                        preferences->notebook, FALSE, FALSE, 4);
    gtk_widget_show_all (GTK_DIALOG (preferences)->vbox);
}
