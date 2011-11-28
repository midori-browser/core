/*
 Copyright (C) 2008-2010 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2011 Peter Hatina <phatina@redhat.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-websettings.h"

#include "sokoke.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>

#include <config.h>
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif
#if defined (G_OS_UNIX)
    #include <sys/utsname.h>
#endif

struct _MidoriWebSettings
{
    WebKitWebSettings parent_instance;

    gboolean remember_last_window_size : 1;
    MidoriWindowState last_window_state : 2;
    gboolean show_menubar : 1;
    gboolean show_navigationbar : 1;
    gboolean show_bookmarkbar : 1;
    gboolean show_panel : 1;
    gboolean show_statusbar : 1;
    MidoriToolbarStyle toolbar_style : 3;
    gboolean compact_sidepanel : 1;
    gboolean right_align_sidepanel : 1;
    gboolean open_panels_in_windows : 1;
    MidoriStartup load_on_startup : 2;
    gboolean show_crash_dialog : 1;
    MidoriPreferredEncoding preferred_encoding : 3;
    gboolean always_show_tabbar : 1;
    gboolean close_buttons_on_tabs : 1;
    gint close_buttons_left;
    MidoriNewPage open_new_pages_in : 2;
    gboolean middle_click_opens_selection : 1;
    gboolean open_tabs_in_the_background : 1;
    gboolean open_tabs_next_to_current : 1;
    gboolean open_popups_in_tabs : 1;
    gboolean zoom_text_and_images : 1;
    gboolean find_while_typing : 1;
    gboolean kinetic_scrolling : 1;
    gboolean first_party_cookies_only : 1;
    gboolean remember_last_visited_pages : 1;
    MidoriProxy proxy_type : 2;
    MidoriIdentity identify_as : 3;

    gint last_window_width;
    gint last_window_height;
    gint last_panel_position;
    gint last_panel_page;
    gint last_web_search;
    gint maximum_cookie_age;
    gint maximum_history_age;
    gint search_width;

    gchar* toolbar_items;
    gchar* homepage;
    gchar* download_folder;
    gchar* text_editor;
    gchar* news_aggregator;
    gchar* location_entry_search;
    gchar* http_proxy;
    gint http_proxy_port;
    #if WEBKIT_CHECK_VERSION (1, 3, 11)
    gint maximum_cache_size;
    #endif
    gchar* http_accept_language;
    gchar* ident_string;

    gint clear_private_data;
    gchar* clear_data;
    #if !WEBKIT_CHECK_VERSION (1, 3, 13)
    gboolean enable_dns_prefetching;
    #endif
    gboolean strip_referer;
    gboolean enforce_font_family;
    gboolean flash_window_on_bg_tabs;
    gchar* user_stylesheet_uri;
    gchar* user_stylesheet_uri_cached;
    GHashTable* user_stylesheets;
};

struct _MidoriWebSettingsClass
{
    WebKitWebSettingsClass parent_class;
};

G_DEFINE_TYPE (MidoriWebSettings, midori_web_settings, WEBKIT_TYPE_WEB_SETTINGS)

enum
{
    PROP_0,

    PROP_REMEMBER_LAST_WINDOW_SIZE,
    PROP_LAST_WINDOW_WIDTH,
    PROP_LAST_WINDOW_HEIGHT,
    PROP_LAST_WINDOW_STATE,
    PROP_LAST_PANEL_POSITION,
    PROP_LAST_PANEL_PAGE,
    PROP_LAST_WEB_SEARCH,

    PROP_SHOW_MENUBAR,
    PROP_SHOW_NAVIGATIONBAR,
    PROP_SHOW_BOOKMARKBAR,
    PROP_SHOW_PANEL,
    PROP_SHOW_STATUSBAR,

    PROP_TOOLBAR_STYLE,
    PROP_TOOLBAR_ITEMS,
    PROP_COMPACT_SIDEPANEL,
    PROP_RIGHT_ALIGN_SIDEPANEL,
    PROP_OPEN_PANELS_IN_WINDOWS,

    PROP_LOAD_ON_STARTUP,
    PROP_HOMEPAGE,
    PROP_SHOW_CRASH_DIALOG,
    PROP_DOWNLOAD_FOLDER,
    PROP_TEXT_EDITOR,
    PROP_NEWS_AGGREGATOR,
    PROP_LOCATION_ENTRY_SEARCH,
    PROP_PREFERRED_ENCODING,

    PROP_ALWAYS_SHOW_TABBAR,
    PROP_CLOSE_BUTTONS_ON_TABS,
    PROP_CLOSE_BUTTONS_LEFT,
    PROP_OPEN_NEW_PAGES_IN,
    PROP_MIDDLE_CLICK_OPENS_SELECTION,
    PROP_OPEN_TABS_IN_THE_BACKGROUND,
    PROP_OPEN_TABS_NEXT_TO_CURRENT,
    PROP_OPEN_POPUPS_IN_TABS,
    PROP_FLASH_WINDOW_ON_BG_TABS,

    PROP_AUTO_LOAD_IMAGES,
    PROP_ENABLE_SCRIPTS,
    PROP_ENABLE_PLUGINS,
    PROP_ENABLE_DEVELOPER_EXTRAS,
    PROP_ENABLE_SPELL_CHECKING,
    PROP_ENABLE_HTML5_DATABASE,
    PROP_ENABLE_HTML5_LOCAL_STORAGE,
    PROP_ENABLE_OFFLINE_WEB_APPLICATION_CACHE,
    PROP_ENABLE_PAGE_CACHE,
    PROP_ZOOM_TEXT_AND_IMAGES,
    PROP_FIND_WHILE_TYPING,
    PROP_KINETIC_SCROLLING,
    PROP_MAXIMUM_COOKIE_AGE,
    PROP_FIRST_PARTY_COOKIES_ONLY,

    PROP_MAXIMUM_HISTORY_AGE,

    PROP_PROXY_TYPE,
    PROP_HTTP_PROXY,
    PROP_HTTP_PROXY_PORT,
    PROP_MAXIMUM_CACHE_SIZE,
    PROP_IDENTIFY_AS,
    PROP_USER_AGENT,
    PROP_PREFERRED_LANGUAGES,

    PROP_CLEAR_PRIVATE_DATA,
    PROP_CLEAR_DATA,
    PROP_ENABLE_DNS_PREFETCHING,
    PROP_STRIP_REFERER,
    PROP_ENFORCE_FONT_FAMILY,
    PROP_USER_STYLESHEET_URI,

    PROP_SEARCH_WIDTH,
};

GType
midori_window_state_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_WINDOW_NORMAL, "MIDORI_WINDOW_NORMAL", "Normal" },
         { MIDORI_WINDOW_MINIMIZED, "MIDORI_WINDOW_MINIMIZED", "Minimized" },
         { MIDORI_WINDOW_MAXIMIZED, "MIDORI_WINDOW_MAXIMIZED", "Maximized" },
         { MIDORI_WINDOW_FULLSCREEN, "MIDORI_WINDOW_FULLSCREEN", "Fullscreen" },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriWindowState", values);
    }
    return type;
}

GType
midori_startup_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_STARTUP_BLANK_PAGE, "MIDORI_STARTUP_BLANK_PAGE", N_("Show Speed Dial") },
         { MIDORI_STARTUP_HOMEPAGE, "MIDORI_STARTUP_HOMEPAGE", N_("Show Homepage") },
         { MIDORI_STARTUP_LAST_OPEN_PAGES, "MIDORI_STARTUP_LAST_OPEN_PAGES", N_("Show last open tabs") },
         { MIDORI_STARTUP_DELAYED_PAGES, "MIDORI_STARTUP_DELAYED_PAGES", N_("Show last tabs without loading") },
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
         { MIDORI_ENCODING_CHINESE, "MIDORI_ENCODING_CHINESE", N_("Chinese (BIG5)") },
         { MIDORI_ENCODING_JAPANESE, "MIDORI_ENCODING_JAPANESE", N_("Japanese (SHIFT_JIS)") },
         { MIDORI_ENCODING_KOREAN, "MIDORI_ENCODING_KOREAN", N_("Korean (EUC-KR)") },
         { MIDORI_ENCODING_RUSSIAN, "MIDORI_ENCODING_RUSSIAN", N_("Russian (KOI8-R)") },
         { MIDORI_ENCODING_UNICODE, "MIDORI_ENCODING_UNICODE", N_("Unicode (UTF-8)") },
         { MIDORI_ENCODING_WESTERN, "MIDORI_ENCODING_WESTERN", N_("Western (ISO-8859-1)") },
         { MIDORI_ENCODING_CUSTOM, "MIDORI_ENCODING_CUSTOM", N_("Custom...") },
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
         { MIDORI_NEW_PAGE_TAB, "MIDORI_NEW_PAGE_TAB", N_("New tab") },
         { MIDORI_NEW_PAGE_WINDOW, "MIDORI_NEW_PAGE_WINDOW", N_("New window") },
         { MIDORI_NEW_PAGE_CURRENT, "MIDORI_NEW_PAGE_CURRENT", N_("Current tab") },
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
         { MIDORI_TOOLBAR_DEFAULT, "MIDORI_TOOLBAR_DEFAULT", N_("Default") },
         { MIDORI_TOOLBAR_ICONS, "MIDORI_TOOLBAR_ICONS", N_("Icons") },
         { MIDORI_TOOLBAR_SMALL_ICONS, "MIDORI_TOOLBAR_SMALL_ICONS", N_("Small icons") },
         { MIDORI_TOOLBAR_TEXT, "MIDORI_TOOLBAR_TEXT", N_("Text") },
         { MIDORI_TOOLBAR_BOTH, "MIDORI_TOOLBAR_BOTH", N_("Icons and text") },
         { MIDORI_TOOLBAR_BOTH_HORIZ, "MIDORI_TOOLBAR_BOTH_HORIZ", N_("Text beside icons") },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriToolbarStyle", values);
    }
    return type;
}

GType
midori_proxy_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_PROXY_AUTOMATIC, "MIDORI_PROXY_AUTOMATIC", N_("Automatic (GNOME or environment)") },
         { MIDORI_PROXY_HTTP, "MIDORI_PROXY_HTTP", N_("HTTP proxy server") },
         { MIDORI_PROXY_NONE, "MIDORI_PROXY_NONE", N_("No proxy server") },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriProxy", values);
    }
    return type;
}

GType
midori_identity_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_IDENT_MIDORI, "MIDORI_IDENT_MIDORI", N_("_Automatic") },
         { MIDORI_IDENT_GENUINE, "MIDORI_IDENT_GENUINE", N_("Midori") },
         { MIDORI_IDENT_SAFARI, "MIDORI_IDENT_SAFARI", N_("Safari") },
         { MIDORI_IDENT_IPHONE, "MIDORI_IDENT_IPHONE", N_("iPhone") },
         { MIDORI_IDENT_FIREFOX, "MIDORI_IDENT_FIREFOX", N_("Firefox") },
         { MIDORI_IDENT_EXPLORER, "MIDORI_IDENT_EXPLORER", N_("Internet Explorer") },
         { MIDORI_IDENT_CUSTOM, "MIDORI_IDENT_CUSTOM", N_("Custom...") },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriIdentity", values);
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

static const gchar*
midori_get_download_dir (void)
{
    const gchar* dir = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
    if (dir)
    {
        katze_mkdir_with_parents (dir, 0700);
        return dir;
    }
    return g_get_home_dir ();
}

static void
midori_web_settings_class_init (MidoriWebSettingsClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_web_settings_finalize;
    gobject_class->set_property = midori_web_settings_set_property;
    gobject_class->get_property = midori_web_settings_get_property;

    flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS;

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
                                     flags | MIDORI_PARAM_DELAY_SAVING));

    g_object_class_install_property (gobject_class,
                                     PROP_LAST_WINDOW_HEIGHT,
                                     g_param_spec_int (
                                     "last-window-height",
                                     _("Last window height"),
                                     _("The last saved window height"),
                                     0, G_MAXINT, 0,
                                     flags | MIDORI_PARAM_DELAY_SAVING));

    /**
    * MidoriWebSettings:last-window-state:
    *
    * The last saved window state.
    *
    * Since: 0.1.3
    */
    g_object_class_install_property (gobject_class,
                                     PROP_LAST_WINDOW_STATE,
                                     g_param_spec_enum (
                                     "last-window-state",
                                     "Last window state",
                                     "The last saved window state",
                                     MIDORI_TYPE_WINDOW_STATE,
                                     MIDORI_WINDOW_NORMAL,
                                     flags | MIDORI_PARAM_DELAY_SAVING));

    g_object_class_install_property (gobject_class,
                                     PROP_LAST_PANEL_POSITION,
                                     g_param_spec_int (
                                     "last-panel-position",
                                     _("Last panel position"),
                                     _("The last saved panel position"),
                                     0, G_MAXINT, 0,
                                     flags | MIDORI_PARAM_DELAY_SAVING));

    g_object_class_install_property (gobject_class,
                                     PROP_LAST_PANEL_PAGE,
                                     g_param_spec_int (
                                     "last-panel-page",
        /* i18n: The internal index of the last opened panel */
                                     _("Last panel page"),
                                     _("The last saved panel page"),
                                     0, G_MAXINT, 0,
                                     flags | MIDORI_PARAM_DELAY_SAVING));

    g_object_class_install_property (gobject_class,
                                     PROP_LAST_WEB_SEARCH,
                                     g_param_spec_int (
                                     "last-web-search",
                                     _("Last Web search"),
                                     _("The last saved Web search"),
                                     0, G_MAXINT, 0,
                                     flags));


    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_MENUBAR,
                                     g_param_spec_boolean (
                                     "show-menubar",
                                     _("Show Menubar"),
                                     _("Whether to show the menubar"),
                                     FALSE,
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
                                     _("Toolbar Style:"),
                                     _("The style of the toolbar"),
                                     MIDORI_TYPE_TOOLBAR_STYLE,
                                     MIDORI_TOOLBAR_DEFAULT,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_TOOLBAR_ITEMS,
                                     g_param_spec_string (
                                     "toolbar-items",
                                     _("Toolbar Items"),
                                     _("The items to show on the toolbar"),
                                     "TabNew,Back,Forward,Next,ReloadStop,BookmarkAdd,Location,Search,Trash,CompactMenu",
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_COMPACT_SIDEPANEL,
                                     g_param_spec_boolean (
                                     "compact-sidepanel",
                                     _("Compact Sidepanel"),
                                     _("Whether to make the sidepanel compact"),
                                     FALSE,
                                     flags));

    /**
    * MidoriWebSettings:right-sidepanel:
    *
    * Whether to align the sidepanel on the right.
    *
    * Since: 0.1.3
    */
    g_object_class_install_property (gobject_class,
                                     PROP_RIGHT_ALIGN_SIDEPANEL,
                                     g_param_spec_boolean (
                                     "right-align-sidepanel",
                                     _("Align sidepanel on the right"),
                                     _("Whether to align the sidepanel on the right"),
                                     FALSE,
                                     flags));

    /**
     * MidoriWebSettings:open-panels-in-window:
     *
     * Whether to open panels in separate windows.
     *
     * Since: 0.2.2
     */
    g_object_class_install_property (gobject_class,
                                     PROP_OPEN_PANELS_IN_WINDOWS,
                                     g_param_spec_boolean (
                                     "open-panels-in-windows",
                                     _("Open panels in separate windows"),
        _("Whether to always open panels in separate windows"),
                                     FALSE,
                                     flags));


    g_object_class_install_property (gobject_class,
                                     PROP_LOAD_ON_STARTUP,
                                     g_param_spec_enum (
                                     "load-on-startup",
                                     _("When Midori starts:"),
                                     _("What to do when Midori starts"),
                                     MIDORI_TYPE_STARTUP,
                                     MIDORI_STARTUP_LAST_OPEN_PAGES,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_HOMEPAGE,
                                     g_param_spec_string (
                                     "homepage",
                                     _("Homepage:"),
                                     _("The homepage"),
                                     "http://www.google.com",
                                     flags));

    /**
    * MidoriWebSettings:show-crash-dialog:
    *
    * Show a dialog after Midori crashed.
    *
    * Since: 0.1.2
    */
    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_CRASH_DIALOG,
                                     g_param_spec_boolean (
                                     "show-crash-dialog",
                                     _("Show crash dialog"),
                                     _("Show a dialog after Midori crashed"),
                                     TRUE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_DOWNLOAD_FOLDER,
                                     g_param_spec_string (
                                     "download-folder",
                                     _("Save downloaded files to:"),
                                     _("The folder downloaded files are saved to"),
                                     midori_get_download_dir (),
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_TEXT_EDITOR,
                                     g_param_spec_string (
                                     "text-editor",
                                     _("Text Editor"),
                                     _("An external text editor"),
                                     NULL,
                                     flags));

    /**
    * MidoriWebSettings:news-aggregator:
    *
    * An external news aggregator.
    *
    * Since: 0.1.6
    */
    g_object_class_install_property (gobject_class,
                                     PROP_NEWS_AGGREGATOR,
                                     g_param_spec_string (
                                     "news-aggregator",
                                     _("News Aggregator"),
                                     _("An external news aggregator"),
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_LOCATION_ENTRY_SEARCH,
                                     g_param_spec_string (
                                     "location-entry-search",
                                     _("Location entry Search"),
                                     _("The search to perform inside the location entry"),
                                     NULL,
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
                                     PROP_ALWAYS_SHOW_TABBAR,
                                     g_param_spec_boolean (
                                     "always-show-tabbar",
                                     _("Always Show Tabbar"),
                                     _("Always show the tabbar"),
                                     FALSE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_CLOSE_BUTTONS_ON_TABS,
                                     g_param_spec_boolean (
                                     "close-buttons-on-tabs",
                                     _("Close Buttons on Tabs"),
                                     _("Whether tabs have close buttons"),
                                     TRUE,
                                     flags));

    /**
     * MidoriWebSettings:close-buttons-left:
     *
     * Whether to show close buttons on the left side.
     *
     * Since: 0.3.1
     */
    g_object_class_install_property (gobject_class,
                                     PROP_CLOSE_BUTTONS_LEFT,
                                     g_param_spec_boolean (
                                     "close-buttons-left",
                                     "Close buttons on the left",
                                     "Whether to show close buttons on the left side",
                                     FALSE,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));


    g_object_class_install_property (gobject_class,
                                     PROP_OPEN_NEW_PAGES_IN,
                                     g_param_spec_enum (
                                     "open-new-pages-in",
                                     _("Open new pages in:"),
                                     _("Where to open new pages"),
                                     MIDORI_TYPE_NEW_PAGE,
                                     MIDORI_NEW_PAGE_TAB,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_MIDDLE_CLICK_OPENS_SELECTION,
                                     g_param_spec_boolean (
                                     "middle-click-opens-selection",
                                     _("Middle click opens Selection"),
                                     _("Load an address from the selection via middle click"),
                                     TRUE,
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
                                     PROP_OPEN_TABS_NEXT_TO_CURRENT,
                                     g_param_spec_boolean (
                                     "open-tabs-next-to-current",
                                     _("Open Tabs next to Current"),
        _("Whether to open new tabs next to the current tab or after the last one"),
                                     TRUE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_OPEN_POPUPS_IN_TABS,
                                     g_param_spec_boolean (
                                     "open-popups-in-tabs",
                                     _("Open popups in tabs"),
                                     _("Whether to open popup windows in tabs"),
                                     TRUE,
                                     flags));


    /* Override properties to localize them for preference proxies */
    g_object_class_install_property (gobject_class,
                                     PROP_AUTO_LOAD_IMAGES,
                                     g_param_spec_boolean (
                                     "auto-load-images",
                                     _("Load images automatically"),
                                     _("Load and display images automatically"),
                                     TRUE,
                                     flags));
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_SCRIPTS,
                                     g_param_spec_boolean (
                                     "enable-scripts",
                                     _("Enable scripts"),
                                     _("Enable embedded scripting languages"),
                                     TRUE,
                                     flags));
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_PLUGINS,
                                     g_param_spec_boolean (
                                     "enable-plugins",
                                     _("Enable Netscape plugins"),
                                     _("Enable embedded Netscape plugin objects"),
    #ifdef G_OS_WIN32
                                     FALSE,
                                     G_PARAM_READABLE));
    #else
                                     TRUE,
                                     flags));
    #endif
    /* Override properties to override defaults */
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_DEVELOPER_EXTRAS,
                                     g_param_spec_boolean (
                                     "enable-developer-extras",
                                     "Enable developer tools",
                                     "Enable special extensions for developers",
                                     TRUE,
                                     flags));
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_SPELL_CHECKING,
                                     g_param_spec_boolean ("enable-spell-checking",
                                                           _("Enable Spell Checking"),
                                                           _("Enable spell checking while typing"),
                                                           TRUE,
                                                           flags));
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_HTML5_DATABASE,
                                     g_param_spec_boolean ("enable-html5-database",
                                                           _("Enable HTML5 database support"),
                                                           _("Whether to enable HTML5 database support"),
                                                           FALSE,
                                                           flags));
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_HTML5_LOCAL_STORAGE,
                                     g_param_spec_boolean ("enable-html5-local-storage",
                                                           _("Enable HTML5 local storage support"),
                                                           _("Whether to enable HTML5 local storage support"),
                                                           FALSE,
                                                           flags));
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_OFFLINE_WEB_APPLICATION_CACHE,
                                     g_param_spec_boolean ("enable-offline-web-application-cache",
                                                           _("Enable offline web application cache"),
                                                           _("Whether to enable offline web application cache"),
                                                           FALSE,
                                                           flags));
    #if WEBKIT_CHECK_VERSION (1, 1, 18)
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_PAGE_CACHE,
                                     g_param_spec_boolean ("enable-page-cache",
                                                           "Enable page cache",
                                                           "Whether the page cache should be used",
                                                           TRUE,
                                                           flags));
    #endif
    g_object_class_install_property (gobject_class,
                                     PROP_FLASH_WINDOW_ON_BG_TABS,
                                     g_param_spec_boolean (
                                     "flash-window-on-new-bg-tabs",
                                     _("Flash window on background tabs"),
                                     _("Flash the browser window if a new tab was opened in the background"),
                                     FALSE,
                                     flags));

    /**
     * MidoriWebSettings:zoom-text-and-images:
     *
     * Whether to zoom text and images.
     *
     * Since: 0.1.3
     */
     g_object_class_install_property (gobject_class,
                                      PROP_ZOOM_TEXT_AND_IMAGES,
                                      g_param_spec_boolean (
                                      "zoom-text-and-images",
                                      _("Zoom Text and Images"),
                                      _("Whether to zoom text and images"),
                                      FALSE,
                                      flags));

    /**
    * MidoriWebSettings:find-while-typing:
    *
    * Whether to automatically find inline while typing something.
    *
    * Since: 0.1.4
    */
    g_object_class_install_property (gobject_class,
                                     PROP_FIND_WHILE_TYPING,
                                     g_param_spec_boolean (
                                     "find-while-typing",
                                     _("Find inline while typing"),
                                     _("Whether to automatically find inline while typing"),
                                     FALSE,
                                     flags));

    /**
    * MidoriWebSettings:kinetic-scrolling:
    *
    * Whether scrolling should kinetically move according to speed.
    *
    * Since: 0.2.0
    */
    g_object_class_install_property (gobject_class,
                                     PROP_KINETIC_SCROLLING,
                                     g_param_spec_boolean (
                                     "kinetic-scrolling",
                                     _("Kinetic scrolling"),
                                     _("Whether scrolling should kinetically move according to speed"),
                                     TRUE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_MAXIMUM_COOKIE_AGE,
                                     g_param_spec_int (
                                     "maximum-cookie-age",
                                     _("Delete old Cookies after:"),
                                     _("The maximum number of days to save cookies for"),
                                     0, G_MAXINT, 30,
                                     flags));

    /**
     * MidoriWebSettings:first-party-cookies-only:
     *
     * Whether only first party cookies should be accepted.
     * WebKitGTK+ 1.1.21 is required for this to work.
     *
     * Since: 0.4.2
     */
     g_object_class_install_property (gobject_class,
                                     PROP_FIRST_PARTY_COOKIES_ONLY,
                                     g_param_spec_boolean (
                                     "first-party-cookies-only",
                                     _("Only accept Cookies from sites you visit"),
                                     _("Block cookies sent by third-party websites"),
    #ifdef HAVE_LIBSOUP_2_29_91
                                     TRUE,
        g_object_class_find_property (gobject_class, /* WebKitGTK+ >= 1.1.21 */
            "enable-file-access-from-file-uris") ? flags : G_PARAM_READABLE));
    #else
                                     FALSE,
                                     G_PARAM_READABLE));
    #endif

    g_object_class_install_property (gobject_class,
                                     PROP_MAXIMUM_HISTORY_AGE,
                                     g_param_spec_int (
                                     "maximum-history-age",
                                     _("Delete pages from history after:"),
                                     _("The maximum number of days to save the history for"),
                                     0, G_MAXINT, 30,
                                     flags));

    /**
     * MidoriWebSettings:proxy-type:
     *
     * The type of proxy server to use.
     *
     * Since: 0.2.5
     */
    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_TYPE,
                                     g_param_spec_enum (
                                     "proxy-type",
                                     _("Proxy server"),
                                     _("The type of proxy server to use"),
                                     MIDORI_TYPE_PROXY,
                                     MIDORI_PROXY_AUTOMATIC,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_HTTP_PROXY,
                                     g_param_spec_string (
                                     "http-proxy",
                                     _("HTTP Proxy Server"),
                                     _("The proxy server used for HTTP connections"),
                                     NULL,
                                     flags));

    /**
     * MidoriWebSettings:http-proxy-port:
     *
     * The proxy server port used for HTTP connections
     *
     * Since: 0.4.2
     */
     g_object_class_install_property (gobject_class,
                                     PROP_HTTP_PROXY_PORT,
                                     g_param_spec_int (
                                     "http-proxy-port",
                                     _("Port"),
                                     _("The proxy server port used for HTTP connections"),
                                     1, 65535, 8080,
                                     flags
                                     ));

    #if WEBKIT_CHECK_VERSION (1, 3, 11)
    /**
     * MidoriWebSettings:maximum-cache-size:
     *
     * The maximum size of cached pages on disk.
     *
     * Since: 0.3.4
     */
    g_object_class_install_property (gobject_class,
                                     PROP_MAXIMUM_CACHE_SIZE,
                                     g_param_spec_int (
                                     "maximum-cache-size",
                                     _("Web Cache"),
                                     _("The maximum size of cached pages on disk"),
                                     0, G_MAXINT, 100,
                                     flags));
    #endif

    /**
    * MidoriWebSettings:identify-as:
    *
    * What to identify as to web pages.
    *
    * Since: 0.1.2
    */
    g_object_class_install_property (gobject_class,
                                     PROP_IDENTIFY_AS,
                                     g_param_spec_enum (
                                     "identify-as",
        /* i18n: This refers to an application, not the 'user agent' string */
                                     _("Identify as"),
                                     _("What to identify as to web pages"),
                                     MIDORI_TYPE_IDENTITY,
                                     MIDORI_IDENT_MIDORI,
                                     flags));

    /**
     * MidoriWebSettings:user-agent:
     *
     * The browser identification string.
     *
     * Since: 0.2.3
     */
    g_object_class_install_property (gobject_class,
                                     PROP_USER_AGENT,
                                     g_param_spec_string (
                                     "user-agent",
                                     _("Identification string"),
                                     _("The application identification string"),
                                     NULL,
                                     flags));

    /**
    * MidoriWebSettings:preferred-languages:
    *
    * A comma separated list of languages preferred for rendering multilingual
    * webpages and spell checking.
    *
    * Since: 0.2.3
    */
    g_object_class_install_property (gobject_class,
                                     PROP_PREFERRED_LANGUAGES,
                                     g_param_spec_string (
                                     "preferred-languages",
                                     _("Preferred languages"),
        _("A comma separated list of languages preferred for rendering multilingual webpages, for example \"de\", \"ru,nl\" or \"en-us;q=1.0, fr-fr;q=0.667\""),
                                     NULL,
                                     flags));

    /**
     * MidoriWebSettings:clear-private-data:
     *
     * The core data selected for deletion.
     *
     * Since: 0.1.7
     */
    g_object_class_install_property (gobject_class,
                                     PROP_CLEAR_PRIVATE_DATA,
                                     g_param_spec_int (
                                     "clear-private-data",
                                     _("Clear private data"),
                                     _("The private data selected for deletion"),
                                     0, G_MAXINT, 0,
                                     flags));

    /**
     * MidoriWebSettings:clear-data:
     *
     * The data selected for deletion, including extensions.
     *
     * Since: 0.2.9
     */
    g_object_class_install_property (gobject_class,
                                     PROP_CLEAR_DATA,
                                     g_param_spec_string (
                                     "clear-data",
                                     _("Clear data"),
                                     _("The data selected for deletion"),
                                     NULL,
                                     flags));
    #if !WEBKIT_CHECK_VERSION (1, 3, 13)
    /**
     * MidoriWebSettings:enable-dns-prefetching:
     *
     * Whether to resolve host names in advance.
     *
     * Since: 0.3.4
     */
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_DNS_PREFETCHING,
                                     g_param_spec_boolean (
                                     "enable-dns-prefetching",
        "Whether to resolve host names in advance",
        "Whether host names on a website or in bookmarks should be prefetched",
                                     TRUE,
                                     flags));
    #endif

    /**
     * MidoriWebSettings:strip-referer:
     *
     * Whether to strip referrer details sent to external sites.
     *
     * Since: 0.3.4
     */
    g_object_class_install_property (gobject_class,
                                     PROP_STRIP_REFERER,
                                     g_param_spec_boolean (
                                     "strip-referer",
    /* i18n: Reworded: Shorten details propagated when going to another page */
        _("Strip referrer details sent to websites"),
    /* i18n: Referer here is not a typo but a technical term */
        _("Whether the \"Referer\" header should be shortened to the hostname"),
                                     FALSE,
                                     flags));
    /**
     * MidoriWebSettings:enforc-font-family:
     *
     * Whether to enforce user font preferences with an internal stylesheet.
     *
     * Since: 0.4.2
     */
    g_object_class_install_property (gobject_class,
                                     PROP_ENFORCE_FONT_FAMILY,
                                     g_param_spec_boolean (
                                     "enforce-font-family",
                                     _("Always use my font choices"),
                                     _("Override fonts picked by websites with user preferences"),
                                     FALSE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_USER_STYLESHEET_URI,
                                     g_param_spec_string (
                                     "user-stylesheet-uri",
                                     "User stylesheet URI",
                                     "Load stylesheets from a local URI",
                                     NULL,
                                     flags | MIDORI_PARAM_DELAY_SAVING));

    /**
     * MidoriWebSettings:search-entry-width:
     *
     * Search action width in pixels
     *
     * Since: 0.4.3
     **/
    g_object_class_install_property (gobject_class,
                                     PROP_SEARCH_WIDTH,
                                     g_param_spec_int (
                                     "search-width",
                                     "Search action width",
                                     "Search action width in pixels",
                                     10, G_MAXINT, 200,
                                     flags | MIDORI_PARAM_DELAY_SAVING));
}

