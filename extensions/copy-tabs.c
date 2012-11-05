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
copy_tabs_apply_cb (GtkWidget*     menuitem,
                    MidoriBrowser* browser)
{
    GList* children;
    GString* text = g_string_sized_new (256);
    GtkClipboard* clipboard = gtk_widget_get_clipboard (menuitem,
                                                        GDK_SELECTION_CLIPBOARD);

    children = midori_browser_get_tabs (MIDORI_BROWSER (browser));
    for (; children; children = g_list_next (children))
    {
        g_string_append (text, midori_view_get_display_uri (children->data));
        g_string_append_c (text, '\n');
    }
    gtk_clipboard_set_text (clipboard, text->str, -1);
    g_string_free (text, TRUE);
    g_list_free (children);
}

static void
copy_tabs_browser_populate_tool_menu_cb (MidoriBrowser*   browser,
                                         GtkWidget*       menu,
                                         MidoriExtension* extension)
{
    GtkWidget* menuitem = gtk_menu_item_new_with_mnemonic (_("Copy Tab _Addresses"));

    g_signal_connect (menuitem, "activate",
        G_CALLBACK (copy_tabs_apply_cb), browser);
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
}

static void
copy_tabs_app_add_browser_cb (MidoriApp*       app,
                              MidoriBrowser*   browser,
                              MidoriExtension* extension);

static void
copy_tabs_deactivate_cb (MidoriExtension* extension,
                         MidoriBrowser*   browser)
{
    MidoriApp* app = midori_extension_get_app (extension);

    g_signal_handlers_disconnect_by_func (
        browser, copy_tabs_browser_populate_tool_menu_cb, extension);
    g_signal_handlers_disconnect_by_func (
        extension, copy_tabs_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, copy_tabs_app_add_browser_cb, extension);
}

static void
copy_tabs_app_add_browser_cb (MidoriApp*       app,
                              MidoriBrowser*   browser,
                              MidoriExtension* extension)
{
    g_signal_connect (browser, "populate-tool-menu",
        G_CALLBACK (copy_tabs_browser_populate_tool_menu_cb), extension);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (copy_tabs_deactivate_cb), browser);
}

static void
copy_tabs_activate_cb (MidoriExtension* extension,
                       MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        copy_tabs_app_add_browser_cb (app, browser, extension);
    g_object_unref (browsers);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (copy_tabs_app_add_browser_cb), extension);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Copy Addresses of Tabs"),
        "description", _("Copy the addresses of all tabs to the clipboard"),
        "version", "0.1" MIDORI_VERSION_SUFFIX,
        "authors", "MonkeyOfDoom <pixelmonkey@ensellitis.com>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (copy_tabs_activate_cb), NULL);

    return extension;
}

