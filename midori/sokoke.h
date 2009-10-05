/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009 Dale Whittaker <dayul@users.sf.net>

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
#include <libsoup/soup.h>
#include <JavaScriptCore/JavaScript.h>

gchar*
sokoke_js_script_eval                   (JSContextRef    js_context,
                                         const gchar*    script,
                                         gchar**         exception);

/* Many themes need this hack for small toolbars to work */
#define GTK_ICON_SIZE_SMALL_TOOLBAR GTK_ICON_SIZE_BUTTON

gboolean
sokoke_show_uri                         (GdkScreen*      screen,
                                         const gchar*    uri,
                                         guint32         timestamp,
                                         GError**        error);

gboolean
sokoke_spawn_program                    (const gchar*    command,
                                         const gchar*    argument,
                                         gboolean        quote);

gchar* sokoke_search_uri                (const gchar*    uri,
                                         const gchar*    keywords);

gchar*
sokoke_idn_to_punycode                  (gchar*          uri);

gchar*
sokoke_magic_uri                        (const gchar*    uri,
                                         KatzeArray*     search_engines);

gchar*
sokoke_format_uri_for_display           (const gchar*    uri);

typedef enum {
    SOKOKE_MENU_POSITION_CURSOR = 0,
    SOKOKE_MENU_POSITION_LEFT,
    SOKOKE_MENU_POSITION_RIGHT
} SokokeMenuPos;

void
sokoke_combo_box_add_strings            (GtkComboBox*    combobox,
                                         const gchar*    label_first,
                                         ...);

void
sokoke_widget_set_visible               (GtkWidget*      widget,
                                         gboolean        visible);

void
sokoke_container_show_children          (GtkContainer*   container);

void
sokoke_widget_popup                     (GtkWidget*      widget,
                                         GtkMenu*        menu,
                                         GdkEventButton* event,
                                         SokokeMenuPos   pos);

GtkWidget*
sokoke_xfce_header_new                  (const gchar*    icon,
                                         const gchar*    title);

GtkWidget*
sokoke_hig_frame_new                    (const gchar*    title);

void
sokoke_widget_set_pango_font_style      (GtkWidget*      widget,
                                         PangoStyle      style);

void
sokoke_entry_set_default_text           (GtkEntry*       entry,
                                         const gchar*    default_text);

gchar*
sokoke_key_file_get_string_default      (GKeyFile*       key_file,
                                         const gchar*    group,
                                         const gchar*    key,
                                         const gchar*    default_value,
                                         GError**        error);

gint
sokoke_key_file_get_integer_default     (GKeyFile*       key_file,
                                         const gchar*    group,
                                         const gchar*    key,
                                         const gint      default_value,
                                         GError**        error);

gdouble
sokoke_key_file_get_double_default      (GKeyFile*       key_file,
                                         const gchar*    group,
                                         const gchar*    key,
                                         gdouble         default_value,
                                         GError**        error);

gboolean
sokoke_key_file_get_boolean_default     (GKeyFile*       key_file,
                                         const gchar*    group,
                                         const gchar*    key,
                                         gboolean        default_value,
                                         GError**        error);

gchar**
sokoke_key_file_get_string_list_default (GKeyFile*       key_file,
                                         const gchar*    group,
                                         const gchar*    key,
                                         gsize*          length,
                                         gchar**         default_value,
                                         gsize*          default_length,
                                         GError*         error);

gboolean
sokoke_key_file_save_to_file            (GKeyFile*       key_file,
                                         const gchar*    filename,
                                         GError**        error);

void
sokoke_widget_get_text_size             (GtkWidget*      widget,
                                         const gchar*    text,
                                         gint*           width,
                                         gint*           height);

GtkWidget*
sokoke_action_create_popup_menu_item    (GtkAction*      action);

GtkWidget*
sokoke_image_menu_item_new_ellipsized   (const gchar*    label);

gint64
sokoke_time_t_to_julian                 (const time_t*   timestamp);

void
sokoke_register_stock_items             (void);

const gchar*
sokoke_set_config_dir                   (const gchar*    new_config_dir);

gboolean
sokoke_remove_path                      (const gchar*    path,
                                         gboolean        ignore_errors);

gchar*
sokoke_find_config_filename             (const gchar*    folder,
                                         const gchar*    filename);

gchar*
sokoke_find_data_filename               (const gchar*    filename);

SoupServer*
sokoke_get_res_server                   (void);

gchar*
sokoke_replace_variables                (const gchar* template,
                                         const gchar* variable_first, ...);

#endif /* !__SOKOKE_H__ */
