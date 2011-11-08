/*
 Copyright (C) 2008-2011 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

typedef struct
{
    const gchar* label;
    gdouble level;
} ZoomLevel;

const ZoomLevel zoom_levels[] =
{
    { "200%", 2.0  },
    { "175%", 1.75 },
    { "150%", 1.5  },
    { "125%", 1.25 },
    { "100%", 1.0  },
    { "50%" , 0.5  },
    { "25%" , 0.25 }
};

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
statusbar_features_browser_notify_tab_cb (MidoriBrowser* browser,
                                          GParamSpec*    pspec,
                                          GtkWidget*     combobox)
{
    MidoriView* view = MIDORI_VIEW (midori_browser_get_current_tab (browser));
    gchar* zoom_level_text = g_strdup_printf ("%d%%",
        (gint)(midori_view_get_zoom_level (view) * 100));
    gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combobox))),
        zoom_level_text);
    g_free (zoom_level_text);
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
    g_signal_handlers_disconnect_matched (browser, G_SIGNAL_MATCH_FUNC,
        0, -1, NULL, statusbar_features_browser_notify_tab_cb, NULL);
}

static void
statusbar_features_zoom_level_changed_cb (GtkWidget*     combobox,
                                          MidoriBrowser* browser)
{
    MidoriView* view = MIDORI_VIEW (midori_browser_get_current_tab (browser));
    GtkWidget* entry = gtk_bin_get_child (GTK_BIN (combobox));
    const gchar* zoom_level_text = gtk_entry_get_text (GTK_ENTRY (entry));
    gdouble zoom_level = g_ascii_strtod (zoom_level_text, NULL);
    midori_view_set_zoom_level (view, zoom_level / 100.0);
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
    gsize i;

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
    button = katze_property_proxy (settings, "identify-as", "custom-user-agent");
    gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 2);
    button = gtk_combo_box_text_new_with_entry ();
    gtk_entry_set_width_chars (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (button))), 4);
    for (i = 0; i < G_N_ELEMENTS (zoom_levels); i++)
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (button), zoom_levels[i].label);
    gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 2);
    g_signal_connect (button, "changed",
        G_CALLBACK (statusbar_features_zoom_level_changed_cb), browser);
    g_signal_connect (browser, "notify::tab",
        G_CALLBACK (statusbar_features_browser_notify_tab_cb), button);
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
        "version", "0.1" MIDORI_VERSION_SUFFIX,
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (statusbar_features_activate_cb), NULL);

    return extension;
}
