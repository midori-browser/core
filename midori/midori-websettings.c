/*
 Copyright (C) 2008-2010 Christian Dywan <christian@twotoasts.de>

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

#if HAVE_CONFIG_H
    #include <config.h>
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
    gboolean show_transferbar : 1;
    gboolean show_statusbar : 1;
    MidoriToolbarStyle toolbar_style : 3;
    gboolean search_engines_in_completion : 1;
    gboolean compact_sidepanel : 1;
    gboolean show_panel_controls : 1;
    gboolean right_align_sidepanel : 1;
    gboolean open_panels_in_windows : 1;
    MidoriStartup load_on_startup : 2;
    gboolean show_crash_dialog : 1;
    gboolean speed_dial_in_new_tabs : 1;
    MidoriPreferredEncoding preferred_encoding : 3;
    gboolean always_show_tabbar : 1;
    gboolean close_buttons_on_tabs : 1;
    gboolean close_buttons_left : 1;
    MidoriNewPage open_new_pages_in : 2;
    MidoriNewPage open_external_pages_in : 2;
    gboolean middle_click_opens_selection : 1;
    gboolean open_tabs_in_the_background : 1;
    gboolean open_tabs_next_to_current : 1;
    gboolean open_popups_in_tabs : 1;
    gboolean zoom_text_and_images : 1;
    gboolean find_while_typing : 1;
    gboolean kinetic_scrolling : 1;
    MidoriAcceptCookies accept_cookies : 2;
    gboolean original_cookies_only : 1;
    gboolean remember_last_visited_pages : 1;
    gboolean remember_last_downloaded_files : 1;
    MidoriProxy proxy_type : 2;
    MidoriIdentity identify_as : 3;

    gint last_window_width;
    gint last_window_height;
    gint last_panel_position;
    gint last_panel_page;
    gint last_web_search;
    gint maximum_cookie_age;
    gint maximum_history_age;

    gchar* toolbar_items;
    gchar* homepage;
    gchar* download_folder;
    gchar* download_manager;
    gchar* text_editor;
    gchar* news_aggregator;
    gchar* location_entry_search;
    gchar* http_proxy;
    gchar* http_accept_language;
    gchar* ident_string;

    gint clear_private_data;
    gchar* clear_data;
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
    PROP_SHOW_TRANSFERBAR,
    PROP_SHOW_STATUSBAR,

    PROP_TOOLBAR_STYLE,
    PROP_SEARCH_ENGINES_IN_COMPLETION,
    PROP_TOOLBAR_ITEMS,
    PROP_COMPACT_SIDEPANEL,
    PROP_SHOW_PANEL_CONTROLS,
    PROP_RIGHT_ALIGN_SIDEPANEL,
    PROP_OPEN_PANELS_IN_WINDOWS,

    PROP_LOAD_ON_STARTUP,
    PROP_HOMEPAGE,
    PROP_SHOW_CRASH_DIALOG,
    PROP_SPEED_DIAL_IN_NEW_TABS,
    PROP_DOWNLOAD_FOLDER,
    PROP_DOWNLOAD_MANAGER,
    PROP_TEXT_EDITOR,
    PROP_NEWS_AGGREGATOR,
    PROP_LOCATION_ENTRY_SEARCH,
    PROP_PREFERRED_ENCODING,

    PROP_ALWAYS_SHOW_TABBAR,
    PROP_CLOSE_BUTTONS_ON_TABS,
    PROP_CLOSE_BUTTONS_LEFT,
    PROP_OPEN_NEW_PAGES_IN,
    PROP_OPEN_EXTERNAL_PAGES_IN,
    PROP_MIDDLE_CLICK_OPENS_SELECTION,
    PROP_OPEN_TABS_IN_THE_BACKGROUND,
    PROP_OPEN_TABS_NEXT_TO_CURRENT,
    PROP_OPEN_POPUPS_IN_TABS,

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
    PROP_ACCEPT_COOKIES,
    PROP_MAXIMUM_COOKIE_AGE,

    PROP_MAXIMUM_HISTORY_AGE,
    PROP_REMEMBER_LAST_DOWNLOADED_FILES,

    PROP_PROXY_TYPE,
    PROP_HTTP_PROXY,
    PROP_IDENTIFY_AS,
    PROP_USER_AGENT,
    PROP_PREFERRED_LANGUAGES,

    PROP_CLEAR_PRIVATE_DATA,
    PROP_CLEAR_DATA
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
         #if WEBKIT_CHECK_VERSION (1, 1, 6)
         { MIDORI_STARTUP_DELAYED_PAGES, "MIDORI_STARTUP_DELAYED_PAGES", N_("Show last tabs without loading") },
         #endif
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
midori_accept_cookies_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_ACCEPT_COOKIES_ALL, "MIDORI_ACCEPT_COOKIES_ALL", N_("All cookies") },
         { MIDORI_ACCEPT_COOKIES_SESSION, "MIDORI_ACCEPT_COOKIES_SESSION", N_("Session cookies") },
         { MIDORI_ACCEPT_COOKIES_NONE, "MIDORI_ACCEPT_COOKIES_NONE", N_("None") },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriAcceptCookies", values);
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
         { MIDORI_IDENT_MIDORI, "MIDORI_IDENT_MIDORI", N_("Midori") },
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
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_LAST_WINDOW_HEIGHT,
                                     g_param_spec_int (
                                     "last-window-height",
                                     _("Last window height"),
                                     _("The last saved window height"),
                                     0, G_MAXINT, 0,
                                     flags));

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
        /* i18n: The internal index of the last opened panel */
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

    /**
     * MidoriWebSettings:show-transferbar:
     *
     * Whether to show the transferbar.
     *
     * Since: 0.1.5
     *
     * Deprecated: 0.3.1
     */
    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_TRANSFERBAR,
                                     g_param_spec_boolean (
                                     "show-transferbar",
                                     _("Show Transferbar"),
                                     _("Whether to show the transferbar"),
                                     TRUE,
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

    /**
    * MidoriWebSettings:search-engines-in-completion:
    *
    * Whether to show search engines in the location completion.
    *
    * Since: 0.1.6
    *
    * Deprecated: 0.3.1
    */
    g_object_class_install_property (gobject_class,
                                     PROP_SEARCH_ENGINES_IN_COMPLETION,
                                     g_param_spec_boolean (
                                     "search-engines-in-completion",
                                     _("Search engines in location completion"),
                                     _("Whether to show search engines in the location completion"),
                                     TRUE,
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
     * MidoriWebSettings:show-panel-controls:
     *
     * Whether to show the operating controls of the panel.
     *
     * Since: 0.1.9
     *
     * Deprecated: 0.3.0
     */
    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_PANEL_CONTROLS,
                                     g_param_spec_boolean (
                                     "show-panel-controls",
                                     _("Show operating controls of the panel"),
                                     _("Whether to show the operating controls of the panel"),
                                     TRUE,
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

    /**
    * MidoriWebSettings:speed-dial-in-new-tabs:
    *
    * Show spee dial in newly opened tabs.
    *
    * Since: 0.1.7
    *
    * Deprecated: 0.3.4
    */
    g_object_class_install_property (gobject_class,
                                     PROP_SPEED_DIAL_IN_NEW_TABS,
                                     g_param_spec_boolean (
                                     "speed-dial-in-new-tabs",
        /* i18n: Speed dial, webpage shortcuts, named for the phone function */
                                     _("Show speed dial in new tabs"),
                                     _("Show speed dial in newly opened tabs"),
                                     TRUE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_DOWNLOAD_FOLDER,
                                     g_param_spec_string (
                                     "download-folder",
                                     _("Save downloaded files to:"),
                                     _("The folder downloaded files are saved to"),
                                     midori_get_download_dir (),
    #if WEBKIT_CHECK_VERSION (1, 1, 3)
                                     flags));
    #else
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
    #endif

    g_object_class_install_property (gobject_class,
                                     PROP_DOWNLOAD_MANAGER,
                                     g_param_spec_string (
                                     "download-manager",
                                     _("Download Manager"),
                                     _("An external download manager"),
                                     NULL,
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
                                     #if HAVE_OSX
                                     TRUE,
                                     #else
                                     FALSE,
                                     #endif
                                     flags));


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
                                     PROP_OPEN_EXTERNAL_PAGES_IN,
                                     g_param_spec_enum (
                                     "open-external-pages-in",
                                     _("Open external pages in:"),
                                     _("Where to open externally opened pages"),
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
                                     TRUE,
                                     flags));
    /* Override properties to override defaults */
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_DEVELOPER_EXTRAS,
                                     g_param_spec_boolean (
                                     "enable-developer-extras",
                                     "Enable developer tools",
                                     "Enable special extensions for developers",
                                     TRUE,
                                     flags));
    #if WEBKIT_CHECK_VERSION (1, 1, 6)
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_SPELL_CHECKING,
                                     g_param_spec_boolean ("enable-spell-checking",
                                                           _("Enable Spell Checking"),
                                                           _("Enable spell checking while typing"),
                                                           TRUE,
                                                           flags));
    #endif
    #if WEBKIT_CHECK_VERSION (1, 1, 8)
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
    #endif
    #if WEBKIT_CHECK_VERSION (1, 1, 13)
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_OFFLINE_WEB_APPLICATION_CACHE,
                                     g_param_spec_boolean ("enable-offline-web-application-cache",
                                                           _("Enable offline web application cache"),
                                                           _("Whether to enable offline web application cache"),
                                                           FALSE,
                                                           flags));
    #endif
    #if WEBKIT_CHECK_VERSION (1, 1, 18)
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_PAGE_CACHE,
                                     g_param_spec_boolean ("enable-page-cache",
                                                           "Enable page cache",
                                                           "Whether the page cache should be used",
                                                           TRUE,
                                                           flags));
    #endif

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
                                     PROP_ACCEPT_COOKIES,
                                     g_param_spec_enum (
                                     "accept-cookies",
                                     _("Accept cookies"),
                                     _("What type of cookies to accept"),
                                     MIDORI_TYPE_ACCEPT_COOKIES,
                                     MIDORI_ACCEPT_COOKIES_ALL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_MAXIMUM_COOKIE_AGE,
                                     g_param_spec_int (
                                     "maximum-cookie-age",
                                     _("Maximum cookie age"),
                                     _("The maximum number of days to save cookies for"),
                                     0, G_MAXINT, 30,
                                     flags));


    g_object_class_install_property (gobject_class,
                                     PROP_MAXIMUM_HISTORY_AGE,
                                     g_param_spec_int (
                                     "maximum-history-age",
                                     _("Maximum history age"),
                                     _("The maximum number of days to save the history for"),
                                     0, G_MAXINT, 30,
                                     flags));

    /**
     * MidoriWebSettings:remember-last-downloaded-files:
     *
     * Whether the last downloaded files are saved.
     *
     * Deprecated: 0.2.9
     **/
    g_object_class_install_property (gobject_class,
                                     PROP_REMEMBER_LAST_DOWNLOADED_FILES,
                                     g_param_spec_boolean (
                                     "remember-last-downloaded-files",
                                     _("Remember last downloaded files"),
                                     _("Whether the last downloaded files are saved"),
                                     TRUE,
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
midori_web_settings_init (MidoriWebSettings* web_settings)
{
    web_settings->download_folder = g_strdup (midori_get_download_dir ());
    web_settings->http_proxy = NULL;
    web_settings->show_panel_controls = TRUE;
    web_settings->open_popups_in_tabs = TRUE;
    web_settings->remember_last_downloaded_files = TRUE;
    web_settings->kinetic_scrolling = TRUE;

    g_signal_connect (web_settings, "notify::default-encoding",
                      G_CALLBACK (notify_default_encoding_cb), NULL);
}

static void
midori_web_settings_finalize (GObject* object)
{
    MidoriWebSettings* web_settings;

    web_settings = MIDORI_WEB_SETTINGS (object);

    katze_assign (web_settings->toolbar_items, NULL);
    katze_assign (web_settings->homepage, NULL);
    katze_assign (web_settings->download_folder, NULL);
    katze_assign (web_settings->download_manager, NULL);
    katze_assign (web_settings->text_editor, NULL);
    katze_assign (web_settings->news_aggregator, NULL);
    katze_assign (web_settings->location_entry_search, NULL);
    katze_assign (web_settings->http_proxy, NULL);
    katze_assign (web_settings->ident_string, NULL);

    G_OBJECT_CLASS (midori_web_settings_parent_class)->finalize (object);
}

#if defined (G_OS_UNIX) && !HAVE_OSX
static gchar*
get_sys_name (void)
{
    static gchar* sys_name = NULL;

    if (!sys_name)
    {
        struct utsname name;
        if (uname (&name) != -1)
            sys_name = g_strdup(name.sysname);
        else
            sys_name = "Unix";
    }
    return sys_name;
}
#endif

static gchar*
generate_ident_string (MidoriIdentity identify_as)
{
    const gchar* platform =
    #ifdef GDK_WINDOWING_X11
    "X11";
    #elif defined(GDK_WINDOWING_WIN32)
    "Windows";
    #elif defined(GDK_WINDOWING_QUARTZ)
    "Macintosh";
    #elif defined(GDK_WINDOWING_DIRECTFB)
    "DirectFB";
    #else
    "Unknown";
    #endif

    const gchar* os =
    #if HAVE_OSX
    "Mac OS X";
    #elif defined (G_OS_UNIX)
    get_sys_name ();
    #elif defined (G_OS_WIN32)
    "Windows";
    #else
    "Unknown";
    #endif

    const gchar* appname = "Midori/"
        G_STRINGIFY (MIDORI_MAJOR_VERSION) "."
        G_STRINGIFY (MIDORI_MINOR_VERSION);

    const gchar* lang = pango_language_to_string (gtk_get_default_language ());

    #ifndef WEBKIT_USER_AGENT_MAJOR_VERSION
        #define WEBKIT_USER_AGENT_MAJOR_VERSION 532
        #define WEBKIT_USER_AGENT_MINOR_VERSION 1
    #endif
    const int webcore_major = WEBKIT_USER_AGENT_MAJOR_VERSION;
    const int webcore_minor = WEBKIT_USER_AGENT_MINOR_VERSION;

    switch (identify_as)
    {
    case MIDORI_IDENT_MIDORI:
        return g_strdup_printf ("%s (%s; %s; U; %s) WebKit/%d.%d+",
            appname, platform, os, lang, webcore_major, webcore_minor);
    case MIDORI_IDENT_SAFARI:
        return g_strdup_printf ("Mozilla/5.0 (%s; U; %s; %s) "
            "AppleWebKit/%d+ (KHTML, like Gecko) Safari/%d.%d+ %s",
            platform, os, lang, webcore_major, webcore_major, webcore_minor, appname);
    case MIDORI_IDENT_IPHONE:
        return g_strdup_printf ("Mozilla/5.0 (iPhone; U; %s; %s) "
            "AppleWebKit/532+ (KHTML, like Gecko) Version/3.0 Mobile/1A538b "
            "Safari/419.3 %s",
                                os, lang, appname);
    case MIDORI_IDENT_FIREFOX:
        return g_strdup_printf ("Mozilla/5.0 (%s; U; %s; %s; rv:1.9.0.2) "
            "Gecko/2008092313 Firefox/3.8 %s",
                                platform, os, lang, appname);
    case MIDORI_IDENT_EXPLORER:
        return g_strdup_printf ("Mozilla/4.0 (compatible; "
            "MSIE 6.0; Windows NT 5.1; %s) %s",
                                lang, appname);
    default:
        return g_strdup_printf ("%s", appname);
    }
}

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
    case PROP_SHOW_TRANSFERBAR:
        web_settings->show_transferbar = g_value_get_boolean (value);
        break;
    case PROP_SHOW_STATUSBAR:
        web_settings->show_statusbar = g_value_get_boolean (value);
        break;

    case PROP_TOOLBAR_STYLE:
        web_settings->toolbar_style = g_value_get_enum (value);
        break;
    case PROP_SEARCH_ENGINES_IN_COMPLETION:
        web_settings->search_engines_in_completion = g_value_get_boolean (value);
        break;
    case PROP_TOOLBAR_ITEMS:
        katze_assign (web_settings->toolbar_items, g_value_dup_string (value));
        break;
    case PROP_COMPACT_SIDEPANEL:
        web_settings->compact_sidepanel = g_value_get_boolean (value);
        break;
    case PROP_SHOW_PANEL_CONTROLS:
        web_settings->show_panel_controls = g_value_get_boolean (value);
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
    case PROP_SPEED_DIAL_IN_NEW_TABS:
        web_settings->speed_dial_in_new_tabs = g_value_get_boolean (value);
        break;
    case PROP_DOWNLOAD_FOLDER:
        katze_assign (web_settings->download_folder, g_value_dup_string (value));
        break;
    case PROP_DOWNLOAD_MANAGER:
        katze_assign (web_settings->download_manager, g_value_dup_string (value));
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
    case PROP_CLOSE_BUTTONS_LEFT:
        web_settings->close_buttons_left = g_value_get_boolean (value);
        break;
    case PROP_OPEN_NEW_PAGES_IN:
        web_settings->open_new_pages_in = g_value_get_enum (value);
        break;
    case PROP_OPEN_EXTERNAL_PAGES_IN:
        web_settings->open_external_pages_in = g_value_get_enum (value);
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
    #if WEBKIT_CHECK_VERSION (1, 1, 6)
    case PROP_ENABLE_SPELL_CHECKING:
        g_object_set (web_settings, "WebKitWebSettings::enable-spell-checking",
                      g_value_get_boolean (value), NULL);
        break;
    #endif
    #if WEBKIT_CHECK_VERSION (1, 1, 8)
    case PROP_ENABLE_HTML5_DATABASE:
        g_object_set (web_settings, "WebKitWebSettings::enable-html5-database",
                      g_value_get_boolean (value), NULL);
        break;
    case PROP_ENABLE_HTML5_LOCAL_STORAGE:
        g_object_set (web_settings, "WebKitWebSettings::enable-html5-local-storage",
                      g_value_get_boolean (value), NULL);
        break;
    #endif
    #if WEBKIT_CHECK_VERSION (1, 1, 13)
    case PROP_ENABLE_OFFLINE_WEB_APPLICATION_CACHE:
        g_object_set (web_settings, "WebKitWebSettings::enable-offline-web-application-cache",
                      g_value_get_boolean (value), NULL);
        break;
    #endif
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
    case PROP_ACCEPT_COOKIES:
        web_settings->accept_cookies = g_value_get_enum (value);
        break;
    case PROP_MAXIMUM_COOKIE_AGE:
        web_settings->maximum_cookie_age = g_value_get_int (value);
        break;

    case PROP_MAXIMUM_HISTORY_AGE:
        web_settings->maximum_history_age = g_value_get_int (value);
        break;
    case PROP_REMEMBER_LAST_DOWNLOADED_FILES:
        web_settings->remember_last_downloaded_files = g_value_get_boolean (value);
        break;

    case PROP_PROXY_TYPE:
        web_settings->proxy_type = g_value_get_enum (value);
    break;
    case PROP_HTTP_PROXY:
        katze_assign (web_settings->http_proxy, g_value_dup_string (value));
        break;
    case PROP_IDENTIFY_AS:
        web_settings->identify_as = g_value_get_enum (value);
        if (web_settings->identify_as != MIDORI_IDENT_CUSTOM)
        {
            gchar* string = generate_ident_string (web_settings->identify_as);
            katze_assign (web_settings->ident_string, string);
            #if WEBKIT_CHECK_VERSION (1, 1, 11)
            g_object_set (web_settings, "user-agent", string, NULL);
            #else
            g_object_notify (object, "user-agent");
            #endif
        }
        break;
    case PROP_USER_AGENT:
        if (web_settings->identify_as == MIDORI_IDENT_CUSTOM)
            katze_assign (web_settings->ident_string, g_value_dup_string (value));
        #if WEBKIT_CHECK_VERSION (1, 1, 11)
        g_object_set (web_settings, "WebKitWebSettings::user-agent",
                                    web_settings->ident_string, NULL);
        #endif
        break;
    case PROP_PREFERRED_LANGUAGES:
        katze_assign (web_settings->http_accept_language, g_value_dup_string (value));
        #if WEBKIT_CHECK_VERSION (1, 1, 6)
        g_object_set (web_settings, "spell-checking-languages",
                      web_settings->http_accept_language, NULL);
        #endif
        break;
    case PROP_CLEAR_PRIVATE_DATA:
        web_settings->clear_private_data = g_value_get_int (value);
        break;
    case PROP_CLEAR_DATA:
        katze_assign (web_settings->clear_data, g_value_dup_string (value));
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
    case PROP_SHOW_TRANSFERBAR:
        g_value_set_boolean (value, web_settings->show_transferbar);
        break;
    case PROP_SHOW_STATUSBAR:
        g_value_set_boolean (value, web_settings->show_statusbar);
        break;

    case PROP_TOOLBAR_STYLE:
        g_value_set_enum (value, web_settings->toolbar_style);
        break;
    case PROP_SEARCH_ENGINES_IN_COMPLETION:
        g_value_set_boolean (value, web_settings->search_engines_in_completion);
        break;
    case PROP_TOOLBAR_ITEMS:
        g_value_set_string (value, web_settings->toolbar_items);
        break;
    case PROP_COMPACT_SIDEPANEL:
        g_value_set_boolean (value, web_settings->compact_sidepanel);
        break;
    case PROP_SHOW_PANEL_CONTROLS:
        g_value_set_boolean (value, web_settings->show_panel_controls);
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
    case PROP_SPEED_DIAL_IN_NEW_TABS:
        g_value_set_boolean (value, web_settings->speed_dial_in_new_tabs);
        break;
    case PROP_DOWNLOAD_FOLDER:
        g_value_set_string (value, web_settings->download_folder);
        break;
    case PROP_DOWNLOAD_MANAGER:
        g_value_set_string (value, web_settings->download_manager);
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
        g_value_set_boolean (value, web_settings->close_buttons_left);
        break;
    case PROP_OPEN_NEW_PAGES_IN:
        g_value_set_enum (value, web_settings->open_new_pages_in);
        break;
    case PROP_OPEN_EXTERNAL_PAGES_IN:
        g_value_set_enum (value, web_settings->open_external_pages_in);
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
    #if WEBKIT_CHECK_VERSION (1, 1, 6)
    case PROP_ENABLE_SPELL_CHECKING:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-spell-checking"));
        break;
    #endif
    #if WEBKIT_CHECK_VERSION (1, 1, 8)
    case PROP_ENABLE_HTML5_DATABASE:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-html5-database"));
        break;
    case PROP_ENABLE_HTML5_LOCAL_STORAGE:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-html5-local-storage"));
        break;
    #endif
    #if WEBKIT_CHECK_VERSION (1, 1, 13)
    case PROP_ENABLE_OFFLINE_WEB_APPLICATION_CACHE:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-offline-web-application-cache"));
        break;
    #endif
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
    case PROP_ACCEPT_COOKIES:
        g_value_set_enum (value, web_settings->accept_cookies);
        break;
    case PROP_MAXIMUM_COOKIE_AGE:
        g_value_set_int (value, web_settings->maximum_cookie_age);
        break;

    case PROP_MAXIMUM_HISTORY_AGE:
        g_value_set_int (value, web_settings->maximum_history_age);
        break;
    case PROP_REMEMBER_LAST_DOWNLOADED_FILES:
        g_value_set_boolean (value, web_settings->remember_last_downloaded_files);
        break;

    case PROP_PROXY_TYPE:
        g_value_set_enum (value, web_settings->proxy_type);
        break;
    case PROP_HTTP_PROXY:
        g_value_set_string (value, web_settings->http_proxy);
        break;
    case PROP_IDENTIFY_AS:
        g_value_set_enum (value, web_settings->identify_as);
        break;
    case PROP_USER_AGENT:
        if (!g_strcmp0 (web_settings->ident_string, ""))
        {
            gchar* string = generate_ident_string (web_settings->identify_as);
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
