/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_SOURCE_H__
#define __MIDORI_SOURCE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_SOURCE \
    (midori_source_get_type ())
#define MIDORI_SOURCE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_SOURCE, MidoriSource))
#define MIDORI_SOURCE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_SOURCE, MidoriSourceClass))
#define MIDORI_IS_SOURCE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_SOURCE))
#define MIDORI_IS_SOURCE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_SOURCE))
#define MIDORI_SOURCE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_SOURCE, MidoriSourceClass))

typedef struct _MidoriSource                MidoriSource;
typedef struct _MidoriSourceClass           MidoriSourceClass;

GType
midori_source_get_type            (void);

GtkWidget*
midori_source_new                 (const gchar*  uri);

void
midori_source_set_uri             (MidoriSource* source,
                                   const gchar*  uri);

G_END_DECLS

#endif /* __MIDORI_SOURCE_H__ */
