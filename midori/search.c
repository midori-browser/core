/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "search.h"

#include "midori-webitem.h"

#include "sokoke.h"

GList* search_engines_new(void)
{
    return NULL;
}

void search_engines_free(GList* searchEngines)
{
    g_list_foreach(searchEngines, (GFunc)g_object_unref, NULL);
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
        MidoriWebItem* web_item = midori_web_item_new ();
        guint j, n_properties;
        GParamSpec** pspecs = g_object_class_list_properties (
            G_OBJECT_GET_CLASS (web_item), &n_properties);
        for (j = 0; j < n_properties; j++)
        {
            const gchar* property = g_param_spec_get_name (pspecs[j]);
            gchar* value = g_key_file_get_string (keyFile, engines[i],
                                                  property, NULL);
            g_object_set (web_item, property, value, NULL);
            g_free (value);
        }
        *searchEngines = g_list_prepend(*searchEngines, web_item);
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
        MidoriWebItem* web_item = (MidoriWebItem*)g_list_nth_data(searchEngines, i);
        const gchar* name = midori_web_item_get_name (web_item);
        guint j, n_properties;
        GParamSpec** pspecs = g_object_class_list_properties (
            G_OBJECT_GET_CLASS (web_item), &n_properties);
        for (j = 0; j < n_properties; j++)
        {
            const gchar* property = g_param_spec_get_name (pspecs[j]);
            gchar* value;
            g_object_get (web_item, property, &value, NULL);
            key_file_set_string (keyFile, name, property, value);
            g_free (value);
        }
    }
    gboolean bSaved = sokoke_key_file_save_to_file(keyFile, filename, error);
    g_key_file_free(keyFile);

    return bSaved;
}
