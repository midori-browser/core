/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_WEB_SETTINGS_H__
#define __MIDORI_WEB_SETTINGS_H__

#include <webkit/webkit.h>

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
typedef struct _MidoriWebSettingsPrivate         MidoriWebSettingsPrivate;
typedef struct _MidoriWebSettingsClass           MidoriWebSettingsClass;

struct _MidoriWebSettings
{
    WebKitWebSettings parent_instance;

    MidoriWebSettingsPrivate* priv;
};

struct _MidoriWebSettingsClass
{
    WebKitWebSettingsClass parent_class;

    /* Signals */
    void
    (*progress_started)       (MidoriWebSettings*    web_settings,
                               guint                 progress);
    void
    (*progress_changed)       (MidoriWebSettings*    web_settings,
                               guint                 progress);
    void
    (*progress_done)          (MidoriWebSettings*    web_settings,
                               guint                 progress);
    void
    (*load_done)              (MidoriWebSettings*    web_settings,
                               WebKitWebFrame*       frame);
    void
    (*statusbar_text_changed) (MidoriWebSettings*    web_settings,
                               const gchar*          text);
    void
    (*element_motion)         (MidoriWebSettings*    web_settings,
                               const gchar*          link_uri);
    void
    (*close)                  (MidoriWebSettings*    web_settings);
    void
    (*new_tab)                (MidoriWebSettings*    web_settings,
                               const gchar*          uri);
    void
    (*new_window)             (MidoriWebSettings*    web_settings,
                               const gchar*          uri);
};

GType
midori_web_settings_get_type               (void);

MidoriWebSettings*
midori_web_settings_new                    (void);

MidoriWebSettings*
midori_web_settings_copy                   (MidoriWebSettings* web_settings);

G_END_DECLS

#endif /* __MIDORI_WEB_SETTINGS_H__ */
