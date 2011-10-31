/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_STOCK_H__
#define __MIDORI_STOCK_H__ 1

/* Custom stock items

   We should distribute these
   Names should match with epiphany and/ or xdg spec */

#define STOCK_BOOKMARK           "stock_bookmark"
#define STOCK_BOOKMARKS          "user-bookmarks"
#define STOCK_CONSOLE            "terminal"
#define STOCK_EXTENSION          "extension"
#define STOCK_EXTENSIONS         "extension"
#define STOCK_HISTORY            "document-open-recent"
#define STOCK_WEB_BROWSER        "web-browser"
#define STOCK_NEWS_FEED          "news-feed"
#define STOCK_STYLE              "gnome-settings-theme"
#define STOCK_TRANSFER           "package"
#define STOCK_TRANSFERS          "package"
#define STOCK_PLUGINS            "gnome-mime-application-x-shockwave-flash"

#define STOCK_BOOKMARK_ADD       "bookmark-new"
#define STOCK_HOMEPAGE           "go-home"
#define STOCK_IMAGE              "gnome-mime-image"
#define STOCK_NETWORK_OFFLINE    "network-offline"
#define STOCK_SCRIPT             "stock_script"
#define STOCK_SCRIPTS            "gnome-settings-theme"
#define STOCK_SEND               "stock_mail-send"
#define STOCK_TAB_NEW            "stock_new-tab"
#define STOCK_USER_TRASH         "gnome-stock-trash"
#define STOCK_WINDOW_NEW         "stock_new-window"

#if defined (HAVE_HILDON) && HAVE_HILDON
    #undef STOCK_BOOKMARKS
    #define STOCK_BOOKMARKS "general_mybookmarks_folder"
    #undef STOCK_NEWS_FEED
    #define STOCK_NEWS_FEED "general_rss"
    #undef STOCK_WEB_BROWSER
    #define STOCK_WEB_BROWSER "general_web"
#endif

#endif /* !__MIDORI_STOCK_H__ */
