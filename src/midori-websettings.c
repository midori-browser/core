/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-websettings.h"

#include "sokoke.h"

#include <glib/gi18n.h>
#include <string.h>

G_DEFINE_TYPE (MidoriWebSettings, midori_web_settings, WEBKIT_TYPE_WEB_SETTINGS)

struct _MidoriWebSettingsPrivate
{
    gboolean remember_last_window_size;
    gint last_window_width;
    gint last_window_height;
    gint last_panel_position;
    gint last_panel_page;
    gint last_web_search;
    gchar* last_pageholder_uri;

    gboolean show_navigationbar;
    gboolean show_bookmarkbar;
    gboolean show_panel;
    gboolean show_statusbar;

    MidoriToolbarStyle toolbar_style;
    gboolean small_toolbar;
    gboolean show_web_search;
    gboolean show_new_tab;
    gboolean show_trash;

    MidoriStartup load_on_startup;
    gchar* homepage;
    gchar* download_folder;
    gboolean show_download_notification;
    gchar* location_entry_search;
    MidoriPreferredEncoding preferred_encoding;

    gint tab_label_size;
    gboolean close_buttons_on_tabs;
    MidoriNewPage open_new_pages_in;
    gboolean middle_click_opens_selection;
    gboolean open_tabs_in_the_background;
    gboolean open_popups_in_tabs;

    MidoriAcceptCookies accept_cookies;
    gboolean original_cookies_only;
    gint maximum_cookie_age;

    gboolean remember_last_visited_pages;
    gint maximum_history_age;
    gboolean remember_last_form_inputs;
    gboolean remember_last_downloaded_files;

    gchar* http_proxy;
    gint cache_size;
};

#define MIDORI_WEB_SETTINGS_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
     MIDORI_TYPE_WEB_SETTINGS, MidoriWebSettingsPrivate))

enum
{
    PROP_0,

    PROP_REMEMBER_LAST_WINDOW_SIZE,
    PROP_LAST_WINDOW_WIDTH,
    PROP_LAST_WINDOW_HEIGHT,
    PROP_LAST_PANEL_POSITION,
    PROP_LAST_PANEL_PAGE,
    PROP_LAST_WEB_SEARCH,
    PROP_LAST_PAGEHOLDER_URI,

    PROP_SHOW_NAVIGATIONBAR,
    PROP_SHOW_BOOKMARKBAR,
    PROP_SHOW_PANEL,
    PROP_SHOW_STATUSBAR,

    PROP_TOOLBAR_STYLE,
    PROP_SMALL_TOOLBAR,
    PROP_SHOW_NEW_TAB,
    PROP_SHOW_WEB_SEARCH,
    PROP_SHOW_TRASH,

    PROP_LOAD_ON_STARTUP,
    PROP_HOMEPAGE,
    PROP_DOWNLOAD_FOLDER,
    PROP_SHOW_DOWNLOAD_NOTIFICATION,
    PROP_LOCATION_ENTRY_SEARCH,
    PROP_PREFERRED_ENCODING,

    PROP_TAB_LABEL_SIZE,
    PROP_CLOSE_BUTTONS_ON_TABS,
    PROP_OPEN_NEW_PAGES_IN,
    PROP_MIDDLE_CLICK_OPENS_SELECTION,
    PROP_OPEN_TABS_IN_THE_BACKGROUND,
    PROP_OPEN_POPUPS_IN_TABS,

    PROP_ACCEPT_COOKIES,
    PROP_ORIGINAL_COOKIES_ONLY,
    PROP_MAXIMUM_COOKIE_AGE,

    PROP_REMEMBER_LAST_VISITED_PAGES,
    PROP_MAXIMUM_HISTORY_AGE,
    PROP_REMEMBER_LAST_FORM_INPUTS,
    PROP_REMEMBER_LAST_DOWNLOADED_FILES,

    PROP_HTTP_PROXY,
    PROP_CACHE_SIZE
};

