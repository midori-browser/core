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
#include "katze/katze.h"
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

gchar*
sokoke_prepare_command                  (const gchar*    command,
                                         gboolean        quote_command,
                                         const gchar*    argument,
                                         gboolean        quote_argument);

gboolean
sokoke_spawn_program                    (const gchar* command,
                                         gboolean        quote_command,
                                         const gchar*    argument,
                                         gboolean        quote_argument,
                                         gboolean        sync);

void
sokoke_spawn_gdb                        (const gchar*    gdb,
                                         gboolean        sync);

void
sokoke_spawn_app                        (const gchar*    uri,
                                         gboolean        inherit_config);

gboolean
sokoke_external_uri                     (const gchar*    uri);

gchar*
sokoke_magic_uri                        (const gchar*    uri,
                                         gboolean        allow_search,
                                         gboolean        allow_realtive);

void
sokoke_widget_set_visible               (GtkWidget*      widget,
                                         gboolean        visible);

GtkWidget*
sokoke_xfce_header_new                  (const gchar*    icon,
                                         const gchar*    title);

gboolean
sokoke_key_file_save_to_file            (GKeyFile*       key_file,
                                         const gchar*    filename,
                                         GError**        error);

void
sokoke_widget_get_text_size             (GtkWidget*      widget,
                                         const gchar*    text,
                                         gint*           width,
                                         gint*           height);

gint64
sokoke_time_t_to_julian                 (const time_t*   timestamp);

gchar*
sokoke_replace_variables                (const gchar* template,
                                         const gchar* variable_first, ...);

gboolean
sokoke_window_activate_key              (GtkWindow*      window,
                                         GdkEventKey*    event);
guint
sokoke_gtk_action_count_modifiers       (GtkAction* action);

gboolean
sokoke_prefetch_uri                     (MidoriWebSettings*  settings,
                                         const char*         uri,
                                         GCallback           callback,
                                         gpointer            user_data);

gboolean
sokoke_resolve_hostname                 (const gchar*        hostname);

void
sokoke_widget_copy_clipboard (GtkWidget*          widget,
                              const gchar*        text,
                              GtkClipboardGetFunc get_cb,
                              gpointer            owner);

GtkWidget*
sokoke_search_entry_new               (const gchar*        placeholder_text);

#ifdef G_OS_WIN32
gchar*
sokoke_get_win32_desktop_lnk_path_for_filename (gchar* filename);

void
sokoke_create_win32_desktop_lnk (gchar* prefix, gchar* filename, gchar* uri);
#endif

#endif /* !__SOKOKE_H__ */
