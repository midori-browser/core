/*
 Copyright (C) 2008-2012 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2011 Peter Hatina <phatina@redhat.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-websettings.h"

#include "midori-app.h"
#include "sokoke.h"
#include <midori/midori-core.h> /* Vala API */

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
#if defined(__FreeBSD__)
    #include <sys/types.h>
    #include <sys/sysctl.h>
#endif

struct _MidoriWebSettings
{
    MidoriSettings parent_instance;

    MidoriToolbarStyle toolbar_style : 3;
    MidoriStartup load_on_startup : 2;
    MidoriPreferredEncoding preferred_encoding : 3;
    gint close_buttons_left;
    MidoriNewPage open_new_pages_in : 2;
    gboolean first_party_cookies_only : 1;
    MidoriProxy proxy_type : 2;
    MidoriIdentity identify_as : 3;

    gchar* http_accept_language;
    gchar* accept;
    gchar* ident_string;

    gint clear_private_data;
    gchar* clear_data;
    gchar* site_data_rules;
    #if !WEBKIT_CHECK_VERSION (1, 3, 13)
    gboolean enable_dns_prefetching;
    #endif
    gboolean enforce_font_family;
    gchar* user_stylesheet_uri;
    gchar* user_stylesheet_uri_cached;
    GHashTable* user_stylesheets;
};

struct _MidoriWebSettingsClass
{
    MidoriSettingsClass parent_class;
};

G_DEFINE_TYPE (MidoriWebSettings, midori_web_settings, MIDORI_TYPE_SETTINGS);

enum
{
    PROP_0,

    PROP_TOOLBAR_STYLE,

    PROP_LOAD_ON_STARTUP,
    PROP_PREFERRED_ENCODING,

    PROP_CLOSE_BUTTONS_LEFT,
    PROP_OPEN_NEW_PAGES_IN,
    PROP_ENABLE_FULLSCREEN,

    PROP_ENABLE_PLUGINS,
    PROP_ENABLE_PAGE_CACHE,

    PROP_PROXY_TYPE,
    PROP_IDENTIFY_AS,
    PROP_USER_AGENT,
    PROP_PREFERRED_LANGUAGES,

    PROP_SITE_DATA_RULES,
    PROP_ENABLE_DNS_PREFETCHING,
    PROP_ENFORCE_FONT_FAMILY,
    PROP_USER_STYLESHEET_URI,
};

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
         { MIDORI_ENCODING_CHINESE, "MIDORI_ENCODING_CHINESE", N_("Chinese Traditional (BIG5)") },
         { MIDORI_ENCODING_CHINESE_SIMPLIFIED, "MIDORI_ENCODING_CHINESE_SIMPLIFIED", N_("Chinese Simplified (GB18030)") },
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
         { MIDORI_IDENT_CHROME, "MIDORI_IDENT_CHROME", N_("Chrome") },
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