GType
midori_startup_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_STARTUP_BLANK, "MIDORI_STARTUP_BLANK", "Blank" },
         { MIDORI_STARTUP_HOMEPAGE, "MIDORI_STARTUP_HOMEPAGE", "Homepage" },
         { MIDORI_STARTUP_LAST_OPEN_PAGES, "MIDORI_STARTUP_LAST_OPEN_PAGES", "Last open pages" },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriStartup", values);
    }
    return type;
}

GType
midori_preferred_encoding_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_ENCODING_CHINESE, "MIDORI_ENCODING_CHINESE", "Chinese (BIG5)" },
         { MIDORI_ENCODING_JAPANESE, "MIDORI_ENCODING_JAPANESE", "Japanese (SHIFT_JIS)" },
         { MIDORI_ENCODING_RUSSIAN, "MIDORI_ENCODING_RUSSIAN", "Russian (KOI8-R)" },
         { MIDORI_ENCODING_UNICODE, "MIDORI_ENCODING_UNICODE", "Unicode (UTF-8)" },
         { MIDORI_ENCODING_WESTERN, "MIDORI_ENCODING_WESTERN", "Western (ISO-8859-1)" },
         { MIDORI_ENCODING_WESTERN, "MIDORI_ENCODING_CUSTOM", "Custom..." },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriPreferredEncoding", values);
    }
    return type;
}

GType
midori_new_page_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_NEW_PAGE_TAB, "MIDORI_NEW_PAGE_TAB", "New tab" },
         { MIDORI_NEW_PAGE_WINDOW, "MIDORI_NEW_PAGE_WINDOW", "New window" },
         { MIDORI_NEW_PAGE_CURRENT, "MIDORI_NEW_PAGE_CURRENT", "Current tab" },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriNewPage", values);
    }
    return type;
}

GType
midori_toolbar_style_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_TOOLBAR_DEFAULT, "MIDORI_TOOLBAR_DEFAULT", "Default" },
         { MIDORI_TOOLBAR_ICONS, "MIDORI_TOOLBAR_ICONS", "Icons" },
         { MIDORI_TOOLBAR_TEXT, "MIDORI_TOOLBAR_TEXT", "Text" },
         { MIDORI_TOOLBAR_BOTH, "MIDORI_TOOLBAR_BOTH", "Both" },
         { MIDORI_TOOLBAR_BOTH_HORIZ, "MIDORI_TOOLBAR_BOTH_HORIZ", "Both horizontal" },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriToolbarStyle", values);
    }
    return type;
}

GType
midori_accept_cookies_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_ACCEPT_COOKIES_ALL, "MIDORI_ACCEPT_COOKIES_ALL", "All cookies" },
         { MIDORI_ACCEPT_COOKIES_SESSION, "MIDORI_ACCEPT_COOKIES_SESSION", "Session cookies" },
         { MIDORI_ACCEPT_COOKIES_NONE, "MIDORI_ACCEPT_COOKIES_NONE", "None" },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriAcceptCookies", values);
    }
    return type;
}

static void
midori_web_settings_finalize (GObject* object);

static void
midori_web_settings_set_property (GObject*      object,
                                  guint         prop_id,
                                  const GValue* value,
                                  GParamSpec*   pspec);

static void
midori_web_settings_get_property (GObject*    object,
                                  guint       prop_id,
                                  GValue*     value,
                                  GParamSpec* pspec);

