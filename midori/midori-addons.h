/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_ADDONS_H__
#define __MIDORI_ADDONS_H__

#include <gtk/gtk.h>

#include <katze/katze.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_ADDONS \
    (midori_addons_get_type ())
#define MIDORI_ADDONS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_ADDONS, MidoriAddons))
#define MIDORI_ADDONS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_ADDONS, MidoriAddonsClass))
#define MIDORI_IS_ADDONS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_ADDONS))
#define MIDORI_IS_ADDONS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_ADDONS))
#define MIDORI_ADDONS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_ADDONS, MidoriAddonsClass))

typedef struct _MidoriAddons                MidoriAddons;
typedef struct _MidoriAddonsPrivate         MidoriAddonsPrivate;
typedef struct _MidoriAddonsClass           MidoriAddonsClass;

struct _MidoriAddons
{
    GtkVBox parent_instance;

    MidoriAddonsPrivate* priv;
};

struct _MidoriAddonsClass
{
    GtkVBoxClass parent_class;
};

typedef enum
{
    MIDORI_ADDON_EXTENSIONS,
    MIDORI_ADDON_USER_SCRIPTS,
    MIDORI_ADDON_USER_STYLES
} MidoriAddonKind;

GType
midori_addon_kind_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_ADDON_KIND \
    (midori_addon_kind_get_type ())

GType
midori_addons_get_type               (void);

GtkWidget*
midori_addons_new                    (GtkWidget*      web_widget,
                                      MidoriAddonKind kind);

GtkWidget*
midori_addons_get_toolbar            (MidoriAddons*       console);

G_END_DECLS

#endif /* __MIDORI_ADDONS_H__ */
