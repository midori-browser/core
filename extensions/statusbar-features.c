/*
 Copyright (C) 2008-2010 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>
#include <midori/sokoke.h>

static void
statusbar_features_app_add_browser_cb (MidoriApp*       app,
                                       MidoriBrowser*   browser,
                                       MidoriExtension* extension);

static void
statusbar_features_toolbar_notify_toolbar_style_cb (GtkWidget*  toolbar,
                                                    GParamSpec* pspec,
                                                    GtkWidget*  button)
{
    GtkToolbarStyle style = katze_object_get_enum (toolbar, "toolbar-style");
    const gchar* text = g_object_get_data (G_OBJECT (button), "feature-label");
    switch (style)
    {
        case GTK_TOOLBAR_BOTH:
        case GTK_TOOLBAR_BOTH_HORIZ:
            gtk_button_set_label (GTK_BUTTON (button), text);
            gtk_widget_show (gtk_button_get_image (GTK_BUTTON (button)));
            break;
        case GTK_TOOLBAR_TEXT:
            gtk_button_set_label (GTK_BUTTON (button), text);
            gtk_widget_hide (gtk_button_get_image (GTK_BUTTON (button)));
            break;
        case GTK_TOOLBAR_ICONS:
            gtk_button_set_label (GTK_BUTTON (button), "");
            gtk_widget_show (gtk_button_get_image (GTK_BUTTON (button)));
            break;
        default:
            g_assert_not_reached ();
    }
}

static void
statusbar_features_deactivate_cb (MidoriExtension* extension,
                                  GtkWidget*       bbox)
{
    MidoriApp* app = midori_extension_get_app (extension);
    MidoriBrowser* browser = midori_browser_get_for_widget (bbox);
    GtkWidget* toolbar = katze_object_get_object (browser, "navigationbar");

    gtk_widget_destroy (bbox);
    g_signal_handlers_disconnect_matched (toolbar, G_SIGNAL_MATCH_FUNC,
        0, -1, NULL, statusbar_features_toolbar_notify_toolbar_style_cb, NULL);
    g_object_unref (toolbar);
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
    GtkWidget* toolbar;
    GtkWidget* button;
    GtkWidget* image;

    /* FIXME: Monitor each view and modify its settings individually
              instead of merely replicating the global preferences. */

    statusbar = katze_object_get_object (browser, "statusbar");
    bbox = gtk_hbox_new (FALSE, 0);
    settings = midori_browser_get_settings (browser);
    toolbar = katze_object_get_object (browser, "navigationbar");
    button = katze_property_proxy (settings, "auto-load-images", "toggle");
    g_object_set_data (G_OBJECT (button), "feature-label", _("Images"));
    image = gtk_image_new_from_stock (STOCK_IMAGE, GTK_ICON_SIZE_MENU);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_tooltip_text (button, _("Load images automatically"));
    statusbar_features_toolbar_notify_toolbar_style_cb (toolbar, NULL, button);
    g_signal_connect (toolbar, "notify::toolbar-style",
        G_CALLBACK (statusbar_features_toolbar_notify_toolbar_style_cb), button);
    gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 2);
    button = katze_property_proxy (settings, "enable-scripts", "toggle");
    g_object_set_data (G_OBJECT (button), "feature-label", _("Scripts"));
    image = gtk_image_new_from_stock (STOCK_SCRIPTS, GTK_ICON_SIZE_MENU);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_tooltip_text (button, _("Enable scripts"));
    statusbar_features_toolbar_notify_toolbar_style_cb (toolbar, NULL, button);
    g_signal_connect (toolbar, "notify::toolbar-style",
        G_CALLBACK (statusbar_features_toolbar_notify_toolbar_style_cb), button);
    gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 2);
    button = katze_property_proxy (settings, "enable-plugins", "toggle");
    g_object_set_data (G_OBJECT (button), "feature-label", _("Netscape plugins"));
    image = gtk_image_new_from_stock (STOCK_PLUGINS, GTK_ICON_SIZE_MENU);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_tooltip_text (button, _("Enable Netscape plugins"));
    statusbar_features_toolbar_notify_toolbar_style_cb (toolbar, NULL, button);
    g_signal_connect (toolbar, "notify::toolbar-style",
        G_CALLBACK (statusbar_features_toolbar_notify_toolbar_style_cb), button);
    gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 2);
    button = katze_property_proxy (settings, "identify-as", NULL);
    gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 2);
    gtk_widget_show_all (bbox);
    gtk_box_pack_start (GTK_BOX (statusbar), bbox, FALSE, FALSE, 3);
    g_object_unref (statusbar);

    g_signal_connect (extension, "deactivate",
        G_CALLBACK (statusbar_features_deactivate_cb), bbox);
}

static void
statusbar_features_activate_cb (MidoriExtension* extension,
                                MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        statusbar_features_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (statusbar_features_app_add_browser_cb), extension);
    g_object_unref (browsers);
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