static void
midori_web_settings_class_init (MidoriWebSettingsClass* class)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_web_settings_finalize;
    gobject_class->set_property = midori_web_settings_set_property;
    gobject_class->get_property = midori_web_settings_get_property;

    GParamFlags flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    g_object_class_install_property (gobject_class,
                                     PROP_REMEMBER_LAST_WINDOW_SIZE,
                                     g_param_spec_boolean (
                                     "remember-last-window-size",
                                     _("Remember last window size"),
                                     _("Whether to save the last window size"),
                                     TRUE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_LAST_WINDOW_WIDTH,
                                     g_param_spec_int (
                                     "last-window-width",
                                     _("Last window width"),
                                     _("The last saved window width"),
                                     0, G_MAXINT, 0,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_LAST_WINDOW_HEIGHT,
                                     g_param_spec_int (
                                     "last-window-height",
                                     _("Last window height"),
                                     _("The last saved window height"),
                                     0, G_MAXINT, 0,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_LAST_PANEL_POSITION,
                                     g_param_spec_int (
                                     "last-panel-position",
                                     _("Last panel position"),
                                     _("The last saved panel position"),
                                     0, G_MAXINT, 0,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_LAST_PANEL_PAGE,
                                     g_param_spec_int (
                                     "last-panel-page",
                                     _("Last panel page"),
                                     _("The last saved panel page"),
                                     0, G_MAXINT, 0,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_LAST_WEB_SEARCH,
                                     g_param_spec_int (
                                     "last-web-search",
                                     _("Last Web search"),
                                     _("The last saved Web search"),
                                     0, G_MAXINT, 0,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_LAST_PAGEHOLDER_URI,
                                     g_param_spec_string (
                                     "last-pageholder-uri",
                                     _("Last pageholder URI"),
                                     _("The URI last opened in the pageholder"),
                                     "",
                                     flags));



    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_NAVIGATIONBAR,
                                     g_param_spec_boolean (
                                     "show-navigationbar",
                                     _("Show Navigationbar"),
                                     _("Whether to show the navigationbar"),
                                     TRUE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_BOOKMARKBAR,
                                     g_param_spec_boolean (
                                     "show-bookmarkbar",
                                     _("Show Bookmarkbar"),
                                     _("Whether to show the bookmarkbar"),
                                     FALSE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_PANEL,
                                     g_param_spec_boolean (
                                     "show-panel",
                                     _("Show Panel"),
                                     _("Whether to show the panel"),
                                     FALSE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_STATUSBAR,
                                     g_param_spec_boolean (
                                     "show-statusbar",
                                     _("Show Statusbar"),
                                     _("Whether to show the statusbar"),
                                     TRUE,
                                     flags));


    g_object_class_install_property (gobject_class,
                                     PROP_TOOLBAR_STYLE,
                                     g_param_spec_enum (
                                     "toolbar-style",
                                     _("Toolbar Style"),
                                     _("The style of the toolbar"),
                                     MIDORI_TYPE_TOOLBAR_STYLE,
                                     MIDORI_TOOLBAR_DEFAULT,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_SMALL_TOOLBAR,
                                     g_param_spec_boolean (
                                     "small-toolbar",
                                     _("Small toolbar"),
                                     _("Use small toolbar icons"),
                                     FALSE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_NEW_TAB,
                                     g_param_spec_boolean (
                                     "show-new-tab",
                                     _("Show New Tab"),
                                     _("Show the New Tab button in the toolbar"),
                                     TRUE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_WEB_SEARCH,
                                     g_param_spec_boolean (
                                     "show-web-search",
                                     _("Show Web search"),
                                     _("Show the Web search entry in the toolbar"),
                                     TRUE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_TRASH,
                                     g_param_spec_boolean (
                                     "show-trash",
                                     _("Show Trash"),
                                     _("Show the Trash button in the toolbar"),
                                     TRUE,
                                     flags));



    g_object_class_install_property (gobject_class,
                                     PROP_LOAD_ON_STARTUP,
                                     g_param_spec_enum (
                                     "load-on-startup",
                                     _("Load on Startup"),
                                     _("What to load on startup"),
                                     MIDORI_TYPE_STARTUP,
                                     MIDORI_STARTUP_HOMEPAGE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_HOMEPAGE,
                                     g_param_spec_string (
                                     "homepage",
                                     _("Homepage"),
                                     _("The homepage"),
                                     "http://www.google.com",
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_DOWNLOAD_FOLDER,
                                     g_param_spec_string (
                                     "download-folder",
                                     _("Download Folder"),
                                     _("The folder downloaded files are saved to"),
                                     g_get_home_dir (),
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_DOWNLOAD_NOTIFICATION,
                                     g_param_spec_boolean (
                                     "show-download-notification",
                                     _("Show Download Notification"),
                                     _("Show a notification window for finished downloads"),
                                     TRUE,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_LOCATION_ENTRY_SEARCH,
                                     g_param_spec_string (
                                     "location-entry-search",
                                     _("Location entry Search"),
                                     _("The search to perform inside the location entry"),
                                     "http://www.google.com/search/?q=%s",
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_PREFERRED_ENCODING,
                                     g_param_spec_enum (
                                     "preferred-encoding",
                                     _("Preferred Encoding"),
                                     _("The preferred character encoding"),
                                     MIDORI_TYPE_PREFERRED_ENCODING,
                                     MIDORI_ENCODING_WESTERN,
                                     flags));



    g_object_class_install_property (gobject_class,
                                     PROP_TAB_LABEL_SIZE,
                                     g_param_spec_int (
                                     "tab-label-size",
                                     _("Tab Label Size"),
                                     _("The desired tab label size"),
                                     0, G_MAXINT, 10,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_CLOSE_BUTTONS_ON_TABS,
                                     g_param_spec_boolean (
                                     "close-buttons-on-tabs",
                                     _("Close Buttons on Tabs"),
                                     _("Whether tabs have close buttons"),
                                     FALSE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_OPEN_NEW_PAGES_IN,
                                     g_param_spec_enum (
                                     "open-new-pages-in",
                                     _("Open new pages in"),
                                     _("Where to open new pages"),
                                     MIDORI_TYPE_NEW_PAGE,
                                     MIDORI_NEW_PAGE_TAB,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_MIDDLE_CLICK_OPENS_SELECTION,
                                     g_param_spec_boolean (
                                     "middle-click-opens-selection",
                                     _("Middle click opens Selection"),
                                     _("Load an URL from the selection via middle click"),
                                     FALSE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_OPEN_TABS_IN_THE_BACKGROUND,
                                     g_param_spec_boolean (
                                     "open-tabs-in-the-background",
                                     _("Open tabs in the background"),
                                     _("Whether to open new tabs in the background"),
                                     FALSE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_OPEN_POPUPS_IN_TABS,
                                     g_param_spec_boolean (
                                     "open-popups-in-tabs",
                                     _("Open popups in tabs"),
                                     _("Whether to open popup windows in tabs"),
                                     TRUE,
                                     flags));



    g_object_class_install_property (gobject_class,
                                     PROP_ACCEPT_COOKIES,
                                     g_param_spec_enum (
                                     "accept-cookies",
                                     _("Accept cookies"),
                                     _("What type of cookies to accept"),
                                     MIDORI_TYPE_ACCEPT_COOKIES,
                                     MIDORI_ACCEPT_COOKIES_ALL,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_ORIGINAL_COOKIES_ONLY,
                                     g_param_spec_boolean (
                                     "original-cookies-only",
                                     _("Original cookies only"),
                                     _("Accept cookies from the original website only"),
                                     FALSE,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_MAXIMUM_COOKIE_AGE,
                                     g_param_spec_int (
                                     "maximum-cookie-age",
                                     _("Maximum cookie age"),
                                     _("The maximum number of days to save cookies for"),
                                     0, G_MAXINT, 30,
                                     G_PARAM_READABLE));



    g_object_class_install_property (gobject_class,
                                     PROP_REMEMBER_LAST_VISITED_PAGES,
                                     g_param_spec_boolean (
                                     "remember-last-visited-pages",
                                     _("Remember last visited pages"),
                                     _("Whether the last visited pages are saved"),
                                     TRUE,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_MAXIMUM_HISTORY_AGE,
                                     g_param_spec_int (
                                     "maximum-history-age",
                                     _("Maximum history age"),
                                     _("The maximum number of days to save the history for"),
                                     0, G_MAXINT, 30,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_REMEMBER_LAST_FORM_INPUTS,
                                     g_param_spec_boolean (
                                     "remember-last-form-inputs",
                                     _("Remember last form inputs"),
                                     _("Whether the last form inputs are saved"),
                                     TRUE,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_REMEMBER_LAST_DOWNLOADED_FILES,
                                     g_param_spec_boolean (
                                     "remember-last-downloaded-files",
                                     _("Remember last downloaded files"),
                                     _("Whether the last downloaded files are saved"),
                                     TRUE,
                                     G_PARAM_READABLE));



    g_object_class_install_property (gobject_class,
                                     PROP_HTTP_PROXY,
                                     g_param_spec_string (
                                     "http-proxy",
                                     _("HTTP Proxy"),
                                     _("The proxy used for HTTP connections"),
                                     g_getenv ("http_proxy"),
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_CACHE_SIZE,
                                     g_param_spec_int (
                                     "cache-size",
                                     _("Cache size"),
                                     _("The allowed size of the cache"),
                                     0, G_MAXINT, 100,
                                     G_PARAM_READABLE));

    g_type_class_add_private (class, sizeof (MidoriWebSettingsPrivate));
}

static void
notify_default_encoding_cb (GObject* object, GParamSpec* pspec)
{
    MidoriWebSettings* web_settings = MIDORI_WEB_SETTINGS (object);
    MidoriWebSettingsPrivate* priv = web_settings->priv;

    const gchar* string;
    g_object_get (object, "default-encoding", &string, NULL);
    const gchar* encoding = string ? string : "";
    if (!strcmp (encoding, "BIG5"))
        priv->preferred_encoding = MIDORI_ENCODING_CHINESE;
    else if (!strcmp (encoding, "SHIFT_JIS"))
        priv->preferred_encoding = MIDORI_ENCODING_JAPANESE;
    else if (!strcmp (encoding, "KOI8-R"))
        priv->preferred_encoding = MIDORI_ENCODING_RUSSIAN;
    else if (!strcmp (encoding, "UTF-8"))
        priv->preferred_encoding = MIDORI_ENCODING_UNICODE;
    else if (!strcmp (encoding, "ISO-8859-1"))
        priv->preferred_encoding = MIDORI_ENCODING_WESTERN;
    else
        priv->preferred_encoding = MIDORI_ENCODING_CUSTOM;
    g_object_notify (object, "preferred-encoding");
}

static void
midori_web_settings_init (MidoriWebSettings* web_settings)
{
    web_settings->priv = MIDORI_WEB_SETTINGS_GET_PRIVATE (web_settings);

    g_signal_connect (web_settings, "notify::default-encoding",
                      G_CALLBACK (notify_default_encoding_cb), NULL);
}

static void
midori_web_settings_finalize (GObject* object)
{
    G_OBJECT_CLASS (midori_web_settings_parent_class)->finalize (object);
}

static void
midori_web_settings_set_property (GObject*      object,
                                  guint         prop_id,
                                  const GValue* value,
                                  GParamSpec*   pspec)
{
    MidoriWebSettings* web_settings = MIDORI_WEB_SETTINGS (object);
    MidoriWebSettingsPrivate* priv = web_settings->priv;

    switch (prop_id)
    {
    case PROP_REMEMBER_LAST_WINDOW_SIZE:
        priv->remember_last_window_size = g_value_get_boolean (value);
        break;
    case PROP_LAST_WINDOW_WIDTH:
        priv->last_window_width = g_value_get_int (value);
        break;
    case PROP_LAST_WINDOW_HEIGHT:
        priv->last_window_height = g_value_get_int (value);
        break;
    case PROP_LAST_PANEL_POSITION:
        priv->last_panel_position = g_value_get_int (value);
        break;
    case PROP_LAST_PANEL_PAGE:
        priv->last_panel_page = g_value_get_int (value);
        break;
    case PROP_LAST_WEB_SEARCH:
        priv->last_web_search = g_value_get_int (value);
        break;
    case PROP_LAST_PAGEHOLDER_URI:
        katze_assign (priv->last_pageholder_uri, g_value_dup_string (value));
        break;

    case PROP_SHOW_NAVIGATIONBAR:
        priv->show_navigationbar = g_value_get_boolean (value);
        break;
    case PROP_SHOW_BOOKMARKBAR:
        priv->show_bookmarkbar = g_value_get_boolean (value);
        break;
    case PROP_SHOW_PANEL:
        priv->show_panel = g_value_get_boolean (value);
        break;
    case PROP_SHOW_STATUSBAR:
        priv->show_statusbar = g_value_get_boolean (value);
        break;

    case PROP_TOOLBAR_STYLE:
        priv->toolbar_style = g_value_get_enum (value);
        break;
    case PROP_SMALL_TOOLBAR:
        priv->small_toolbar = g_value_get_boolean (value);
        break;
    case PROP_SHOW_NEW_TAB:
        priv->show_new_tab = g_value_get_boolean (value);
        break;
    case PROP_SHOW_WEB_SEARCH:
        priv->show_web_search = g_value_get_boolean (value);
        break;
    case PROP_SHOW_TRASH:
        priv->show_trash = g_value_get_boolean (value);
        break;

    case PROP_LOAD_ON_STARTUP:
        priv->load_on_startup = g_value_get_enum (value);
        break;
    case PROP_HOMEPAGE:
        katze_assign (priv->homepage, g_value_dup_string (value));
        break;
    case PROP_DOWNLOAD_FOLDER:
        katze_assign (priv->download_folder, g_value_dup_string (value));
        break;
    case PROP_SHOW_DOWNLOAD_NOTIFICATION:
        priv->show_download_notification = g_value_get_boolean (value);
        break;
    case PROP_LOCATION_ENTRY_SEARCH:
        katze_assign (priv->location_entry_search, g_value_dup_string (value));
        break;
    case PROP_PREFERRED_ENCODING:
        priv->preferred_encoding = g_value_get_enum (value);
        switch (priv->preferred_encoding)
        {
        case MIDORI_ENCODING_CHINESE:
            g_object_set (object, "default-encoding", "BIG5", NULL);
            break;
        case MIDORI_ENCODING_JAPANESE:
            g_object_set (object, "default-encoding", "SHIFT_JIS", NULL);
            break;
        case MIDORI_ENCODING_RUSSIAN:
            g_object_set (object, "default-encoding", "KOI8-R", NULL);
            break;
        case MIDORI_ENCODING_UNICODE:
            g_object_set (object, "default-encoding", "UTF-8", NULL);
            break;
        case MIDORI_ENCODING_WESTERN:
            g_object_set (object, "default-encoding", "ISO-8859-1", NULL);
            break;
        case MIDORI_ENCODING_CUSTOM:
            g_object_set (object, "default-encoding", "", NULL);
        }
        break;

    case PROP_TAB_LABEL_SIZE:
        priv->tab_label_size = g_value_get_int (value);
        break;
    case PROP_CLOSE_BUTTONS_ON_TABS:
        priv->close_buttons_on_tabs = g_value_get_boolean (value);
        break;
    case PROP_OPEN_NEW_PAGES_IN:
        priv->open_new_pages_in = g_value_get_enum (value);
        break;
    case PROP_MIDDLE_CLICK_OPENS_SELECTION:
        priv->middle_click_opens_selection = g_value_get_boolean (value);
        break;
    case PROP_OPEN_TABS_IN_THE_BACKGROUND:
        priv->open_tabs_in_the_background = g_value_get_boolean (value);
        break;
    case PROP_OPEN_POPUPS_IN_TABS:
        priv->open_popups_in_tabs = g_value_get_boolean (value);
        break;

    case PROP_ACCEPT_COOKIES:
        priv->accept_cookies = g_value_get_enum (value);
        break;
    case PROP_ORIGINAL_COOKIES_ONLY:
        priv->original_cookies_only = g_value_get_boolean (value);
        break;
    case PROP_MAXIMUM_COOKIE_AGE:
        priv->maximum_cookie_age = g_value_get_int (value);
        break;

    case PROP_REMEMBER_LAST_VISITED_PAGES:
        priv->remember_last_visited_pages = g_value_get_boolean (value);
        break;
    case PROP_MAXIMUM_HISTORY_AGE:
        priv->maximum_history_age = g_value_get_int (value);
        break;
    case PROP_REMEMBER_LAST_FORM_INPUTS:
        priv->remember_last_form_inputs = g_value_get_boolean (value);
        break;
    case PROP_REMEMBER_LAST_DOWNLOADED_FILES:
        priv->remember_last_downloaded_files = g_value_get_boolean (value);
        break;

    case PROP_HTTP_PROXY:
        katze_assign (priv->http_proxy, g_value_dup_string (value));
        g_setenv ("http_proxy", priv->http_proxy ? priv->http_proxy : "", TRUE);
        break;
    case PROP_CACHE_SIZE:
        priv->cache_size = g_value_get_int (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_web_settings_get_property (GObject*    object,
                                  guint       prop_id,
                                  GValue*     value,
                                  GParamSpec* pspec)
{
    MidoriWebSettings* web_settings = MIDORI_WEB_SETTINGS (object);
    MidoriWebSettingsPrivate* priv = web_settings->priv;

    switch (prop_id)
    {
    case PROP_REMEMBER_LAST_WINDOW_SIZE:
        g_value_set_boolean (value, priv->remember_last_window_size);
        break;
    case PROP_LAST_WINDOW_WIDTH:
        g_value_set_int (value, priv->last_window_width);
        break;
    case PROP_LAST_WINDOW_HEIGHT:
        g_value_set_int (value, priv->last_window_height);
        break;
    case PROP_LAST_PANEL_POSITION:
        g_value_set_int (value, priv->last_panel_position);
        break;
    case PROP_LAST_PANEL_PAGE:
        g_value_set_int (value, priv->last_panel_page);
        break;
    case PROP_LAST_WEB_SEARCH:
        g_value_set_int (value, priv->last_web_search);
        break;
    case PROP_LAST_PAGEHOLDER_URI:
        g_value_set_string (value, priv->last_pageholder_uri);
        break;

    case PROP_SHOW_NAVIGATIONBAR:
        g_value_set_boolean (value, priv->show_navigationbar);
        break;
    case PROP_SHOW_BOOKMARKBAR:
        g_value_set_boolean (value, priv->show_bookmarkbar);
        break;
    case PROP_SHOW_PANEL:
        g_value_set_boolean (value, priv->show_panel);
        break;
    case PROP_SHOW_STATUSBAR:
        g_value_set_boolean (value, priv->show_statusbar);
        break;

    case PROP_TOOLBAR_STYLE:
        g_value_set_enum (value, priv->toolbar_style);
        break;
    case PROP_SMALL_TOOLBAR:
        g_value_set_boolean (value, priv->small_toolbar);
        break;
    case PROP_SHOW_NEW_TAB:
        g_value_set_boolean (value, priv->show_new_tab);
        break;
    case PROP_SHOW_WEB_SEARCH:
        g_value_set_boolean (value, priv->show_web_search);
        break;
    case PROP_SHOW_TRASH:
        g_value_set_boolean (value, priv->show_trash);
        break;

    case PROP_LOAD_ON_STARTUP:
        g_value_set_enum (value, priv->load_on_startup);
        break;
    case PROP_HOMEPAGE:
        g_value_set_string (value, priv->homepage);
        break;
    case PROP_DOWNLOAD_FOLDER:
        g_value_set_string (value, priv->download_folder);
        break;
    case PROP_SHOW_DOWNLOAD_NOTIFICATION:
        g_value_set_boolean (value, priv->show_download_notification);
        break;
    case PROP_LOCATION_ENTRY_SEARCH:
        g_value_set_string (value, priv->location_entry_search);
        break;
    case PROP_PREFERRED_ENCODING:
        g_value_set_enum (value, priv->preferred_encoding);
        break;

    case PROP_TAB_LABEL_SIZE:
        g_value_set_int (value, priv->tab_label_size);
        break;
    case PROP_CLOSE_BUTTONS_ON_TABS:
        g_value_set_boolean (value, priv->close_buttons_on_tabs);
        break;
    case PROP_OPEN_NEW_PAGES_IN:
        g_value_set_enum (value, priv->open_new_pages_in);
        break;
    case PROP_MIDDLE_CLICK_OPENS_SELECTION:
        g_value_set_boolean (value, priv->middle_click_opens_selection);
        break;
    case PROP_OPEN_TABS_IN_THE_BACKGROUND:
        g_value_set_boolean (value, priv->open_tabs_in_the_background);
        break;
    case PROP_OPEN_POPUPS_IN_TABS:
        g_value_set_boolean (value, priv->open_popups_in_tabs);
        break;

    case PROP_ACCEPT_COOKIES:
        g_value_set_enum (value, priv->accept_cookies);
        break;
    case PROP_ORIGINAL_COOKIES_ONLY:
        g_value_set_boolean (value, priv->original_cookies_only);
        break;
    case PROP_MAXIMUM_COOKIE_AGE:
        g_value_set_int (value, priv->maximum_cookie_age);
        break;

    case PROP_REMEMBER_LAST_VISITED_PAGES:
        g_value_set_boolean (value, priv->remember_last_visited_pages);
        break;
    case PROP_MAXIMUM_HISTORY_AGE:
        g_value_set_int (value, priv->maximum_history_age);
        break;
    case PROP_REMEMBER_LAST_FORM_INPUTS:
        g_value_set_boolean (value, priv->remember_last_form_inputs);
        break;
    case PROP_REMEMBER_LAST_DOWNLOADED_FILES:
        g_value_set_boolean (value, priv->remember_last_downloaded_files);
        break;

    case PROP_HTTP_PROXY:
        g_value_set_string (value, priv->http_proxy);
        break;
    case PROP_CACHE_SIZE:
        g_value_set_int (value, priv->cache_size);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * midori_web_settings_new:
 *
 * Creates a new #MidoriWebSettings instance with default values.
 *
 * You will typically want to assign this to a #MidoriWebView or #MidoriBrowser.
 *
 * Return value: a new #MidoriWebSettings
 **/
MidoriWebSettings*
midori_web_settings_new (void)
{
    MidoriWebSettings* web_settings = g_object_new (MIDORI_TYPE_WEB_SETTINGS,
                                                    NULL);

    return web_settings;
}

/**
 * midori_web_settings_copy:
 *
 * Copies an existing #MidoriWebSettings instance.
 *
 * Return value: a new #MidoriWebSettings
 **/
MidoriWebSettings*
midori_web_settings_copy (MidoriWebSettings* web_settings)
{
    g_return_val_if_fail (MIDORI_IS_WEB_SETTINGS (web_settings), NULL);

    MidoriWebSettingsPrivate* priv = web_settings->priv;

    MidoriWebSettings* copy;
    copy = MIDORI_WEB_SETTINGS (webkit_web_settings_copy (
        WEBKIT_WEB_SETTINGS (web_settings)));
    g_object_set (copy,
                  "load-on-startup", priv->load_on_startup,
                  "homepage", priv->homepage,
                  "download-folder", priv->download_folder,
                  "show-download-notification", priv->show_download_notification,
                  "location-entry-search", priv->location_entry_search,
                  "preferred-encoding", priv->preferred_encoding,

                  "toolbar-style", priv->toolbar_style,
                  "small-toolbar", priv->small_toolbar,
                  "show-web-search", priv->show_web_search,
                  "show-new-tab", priv->show_new_tab,
                  "show-trash", priv->show_trash,

                  "tab-label-size", priv->tab_label_size,
                  "close-buttons-on-tabs", priv->close_buttons_on_tabs,
                  "open-new-pages-in", priv->open_new_pages_in,
                  "middle-click-opens-selection", priv->middle_click_opens_selection,
                  "open-tabs-in-the-background", priv->open_tabs_in_the_background,
                  "open-popups-in-tabs", priv->open_popups_in_tabs,

                  "accept-cookies", priv->accept_cookies,
                  "original-cookies-only", priv->original_cookies_only,
                  "maximum-cookie-age", priv->maximum_cookie_age,

                  "remember-last-visited-pages", priv->remember_last_visited_pages,
                  "maximum-history-age", priv->maximum_history_age,
                  "remember-last-form-inputs", priv->remember_last_form_inputs,
                  "remember-last-downloaded-files", priv->remember_last_downloaded_files,

                  "http-proxy", priv->http_proxy,
                  "cache-size", priv->cache_size,
                  NULL);

    return copy;
}