static void
notify_default_encoding_cb (GObject*    object,
                            GParamSpec* pspec)
{
    MidoriWebSettings* web_settings;
    gchar* string;
    const gchar* encoding;

    web_settings = MIDORI_WEB_SETTINGS (object);

    g_object_get (object, "default-encoding", &string, NULL);
    encoding = string ? string : "";
    if (!strcmp (encoding, "BIG5"))
        web_settings->preferred_encoding = MIDORI_ENCODING_CHINESE;
    else if (!strcmp (encoding, "SHIFT_JIS"))
        web_settings->preferred_encoding = MIDORI_ENCODING_JAPANESE;
    else if (!strcmp (encoding, "EUC-KR"))
        web_settings->preferred_encoding = MIDORI_ENCODING_KOREAN;
    else if (!strcmp (encoding, "KOI8-R"))
        web_settings->preferred_encoding = MIDORI_ENCODING_RUSSIAN;
    else if (!strcmp (encoding, "UTF-8"))
        web_settings->preferred_encoding = MIDORI_ENCODING_UNICODE;
    else if (!strcmp (encoding, "ISO-8859-1"))
        web_settings->preferred_encoding = MIDORI_ENCODING_WESTERN;
    else
        web_settings->preferred_encoding = MIDORI_ENCODING_CUSTOM;
    g_free (string);
    g_object_notify (object, "preferred-encoding");
}

