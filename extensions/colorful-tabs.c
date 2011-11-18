/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2010 Samuel Creshal <creshal@arcor.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

static void
colorful_tabs_modify_fg (GtkWidget* label,
                         GdkColor*  color)
{
    GtkWidget* box = gtk_bin_get_child (GTK_BIN (label));
    GList* children = gtk_container_get_children (GTK_CONTAINER (box));
    for (; children != NULL; children = g_list_next (children))
    {
        if (GTK_IS_LABEL (children->data))
        {
            gtk_widget_modify_fg (children->data, GTK_STATE_ACTIVE, color);
            gtk_widget_modify_fg (children->data, GTK_STATE_NORMAL, color);
            /* Also modify the label itself, for Tab Panel */
            gtk_widget_modify_fg (label, GTK_STATE_NORMAL, color);
            break;
        }
    }
    g_list_free (children);
}

static void
colorful_tabs_view_notify_uri_cb (MidoriView*      view,
                                  GParamSpec*      pspec,
                                  MidoriExtension* extension)
{
    GtkWidget* label;
    gchar* hostname;
    gchar* colorstr;
    GdkColor color;
    GdkColor fgcolor;
    GdkPixbuf* icon;

    label = midori_view_get_proxy_tab_label (view);

    if (!midori_uri_is_blank (midori_view_get_display_uri (view))
      && (hostname = midori_uri_parse_hostname (midori_view_get_display_uri (view), NULL))
      && katze_object_get_enum (view, "load-status") == MIDORI_LOAD_FINISHED)
    {
        icon = midori_view_get_icon (view);

        if (midori_view_get_icon_uri (view) != NULL)
        {
            GdkPixbuf* newpix;
            guchar* pixels;

            newpix = gdk_pixbuf_scale_simple (icon, 1, 1, GDK_INTERP_BILINEAR);
            g_return_if_fail (gdk_pixbuf_get_bits_per_sample (newpix) == 8);
            pixels = gdk_pixbuf_get_pixels (newpix);
            color.red = pixels[0] * 225;
            color.green = pixels[1] * 225;
            color.blue = pixels[2] * 225;
        }
        else
        {
            gchar* hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, hostname, 1);
            colorstr = g_strndup (hash, 6 + 1);
            g_free (hash);
            colorstr[0] = '#';
            gdk_color_parse (colorstr, &color);
        }
        g_free (hostname);

        if ((color.red   < 35000)
         && (color.green < 35000)
         && (color.blue  < 35000))
        {
            color.red   += 20000;
            color.green += 20000;
            color.blue  += 20000;
        }

        /* Ensure high contrast by enforcing black/ white text colour. */
        if ((color.red   < 41000)
         && (color.green < 41000)
         && (color.blue  < 41000))
            gdk_color_parse ("#fff", &fgcolor);
        else
            gdk_color_parse ("#000", &fgcolor);

        gtk_event_box_set_visible_window (GTK_EVENT_BOX (label), TRUE);

        colorful_tabs_modify_fg (label, &fgcolor);
        gtk_widget_modify_bg (label, GTK_STATE_NORMAL, &color);

        if (color.red < 10000)
            color.red = 5000;
        else
            color.red -= 5000;
        if (color.blue < 10000)
            color.blue = 5000;
        else
            color.blue -= 5000;
        if (color.green < 10000)
            color.green = 5000;
        else
            color.green -= 5000;

        gtk_widget_modify_bg (label, GTK_STATE_ACTIVE, &color);
    }
    else
    {
        gtk_widget_modify_bg (label, GTK_STATE_NORMAL, NULL);
        gtk_widget_modify_bg (label, GTK_STATE_ACTIVE, NULL);
        colorful_tabs_modify_fg (label, NULL);
    }
}

static void
colorful_tabs_browser_add_tab_cb (MidoriBrowser*   browser,
                                  GtkWidget*       view,
                                  MidoriExtension* extension)
{
    colorful_tabs_view_notify_uri_cb (MIDORI_VIEW (view), NULL, extension);
    g_signal_connect (view, "notify::icon",
        G_CALLBACK (colorful_tabs_view_notify_uri_cb), extension);
}

static void
colorful_tabs_app_add_browser_cb (MidoriApp*       app,
                                  MidoriBrowser*   browser,
                                  MidoriExtension* extension);

static void
colorful_tabs_deactivate_cb (MidoriExtension* extension,
                             MidoriBrowser*   browser)
{
    guint i;
    GtkWidget* view;
    MidoriApp* app = midori_extension_get_app (extension);

    g_signal_handlers_disconnect_by_func (
        app, colorful_tabs_app_add_browser_cb, extension);
    g_signal_handlers_disconnect_by_func (
        browser, colorful_tabs_browser_add_tab_cb, extension);
    g_signal_handlers_disconnect_by_func (
        extension, colorful_tabs_deactivate_cb, browser);
    i = 0;
    while ((view = midori_browser_get_nth_tab (browser, i++)))
    {
        GtkWidget* label = midori_view_get_proxy_tab_label (MIDORI_VIEW (view));
        gtk_event_box_set_visible_window (GTK_EVENT_BOX (label), FALSE);
        gtk_widget_modify_bg (label, GTK_STATE_NORMAL, NULL);
        gtk_widget_modify_bg (label, GTK_STATE_ACTIVE, NULL);
        colorful_tabs_modify_fg (label, NULL);
        g_signal_handlers_disconnect_by_func (
            view, colorful_tabs_view_notify_uri_cb, extension);
    }
}

static void
colorful_tabs_app_add_browser_cb (MidoriApp*       app,
                                  MidoriBrowser*   browser,
                                  MidoriExtension* extension)
{
    guint i;
    GtkWidget* view;

    i = 0;
    while ((view = midori_browser_get_nth_tab (browser, i++)))
        colorful_tabs_browser_add_tab_cb (browser, view, extension);
    g_signal_connect (browser, "add-tab",
        G_CALLBACK (colorful_tabs_browser_add_tab_cb), extension);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (colorful_tabs_deactivate_cb), browser);
}


static void
colorful_tabs_activate_cb (MidoriExtension* extension,
                           MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        colorful_tabs_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (colorful_tabs_app_add_browser_cb), extension);

    g_object_unref (browsers);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Colorful Tabs"),
        "description", _("Tint each tab distinctly"),
        "version", "0.5" MIDORI_VERSION_SUFFIX,
        "authors", "Christian Dywan <christian@twotoasts.de>, Samuel Creshal <creshal@arcor.de>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (colorful_tabs_activate_cb), NULL);

    return extension;
}
