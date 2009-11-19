/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

static void
colorful_tabs_view_notify_uri_cb (MidoriView*      view,
                                  GParamSpec*      pspec,
                                  MidoriExtension* extension)
{
    GtkWidget* label;
    SoupURI* uri;
    gchar* hash;
    gchar* colorstr;
    GdkColor color;

    label = midori_view_get_proxy_tab_label (view);

    /* Find a color that is unique to an address. We merely compute
       a hash value, pick the first 6 + 1 characters and turn the
       first into a hash sign, ie. #8b424b. In case a color is too
       dark, we lighten it up a litte. Finally we make the event box
       visible and modify its background. */

    if ((uri = soup_uri_new (midori_view_get_display_uri (view))) && uri->host)
    {
        hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri->host, -1);
        soup_uri_free (uri);
        colorstr = g_strndup (hash, 6 + 1);
        g_free (hash);
        colorstr[0] = '#';
        gdk_color_parse (colorstr, &color);
        if (color.red < 35000)
            color.red += 25000 + (color.blue + 1) / 2;
        if (color.green < 35000)
            color.green += 25000 + (color.red + 1) / 2;
        if (color.blue < 35000)
            color.blue += 25000 + (color.green + 1) / 2;
        gtk_event_box_set_visible_window (GTK_EVENT_BOX (label), TRUE);
        gtk_widget_modify_bg (label, GTK_STATE_NORMAL, &color);
        gtk_widget_modify_bg (label, GTK_STATE_ACTIVE, &color);
    }
    else
    {
        gtk_widget_modify_bg (label, GTK_STATE_NORMAL, NULL);
        gtk_widget_modify_bg (label, GTK_STATE_ACTIVE, NULL);
    }
}
static void
colorful_tabs_browser_add_tab_cb (MidoriBrowser*   browser,
                                  GtkWidget*       view,
                                  MidoriExtension* extension)
{
    colorful_tabs_view_notify_uri_cb (MIDORI_VIEW (view), NULL, extension);
    g_signal_connect (view, "notify::uri",
        G_CALLBACK (colorful_tabs_view_notify_uri_cb), extension);
}

static void
colorful_tabs_deactivate_cb (MidoriExtension* extension,
                             MidoriBrowser*   browser)
{
    guint i;
    GtkWidget* view;

    g_signal_handlers_disconnect_by_func (
        extension, colorful_tabs_deactivate_cb, browser);
    i = 0;
    while ((view = midori_browser_get_nth_tab (browser, i++)))
    {
        GtkWidget* label = midori_view_get_proxy_tab_label (MIDORI_VIEW (view));
        gtk_event_box_set_visible_window (GTK_EVENT_BOX (label), FALSE);
        gtk_widget_modify_bg (label, GTK_STATE_NORMAL, NULL);
        gtk_widget_modify_bg (label, GTK_STATE_ACTIVE, NULL);
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
    guint i;

    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++)))
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
        "version", "0.1",
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (colorful_tabs_activate_cb), NULL);

    return extension;
}
