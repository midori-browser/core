/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_EXTENSIONS_H__
#define __MIDORI_EXTENSIONS_H__

#include <gtk/gtk.h>

#include <katze/katze.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_EXTENSIONS \
    (midori_extensions_get_type ())
#define MIDORI_EXTENSIONS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_EXTENSIONS, MidoriExtensions))
#define MIDORI_EXTENSIONS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_EXTENSIONS, MidoriExtensionsClass))
#define MIDORI_IS_EXTENSIONS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_EXTENSIONS))
#define MIDORI_IS_EXTENSIONS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_EXTENSIONS))
#define MIDORI_EXTENSIONS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_EXTENSIONS, MidoriExtensionsClass))

typedef struct _MidoriExtensions                MidoriExtensions;
typedef struct _MidoriExtensionsClass           MidoriExtensionsClass;

GType
midori_extensions_get_type               (void);

GtkWidget*
midori_extensions_new                    (void);

G_END_DECLS

#endif /* __MIDORI_EXTENSIONS_H__ */
