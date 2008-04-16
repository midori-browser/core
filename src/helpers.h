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

#include <gtk/gtk.h>

#include "midori-browser.h"

GtkWidget*
check_menu_item_new(const gchar*, GCallback, gboolean, gboolean, gpointer);

GtkWidget*
radio_button_new(GtkRadioButton*, const gchar*);

void
show_error(const gchar*, const gchar*, MidoriBrowser*);

GdkPixbuf*
load_web_icon(const gchar*, GtkIconSize, GtkWidget*);

void
entry_setup_completion(GtkEntry*);

void
entry_completion_append(GtkEntry*, const gchar*);

gchar*
magic_uri(const gchar*, gboolean bSearch);

gchar*
get_default_font(void);

#endif /* !__HELPERS_H__ */
