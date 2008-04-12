/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "conf.h"

#include "sokoke.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

CConfig* config_new(void)
{
    return g_new0(CConfig, 1);
}

void config_free(CConfig* config)
{
    g_free(config->homepage);
    g_free(config->locationSearch);
    g_free(config->panelPageholder);
    g_free(config);
}

gboolean config_from_file(CConfig* config, const gchar* filename, GError** error)
{
    GKeyFile* keyFile = g_key_file_new();
    g_key_file_load_from_file(keyFile, filename, G_KEY_FILE_KEEP_COMMENTS, error);
    /*g_key_file_load_from_data_dirs(keyFile, sFilename, NULL
     , G_KEY_FILE_KEEP_COMMENTS, error);*/
    #define GET_INT(var, key, default) \
     var = sokoke_key_file_get_integer_default( \
     keyFile, "browser", key, default, NULL)
    #define GET_STR(var, key, default) \
     var = sokoke_key_file_get_string_default( \
     keyFile, "browser", key, default, NULL)
    GET_INT(config->startup, "Startup", CONFIG_STARTUP_HOMEPAGE);
    GET_STR(config->homepage, "Homepage", "http://www.google.com");
    GET_STR(config->locationSearch, "LocationSearch", "http://www.google.com/search?q=%s");
    GET_INT(config->toolbarNavigation, "ToolbarNavigation", TRUE);
    GET_INT(config->toolbarBookmarks, "ToolbarBookmarks", FALSE);
    GET_INT(config->toolbarStatus, "ToolbarStatus", TRUE);
    //GET_INT(config->toolbarTransfer, "ToolbarTransfer", FALSE);
    GET_INT(config->toolbarStyle, "ToolbarStyle", CONFIG_TOOLBAR_DEFAULT);
    GET_INT(config->toolbarSmall, "ToolbarSmall", FALSE);
    GET_INT(config->toolbarWebSearch, "ToolbarWebSearch", TRUE);
    GET_INT(config->toolbarNewTab, "ToolbarNewTab", TRUE);
    GET_INT(config->toolbarClosedTabs, "ToolbarClosedTabs", TRUE);
    GET_INT(config->panelShow, "PanelShow", FALSE);
    GET_INT(config->panelActive, "PanelActive", 0);
    GET_STR(config->panelPageholder, "PanelPageholder", "http://www.google.com");
    GET_INT(config->tabSize, "TabSize", 10);
    GET_INT(config->tabClose, "TabClose", TRUE);
    GET_INT(config->newPages, "NewPages", CONFIG_NEWPAGES_TAB_NEW);
    GET_INT(config->openTabsInTheBackground, "OpenTabsInTheBackground", FALSE);
    GET_INT(config->openPopupsInTabs, "OpenPopupsInTabs", FALSE);
    GET_INT(config->middleClickGoto, "MiddleClickGoto", FALSE);
    #undef GET_INT
    #undef GET_STR

    #define GET_INT(var, key, default) \
     var = sokoke_key_file_get_integer_default( \
     keyFile, "content", key, default, NULL)
    #define GET_STR(var, key, default) \
     var = sokoke_key_file_get_string_default( \
     keyFile, "content", key, default, NULL)
    GET_STR(config->defaultFontFamily, "DefaultFontFamily", "Sans");
    GET_INT(config->defaultFontSize, "DefaultFontSize", 10);
    GET_INT(config->minimumFontSize, "MinimumFontSize", 5);
    GET_STR(config->defaultEncoding, "DefaultEncoding", "UTF-8");
    GET_INT(config->autoLoadImages, "AutoLoadImages", TRUE);
    GET_INT(config->autoShrinkImages, "AutoShrinkImages", TRUE);
    GET_INT(config->printBackgrounds, "PrintBackgrounds", FALSE);
    GET_INT(config->resizableTextAreas, "ResizableTextAreas", FALSE);
    GET_INT(config->userStylesheet, "UserStylesheet", FALSE);
    GET_STR(config->userStylesheetUri, "UserStylesheetUri", "");
    GET_INT(config->enableScripts, "EnableScripts", TRUE);
    GET_INT(config->enablePlugins, "EnablePlugins", TRUE);
    #undef GET_INT
    #undef GET_STR

    #define GET_INT(var, key, default) \
     var = sokoke_key_file_get_integer_default( \
     keyFile, "session", key, default, NULL)
    #define GET_STR(var, key, default) \
     var = sokoke_key_file_get_string_default( \
     keyFile, "session", key, default, NULL)
    GET_INT(config->rememberWinSize, "RememberWinSize", TRUE);
    GET_INT(config->winWidth, "WinWidth", 0);
    GET_INT(config->winHeight, "WinHeight", 0);
    GET_INT(config->winPanelPos, "WinPanelPos", 0);
    GET_INT(config->searchEngine, "SearchEngine", 0);
    #undef GET_INT
    #undef GET_STR

    g_key_file_free(keyFile);
    return !(error && *error);
}