static void
notify_default_font_family_cb (GObject*    object,
                               GParamSpec* pspec)
{
    if (katze_object_get_boolean (object, "enforce-font-family"))
        g_object_set (object, "enforce-font-family", TRUE, NULL);
}
static void
midori_web_settings_init (MidoriWebSettings* web_settings)
{
    web_settings->download_folder = g_strdup (midori_get_download_dir ());
    web_settings->http_proxy = NULL;
    web_settings->open_popups_in_tabs = TRUE;
    web_settings->kinetic_scrolling = TRUE;
    web_settings->user_stylesheet_uri = web_settings->user_stylesheet_uri_cached = NULL;
    web_settings->user_stylesheets = NULL;

    #if WEBKIT_CHECK_VERSION (1, 2, 6) && !WEBKIT_CHECK_VERSION (1, 2, 8)
    /* Shadows are very slow with WebKitGTK+ 1.2.7 */
    midori_web_settings_add_style (web_settings, "box-shadow-workaround",
        "* { -webkit-box-shadow: none !important; }");
    #endif

    g_signal_connect (web_settings, "notify::default-encoding",
                      G_CALLBACK (notify_default_encoding_cb), NULL);
    g_signal_connect (web_settings, "notify::default-font-family",
                      G_CALLBACK (notify_default_font_family_cb), NULL);
}

