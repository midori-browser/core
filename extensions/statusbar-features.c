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
statusbar_features_deactivate_cb (MidoriExtension* extension,
                                  GtkWidget*       bbox)
{
    gtk_widget_destroy (bbox);
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

    /* FIXME: Monitor each view and modify its settings individually
              instead of merely replicating the global preferences. */

    statusbar = katze_object_get_object (browser, "statusbar");
    bbox = gtk_hbox_new (FALSE, 0);
    settings = katze_object_get_object (browser, "settings");
    button = katze_property_proxy (settings, "auto-load-images", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Images"));
    #if GTK_CHECK_VERSION(2, 12, 0)
    gtk_widget_set_tooltip_text (button, _("Load images automatically"));
    #endif
    gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 2);
    gtk_widget_show (button);
    button = katze_property_proxy (settings, "enable-scripts", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Scripts"));
    #if GTK_CHECK_VERSION(2, 12, 0)
    gtk_widget_set_tooltip_text (button, _("Enable scripts"));
    #endif
    gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 2);
    gtk_widget_show (button);
    button = katze_property_proxy (settings, "enable-plugins", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Plugins"));
    #if GTK_CHECK_VERSION(2, 12, 0)
    gtk_widget_set_tooltip_text (button, _("Enable plugins"));
    #endif
    gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 2);
    gtk_widget_show (button);
    gtk_widget_show (bbox);
    gtk_box_pack_start (GTK_BOX (statusbar), bbox, FALSE, FALSE, 3);

    g_signal_connect (extension, "deactivate",
        G_CALLBACK (statusbar_features_deactivate_cb), bbox);
}

static void
statusbar_features_activate_cb (MidoriExtension* extension,
                                MidoriApp*       app)
{
    g_signal_connect (app, "add-browser",
        G_CALLBACK (statusbar_features_app_add_browser_cb), extension);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", "Statusbar Features",
        "description", "",
        "version", "0.1",
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (statusbar_features_activate_cb), NULL);

    return extension;
}