static gboolean
midori_web_settings_low_memory_profile ()
{
#ifdef _WIN32
    /* See http://msdn.microsoft.com/en-us/library/windows/desktop/aa366589(v=vs.85).aspx */
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof (mem);
    if (GlobalMemoryStatusEx (&mem))
        return mem.ullTotalPhys / 1024 / 1024 < 352;
#elif defined(__FreeBSD__)
    size_t size;
    int mem_total;
    size = sizeof mem_total;

    sysctlbyname("hw.realmem", &mem_total, &size, NULL, 0);

    return mem_total / 1048576 < 352;
#else
    gchar* contents;
    const gchar* total;
    if (!g_file_get_contents ("/proc/meminfo", &contents, NULL, NULL))
        return FALSE;
    if (contents && (total = strstr (contents, "MemTotal:")) && *total)
    {
        const gchar* value = katze_skip_whitespace (total + 9);
        gdouble mem_total = g_ascii_strtoll (value, NULL, 0);
        return mem_total / 1024.0 < 352 + 1;
    }
#endif
    return FALSE;
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
                                     PROP_TOOLBAR_STYLE,
                                     g_param_spec_enum (
                                     "toolbar-style",
                                     _("Toolbar Style:"),
                                     _("The style of the toolbar"),
                                     MIDORI_TYPE_TOOLBAR_STYLE,
                                     MIDORI_TOOLBAR_DEFAULT,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_LOAD_ON_STARTUP,
                                     g_param_spec_enum (
                                     "load-on-startup",
                                     _("When Midori starts:"),
                                     "What to do when Midori starts",
                                     MIDORI_TYPE_STARTUP,
                                     MIDORI_STARTUP_LAST_OPEN_PAGES,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_PREFERRED_ENCODING,
                                     g_param_spec_enum (
                                     "preferred-encoding",
                                     "Preferred Encoding",
                                     "The preferred character encoding",
                                     MIDORI_TYPE_PREFERRED_ENCODING,
                                     MIDORI_ENCODING_WESTERN,
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
                                     "Where to open new pages",
                                     MIDORI_TYPE_NEW_PAGE,
                                     MIDORI_NEW_PAGE_TAB,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_PLUGINS,
                                     g_param_spec_boolean (
                                     "enable-plugins",
                                     "Enable Netscape plugins",
                                     "Enable embedded Netscape plugin objects",
                                     TRUE,
                                     flags));

    #if WEBKIT_CHECK_VERSION (1, 1, 18)
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_PAGE_CACHE,
                                     g_param_spec_boolean ("enable-page-cache",
                                                           "Enable page cache",
                                                           "Whether the page cache should be used",
        !midori_web_settings_low_memory_profile (),
                                                           flags));
    #endif
    if (g_object_class_find_property (gobject_class, "enable-fullscreen"))
    g_object_class_install_property (gobject_class,
                                     PROP_ENABLE_FULLSCREEN,
                                     g_param_spec_boolean (
                                     "enable-fullscreen",
                                     "Enable Fullscreen",
                                     "Allow experimental fullscreen API",
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
                                     "Proxy server",
                                     "The type of proxy server to use",
                                     MIDORI_TYPE_PROXY,
                                     MIDORI_PROXY_AUTOMATIC,
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
                                     "Identification string",
                                     "The application identification string",
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
                                     "Preferred languages",
                                     "A comma separated list of languages",
                                     NULL,
                                     flags));

    /**
     * MidoriWebSettings:site-data-rules:
     *
     * Rules for accepting, denying and preserving cookies and other data.
     * See midori_web_settings_get_site_data_policy() for details.
     *
     * Since: 0.4.4
     */
    g_object_class_install_property (gobject_class,
                                     PROP_SITE_DATA_RULES,
                                     g_param_spec_string (
                                     "site-data-rules",
        "Rules for accepting, denying and preserving cookies and other data",
        "Cookies, HTML5 databases, local storage and application cache blocking",
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
    else if (!strcmp (encoding, "GB18030"))
        web_settings->preferred_encoding = MIDORI_ENCODING_CHINESE_SIMPLIFIED;
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
    web_settings->user_stylesheet_uri = web_settings->user_stylesheet_uri_cached = NULL;
    web_settings->user_stylesheets = NULL;

    #if WEBKIT_CHECK_VERSION (1, 2, 6) && !WEBKIT_CHECK_VERSION (1, 2, 8)
    /* Shadows are very slow with WebKitGTK+ 1.2.7 */
    midori_web_settings_add_style (web_settings, "box-shadow-workaround",
        "* { -webkit-box-shadow: none !important; }");
    #endif

    #if defined (_WIN32) && WEBKIT_CHECK_VERSION (1, 7, 1) && !GTK_CHECK_VERSION (3, 0, 0)
    /* Try to work-around black borders on native widgets and GTK+2 on Win32 */
    midori_web_settings_add_style (web_settings, "black-widgets-workaround",
    "input[type='checkbox'] { -webkit-appearance: checkbox !important }"
    " input[type='radio'] { -webkit-appearance: radio !important }"
    " * { -webkit-appearance: none !important }");
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

    katze_assign (web_settings->http_accept_language, NULL);
    katze_assign (web_settings->accept, NULL);
    katze_assign (web_settings->ident_string, NULL);
    katze_assign (web_settings->user_stylesheet_uri, NULL);
    katze_assign (web_settings->user_stylesheet_uri_cached, NULL);
    if (web_settings->user_stylesheets != NULL)
        g_hash_table_destroy (web_settings->user_stylesheets);

    G_OBJECT_CLASS (midori_web_settings_parent_class)->finalize (object);
}

/**
 * midori_web_settings_has_plugin_support:
 *
 * Determines if Netscape plugins are supported.
 *
 * Returns: %TRUE if Netscape plugins can be used
 *
 * Since: 0.4.4
 **/
gboolean
midori_web_settings_has_plugin_support (void)
{
    #if !WEBKIT_CHECK_VERSION (1, 8, 2) && defined G_OS_WIN32
    return FALSE;
    #else
    return !midori_debug ("unarmed")  && g_strcmp0 (g_getenv ("MOZ_PLUGIN_PATH"), "/");
    #endif
}

/**
 * midori_web_settings_get_site_data_policy:
 *
 * Tests if @uri may store site data.
 *
 * Returns: a #MidoriSiteDataPolicy
 *
 * Since: 0.4.4
 **/
MidoriSiteDataPolicy
midori_web_settings_get_site_data_policy (MidoriWebSettings* settings,
                                          const gchar*       uri)
{
    MidoriSiteDataPolicy policy = MIDORI_SITE_DATA_UNDETERMINED;
    gchar* hostname;
    const gchar* match;

    g_return_val_if_fail (MIDORI_IS_WEB_SETTINGS (settings), policy);

    if (!(settings->site_data_rules && *settings->site_data_rules))
        return policy;

    /*
     * Values prefixed with "-" are always blocked
     * Values prefixed with "+" are always accepted
     * Values prefixed with "!" are not cleared in Clear Private Data
     * FIXME: "*" is a wildcard
     * FIXME: indicate type of storage the rule applies to
     * FIXME: support matching of the whole URI
     **/
    hostname = midori_uri_parse_hostname (uri, NULL);
    match = strstr (settings->site_data_rules, hostname ? hostname : uri);
    if (match != NULL && match != settings->site_data_rules)
    {
        const gchar* prefix = match - 1;
        if (*prefix == '-')
            policy = MIDORI_SITE_DATA_BLOCK;
        else if (*prefix == '+')
            policy = MIDORI_SITE_DATA_ACCEPT;
        else if (*prefix == '!')
            policy = MIDORI_SITE_DATA_PRESERVE;
        else
            g_warning ("%s: Matched with no prefix '%s'", G_STRFUNC, match);
    }
    g_free (hostname);
    return policy;
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
    case MIDORI_IDENT_CHROME:
        return g_strdup_printf ("Mozilla/5.0 (%s %s) AppleWebKit/%d.%d "
            "(KHTML, like Gecko) Chrome/18.0.1025.133 Safari/%d.%d %s",
            platform, os, webcore_major, webcore_minor, webcore_major, webcore_minor, appname);
    case MIDORI_IDENT_SAFARI:
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

/* Provide a new way for SoupSession to assume an 'Accept-Language'
   string automatically from the return value of g_get_language_names(),
   properly formatted according to RFC2616.
   Copyright (C) 2009 Mario Sanchez Prada <msanchez@igalia.com>
   Copyright (C) 2009 Dan Winship <danw@gnome.org>
   Mostly copied from libSoup 2.29, coding style adjusted */

/* Converts a language in POSIX format and to be RFC2616 compliant    */
/* Based on code from epiphany-webkit (ephy_langs_append_languages()) */

static gchar *
sokoke_posix_lang_to_rfc2616 (const gchar *language)
{
    if (!strchr (language, '.') && !strchr (language, '@') && language[0] != 'C')
        /* change to lowercase and '_' to '-' */
        return g_strdelimit (g_ascii_strdown (language, -1), "_", '-');

    return NULL;
}

/* Adds a quality value to a string (any value between 0 and 1). */
static gchar *
sokoke_add_quality_value (const gchar *str,
                          float        qvalue)
{
    if ((qvalue >= 0.0) && (qvalue <= 1.0))
    {
        int qv_int = (qvalue * 1000 + 0.5);
        return g_strdup_printf ("%s;q=%d.%d",
                                str, (int) (qv_int / 1000), qv_int % 1000);
    }

    return g_strdup (str);
}

/* Returns a RFC2616 compliant languages list from system locales */
static gchar *
sokoke_accept_languages (const gchar* const * lang_names)
{
    GArray *langs_garray = NULL;
    char *cur_lang = NULL;
    char *prev_lang = NULL;
    char **langs_array;
    char *langs_str;
    float delta;
    int i, n_lang_names;

    /* Calculate delta for setting the quality values */
    n_lang_names = g_strv_length ((gchar **)lang_names);
    delta = 0.999 / (n_lang_names - 1);

    /* Build the array of languages */
    langs_garray = g_array_new (TRUE, FALSE, sizeof (char*));
    for (i = 0; lang_names[i] != NULL; i++)
    {
        cur_lang = sokoke_posix_lang_to_rfc2616 (lang_names[i]);

        /* Apart from getting a valid RFC2616 compliant
           language, also get rid of extra variants */
        if (cur_lang && (!prev_lang ||
           (!strcmp (prev_lang, cur_lang) || !strstr (prev_lang, cur_lang))))
        {

            gchar *qv_lang = NULL;

            /* Save reference for further comparison */
            prev_lang = cur_lang;

            /* Add the quality value and append it */
            qv_lang = sokoke_add_quality_value (cur_lang, 1 - i * delta);
            g_array_append_val (langs_garray, qv_lang);
        }
    }

    /* Fallback: add "en" if list is empty */
    if (langs_garray->len == 0)
    {
        gchar* fallback = g_strdup ("en");
        g_array_append_val (langs_garray, fallback);
    }

    langs_array = (char **) g_array_free (langs_garray, FALSE);
    langs_str = g_strjoinv (", ", langs_array);

    return langs_str;
}


static void
midori_web_settings_update_accept_language (MidoriWebSettings* settings)
{
    gchar* languages = settings->http_accept_language;
    /* Empty, use the system locales */
    if (!(languages && *languages))
        katze_assign (settings->accept, sokoke_accept_languages (g_get_language_names ()));
    /* No =, no ., looks like a list of language names */
    else if (!(strchr (languages, '=') && strchr (languages, '.')))
    {
        gchar ** lang_names = g_strsplit_set (languages, ",; ", -1);
        katze_assign (settings->accept, sokoke_accept_languages ((const gchar* const *)lang_names));
        g_strfreev (lang_names);
    }
    /* Presumably a well formatted list including priorities */
    else
        katze_assign (settings->accept, g_strdup (languages));
}

const gchar*
midori_web_settings_get_accept_language (MidoriWebSettings* settings)
{
    if (!settings->accept)
        midori_web_settings_update_accept_language (settings);
    return settings->accept;
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
    case PROP_TOOLBAR_STYLE:
        web_settings->toolbar_style = g_value_get_enum (value);
        break;

    case PROP_LOAD_ON_STARTUP:
        web_settings->load_on_startup = g_value_get_enum (value);
        break;
    case PROP_PREFERRED_ENCODING:
        web_settings->preferred_encoding = g_value_get_enum (value);
        switch (web_settings->preferred_encoding)
        {
        case MIDORI_ENCODING_CHINESE:
            g_object_set (object, "default-encoding", "BIG5", NULL);
            break;
        case MIDORI_ENCODING_CHINESE_SIMPLIFIED:
            g_object_set (object, "default-encoding", "GB18030", NULL);
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

    case PROP_OPEN_NEW_PAGES_IN:
        web_settings->open_new_pages_in = g_value_get_enum (value);
        break;

    case PROP_ENABLE_PLUGINS:
        g_object_set (web_settings,
            "WebKitWebSettings::enable-plugins", g_value_get_boolean (value),
        #if WEBKIT_CHECK_VERSION (1, 1, 22)
            "enable-java-applet", g_value_get_boolean (value),
        #endif
            NULL);
        break;
    #if WEBKIT_CHECK_VERSION (1, 1, 18)
    case PROP_ENABLE_PAGE_CACHE:
        g_object_set (web_settings, "WebKitWebSettings::enable-page-cache",
                      g_value_get_boolean (value), NULL);
        break;
    #endif

    case PROP_PROXY_TYPE:
        web_settings->proxy_type = g_value_get_enum (value);
    break;
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
        midori_web_settings_update_accept_language (web_settings);
        break;
    case PROP_SITE_DATA_RULES:
        katze_assign (web_settings->site_data_rules, g_value_dup_string (value));
        break;
    #if !WEBKIT_CHECK_VERSION (1, 3, 13)
    case PROP_ENABLE_DNS_PREFETCHING:
        web_settings->enable_dns_prefetching = g_value_get_boolean (value);
        break;
    #endif
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
    case PROP_ENABLE_FULLSCREEN:
        g_object_set (web_settings, "WebKitWebSettings::enable-fullscreen",
                      g_value_get_boolean (value), NULL);
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
    case PROP_TOOLBAR_STYLE:
        g_value_set_enum (value, web_settings->toolbar_style);
        break;

    case PROP_LOAD_ON_STARTUP:
        g_value_set_enum (value, web_settings->load_on_startup);
        break;
    case PROP_PREFERRED_ENCODING:
        g_value_set_enum (value, web_settings->preferred_encoding);
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
            gchar* filename = midori_paths_get_data_filename (theme_file, FALSE);
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

    case PROP_ENABLE_PLUGINS:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-plugins"));
        break;
    #if WEBKIT_CHECK_VERSION (1, 1, 18)
    case PROP_ENABLE_PAGE_CACHE:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
                             "WebKitWebSettings::enable-page-cache"));
        break;
    #endif

    case PROP_PROXY_TYPE:
        g_value_set_enum (value, web_settings->proxy_type);
        break;
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
    case PROP_SITE_DATA_RULES:
        g_value_set_string (value, web_settings->site_data_rules);
        break;
    #if !WEBKIT_CHECK_VERSION (1, 3, 13)
    case PROP_ENABLE_DNS_PREFETCHING:
        g_value_set_boolean (value, web_settings->enable_dns_prefetching);
        break;
    #endif
    case PROP_ENFORCE_FONT_FAMILY:
        g_value_set_boolean (value, web_settings->enforce_font_family);
        break;
    case PROP_ENABLE_FULLSCREEN:
        g_value_set_boolean (value, katze_object_get_boolean (web_settings,
            "WebKitWebSettings::enable-fullscreen"));
        break;
    case PROP_USER_STYLESHEET_URI:
        g_value_take_string (value, katze_object_get_string (web_settings,
            "WebKitWebSettings::user-stylesheet-uri"));
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