static void
midori_web_settings_finalize (GObject* object)
{
    MidoriWebSettings* web_settings;

    web_settings = MIDORI_WEB_SETTINGS (object);

    katze_assign (web_settings->toolbar_items, NULL);
    katze_assign (web_settings->homepage, NULL);
    katze_assign (web_settings->download_folder, NULL);
    katze_assign (web_settings->text_editor, NULL);
    katze_assign (web_settings->news_aggregator, NULL);
    katze_assign (web_settings->location_entry_search, NULL);
    katze_assign (web_settings->http_proxy, NULL);
    katze_assign (web_settings->ident_string, NULL);
    katze_assign (web_settings->user_stylesheet_uri, NULL);
    katze_assign (web_settings->user_stylesheet_uri_cached, NULL);
    if (web_settings->user_stylesheets != NULL)
        g_hash_table_destroy (web_settings->user_stylesheets);

    G_OBJECT_CLASS (midori_web_settings_parent_class)->finalize (object);
}

#if (!HAVE_OSX && defined (G_OS_UNIX)) || defined (G_OS_WIN32)
static gchar*
get_sys_name (gchar** architecture)
{
    static gchar* sys_name = NULL;
    static gchar* sys_architecture = NULL;

    if (!sys_name)
    {
        #ifdef G_OS_WIN32
        /* 6.1 Win7, 6.0 Vista, 5.1 XP and 5.0 Win2k */
        guint version = g_win32_get_windows_version ();
        sys_name = g_strdup_printf ("NT %d.%d", LOBYTE (version), HIBYTE (version));
        #else
        struct utsname name;
        if (uname (&name) != -1)
        {
            sys_name = g_strdup (name.sysname);
            sys_architecture = g_strdup (name.machine);
        }
        else
            sys_name = "Linux";
        #endif
    }

    if (architecture != NULL)
        *architecture = sys_architecture;
    return sys_name;
}
#endif

