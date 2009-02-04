/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_EXTENSION_H__
#define __MIDORI_EXTENSION_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_EXTENSION \
    (midori_extension_get_type ())
#define MIDORI_EXTENSION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_EXTENSION, MidoriExtension))
#define MIDORI_EXTENSION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_EXTENSION, MidoriExtensionClass))
#define MIDORI_IS_EXTENSION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_EXTENSION))
#define MIDORI_IS_EXTENSION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_EXTENSION))
#define MIDORI_EXTENSION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_EXTENSION, MidoriExtensionClass))

typedef struct _MidoriExtension                MidoriExtension;
typedef struct _MidoriExtensionClass           MidoriExtensionClass;
typedef struct _MidoriExtensionPrivate         MidoriExtensionPrivate;

struct _MidoriExtension
{
    GObject parent_instance;

    MidoriExtensionPrivate* priv;
};

struct _MidoriExtensionClass
{
    GObjectClass parent_class;
};

GType
midori_extension_get_type            (void);

gboolean
midori_extension_is_prepared         (MidoriExtension* extension);

gboolean
midori_extension_is_active           (MidoriExtension* extension);

void
midori_extension_deactivate          (MidoriExtension* extension);

const gchar*
midori_extension_get_config_dir      (MidoriExtension* extension);

void
midori_extension_install_string      (MidoriExtension* extension,
                                      const gchar*     name,
                                      const gchar*     default_value);

const gchar*
midori_extension_get_string          (MidoriExtension* extension,
                                      const gchar*     name);

void
midori_extension_set_string          (MidoriExtension* extension,
                                      const gchar*     name,
                                      const gchar*     value);

G_END_DECLS

#endif /* __MIDORI_EXTENSION_H__ */
