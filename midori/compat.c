/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "compat.h"

#ifndef WEBKIT_CHECK_VERSION

/**
 * webkit_web_view_get_zoom_level:
 * @web_view: a #WebKitWebView
 *
 * Retrieves the current zoom level.
 *
 * Return value: the zoom level, always 1.0 if not supported
 **/
gfloat
webkit_web_view_get_zoom_level (WebKitWebView* web_view)
{
    g_return_val_if_fail (WEBKIT_IS_WEB_VIEW (web_view), 1.0);

    if (g_object_class_find_property (G_OBJECT_GET_CLASS (web_view),
                                      "zoom-level"))
    {
        gfloat zoom_level;
        g_object_get (web_view, "zoom-level", &zoom_level, NULL);
        return zoom_level;
    }
    return 1.0;
}

/**
 * webkit_web_view_set_zoom_level:
 * @web_view: a #WebKitWebView
 *
 * Sets the current zoom level.
 *
 * Does nothing if not supported.
 **/
void
webkit_web_view_set_zoom_level (WebKitWebView* web_view,
                                gfloat         zoom_level)
{
    g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));

    if (g_object_class_find_property (G_OBJECT_GET_CLASS (web_view),
                                      "zoom-level"))
        g_object_set (web_view, "zoom-level", zoom_level, NULL);
}

/**
 * webkit_web_view_zoom_in:
 * @web_view: a #WebKitWebView
 *
 * Increases the current zoom level.
 *
 * Does nothing if not supported.
 **/
void
webkit_web_view_zoom_in (WebKitWebView* web_view)
{
    g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));

    gfloat zoom_level = webkit_web_view_get_zoom_level (web_view);
    WebKitWebSettings* settings = webkit_web_view_get_settings (web_view);
    gfloat zoom_step;
    g_object_get (settings, "zoom-step", &zoom_step, NULL);
    webkit_web_view_set_zoom_level (web_view, zoom_level + zoom_step);
}

/**
 * webkit_web_view_zoom_out:
 * @web_view: a #WebKitWebView
 *
 * Decreases the current zoom level.
 *
 * Does nothing if not supported.
 **/
void
webkit_web_view_zoom_out (WebKitWebView* web_view)
{
    g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));

    gfloat zoom_level = webkit_web_view_get_zoom_level (web_view);
    WebKitWebSettings* settings = webkit_web_view_get_settings (web_view);
    if (g_object_class_find_property (G_OBJECT_GET_CLASS (settings),
                                      "zoom-step"))
    {
        gfloat zoom_step;
        g_object_get (settings, "zoom-step", &zoom_step, NULL);
        webkit_web_view_set_zoom_level (web_view, zoom_level - zoom_step);
    }
}

#endif