/**
 * midori_web_settings_get_system_name:
 * @architecture: location of a string, or %NULL
 * @platform: location of a string, or %NULL
 *
 * Determines the system name, architecture and platform.
 * @architecturce can have a %NULL value.
 *
 * Returns: a string
 *
 * Since: 0.4.2
 **/
const gchar*
midori_web_settings_get_system_name (gchar** architecture,
                                     gchar** platform)
{
    if (architecture != NULL)
        *architecture = NULL;

    if (platform != NULL)
        *platform =
    #if HAVE_HILDON
    "Maemo;"
    #elif defined (G_OS_WIN32)
    "Windows";
    #elif defined(GDK_WINDOWING_QUARTZ)
    "Macintosh;";
    #elif defined(GDK_WINDOWING_DIRECTFB)
    "DirectFB;";
    #else
    "X11;";
    #endif

    return
    #if HAVE_OSX
    "Mac OS X";
    #elif defined (G_OS_UNIX) || defined (G_OS_WIN32)
    get_sys_name (architecture);
    #else
    "Linux";
    #endif
}

static gchar*
generate_ident_string (MidoriWebSettings* web_settings,
                       MidoriIdentity     identify_as)
{
    const gchar* appname = "Midori/"
        G_STRINGIFY (MIDORI_MAJOR_VERSION) "."
        G_STRINGIFY (MIDORI_MINOR_VERSION);

    const gchar* lang = pango_language_to_string (gtk_get_default_language ());
    gchar* platform;
    const gchar* os = midori_web_settings_get_system_name (NULL, &platform);

    const int webcore_major = WEBKIT_USER_AGENT_MAJOR_VERSION;
    const int webcore_minor = WEBKIT_USER_AGENT_MINOR_VERSION;

    #if WEBKIT_CHECK_VERSION (1, 1, 18)
    g_object_set (web_settings, "enable-site-specific-quirks",
        identify_as != MIDORI_IDENT_GENUINE, NULL);
    #endif

    switch (identify_as)
    {
    case MIDORI_IDENT_GENUINE:
        return g_strdup_printf ("Mozilla/5.0 (%s %s) AppleWebKit/%d.%d+ %s",
            platform, os, webcore_major, webcore_minor, appname);
    case MIDORI_IDENT_MIDORI:
    case MIDORI_IDENT_SAFARI:
        g_object_set (web_settings, "enable-site-specific-quirks", TRUE, NULL);
        return g_strdup_printf ("Mozilla/5.0 (Macintosh; U; Intel Mac OS X; %s) "
            "AppleWebKit/%d+ (KHTML, like Gecko) Version/5.0 Safari/%d.%d+ %s",
            lang, webcore_major, webcore_major, webcore_minor, appname);
    case MIDORI_IDENT_IPHONE:
        return g_strdup_printf ("Mozilla/5.0 (iPhone; U; CPU like Mac OS X; %s) "
            "AppleWebKit/532+ (KHTML, like Gecko) Version/3.0 Mobile/1A538b Safari/419.3 %s",
                                lang, appname);
    case MIDORI_IDENT_FIREFOX:
        return g_strdup_printf ("Mozilla/5.0 (%s %s; rv:2.0.1) Gecko/20100101 Firefox/4.0.1 %s",
                                platform, os, appname);
    case MIDORI_IDENT_EXPLORER:
        return g_strdup_printf ("Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; %s) %s",
                                lang, appname);
    default:
        return g_strdup_printf ("%s", appname);
    }
}

static void
midori_web_settings_process_stylesheets (MidoriWebSettings* settings,
                                         gint               delta_len);

static void
base64_space_pad (gchar* base64,
                  guint  len);

