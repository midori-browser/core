/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_WEB_SETTINGS_H__
#define __MIDORI_WEB_SETTINGS_H__

#include <katze/katze.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_WEB_SETTINGS \
    (midori_web_settings_get_type ())
#define MIDORI_WEB_SETTINGS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_WEB_SETTINGS, MidoriWebSettings))
#define MIDORI_WEB_SETTINGS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_WEB_SETTINGS, MidoriWebSettingsClass))
#define MIDORI_IS_WEB_SETTINGS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_WEB_SETTINGS))
#define MIDORI_IS_WEB_SETTINGS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_WEB_SETTINGS))
#define MIDORI_WEB_SETTINGS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_WEB_SETTINGS, MidoriWebSettingsClass))

typedef struct _MidoriWebSettings                MidoriWebSettings;
typedef struct _MidoriWebSettingsClass           MidoriWebSettingsClass;

enum
{
    MIDORI_CLEAR_NONE = 0,
    MIDORI_CLEAR_HISTORY = 1,
    MIDORI_CLEAR_COOKIES = 2, /* deprecated */
    MIDORI_CLEAR_FLASH_COOKIES = 4, /* deprecated */
    MIDORI_CLEAR_WEBSITE_ICONS = 8, /* deprecated */
    MIDORI_CLEAR_TRASH = 16, /* deprecated */
    MIDORI_CLEAR_ON_QUIT = 32,
    MIDORI_CLEAR_WEB_CACHE = 64, /* deprecated */
    MIDORI_CLEAR_SESSION = 128,
};

/* values >= MIDORI_STARTUP_LAST_OPEN_PAGES mean session is saved */
typedef enum
{
    MIDORI_STARTUP_BLANK_PAGE, /* One blank tab or speed dial is opened */
    MIDORI_STARTUP_HOMEPAGE, /* One homepage tab is opened */
    MIDORI_STARTUP_LAST_OPEN_PAGES, /* The session is loaded and saved */
    MIDORI_STARTUP_DELAYED_PAGES /* The session is saved, loading pages is delayed */
} MidoriStartup;

GType
midori_startup_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_STARTUP \
    (midori_startup_get_type ())

typedef enum
{
    MIDORI_NEWTAB_BLANK_PAGE,
    MIDORI_NEWTAB_HOMEPAGE,
    MIDORI_NEWTAB_SEARCH,
    MIDORI_NEWTAB_SPEED_DIAL,
    MIDORI_NEWTAB_CUSTOM,
} MidoriNewTabType;

GType
midori_newtab_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_NEWTAB \
    (midori_newtab_get_type ())

typedef enum
{
    MIDORI_ENCODING_CHINESE /* Traditional */,
    MIDORI_ENCODING_CHINESE_SIMPLIFIED,
    MIDORI_ENCODING_JAPANESE,
    MIDORI_ENCODING_KOREAN,
    MIDORI_ENCODING_RUSSIAN,
    MIDORI_ENCODING_UNICODE,
    MIDORI_ENCODING_WESTERN,
    MIDORI_ENCODING_CUSTOM
} MidoriPreferredEncoding;

GType
midori_preferred_encoding_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_PREFERRED_ENCODING \
    (midori_preferred_encoding_get_type ())

typedef enum
{
    MIDORI_NEW_PAGE_TAB,
    MIDORI_NEW_PAGE_WINDOW,
    MIDORI_NEW_PAGE_CURRENT
} MidoriNewPage;

GType
midori_new_page_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_NEW_PAGE \
    (midori_new_page_get_type ())

typedef enum
{
    MIDORI_TOOLBAR_DEFAULT,
    MIDORI_TOOLBAR_ICONS,
    MIDORI_TOOLBAR_SMALL_ICONS,
    MIDORI_TOOLBAR_TEXT,
    MIDORI_TOOLBAR_BOTH,
    MIDORI_TOOLBAR_BOTH_HORIZ
} MidoriToolbarStyle;

GType
midori_toolbar_style_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_TOOLBAR_STYLE \
    (midori_toolbar_style_get_type ())

typedef enum
{
    MIDORI_PROXY_AUTOMATIC,
    MIDORI_PROXY_HTTP,
    MIDORI_PROXY_NONE
} MidoriProxy;

GType
midori_proxy_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_PROXY \
    (midori_proxy_get_type ())

typedef enum
{
    MIDORI_IDENT_MIDORI /* Automatic */,
    MIDORI_IDENT_GENUINE /* Midori */,
    MIDORI_IDENT_CHROME,
    MIDORI_IDENT_SAFARI,
    MIDORI_IDENT_IPHONE,
    MIDORI_IDENT_FIREFOX,
    MIDORI_IDENT_EXPLORER,
    MIDORI_IDENT_CUSTOM,
} MidoriIdentity;

GType
midori_identity_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_IDENTITY \
    (midori_identity_get_type ())

GType
midori_web_settings_get_type               (void) G_GNUC_CONST;

MidoriWebSettings*
midori_web_settings_new                    (void);

void
midori_web_settings_add_style              (MidoriWebSettings* settings,
                                            const gchar*       rule_id,
                                            const gchar*       style);

void
midori_web_settings_remove_style           (MidoriWebSettings* settings,
                                            const gchar*       rule_id);

const gchar*
midori_web_settings_get_system_name        (gchar**            architecture,
                                            gchar**            platform);

gboolean
midori_web_settings_has_plugin_support     (void);

gboolean
midori_web_settings_skip_plugin            (const gchar* path);

typedef enum
{
    MIDORI_SITE_DATA_UNDETERMINED,
    MIDORI_SITE_DATA_BLOCK,
    MIDORI_SITE_DATA_ACCEPT,
    MIDORI_SITE_DATA_PRESERVE,
} MidoriSiteDataPolicy;

MidoriSiteDataPolicy
midori_web_settings_get_site_data_policy   (MidoriWebSettings* settings,
                                            const gchar*       uri);

MidoriWebSettings*
midori_settings_new_full                   (gchar***           extensions);

gboolean
midori_settings_save_to_file               (MidoriWebSettings* settings,
                                            GObject*           app,
                                            const gchar*       filename,
                                            GError**           error);

G_END_DECLS

#endif /* __MIDORI_WEB_SETTINGS_H__ */
