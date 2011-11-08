/*
 Copyright (C) 2008-2010 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

static void
tabs_minimized_app_add_browser_cb (MidoriApp*       app,
                                   MidoriBrowser*   browser,
                                   MidoriExtension* extension);

static void
tabs_minimized_add_tab_cb (MidoriBrowser*   browser,
                           MidoriView*      tab,
                           MidoriExtension* extension);

static void
tabs_minimized_deactivate_cb (MidoriExtension* extension,
                              MidoriBrowser*   browser)
{
    MidoriApp* app = midori_extension_get_app (extension);

    g_signal_handlers_disconnect_by_func (
        extension, tabs_minimized_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, tabs_minimized_app_add_browser_cb, extension);
    g_signal_handlers_disconnect_by_func (
        browser, tabs_minimized_add_tab_cb, extension);
}

static void
tabs_minimized_add_tab_cb (MidoriBrowser*   browser,
                           MidoriView*      tab,
                           MidoriExtension* extension)
{
    g_object_set (tab, "minimized", TRUE, NULL);
}

static void
tabs_minimized_app_add_browser_cb (MidoriApp*       app,
                                   MidoriBrowser*   browser,
                                   MidoriExtension* extension)
{
    g_signal_connect (browser, "add-tab",
        G_CALLBACK (tabs_minimized_add_tab_cb), extension);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (tabs_minimized_deactivate_cb), browser);
}

static void
tabs_minimized_activate_cb (MidoriExtension* extension,
                            MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        tabs_minimized_app_add_browser_cb (app, browser, extension);
    g_object_unref (browsers);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (tabs_minimized_app_add_browser_cb), extension);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Only Icons on Tabs by default"),
        "description", _("New tabs have no label by default"),
        "version", "0.1" MIDORI_VERSION_SUFFIX,
        "authors", "MonkeyOfDoom <pixelmonkey@ensellitis.com>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (tabs_minimized_activate_cb), NULL);

    return extension;
}

