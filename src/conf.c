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

    g_key_file_set_integer(keyFile, "session", "RememberWinSize", config->rememberWinSize);
    g_key_file_set_integer(keyFile, "session", "WinWidth", config->winWidth);
    g_key_file_set_integer(keyFile, "session", "WinHeight", config->winHeight);
    g_key_file_set_integer(keyFile, "session", "WinPanelPos", config->winPanelPos);
    g_key_file_set_integer(keyFile, "session", "SearchEngine", config->searchEngine);

    gboolean saved = sokoke_key_file_save_to_file(keyFile, filename, error);
    g_key_file_free(keyFile);

    return saved;
}
