/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "search.h"

#include "sokoke.h"
#include <katze/katze.h>

#include <stdio.h>
#include <string.h>

GList* search_engines_new(void)
{
    return NULL;
}

void search_engines_free(GList* searchEngines)
{
    g_list_foreach(searchEngines, (GFunc)search_engine_free, NULL);
    g_list_free(searchEngines);
}

gboolean search_engines_from_file(GList** searchEngines, const gchar* filename
 , GError** error)
{
    g_return_val_if_fail(!g_list_nth(*searchEngines, 0), FALSE);
    GKeyFile* keyFile = g_key_file_new();
    g_key_file_load_from_file(keyFile, filename, G_KEY_FILE_KEEP_COMMENTS, error);
    /*g_key_file_load_from_data_dirs(keyFile, sFilename, NULL
     , G_KEY_FILE_KEEP_COMMENTS, error);*/
    gchar** engines = g_key_file_get_groups(keyFile, NULL);
    guint i;
    for(i = 0; engines[i] != NULL; i++)
    {
        SearchEngine* engine = search_engine_new();
        search_engine_set_short_name(engine, engines[i]);
        engine->description = g_key_file_get_string(keyFile, engines[i], "description", NULL);
        engine->url = g_key_file_get_string(keyFile, engines[i], "url", NULL);
        engine->inputEncoding = g_key_file_get_string(keyFile, engines[i], "input-encoding", NULL);
        engine->icon = g_key_file_get_string(keyFile, engines[i], "icon", NULL);
        engine->keyword = g_key_file_get_string(keyFile, engines[i], "keyword", NULL);
        *searchEngines = g_list_prepend(*searchEngines, engine);
    }
    *searchEngines = g_list_reverse(*searchEngines);
    g_strfreev(engines);
    g_key_file_free(keyFile);
    return !(error && *error);
}

static void key_file_set_string(GKeyFile* keyFile, const gchar* group
 , const gchar* key, const gchar* string)
{
    g_return_if_fail(group);
    if(string)
        g_key_file_set_string(keyFile, group, key, string);
}

gboolean search_engines_to_file(GList* searchEngines, const gchar* filename
 , GError** error)
{
    GKeyFile* keyFile = g_key_file_new();
    guint n = g_list_length(searchEngines);
    guint i;
    for(i = 0; i < n; i++)
    {
        SearchEngine* engine = (SearchEngine*)g_list_nth_data(searchEngines, i);
        const gchar* name = search_engine_get_short_name(engine);
        key_file_set_string(keyFile, name, "description", engine->description);
        key_file_set_string(keyFile, name, "url", engine->url);
        key_file_set_string(keyFile, name, "input-encoding", engine->inputEncoding);
        key_file_set_string(keyFile, name, "icon", engine->icon);
        key_file_set_string(keyFile, name, "keyword", engine->keyword);
    }
    gboolean bSaved = sokoke_key_file_save_to_file(keyFile, filename, error);
    g_key_file_free(keyFile);

    return bSaved;
}

SearchEngine* search_engine_new()
{
    SearchEngine* engine = g_new0(SearchEngine, 1);
    engine->shortName = g_strdup("");
    return engine;
}

void search_engine_free(SearchEngine* engine)
{
    g_return_if_fail(engine);
    g_free(engine->shortName);
    g_free(engine->description);
    g_free(engine->url);
    g_free(engine->inputEncoding);
    g_free(engine->icon);
    g_free(engine->keyword);
    g_free(engine);
}

SearchEngine* search_engine_copy(SearchEngine* engine)
{
    g_return_val_if_fail(engine, NULL);
    SearchEngine* copy = search_engine_new();
    search_engine_set_short_name(copy, engine->shortName);
    search_engine_set_description(copy, engine->description);
    search_engine_set_url(copy, engine->url);
    search_engine_set_input_encoding(copy, engine->inputEncoding);
    search_engine_set_icon(copy, engine->icon);
    search_engine_set_keyword(copy, engine->keyword);
    return engine;
}

GType search_engine_get_type()
{
    static GType type = 0;
    if(!type)
        type = g_pointer_type_register_static("search_engine");
    return type;
}

G_CONST_RETURN gchar* search_engine_get_short_name(SearchEngine* engine)
{
    g_return_val_if_fail(engine, NULL);
    return engine->shortName;
}

G_CONST_RETURN gchar* search_engine_get_description(SearchEngine* engine)
{
    g_return_val_if_fail(engine, NULL);
    return engine->description;
}

G_CONST_RETURN gchar* search_engine_get_url(SearchEngine* engine)
{
    g_return_val_if_fail(engine, NULL);
    return engine->url;
}

G_CONST_RETURN gchar* search_engine_get_input_encoding(SearchEngine* engine)
{
    g_return_val_if_fail(engine, NULL);
    return engine->inputEncoding;
}

G_CONST_RETURN gchar* search_engine_get_icon(SearchEngine* engine)
{
    g_return_val_if_fail(engine, NULL);
    return engine->icon;
}

G_CONST_RETURN gchar* search_engine_get_keyword(SearchEngine* engine)
{
    g_return_val_if_fail(engine, NULL);
    return engine->keyword;
}

void search_engine_set_short_name(SearchEngine* engine, const gchar* shortName)
{
    g_return_if_fail(engine);
    g_return_if_fail(shortName);
    katze_assign(engine->shortName, g_strdup(shortName));
}

void search_engine_set_description(SearchEngine* engine, const gchar* description)
{
    g_return_if_fail(engine);
    katze_assign(engine->description, g_strdup(description));
}

void search_engine_set_url(SearchEngine* engine, const gchar* url)
{
    g_return_if_fail(engine);
    katze_assign(engine->url, g_strdup(url));
}

void search_engine_set_input_encoding(SearchEngine* engine, const gchar* inputEncoding)
{
    g_return_if_fail(engine);
    katze_assign(engine->inputEncoding, g_strdup(inputEncoding));
}

void search_engine_set_icon(SearchEngine* engine, const gchar* icon)
{
    g_return_if_fail(engine);
    katze_assign(engine->icon, g_strdup(icon));
}

void search_engine_set_keyword(SearchEngine* engine, const gchar* keyword)
{
    g_return_if_fail(engine);
    katze_assign(engine->keyword, g_strdup(keyword));
}