static void
midori_web_settings_set_property (GObject*      object,
                                  guint         prop_id,
                                  const GValue* value,
                                  GParamSpec*   pspec)
{
    MidoriWebSettings* web_settings = MIDORI_WEB_SETTINGS (object);

    switch (prop_id)
    {
    case PROP_REMEMBER_LAST_WINDOW_SIZE:
        web_settings->remember_last_window_size = g_value_get_boolean (value);
        break;
    case PROP_LAST_WINDOW_WIDTH:
        web_settings->last_window_width = g_value_get_int (value);
        break;
    case PROP_LAST_WINDOW_HEIGHT:
        web_settings->last_window_height = g_value_get_int (value);
        break;
    case PROP_LAST_WINDOW_STATE:
        web_settings->last_window_state = g_value_get_enum (value);
        break;
    case PROP_LAST_PANEL_POSITION:
        web_settings->last_panel_position = g_value_get_int (value);
        break;
    case PROP_LAST_PANEL_PAGE:
        web_settings->last_panel_page = g_value_get_int (value);
        break;
    case PROP_LAST_WEB_SEARCH:
        web_settings->last_web_search = g_value_get_int (value);
        break;

    case PROP_SHOW_MENUBAR:
        web_settings->show_menubar = g_value_get_boolean (value);
        break;
    case PROP_SHOW_NAVIGATIONBAR:
        web_settings->show_navigationbar = g_value_get_boolean (value);
        break;
    case PROP_SHOW_BOOKMARKBAR:
        web_settings->show_bookmarkbar = g_value_get_boolean (value);
        break;
    case PROP_SHOW_PANEL:
        web_settings->show_panel = g_value_get_boolean (value);
        break;
    case PROP_SHOW_STATUSBAR:
        web_settings->show_statusbar = g_value_get_boolean (value);
        break;

    case PROP_TOOLBAR_STYLE:
        web_settings->toolbar_style = g_value_get_enum (value);
        break;
    case PROP_TOOLBAR_ITEMS:
        katze_assign (web_settings->toolbar_items, g_value_dup_string (value));
        break;
    case PROP_COMPACT_SIDEPANEL:
        web_settings->compact_sidepanel = g_value_get_boolean (value);
        break;
    case PROP_RIGHT_ALIGN_SIDEPANEL:
        web_settings->right_align_sidepanel = g_value_get_boolean (value);
        break;
    case PROP_OPEN_PANELS_IN_WINDOWS:
        web_settings->open_panels_in_windows = g_value_get_boolean (value);
        break;

    case PROP_LOAD_ON_STARTUP:
        web_settings->load_on_startup = g_value_get_enum (value);
        break;
    case PROP_HOMEPAGE:
        katze_assign (web_settings->homepage, g_value_dup_string (value));
        break;
    case PROP_SHOW_CRASH_DIALOG:
        web_settings->show_crash_dialog = g_value_get_boolean (value);
        break;
    case PROP_DOWNLOAD_FOLDER:
        katze_assign (web_settings->download_folder, g_value_dup_string (value));
        break;
    case PROP_TEXT_EDITOR:
        katze_assign (web_settings->text_editor, g_value_dup_string (value));
        break;
    case PROP_NEWS_AGGREGATOR:
        katze_assign (web_settings->news_aggregator, g_value_dup_string (value));
        break;
    case PROP_LOCATION_ENTRY_SEARCH:
        katze_assign (web_settings->location_entry_search, g_value_dup_string (value));
        break;
    case PROP_PREFERRED_ENCODING:
        web_settings->preferred_encoding = g_value_get_enum (value);
        switch (web_settings->preferred_encoding)
        {
        case MIDORI_ENCODING_CHINESE:
            g_object_set (object, "default-encoding", "BIG5", NULL);
            break;
        case MIDORI_ENCODING_JAPANESE:
            g_object_set (object, "default-encoding", "SHIFT_JIS", NULL);
            break;
       case MIDORI_ENCODING_KOREAN:
            g_object_set (object, "default-encoding", "EUC-KR", NULL);
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

    case PROP_ALWAYS_SHOW_TABBAR:
        web_settings->always_show_tabbar = g_value_get_boolean (value);
        break;
    case PROP_CLOSE_BUTTONS_ON_TABS:
        web_settings->close_buttons_on_tabs = g_value_get_boolean (value);
        break;
    case PROP_OPEN_NEW_PAGES_IN:
        web_settings->open_new_pages_in = g_value_get_enum (value);
        break;
    case PROP_MIDDLE_CLICK_OPENS_SELECTION:
        web_settings->middle_click_opens_selection = g_value_get_boolean (value);
        break;
    case PROP_OPEN_TABS_IN_THE_BACKGROUND:
        web_settings->open_tabs_in_the_background = g_value_get_boolean (value);
        break;
    case PROP_OPEN_TABS_NEXT_TO_CURRENT:
        web_settings->open_tabs_next_to_current = g_value_get_boolean (value);
        break;
    case PROP_OPEN_POPUPS_IN_TABS:
        web_settings->open_popups_in_tabs = g_value_get_boolean (value);
        break;

    case PROP_AUTO_LOAD_IMAGES:
        g_object_set (web_settings, "WebKitWebSettings::auto-load-images",
                      g_value_get_boolean (value), NULL);
        break;
    case PROP_ENABLE_SCRIPTS:
        g_object_set (web_settings, "WebKitWebSettings::enable-scripts",
                      g_value_get_boolean (value), NULL);
        break;
    case PROP_ENABLE_PLUGINS:
        g_object_set (web_settings,
            "WebKitWebSettings::enable-plugins", g_value_get_boolean (value),
        #if WEBKIT_CHECK_VERSION (1, 1, 22)
            "enable-java-applet", g_value_get_boolean (value),
        #endif
            NULL);
        break;
    case PROP_ENABLE_DEVELOPER_EXTRAS:
        g_object_set (web_settings, "WebKitWebSettings::enable-developer-extras",
                      g_value_get_boolean (value), NULL);
        break;
    case PROP_ENABLE_SPELL_CHECKING:
        g_object_set (web_settings, "WebKitWebSettings::enable-spell-checking",
                      g_value_get_boolean (value), NULL);
        break;
    case PROP_ENABLE_HTML5_DATABASE:
        g_object_set (web_settings, "WebKitWebSettings::enable-html5-database",
                      g_value_get_boolean (value), NULL);
        break;
    case PROP_ENABLE_HTML5_LOCAL_STORAGE:
        g_object_set (web_settings, "WebKitWebSettings::enable-html5-local-storage",
                      g_value_get_boolean (value), NULL);
        break;
    case PROP_ENABLE_OFFLINE_WEB_APPLICATION_CACHE:
        g_object_set (web_settings, "WebKitWebSettings::enable-offline-web-application-cache",
                      g_value_get_boolean (value), NULL);
        break;
    #if WEBKIT_CHECK_VERSION (1, 1, 18)
    case PROP_ENABLE_PAGE_CACHE:
        g_object_set (web_settings, "WebKitWebSettings::enable-page-cache",
                      g_value_get_boolean (value), NULL);
        break;
    #endif
    case PROP_ZOOM_TEXT_AND_IMAGES:
        web_settings->zoom_text_and_images = g_value_get_boolean (value);
        break;
    case PROP_FIND_WHILE_TYPING:
        web_settings->find_while_typing = g_value_get_boolean (value);
        break;
    case PROP_KINETIC_SCROLLING:
        web_settings->kinetic_scrolling = g_value_get_boolean (value);
        break;
    case PROP_MAXIMUM_COOKIE_AGE:
        web_settings->maximum_cookie_age = g_value_get_int (value);
        break;
    case PROP_FIRST_PARTY_COOKIES_ONLY:
        web_settings->first_party_cookies_only = g_value_get_boolean (value);
        break;

    case PROP_MAXIMUM_HISTORY_AGE:
        web_settings->maximum_history_age = g_value_get_int (value);
        break;

    case PROP_PROXY_TYPE:
        web_settings->proxy_type = g_value_get_enum (value);
    break;
    case PROP_HTTP_PROXY:
        katze_assign (web_settings->http_proxy, g_value_dup_string (value));
        break;
    case PROP_HTTP_PROXY_PORT:
        web_settings->http_proxy_port = g_value_get_int (value);
        break;
    #if WEBKIT_CHECK_VERSION (1, 3, 11)
    case PROP_MAXIMUM_CACHE_SIZE:
        web_settings->maximum_cache_size = g_value_get_int (value);
        break;
    #endif
    case PROP_IDENTIFY_AS:
        web_settings->identify_as = g_value_get_enum (value);
        if (web_settings->identify_as != MIDORI_IDENT_CUSTOM)
        {
            gchar* string = generate_ident_string (web_settings, web_settings->identify_as);
            katze_assign (web_settings->ident_string, string);
            g_object_set (web_settings, "user-agent", string, NULL);
        }
        break;
    case PROP_USER_AGENT:
        if (web_settings->identify_as == MIDORI_IDENT_CUSTOM)
            katze_assign (web_settings->ident_string, g_value_dup_string (value));
        g_object_set (web_settings, "WebKitWebSettings::user-agent",
                                    web_settings->ident_string, NULL);
        break;
    case PROP_PREFERRED_LANGUAGES:
        katze_assign (web_settings->http_accept_language, g_value_dup_string (value));
        g_object_set (web_settings, "spell-checking-languages",
                      web_settings->http_accept_language, NULL);
        break;
    case PROP_CLEAR_PRIVATE_DATA:
        web_settings->clear_private_data = g_value_get_int (value);
        break;
    case PROP_CLEAR_DATA:
        katze_assign (web_settings->clear_data, g_value_dup_string (value));
        break;
    #if !WEBKIT_CHECK_VERSION (1, 3, 13)
    case PROP_ENABLE_DNS_PREFETCHING:
        web_settings->enable_dns_prefetching = g_value_get_boolean (value);
        break;
    #endif
    case PROP_STRIP_REFERER:
        web_settings->strip_referer = g_value_get_boolean (value);
        break;
    case PROP_ENFORCE_FONT_FAMILY:
        if ((web_settings->enforce_font_family = g_value_get_boolean (value)))
        {
            gchar* font_family = katze_object_get_string (web_settings,
                                                          "default-font-family");
            gchar* monospace = katze_object_get_string (web_settings,
                                                        "monospace-font-family");
            gchar* css = g_strdup_printf ("body * { font-family: %s !important; } \
                code, code *, pre, pre *, blockquote, blockquote *, \
                input, textarea { font-family: %s !important; }",
                                          font_family, monospace);
            midori_web_settings_add_style (web_settings, "enforce-font-family", css);
            g_free (font_family);
            g_free (monospace);
            g_free (css);
        }
        else
            midori_web_settings_remove_style (web_settings, "enforce-font-family");
        break;
    case PROP_FLASH_WINDOW_ON_BG_TABS:
        web_settings->flash_window_on_bg_tabs = g_value_get_boolean (value);
        break;
    case PROP_USER_STYLESHEET_URI:
        {
            gint old_len = web_settings->user_stylesheet_uri_cached
                ? strlen (web_settings->user_stylesheet_uri_cached) : 0;
            gint new_len = 0;
            if ((web_settings->user_stylesheet_uri = g_value_dup_string (value)))
            {
                gchar* import = g_strdup_printf ("@import url(\"%s\");",
                    web_settings->user_stylesheet_uri);
                gchar* encoded = g_base64_encode ((const guchar*)import, strlen (import));
                new_len = strlen (encoded);
                base64_space_pad (encoded, new_len);
                g_free (import);
                katze_assign (web_settings->user_stylesheet_uri_cached, encoded);
            }
            /* Make original user-stylesheet-uri available to main.c */
            g_object_set_data (G_OBJECT (web_settings), "user-stylesheet-uri",
                web_settings->user_stylesheet_uri);
            midori_web_settings_process_stylesheets (web_settings, new_len - old_len);
        }
        break;
    case PROP_SEARCH_WIDTH:
        web_settings->search_width = g_value_get_int (value);
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

    switch (prop_id)
    {
    case PROP_REMEMBER_LAST_WINDOW_SIZE:
        g_value_set_boolean (value, web_settings->remember_last_window_size);
        break;
    case PROP_LAST_WINDOW_WIDTH:
        g_value_set_int (value, web_settings->last_window_width);
        break;
    case PROP_LAST_WINDOW_HEIGHT:
        g_value_set_int (value, web_settings->last_window_height);
        break;
    case PROP_LAST_WINDOW_STATE:
        g_value_set_enum (value, web_settings->last_window_state);
        break;
    case PROP_LAST_PANEL_POSITION:
        g_value_set_int (value, web_settings->last_panel_position);
        break;
    case PROP_LAST_PANEL_PAGE:
        g_value_set_int (value, web_settings->last_panel_page);
        break;
    case PROP_LAST_WEB_SEARCH:
        g_value_set_int (value, web_settings->last_web_search);
        break;

    case PROP_SHOW_MENUBAR:
        g_value_set_boolean (value, web_settings->show_menubar);
        break;
    case PROP_SHOW_NAVIGATIONBAR:
        g_value_set_boolean (value, web_settings->show_navigationbar);
        break;
    case PROP_SHOW_BOOKMARKBAR:
        g_value_set_boolean (value, web_settings->show_bookmarkbar);
        break;
    case PROP_SHOW_PANEL:
        g_value_set_boolean (value, web_settings->show_panel);
        break;
    case PROP_SHOW_STATUSBAR:
        g_value_set_boolean (value, web_settings->show_statusbar);
        break;

    case PROP_TOOLBAR_STYLE:
        g_value_set_enum (value, web_settings->toolbar_style);
        break;
    case PROP_TOOLBAR_ITEMS:
        g_value_set_string (value, web_settings->toolbar_items);
        break;
    case PROP_COMPACT_SIDEPANEL:
        g_value_set_boolean (value, web_settings->compact_sidepanel);
        break;
    case PROP_RIGHT_ALIGN_SIDEPANEL:
        g_value_set_boolean (value, web_settings->right_align_sidepanel);
        break;
    case PROP_OPEN_PANELS_IN_WINDOWS:
        g_value_set_boolean (value, web_settings->open_panels_in_windows);
        break;

    case PROP_LOAD_ON_STARTUP:
        g_value_set_enum (value, web_settings->load_on_startup);
        break;
    case PROP_HOMEPAGE:
        g_value_set_string (value, web_settings->homepage);
        break;
    case PROP_SHOW_CRASH_DIALOG:
        g_value_set_boolean (value, web_settings->show_crash_dialog);
        break;
    case PROP_DOWNLOAD_FOLDER:
        g_value_set_string (value, web_settings->download_folder);
        break;
    case PROP_TEXT_EDITOR:
        g_value_set_string (value, web_settings->text_editor);
        break;
    case PROP_NEWS_AGGREGATOR:
        g_value_set_string (value, web_settings->news_aggregator);
        break;
    case PROP_LOCATION_ENTRY_SEARCH:
        g_value_set_string (value, web_settings->location_entry_search);
        break;
    case PROP_PREFERRED_ENCODING:
        g_value_set_enum (value, web_settings->preferred_encoding);
        break;

    case PROP_ALWAYS_SHOW_TABBAR:
        g_value_set_boolean (value, web_settings->always_show_tabbar);
        break;
    case PROP_CLOSE_BUTTONS_ON_TABS:
        g_value_set_boolean (value, web_settings->close_buttons_on_tabs);
        break;
    case PROP_CLOSE_BUTTONS_LEFT:
        #if HAVE_OSX
        g_value_set_boolean (value, TRUE);
        #elif defined (G_OS_WIN32)
        g_value_set_boolean (value, FALSE);
        #else
        if (!web_settings->close_buttons_left)
        {
            /* Look for close button in layout specified in index.theme */
            GdkScreen* screen = gdk_screen_get_default ();
            GtkSettings* settings = gtk_settings_get_for_screen (screen);
            gchar* theme = katze_object_get_string (settings, "gtk-theme-name");
            gchar* theme_file = g_build_filename ("themes", theme, "index.theme", NULL);
            gchar* filename = sokoke_find_data_filename (theme_file, FALSE);
            g_free (theme_file);
            web_settings->close_buttons_left = 1;
            if (g_access (filename, F_OK) != 0)
                katze_assign (filename,
                   g_build_filename (g_get_home_dir (), ".themes",
                                     theme, "index.theme", NULL));
            g_free (theme);
            if (g_access (filename, F_OK) == 0)
            {
                GKeyFile* keyfile = g_key_file_new ();
                gchar* button_layout;
                g_key_file_load_from_file (keyfile, filename, 0, NULL);
                button_layout = g_key_file_get_string (keyfile,
                    "X-GNOME-Metatheme", "ButtonLayout", NULL);
                if (button_layout && strstr (button_layout, "close:"))
                    web_settings->close_buttons_left = 2;
                g_free (button_layout);
                g_key_file_free (keyfile);
            }
            g_free (filename);
        }
        g_value_set_boolean (value, web_settings->close_buttons_left == 2);
        #endif
        break;
    case PROP_OPEN_NEW_PAGES_IN:
        g_value_set_enum (value, web_settings->open_new_pages_in);
        break;
    case PROP_MIDDLE_CLICK_OPENS_SELECTION:
        g_value_set_boolean (value, web_settings->middle_click_opens_selection);
        break;
    case PROP_OPEN_TABS_IN_THE_BACKGROUND:
        g_value_set_boolean (value, web_settings->open_tabs_in_the_background);
        break;
    case PROP_OPEN_TABS_NEXT_TO_CURRENT:
        g_value_set_boolean (value, web_settings->open_tabs_next_to_current);
        break;
    case PROP_OPEN_POPUPS_IN_TABS:
        g_value_set_boolean (value, web_settings->open_popups_in_tabs);
        break;

    case PROP_AUTO_LOAD_IMAGES:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::auto-load-images"));
        break;
    case PROP_ENABLE_SCRIPTS:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-scripts"));
        break;
    case PROP_ENABLE_PLUGINS:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-plugins"));
        break;
    case PROP_ENABLE_DEVELOPER_EXTRAS:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-developer-extras"));
        break;
    case PROP_ENABLE_SPELL_CHECKING:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-spell-checking"));
        break;
    case PROP_ENABLE_HTML5_DATABASE:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-html5-database"));
        break;
    case PROP_ENABLE_HTML5_LOCAL_STORAGE:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-html5-local-storage"));
        break;
    case PROP_ENABLE_OFFLINE_WEB_APPLICATION_CACHE:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-offline-web-application-cache"));
        break;
    #if WEBKIT_CHECK_VERSION (1, 1, 18)
    case PROP_ENABLE_PAGE_CACHE:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-page-cache"));
        break;
    #endif
    case PROP_ZOOM_TEXT_AND_IMAGES:
        g_value_set_boolean (value, web_settings->zoom_text_and_images);
        break;
    case PROP_FIND_WHILE_TYPING:
        g_value_set_boolean (value, web_settings->find_while_typing);
        break;
    case PROP_KINETIC_SCROLLING:
        g_value_set_boolean (value, web_settings->kinetic_scrolling);
        break;
    case PROP_MAXIMUM_COOKIE_AGE:
        g_value_set_int (value, web_settings->maximum_cookie_age);
        break;
    case PROP_FIRST_PARTY_COOKIES_ONLY:
        g_value_set_boolean (value, web_settings->first_party_cookies_only);
        break;

    case PROP_MAXIMUM_HISTORY_AGE:
        g_value_set_int (value, web_settings->maximum_history_age);
        break;

    case PROP_PROXY_TYPE:
        g_value_set_enum (value, web_settings->proxy_type);
        break;
    case PROP_HTTP_PROXY:
        g_value_set_string (value, web_settings->http_proxy);
        break;
    case PROP_HTTP_PROXY_PORT:
        g_value_set_int (value, web_settings->http_proxy_port);
        break;
    #if WEBKIT_CHECK_VERSION (1, 3, 11)
    case PROP_MAXIMUM_CACHE_SIZE:
        g_value_set_int (value, web_settings->maximum_cache_size);
        break;
    #endif
    case PROP_IDENTIFY_AS:
        g_value_set_enum (value, web_settings->identify_as);
        break;
    case PROP_USER_AGENT:
        if (!g_strcmp0 (web_settings->ident_string, ""))
        {
            gchar* string = generate_ident_string (web_settings, web_settings->identify_as);
            katze_assign (web_settings->ident_string, string);
        }
        g_value_set_string (value, web_settings->ident_string);
        break;
    case PROP_PREFERRED_LANGUAGES:
        g_value_set_string (value, web_settings->http_accept_language);
        break;
    case PROP_CLEAR_PRIVATE_DATA:
        g_value_set_int (value, web_settings->clear_private_data);
        break;
    case PROP_CLEAR_DATA:
        g_value_set_string (value, web_settings->clear_data);
        break;
    #if !WEBKIT_CHECK_VERSION (1, 3, 13)
    case PROP_ENABLE_DNS_PREFETCHING:
        g_value_set_boolean (value, web_settings->enable_dns_prefetching);
        break;
    #endif
    case PROP_STRIP_REFERER:
        g_value_set_boolean (value, web_settings->strip_referer);
        break;
    case PROP_ENFORCE_FONT_FAMILY:
        g_value_set_boolean (value, web_settings->enforce_font_family);
        break;
    case PROP_FLASH_WINDOW_ON_BG_TABS:
        g_value_set_boolean (value, web_settings->flash_window_on_bg_tabs);
        break;
    case PROP_USER_STYLESHEET_URI:
        g_value_take_string (value, katze_object_get_string (web_settings,
            "WebKitWebSettings::user-stylesheet-uri"));
        break;
    case PROP_SEARCH_WIDTH:
        g_value_set_int (value, web_settings->search_width);
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

static void
midori_web_settings_process_stylesheets (MidoriWebSettings* settings,
                                         gint               delta_len)
{
    GHashTableIter it;
    GString* css;
    gchar* encoded;
    gpointer value;
    static guint length = 0;

    g_return_if_fail ((gint)length >= -delta_len);

    length += delta_len;

    /* Precalculate size to avoid re-allocations */
    css = g_string_sized_new (length);

    if (settings->user_stylesheet_uri_cached != NULL)
        g_string_append (css, settings->user_stylesheet_uri_cached);

    if (settings->user_stylesheets != NULL)
    {
        g_hash_table_iter_init (&it, settings->user_stylesheets);
        while (g_hash_table_iter_next (&it, NULL, &value))
            g_string_append (css, (gchar*)value);
    }

    /* data: uri prefix from Source/WebCore/page/Page.cpp:700 in WebKit */
    encoded = g_strconcat ("data:text/css;charset=utf-8;base64,", css->str, NULL);
    g_object_set (G_OBJECT (settings), "WebKitWebSettings::user-stylesheet-uri", encoded, NULL);
    g_free (encoded);
    g_string_free (css, TRUE);
}

static void
base64_space_pad (gchar* base64,
                  guint  len)
{
    /* Replace '=' padding at the end with encoded spaces
       so WebKit will accept concatenations to this string */
    if (len > 2 && base64[len - 2] == '=')
    {
        base64[len - 3] += 2;
        base64[len - 2] = 'A';
    }
    if (len > 1 && base64[len - 1] == '=')
        base64[len - 1] = 'g';
}

/**
 * midori_web_settings_add_style:
 * @rule_id: a static string identifier
 * @style: a CSS stylesheet
 *
 * Adds or replaces a custom stylesheet.
 *
 * Since: 0.4.2
 **/
void
midori_web_settings_add_style (MidoriWebSettings* settings,
                               const gchar*       rule_id,
                               const gchar*       style)
{
    gchar* base64;
    guint len;

    g_return_if_fail (MIDORI_IS_WEB_SETTINGS (settings));
    g_return_if_fail (rule_id != NULL);
    g_return_if_fail (style != NULL);

    len = strlen (style);
    base64 = g_base64_encode ((const guchar*)style, len);
    len = ((len + 2) / 3) * 4;
    base64_space_pad (base64, len);

    if (settings->user_stylesheets == NULL)
        settings->user_stylesheets = g_hash_table_new_full (g_str_hash, NULL,
                                                            NULL, g_free);

    g_hash_table_insert (settings->user_stylesheets, (gchar*)rule_id, base64);
    midori_web_settings_process_stylesheets (settings, len);
}

/**
 * midori_web_settings_remove_style:
 * @rule_id: the string identifier used previously
 *
 * Removes a stylesheet from midori settings.
 *
 * Since: 0.4.2
 **/
void
midori_web_settings_remove_style (MidoriWebSettings* settings,
                                  const gchar*       rule_id)
{
    gchar* str;

    g_return_if_fail (MIDORI_IS_WEB_SETTINGS (settings));
    g_return_if_fail (rule_id != NULL);

    if (settings->user_stylesheets != NULL)
    {
        if ((str = g_hash_table_lookup (settings->user_stylesheets, rule_id)))
        {
            guint len = strlen (str);
            g_hash_table_remove (settings->user_stylesheets, rule_id);
            midori_web_settings_process_stylesheets (settings, -len);
        }
    }
}
