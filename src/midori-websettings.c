/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-webview.h"

#include "sokoke.h"

G_DEFINE_TYPE (MidoriWebSettings, midori_web_settings, WEBKIT_TYPE_WEB_SETTINGS)

struct _MidoriWebSettingsPrivate
{
    gint tab_label_size;
    gboolean close_button;
    gboolean middle_click_goto;
};

#define MIDORI_WEB_SETTINGS_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
     MIDORI_TYPE_WEB_SETTINGS, MidoriWebSettingsPrivate))

enum
{
    PROP_0,

    PROP_TAB_LABEL_SIZE,
    PROP_CLOSE_BUTTON,
    PROP_MIDDLE_CLICK_GOTO
};

static void
midori_web_settings_finalize (GObject* object);

static void
midori_web_settings_set_property (GObject* object,
                                  guint prop_id,
                                  const GValue* value,
                                  GParamSpec* pspec);

static void
midori_web_settings_get_property (GObject* object,
                                  guint prop_id,
                                  GValue* value,
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
                                     PROP_TAB_LABEL_SIZE,
                                     g_param_spec_int (
                                     "tab-label-size",
                                     "Tab Label Size",
                                     "The desired tab label size",
                                     0, G_MAXINT, 10,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_CLOSE_BUTTON,
                                     g_param_spec_boolean (
                                     "close-button",
                                     "Close Button",
                                     "Whether the associated tab has a close button",
                                     FALSE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_MIDDLE_CLICK_GOTO,
                                     g_param_spec_boolean (
                                     "middle-click-goto",
                                     "Middle Click Goto",
                                     "Load an uri from the selection via middle click",
                                     FALSE,
                                     flags));

    g_type_class_add_private (class, sizeof (MidoriWebSettingsPrivate));
}



static void
midori_web_settings_init (MidoriWebSettings* web_settings)
{
    web_settings->priv = MIDORI_WEB_SETTINGS_GET_PRIVATE (web_settings);
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
    case PROP_TAB_LABEL_SIZE:
        priv->tab_label_size = g_value_get_int (value);
        break;
    case PROP_CLOSE_BUTTON:
        priv->close_button = g_value_get_boolean (value);
        break;
    case PROP_MIDDLE_CLICK_GOTO:
        priv->middle_click_goto = g_value_get_boolean (value);
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
    case PROP_TAB_LABEL_SIZE:
        g_value_set_int (value, priv->tab_label_size);
        break;
    case PROP_CLOSE_BUTTON:
        g_value_set_boolean (value, priv->close_button);
        break;
    case PROP_MIDDLE_CLICK_GOTO:
        g_value_set_boolean (value, priv->middle_click_goto);
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
    copy = MIDORI_WEB_SETTINGS (webkit_web_settings_copy (WEBKIT_WEB_SETTINGS (web_settings)));
    g_object_set (copy,
                 "tab-label-size", priv->tab_label_size,
                 "close-button", priv->close_button,
                 "middle-click-goto", priv->middle_click_goto,
                 NULL);

    return copy;
}
