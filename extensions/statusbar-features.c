/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

static void
statusbar_features_app_add_browser_cb (MidoriApp*       app,
                                       MidoriBrowser*   browser,
                                       MidoriExtension* extension);

static void
statusbar_features_deactivate_cb (MidoriExtension* extension,
                                  GtkWidget*       bbox)
{
    MidoriApp* app = midori_extension_get_app (extension);

    gtk_widget_destroy (bbox);
    g_signal_handlers_disconnect_by_func (
        extension, statusbar_features_deactivate_cb, bbox);
    g_signal_handlers_disconnect_by_func (
        app, statusbar_features_app_add_browser_cb, extension);
}

static void
statusbar_features_app_add_browser_cb (MidoriApp*       app,
                                       MidoriBrowser*   browser,
                                       MidoriExtension* extension)
{
    GtkWidget* statusbar;
    GtkWidget* bbox;
    MidoriWebSettings* settings;
    GtkWidget* button;
    GtkWidget* image;

    /* FIXME: Monitor each view and modify its settings individually
              instead of merely replicating the global preferences. */

    statusbar = katze_object_get_object (browser, "statusbar");
    bbox = gtk_hbox_new (FALSE, 0);
    settings = katze_object_get_object (browser, "settings");
    button = katze_property_proxy (settings, "auto-load-images", "toggle");
    image = gtk_image_new_from_stock (STOCK_IMAGE, GTK_ICON_SIZE_MENU);
    gtk_widget_show (image);
    gtk_container_add (GTK_CONTAINER (button), image);
    #if GTK_CHECK_VERSION(2, 12, 0)
    gtk_widget_set_tooltip_text (button, _("Load images automatically"));
    #endif
    gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 2);
    gtk_widget_show (button);
    button = katze_property_proxy (settings, "enable-scripts", "toggle");
    image = gtk_image_new_from_stock (STOCK_SCRIPTS, GTK_ICON_SIZE_MENU);
    gtk_widget_show (image);
    gtk_container_add (GTK_CONTAINER (button), image);
    #if GTK_CHECK_VERSION(2, 12, 0)
    gtk_widget_set_tooltip_text (button, _("Enable scripts"));
    #endif
    gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 2);
    gtk_widget_show (button);
    button = katze_property_proxy (settings, "enable-plugins", "toggle");
    image = gtk_image_new_from_stock (STOCK_PLUGINS, GTK_ICON_SIZE_MENU);
    gtk_widget_show (image);
    gtk_container_add (GTK_CONTAINER (button), image);
    #if GTK_CHECK_VERSION(2, 12, 0)
    gtk_widget_set_tooltip_text (button, _("Enable Netscape plugins"));
    #endif
    gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 2);
    gtk_widget_show (button);
    gtk_widget_show (bbox);
    gtk_box_pack_start (GTK_BOX (statusbar), bbox, FALSE, FALSE, 3);
    g_object_unref (settings);

    g_signal_connect (extension, "deactivate",
        G_CALLBACK (statusbar_features_deactivate_cb), bbox);
}

static void
statusbar_features_activate_cb (MidoriExtension* extension,
                                MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;
    guint i;

    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++)))
        statusbar_features_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (statusbar_features_app_add_browser_cb), extension);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Statusbar Features"),
        "description", _("Easily toggle features on web pages on and off"),
        "version", "0.1",
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (statusbar_features_activate_cb), NULL);

    return extension;
}
