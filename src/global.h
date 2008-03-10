/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __GLOBAL_H__
#define __GLOBAL_H__ 1

#include "conf.h"

#include "midori-websettings.h"
#include "../katze/katze.h"

#include <gtk/gtk.h>
#include <webkit/webkit.h>

// FIXME: Remove these globals

GList* searchEngines; // Items of type 'SearchEngine'
KatzeXbelItem* bookmarks;
CConfig* config;
MidoriWebSettings* webSettings;

// Custom stock items

// We should distribute these
// Names should match with epiphany and/ or xdg spec
/* NOTE: Those uncommented were replaced with remotely related icons
         in order to reduce the amount of warnings :D */

#define STOCK_BOOKMARK           GTK_STOCK_FILE // "stock_bookmark" // "bookmark-web"
#define STOCK_FORM_FILL          "insert-text" // "form-fill" // MISSING
#define STOCK_LOCATION           "location-entry"
#define STOCK_NEWSFEED           "gtk-index" // "newsfeed" // MISSING
#define STOCK_PLUGINS            "plugin" // MISSING
#define STOCK_POPUPS_BLOCKED     "popup-hidden"
#define STOCK_SOURCE_VIEW        "stock_view-html-source" // MISSING
#define STOCK_TAB_CLOSE          "tab-close" // MISSING
#define STOCK_WINDOW_CLOSE       "window-close" // MISSING

// We can safely use standard icons
// Assuming that we have reliable fallback icons

#define STOCK_BOOKMARK_NEW       "bookmark-new"
#define STOCK_BOOKMARK_NEW_      "stock_add-bookmark"
#define STOCK_HOMEPAGE           GTK_STOCK_HOME
#define STOCK_IMAGE              "image-x-generic"
#define STOCK_IMAGE_             "gnome-mime-image"
#define STOCK_LOCK_OPEN          "stock_lock-open"
#define STOCK_LOCK_SECURE        "stock_lock"
#define STOCK_LOCK_BROKEN        "stock_lock-broken"
#define STOCK_NETWORK_OFFLINE    "connect_no"
#define STOCK_NETWORK_OFFLINE_   "network-offline"
#define STOCK_SCRIPT             "stock_script"
#define STOCK_SEND               "mail-send"
#define STOCK_SEND_              "stock_mail-send"
#define STOCK_TAB_NEW            "tab-new"
#define STOCK_TAB_NEW_           "stock_new-tab"
#define STOCK_THEME              "gnome-settings-theme"
#define STOCK_USER_TRASH         "user-trash"
#define STOCK_USER_TRASH_        "gnome-stock-trash"
#define STOCK_WINDOW_NEW         "window-new"
#define STOCK_WINDOW_NEW_        "stock_new-window"

// For backwards compatibility

#if !GTK_CHECK_VERSION(2, 10, 0)
#define GTK_STOCK_SELECT_ALL     "gtk-select-all"
#endif
#if !GTK_CHECK_VERSION(2, 8, 0)
#define GTK_STOCK_FULLSCREEN "gtk-fullscreen"
#define GTK_STOCK_LEAVE_FULLSCREEN "gtk-leave-fullscreen"
#endif

#endif /* !__GLOBAL_H__ */
