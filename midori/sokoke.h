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

#include <JavaScriptCore/JavaScript.h>
#include <midori/midori-websettings.h>
#include <katze/gtk3-compat.h>

gchar*
sokoke_js_script_eval                   (JSContextRef    js_context,
                                         const gchar*    script,
                                         gchar**         exception);

void
sokoke_message_dialog                   (GtkMessageType  message_type,
                                         const gchar*    short_message,
                                         const gchar*    detailed_message,
                                         gboolean        modal);

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
sokoke_spawn_program                    (const gchar* command,
                                         const gchar* argument);

void
sokoke_spawn_app                        (const gchar*    uri,
                                         gboolean        inherit_config);

gboolean
sokoke_external_uri                     (const gchar*    uri);

gchar*
sokoke_magic_uri                        (const gchar*    uri);

void
sokoke_widget_set_visible               (GtkWidget*      widget,
                                         gboolean        visible);

GtkWidget*
sokoke_xfce_header_new                  (const gchar*    icon,
                                         const gchar*    title);

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

const gchar*
sokoke_set_config_dir                   (const gchar*    new_config_dir);

gboolean
sokoke_is_app_or_private                (void);

gboolean
sokoke_remove_path                      (const gchar*    path,
                                         gboolean        ignore_errors);

gchar*
sokoke_find_config_filename             (const gchar*    folder,
                                         const gchar*    filename);

gchar*
sokoke_find_lib_path                    (const gchar*    folder);

gchar*
sokoke_find_data_filename               (const gchar*    filename,
                                         gboolean        res);

gchar**
sokoke_get_argv                         (gchar**         argument_vector);

gchar*
sokoke_replace_variables                (const gchar* template,
                                         const gchar* variable_first, ...);

gboolean
sokoke_window_activate_key              (GtkWindow*      window,
                                         GdkEventKey*    event);
guint
sokoke_gtk_action_count_modifiers       (GtkAction* action);

GtkWidget*
sokoke_file_chooser_dialog_new          (const gchar*         title,
                                         GtkWindow*           window,
                                         GtkFileChooserAction action);

gboolean
sokoke_prefetch_uri                     (MidoriWebSettings*  settings,
                                         const char*         uri,
                                         SoupAddressCallback callback,
                                         gpointer            user_data);

gboolean
sokoke_resolve_hostname                 (const gchar*        hostname);

gchar *
sokoke_accept_languages                 (const gchar* const * lang_names);

gboolean
sokoke_recursive_fork_protection        (const gchar*         uri,
                                         gboolean             set_uri);

typedef struct
{
    gchar* name;
    gchar* label;
    GCallback clear;
} SokokePrivacyItem;

GList*
sokoke_register_privacy_item (const gchar* name,
                              const gchar* label,
                              GCallback    clear);

void
sokoke_widget_copy_clipboard (GtkWidget*   widget,
                              const gchar* text);

gchar*
sokoke_build_thumbnail_path (const gchar* name);

gchar*
midori_download_prepare_tooltip_text (WebKitDownload* download);

#endif /* !__SOKOKE_H__ */
