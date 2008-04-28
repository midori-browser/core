/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_UTILS_H__
#define __KATZE_UTILS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * KATZE_OBJECT_NAME:
 * @object: an object
 *
 * Return the name of an object's class structure's type.
 **/
#define KATZE_OBJECT_NAME(object) \
    G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (object))

/**
 * katze_assign:
 * @lvalue: a pointer
 * @rvalue: the new value
 *
 * Frees @lvalue if needed and assigns it the value of @rvalue.
 **/
#define katze_assign(lvalue, rvalue) \
    if (1) \
    { \
        g_free (lvalue); \
        lvalue = rvalue; \
    }

/**
 * katze_object_assign:
 * @lvalue: a gobject
 * @rvalue: the new value
 *
 * Unrefs @lvalue if needed and assigns it the value of @rvalue.
 **/
#define katze_object_assign(lvalue, rvalue) \
    if (1) \
    { \
        if (lvalue) \
            g_object_unref (lvalue); \
        lvalue = rvalue; \
    }

GtkWidget*
katze_property_proxy                (gpointer     object,
                                     const gchar* property,
                                     const gchar* hint);

GtkWidget*
katze_property_label                (gpointer     object,
                                     const gchar* property);

G_END_DECLS

#endif /* __KATZE_UTILS_H__ */
