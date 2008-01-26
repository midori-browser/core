/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __webView_H__
#define __webView_H__ 1

#include <gtk/gtk.h>
#include "browser.h"
#include "debug.h"

#include <webkit.h>

WebKitNavigationResponse
on_webView_navigation_requested(GtkWidget* webView, WebKitWebFrame* frame
 , WebKitNetworkRequest* networkRequest);

void
on_webView_title_changed(GtkWidget*, WebKitWebFrame*, const gchar*, CBrowser*);

void
on_webView_icon_changed(GtkWidget*, WebKitWebFrame*, CBrowser*);

void
on_webView_load_started(GtkWidget* , WebKitWebFrame*, CBrowser*);

void
on_webView_load_committed(GtkWidget* , WebKitWebFrame*, CBrowser*);

void
on_webView_load_changed(GtkWidget*, gint progress, CBrowser*);

void
on_webView_load_finished(GtkWidget*, WebKitWebFrame*, CBrowser*);

void
on_webView_status_message(GtkWidget*, const gchar*, CBrowser*);

void
on_webView_selection_changed(GtkWidget*, CBrowser*);

gboolean
on_webView_console_message(GtkWidget*, const gchar*, gint, const gchar*, CBrowser*);

void
on_webView_link_hover(GtkWidget*, const gchar*, const gchar*, CBrowser*);

/*
GtkWidget*
on_webView_window_open(GtkWidget*, const gchar*, CBrowser*);
*/

gboolean
on_webView_button_press(GtkWidget*, GdkEventButton*, CBrowser*);

gboolean
on_webView_button_press_after(GtkWidget*, GdkEventButton*, CBrowser*);

void
on_webView_popup(GtkWidget*, CBrowser*);

gboolean
on_webView_scroll(GtkWidget*, GdkEventScroll*, CBrowser*);

gboolean
on_webView_leave(GtkWidget*, GdkEventCrossing*, CBrowser*);

void
on_webView_destroy(GtkWidget*, CBrowser*);

GtkWidget*
webView_new(GtkWidget**);

void
webView_open(GtkWidget*, const gchar*);

void
webView_close(GtkWidget*, CBrowser*);

#endif /* !__webView_H__ */
