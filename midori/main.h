/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MAIN_H__
#define __MAIN_H__ 1

#include <katze/katze.h>

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include <glib/gi18n.h>

// FIXME: Remove these globals

GList* searchEngines; // Items of type 'SearchEngine'
KatzeXbelItem* bookmarks;

// Custom stock items

// We should distribute these
// Names should match with epiphany and/ or xdg spec
/* NOTE: Those uncommented were replaced with remotely related icons
         in order to reduce the amount of warnings :D */

#define STOCK_BOOKMARK           GTK_STOCK_FILE // "stock_bookmark" "bookmark-web"
#define STOCK_FORM_FILL          GTK_STOCK_JUSTIFY_FILL // "insert-text" "form-fill"
#define STOCK_NEWSFEED           GTK_STOCK_INDEX

// We assume that these legacy icon names are usually present

#define STOCK_BOOKMARK_ADD       "stock_add-bookmark"
#define STOCK_HOMEPAGE           GTK_STOCK_HOME
#define STOCK_IMAGE              "gnome-mime-image"
#define STOCK_LOCK_OPEN          "stock_lock-open"
#define STOCK_LOCK_SECURE        "stock_lock"
#define STOCK_LOCK_BROKEN        "stock_lock-broken"
#define STOCK_NETWORK_OFFLINE    "network-offline"
#define STOCK_SCRIPT             "stock_script"
#define STOCK_SEND               "stock_mail-send"
#define STOCK_TAB_NEW            "stock_new-tab"
#define STOCK_THEME              "gnome-settings-theme"
#define STOCK_USER_TRASH         "gnome-stock-trash"
#define STOCK_WINDOW_NEW         "stock_new-window"

// For backwards compatibility

#if !GTK_CHECK_VERSION(2, 10, 0)
#define GTK_STOCK_SELECT_ALL     "gtk-select-all"
#endif
#if !GTK_CHECK_VERSION(2, 8, 0)
#define GTK_STOCK_FULLSCREEN "gtk-fullscreen"
#define GTK_STOCK_LEAVE_FULLSCREEN "gtk-leave-fullscreen"
#endif

#endif /* !__MAIN_H__ */
