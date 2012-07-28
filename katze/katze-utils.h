/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_UTILS_H__
#define __KATZE_UTILS_H__

#include <gtk/gtk.h>
#include "katze-array.h"
#include "gtk3-compat.h"

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
#define katze_assign(lvalue, rvalue) lvalue = (g_free (lvalue), rvalue)

/**
 * katze_object_assign:
 * @lvalue: a gobject
 * @rvalue: the new value
 *
 * Unrefs @lvalue if needed and assigns it the value of @rvalue.
 **/
#define katze_object_assign(lvalue, rvalue) \
    lvalue = ((lvalue ? g_object_unref (lvalue) : lvalue), rvalue)

/**
 * katze_strv_assign:
 * @lvalue: a string list
 * @rvalue: the new value
 *
 * Frees @lvalue if needed and assigns it the value of @rvalue.
 *
 * Since: 0.1.7
 **/
#define katze_strv_assign(lvalue, rvalue) lvalue = (g_strfreev (lvalue), rvalue)

/**
 * katze_str_non_null:
 * @str: a string, or %NULL
 *
 * Returns "" if @str is %NULL.
 *
 * Since: 0.4.4
 **/
static inline const gchar*
katze_str_non_null (const gchar* str)
{
    return str ? str : "";
}

GtkWidget*
katze_property_proxy                (gpointer     object,
                                     const gchar* property,
                                     const gchar* hint);

GtkWidget*
katze_property_label                (gpointer     object,
                                     const gchar* property);

typedef enum {
    KATZE_MENU_POSITION_CURSOR = 0,
    KATZE_MENU_POSITION_LEFT,
    KATZE_MENU_POSITION_RIGHT
} KatzeMenuPos;

void
katze_widget_popup                   (GtkWidget*      widget,
                                      GtkMenu*        menu,
                                      GdkEventButton* event,
                                      KatzeMenuPos    pos);

GtkWidget*
katze_image_menu_item_new_ellipsized (const gchar*   label);

GdkPixbuf*
katze_pixbuf_new_from_buffer         (const guchar* buffer,
                                      gsize         length,
                                      const gchar*  mime_type,
                                      GError**      error);

gboolean
katze_tree_view_get_selected_iter    (GtkTreeView*   treeview,
                                      GtkTreeModel** model,
                                      GtkTreeIter*   iter);

void
katze_bookmark_populate_tree_view    (KatzeArray* array,
                                      GtkTreeStore* model,
                                      GtkTreeIter*  parent);

gchar*
katze_strip_mnemonics                (const gchar*    original);

const gchar*
katze_skip_whitespace                (const gchar*    str);

gboolean
katze_object_has_property            (gpointer     object,
                                      const gchar* property);

gint
katze_object_get_boolean             (gpointer     object,
                                      const gchar* property);

gint
katze_object_get_int                 (gpointer     object,
                                      const gchar* property);

gfloat
katze_object_get_float               (gpointer     object,
                                      const gchar* property);

gint
katze_object_get_enum                (gpointer     object,
                                      const gchar* property);

gchar*
katze_object_get_string              (gpointer     object,
                                      const gchar* property);

gpointer
katze_object_get_object              (gpointer     object,
                                      const gchar* property);

int
katze_mkdir_with_parents             (const gchar* pathname,
                                      int          mode);

gboolean
katze_widget_has_touchscreen_mode    (GtkWidget*      widget);

GdkPixbuf*
katze_load_cached_icon               (const gchar*    uri,
                                      GtkWidget*      widget);

GtkWidget*
katze_uri_entry_new                  (GtkWidget*      other_widget);

void
katze_widget_add_class               (GtkWidget*      widget,
                                      const gchar*    class_name);

void
katze_assert_str_equal               (const gchar*    input,
                                      const gchar*    result,
                                      const gchar*    expected);

G_END_DECLS

#endif /* __KATZE_UTILS_H__ */
