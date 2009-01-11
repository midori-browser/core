/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __SOKOKE_H__
#define __SOKOKE_H__ 1

#include <katze/katze.h>

#include <gtk/gtk.h>

#include <JavaScriptCore/JavaScript.h>

JSValueRef
sokoke_js_script_eval (JSContextRef js_context,
                       const gchar* script,
                       gchar**      exception);

/* Many themes need this hack for small toolbars to work */
#define GTK_ICON_SIZE_SMALL_TOOLBAR GTK_ICON_SIZE_BUTTON

gboolean
sokoke_spawn_program                (const gchar* command,
                                     const gchar* argument);

gchar*
sokoke_magic_uri                    (const gchar*    uri,
                                     KatzeArray*     search_engines);

typedef enum {
    SOKOKE_MENU_POSITION_CURSOR = 0,
    SOKOKE_MENU_POSITION_LEFT,
    SOKOKE_MENU_POSITION_RIGHT
} SokokeMenuPos;

void
sokoke_combo_box_add_strings        (GtkComboBox*    combobox,
                                     const gchar*    label_first,
                                     ...);

void
sokoke_widget_set_visible           (GtkWidget*      widget,
                                     gboolean        visible);

void
sokoke_container_show_children      (GtkContainer*   container);

void
sokoke_widget_popup                 (GtkWidget*      widget,
                                     GtkMenu*        menu,
                                     GdkEventButton* event,
                                     SokokeMenuPos   pos);

GtkWidget*
sokoke_xfce_header_new              (const gchar*    icon,
                                     const gchar*    title);

GtkWidget*
sokoke_superuser_warning_new        (void);

GtkWidget*
sokoke_hig_frame_new                (const gchar*    title);

void
sokoke_widget_set_pango_font_style  (GtkWidget*      widget,
                                     PangoStyle      style);

void
sokoke_entry_set_default_text       (GtkEntry*       entry,
                                     const gchar*    default_text);

gchar*
sokoke_key_file_get_string_default  (GKeyFile*       key_file,
                                     const gchar*    group,
                                     const gchar*    key,
                                     const gchar*    default_value,
                                     GError**        error);

gint
sokoke_key_file_get_integer_default  (GKeyFile*      key_file,
                                      const gchar*   group,
                                      const gchar*   key,
                                      const gint     default_value,
                                      GError**       error);

gdouble
sokoke_key_file_get_double_default   (GKeyFile*      key_file,
                                      const gchar*   group,
                                      const gchar*   key,
                                      gdouble        default_value,
                                      GError**       error);

gboolean
sokoke_key_file_get_boolean_default  (GKeyFile*      key_file,
                                      const gchar*   group,
                                      const gchar*   key,
                                      gboolean       default_value,
                                      GError**       error);

gboolean
sokoke_key_file_save_to_file         (GKeyFile*      key_file,
                                      const gchar*   filename,
                                      GError**       error);

void
sokoke_widget_get_text_size          (GtkWidget*     widget,
                                      const gchar*   text,
                                      gint*          width,
                                      gint*          height);

GtkWidget*
sokoke_action_create_popup_menu_item (GtkAction*     action);

GtkWidget*
sokoke_image_menu_item_new_ellipsized (const gchar*  label);

gboolean
sokoke_tree_view_get_selected_iter   (GtkTreeView*   tree_view,
                                      GtkTreeModel** model,
                                      GtkTreeIter*   iter);

gint
sokoke_days_between                  (const time_t*  day1,
                                      const time_t*  day2);

#endif /* !__SOKOKE_H__ */
