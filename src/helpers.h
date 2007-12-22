/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __HELPERS_H__
#define __HELPERS_H__ 1

#include "browser.h"

#include <gtk/gtk.h>

GtkIconTheme*
get_icon_theme(GtkWidget*);

GtkWidget*
menu_item_new(const gchar*, const gchar*, GCallback, gboolean, gpointer);

GtkToolItem*
tool_button_new(const gchar*, const gchar*
 , gboolean, gboolean, GCallback, const gchar*, gpointer);

GtkWidget*
check_menu_item_new(const gchar*, GCallback, gboolean, gboolean, CBrowser*);

GtkWidget*
radio_button_new(GtkRadioButton*, const gchar*);

void
show_error(const gchar*, const gchar*, CBrowser*);

gboolean
spawn_protocol_command(const gchar*, const gchar*);

GdkPixbuf*
load_web_icon(const gchar*, GtkIconSize, GtkWidget*);

void
entry_setup_completion(GtkEntry*);

void
entry_completion_append(GtkEntry*, const gchar*);

GtkWidget*
get_nth_webView(gint, CBrowser*);

gint
get_webView_index(GtkWidget*, CBrowser*);

CBrowser*
get_browser_from_webView(GtkWidget*);

void
update_favicon(CBrowser*);

void
update_security(CBrowser*);

void
update_visibility(CBrowser*, gboolean);

void
action_set_active(const gchar*, gboolean, CBrowser*);

void
action_set_sensitive(const gchar*, gboolean, CBrowser*);

void
action_set_visible(const gchar*, gboolean, CBrowser*);

void
update_statusbar(CBrowser*);

void
update_edit_items(CBrowser*);

void
update_gui_state(CBrowser*);

void
update_feeds(CBrowser*);

void
update_search_engines(CBrowser*);

void
update_browser_actions(CBrowser*);

gchar*
magic_uri(const gchar*, gboolean bSearch);

gchar*
get_default_font(void);

GtkToolbarStyle
config_to_toolbarstyle();

GtkToolbarStyle
config_to_toolbariconsize(gboolean);

#endif /* !__HELPERS_H__ */
