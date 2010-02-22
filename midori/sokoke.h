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

/* Common behavior modifiers */
#define MIDORI_MOD_NEW_WINDOW(state) (state & GDK_SHIFT_MASK)
#define MIDORI_MOD_NEW_TAB(state) (state & GDK_CONTROL_MASK)
#define MIDORI_MOD_BACKGROUND(state) (state & GDK_SHIFT_MASK)
#define MIDORI_MOD_SCROLL(state) (state & GDK_CONTROL_MASK)

#include <katze/katze.h>

#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>

#if !GLIB_CHECK_VERSION (2, 14, 0)
    #define G_PARAM_STATIC_STRINGS \
    (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)
    #define gtk_dialog_get_content_area(dlg) dlg->vbox
#endif

#if !GTK_CHECK_VERSION (2, 16, 0)
    #define GTK_ACTIVATABLE GTK_WIDGET
    #define gtk_activatable_get_related_action gtk_widget_get_action
#endif

#if !GTK_CHECK_VERSION (2, 18, 0)
    #define gtk_widget_is_toplevel(widget) GTK_WIDGET_TOPLEVEL (widget)
    #define gtk_widget_has_focus(widget) GTK_WIDGET_HAS_FOCUS (widget)
#endif

#if !GTK_CHECK_VERSION(2, 12, 0)

void
gtk_widget_set_has_tooltip             (GtkWidget*         widget,
                                        gboolean           has_tooltip);

void
gtk_widget_set_tooltip_text            (GtkWidget*         widget,
                                        const gchar*       text);

void
gtk_tool_item_set_tooltip_text         (GtkToolItem*       toolitem,
                                        const gchar*       text);

#endif

gchar*
sokoke_js_script_eval                   (JSContextRef    js_context,
                                         const gchar*    script,
                                         gchar**         exception);

void
sokoke_message_dialog                   (GtkMessageType  message_type,
                                         const gchar*    short_message,
                                         const gchar*    detailed_message);

gboolean
sokoke_show_uri_with_mime_type          (GdkScreen*      screen,
                                         const gchar*    uri,
                                         const gchar*    mime_type,
                                         guint32         timestamp,
                                         GError**        error);

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
sokoke_hostname_from_uri                (const gchar*    uri,
                                         gchar**         path);

gchar*
sokoke_uri_to_ascii                     (const gchar*    uri);

gchar*
sokoke_magic_uri                        (const gchar*    uri);

gchar*
sokoke_format_uri_for_display           (const gchar*    uri);

void
sokoke_combo_box_add_strings            (GtkComboBox*    combobox,
                                         const gchar*    label_first,
                                         ...);

void
sokoke_widget_set_visible               (GtkWidget*      widget,
                                         gboolean        visible);

void
sokoke_container_show_children          (GtkContainer*   container);

GtkWidget*
sokoke_xfce_header_new                  (const gchar*    icon,
                                         const gchar*    title);

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

gint64
sokoke_time_t_to_julian                 (const time_t*   timestamp);

gint
sokoke_days_between                     (const time_t*   day1,
                                         const time_t*   day2);

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

gchar**
sokoke_get_argv                         (gchar**         argument_vector);

#if !WEBKIT_CHECK_VERSION (1, 1, 14)
SoupServer*
sokoke_get_res_server                   (void);
#endif

gchar*
sokoke_replace_variables                (const gchar* template,
                                         const gchar* variable_first, ...);

gboolean
sokoke_window_activate_key              (GtkWindow*      window,
                                         GdkEventKey*    event);

GtkWidget*
sokoke_file_chooser_dialog_new          (const gchar*         title,
                                         GtkWindow*           window,
                                         GtkFileChooserAction action);

gboolean
sokoke_prefetch_uri                     (const char* uri);

gchar *
sokoke_accept_languages                 (const gchar* const * lang_names);

#endif /* !__SOKOKE_H__ */
