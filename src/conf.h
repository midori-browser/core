/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __CONF_H__
#define __CONF_H__ 1

#include <glib.h>

typedef struct _CConfig
{
    guint    startup;
    gchar*   homepage;
    gchar*   locationSearch;
    gboolean toolbarNavigation;
    gboolean toolbarBookmarks;
    //gboolean toolbarDownloads;
    gboolean toolbarStatus;
    guint    toolbarStyle;
    gboolean toolbarSmall;
    gboolean panelShow;
    guint    panelActive;
    gchar*   panelPageholder;
    guint    tabSize; // tab size in charcters
    gboolean tabClose;
    gboolean toolbarWebSearch;
    gboolean toolbarNewTab;
    gboolean toolbarClosedTabs;
    guint    newPages; // where to open new pages
    gboolean openTabsInTheBackground;
    gboolean openPopupsInTabs;

    
    gboolean autoLoadImages;
    gboolean autoShrinkImages;
    gboolean printBackgrounds;
    gboolean resizableTextAreas;
    gchar* userStylesheetUri;
    gboolean enableScripts;
    gboolean enablePlugins;

    gboolean rememberWinSize; // Restore window size upon startup?
    gint     winWidth;
    gint     winHeight;
    guint    winPanelPos;
    guint    searchEngine; // last selected search engine

    GPtrArray* protocols_names;
    GData*     protocols_commands;
} CConfig;

enum
{
    CONFIG_STARTUP_BLANK,
    CONFIG_STARTUP_HOMEPAGE,
    CONFIG_STARTUP_SESSION
};

enum
{
    CONFIG_TOOLBAR_DEFAULT,
    CONFIG_TOOLBAR_ICONS,
    CONFIG_TOOLBAR_TEXT,
    CONFIG_TOOLBAR_BOTH,
    CONFIG_TOOLBAR_BOTH_HORIZ
};

enum
{
    CONFIG_NEWPAGES_TAB_NEW,
    CONFIG_NEWPAGES_WINDOW_NEW,
    CONFIG_NEWPAGES_TAB_CURRENT
};

CConfig*
config_new(void);

void
config_free(CConfig*);

gboolean
config_from_file(CConfig*, const gchar*, GError**);

gboolean
config_to_file(CConfig*, const gchar*, GError**);

#endif /* !__CONF_H__ */
