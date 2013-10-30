/*
 Copyright (C) 2009-2013 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2010 Samuel Creshal <creshal@arcor.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

static void
get_foreground_color_for_GdkColor (GdkColor* color,
                                   GdkColor* fgcolor)
{
    gfloat brightness, r, g, b;

    r = color->red / 255;
    g = color->green / 255;
    b = color->blue / 255;

    /* For math used see algorithms for converting from rgb to yuv */
    brightness = 0.299 * r + 0.587 * g + 0.114 * b;

    /* Ensure high contrast by enforcing black/ white text colour. */
    /* Brigthness (range 0-255) equals value of y from YUV color space. */
    if (brightness < 128)
        gdk_color_parse ("white", fgcolor);
    else
        gdk_color_parse ("black", fgcolor);
}

static void
adjust_brightness (GdkColor* color)
{
    guint dark_grey = 137 * 255;
    guint adjustment = 78 * 255;
    guint blue = 39 * 255;
    guint readjust = 19 * 255;

    if ((color->red   < dark_grey)
     && (color->green < dark_grey)
     && (color->blue  < dark_grey))
    {
        color->red   += adjustment;
        color->green += adjustment;
        color->blue  += adjustment;
    }

    if (color->red < blue)
        color->red = readjust;
    else
        color->red -= readjust;

    if (color->blue < blue)
        color->blue = readjust;
    else
        color->blue -= readjust;

    if (color->green < blue)
        color->green = readjust;
    else
        color->green -= readjust;
}

static void
view_get_bgcolor_for_favicon (GdkPixbuf* icon,
                              GdkColor*  color)
{
    GdkPixbuf* newpix;
    guchar* pixels;

    newpix = gdk_pixbuf_scale_simple (icon, 1, 1, GDK_INTERP_BILINEAR);
    pixels = gdk_pixbuf_get_pixels (newpix);
    color->red = pixels[0] * 255;
    color->green = pixels[1] * 255;
    color->blue = pixels[2] * 255;

    adjust_brightness (color);
}

static void
view_get_bgcolor_for_hostname (gchar*    hostname,
                               GdkColor* color)
{
    gchar* hash, *colorstr;

    hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, hostname, 1);
    colorstr = g_strndup (hash, 6 + 1);
    colorstr[0] = '#';
    gdk_color_parse (colorstr, color);

    g_free (hash);
    g_free (colorstr);

    adjust_brightness (color);
}

static void
colorful_tabs_view_notify_uri_cb (MidoriView*      view,
                                  GParamSpec*      pspec,
                                  MidoriExtension* extension)
{
    const gchar* uri = midori_view_get_display_uri (view);
    if (!*uri)
        return;

    if (!midori_uri_is_blank (uri))
    {
        gchar* hostname = midori_uri_parse_hostname (uri, NULL);
        if (hostname)
        {
            GdkColor fgcolor, color;
            GdkPixbuf* icon = midori_view_get_icon (view);

            if (icon)
                view_get_bgcolor_for_favicon (icon, &color);
            else
                view_get_bgcolor_for_hostname (hostname, &color);

            get_foreground_color_for_GdkColor (&color, &fgcolor);
            midori_view_set_colors (view, &fgcolor, &color);

            g_free (hostname);
        }
    }
    else
        midori_view_set_colors (view, NULL, NULL);
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
    GList* children;
    MidoriApp* app = midori_extension_get_app (extension);

    g_signal_handlers_disconnect_by_func (
        app, colorful_tabs_app_add_browser_cb, extension);
    g_signal_handlers_disconnect_by_func (
        browser, colorful_tabs_browser_add_tab_cb, extension);
    g_signal_handlers_disconnect_by_func (
        extension, colorful_tabs_deactivate_cb, browser);

    children = midori_browser_get_tabs (MIDORI_BROWSER (browser));
    for (; children; children = g_list_next (children))
    {
        midori_view_set_colors (children->data, NULL, NULL);
        g_signal_handlers_disconnect_by_func (
            children->data, colorful_tabs_view_notify_uri_cb, extension);
    }
    g_list_free (children);
}

static void
colorful_tabs_app_add_browser_cb (MidoriApp*       app,
                                  MidoriBrowser*   browser,
                                  MidoriExtension* extension)
{
    GList* children;

    children = midori_browser_get_tabs (MIDORI_BROWSER (browser));
    for (; children; children = g_list_next (children))
        colorful_tabs_browser_add_tab_cb (browser, children->data, extension);
    g_list_free (children);

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

void test_colour_for_hostname (void)
{
    GdkColor color;
    GdkColor fgcolor;

    typedef struct
    {
        const gchar* host;
        const gchar* fgcolor;
        const gchar* color;
    } ColorItem;

    static const ColorItem items[] = {
     { "www.last.fm", "#ffffffffffff", "#12ed7da312ed" },
     { "git.xfce.org", "#ffffffffffff", "#1c424c72e207" },
     { "elementaryos.org", "#000000000000", "#50dbac36b43e" },
     { "news.ycombinator.com", "#000000000000", "#a5cba6cc5278" },
     { "cgit.freedesktop.org", "#000000000000", "#95bb8db37ca2" },
     { "get.cm", "#ffffffffffff", "#1c424c72e207" },
    };

    guint i;
    for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
        view_get_bgcolor_for_hostname ((gchar*)items[i].host, &color);
        get_foreground_color_for_GdkColor (&color, &fgcolor);

        g_assert_cmpstr (items[i].color, ==, gdk_color_to_string (&color));
        g_assert_cmpstr (items[i].fgcolor, ==, gdk_color_to_string (&fgcolor));
    }
}

void
extension_test (void)
{
    #ifndef HAVE_WEBKIT2
    g_object_set_data (G_OBJECT (webkit_get_default_session ()),
                       "midori-session-initialized", (void*)1);
    #endif

    /* TODO: Add test which uses favicon codepath */

    g_test_add_func ("/extensions/colorful_tabs/hostname_colour", test_colour_for_hostname);
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