gboolean config_to_file(CConfig* config, const gchar* filename, GError** error)
{
    GKeyFile* keyFile = g_key_file_new();

    g_key_file_set_integer(keyFile, "browser", "Startup", config->startup);
    g_key_file_set_string (keyFile, "browser", "Homepage", config->homepage);
    g_key_file_set_string (keyFile, "browser", "LocationSearch", config->locationSearch);
    g_key_file_set_integer(keyFile, "browser", "ToolbarNavigation", config->toolbarNavigation);
    g_key_file_set_integer(keyFile, "browser", "ToolbarBookmarks", config->toolbarBookmarks);
    //g_key_file_set_integer(keyFile, "browser", "ToolbarTransfers", config->toolbarTransfers);
    g_key_file_set_integer(keyFile, "browser", "ToolbarStatus", config->toolbarStatus);
    g_key_file_set_integer(keyFile, "browser", "ToolbarStyle", config->toolbarStyle);
    g_key_file_set_integer(keyFile, "browser", "ToolbarSmall", config->toolbarSmall);
    g_key_file_set_integer(keyFile, "browser", "ToolbarWebSearch", config->toolbarWebSearch);
    g_key_file_set_integer(keyFile, "browser", "ToolbarNewTab", config->toolbarNewTab);
    g_key_file_set_integer(keyFile, "browser", "ToolbarClosedTabs", config->toolbarClosedTabs);
    g_key_file_set_integer(keyFile, "browser", "PanelShow", config->panelShow);
    g_key_file_set_integer(keyFile, "browser", "PanelActive", config->panelActive);
    g_key_file_set_string (keyFile, "browser", "PanelPageholder", config->panelPageholder);
    g_key_file_set_integer(keyFile, "browser", "TabSize", config->tabSize);
    g_key_file_set_integer(keyFile, "browser", "TabClose", config->tabClose);
    g_key_file_set_integer(keyFile, "browser", "NewPages", config->newPages);
    g_key_file_set_integer(keyFile, "browser", "OpenTabsInTheBackground", config->openTabsInTheBackground);
    g_key_file_set_integer(keyFile, "browser", "OpenPopupsInTabs", config->openPopupsInTabs);
    g_key_file_set_integer(keyFile, "browser", "MiddleClickGoto", config->middleClickGoto);

    g_key_file_set_string (keyFile, "content", "DefaultFontFamily", config->defaultFontFamily);
    g_key_file_set_integer(keyFile, "content", "DefaultFontSize", config->defaultFontSize);
    g_key_file_set_integer(keyFile, "content", "MinimumFontSize", config->minimumFontSize);
    g_key_file_set_string (keyFile, "content", "DefaultEncoding", config->defaultEncoding);
    g_key_file_set_integer(keyFile, "content", "AutoLoadImages", config->autoLoadImages);
    g_key_file_set_integer(keyFile, "content", "AutoShrinkImages", config->autoShrinkImages);
    g_key_file_set_integer(keyFile, "content", "PrintBackgrounds", config->printBackgrounds);
    g_key_file_set_integer(keyFile, "content", "ResizableTextAreas", config->resizableTextAreas);
    g_key_file_set_integer(keyFile, "content", "UserStylesheet", config->userStylesheet);
    g_key_file_set_string (keyFile, "content", "UserStylesheetUri", config->userStylesheetUri);
    g_key_file_set_integer(keyFile, "content", "EnableScripts", config->enableScripts);
    g_key_file_set_integer(keyFile, "content", "EnablePlugins", config->enablePlugins);

    g_key_file_set_integer(keyFile, "session", "RememberWinSize", config->rememberWinSize);
    g_key_file_set_integer(keyFile, "session", "WinWidth", config->winWidth);
    g_key_file_set_integer(keyFile, "session", "WinHeight", config->winHeight);
    g_key_file_set_integer(keyFile, "session", "WinPanelPos", config->winPanelPos);
    g_key_file_set_integer(keyFile, "session", "SearchEngine", config->searchEngine);

    gboolean saved = sokoke_key_file_save_to_file(keyFile, filename, error);
    g_key_file_free(keyFile);

    return saved;
}
