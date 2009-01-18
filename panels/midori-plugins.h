/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_PLUGINS_H__
#define __MIDORI_PLUGINS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_PLUGINS \
    (midori_plugins_get_type ())
#define MIDORI_PLUGINS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_PLUGINS, MidoriPlugins))
#define MIDORI_PLUGINS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_PLUGINS, MidoriPluginsClass))
#define MIDORI_IS_PLUGINS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_PLUGINS))
#define MIDORI_IS_PLUGINS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_PLUGINS))
#define MIDORI_PLUGINS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_PLUGINS, MidoriPluginsClass))

typedef struct _MidoriPlugins                MidoriPlugins;
typedef struct _MidoriPluginsClass           MidoriPluginsClass;

GType
midori_plugins_get_type                      (void);

GtkWidget*
midori_plugins_new                           (void);

G_END_DECLS

#endif /* __MIDORI_PLUGINS_H__ */
