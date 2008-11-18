/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __TAB_PANEL_EXTENSION_H__
#define __TAB_PANEL_EXTENSION_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TAB_PANEL_TYPE_EXTENSION \
    (midori_extension_get_type ())
#define TAB_PANEL_EXTENSION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), TAB_PANEL_TYPE_EXTENSION, TabPanelExtension))
#define TAB_PANEL_EXTENSION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), TAB_PANEL_TYPE_EXTENSION, TabPanelExtensionClass))
#define TAB_PANEL_IS_EXTENSION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TAB_PANEL_TYPE_EXTENSION))
#define TAB_PANEL_IS_EXTENSION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), TAB_PANEL_TYPE_EXTENSION))
#define TAB_PANEL_EXTENSION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), TAB_PANEL_TYPE_EXTENSION, TabPanelExtensionClass))

typedef struct _TabPanelExtension                TabPanelExtension;
typedef struct _TabPanelExtensionClass           TabPanelExtensionClass;

GType
tab_panel_extension_get_type            (void);

/* There is no API for TabPanelExtension. Please use the
   available properties and signals. */

G_END_DECLS

#endif /* __TAB_PANEL_EXTENSION_H__ */
