/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __SEARCH_H__
#define __SEARCH_H__ 1

#include <glib.h>
#include <glib-object.h>

// Note: This structure is entirely private.
typedef struct
{
    gchar* shortName;
    gchar* description;
    gchar* url;
    gchar* inputEncoding;
    gchar* icon;
    gchar* keyword;
} SearchEngine;

GList*
search_engines_new(void);

void
search_engines_free(GList*);

gboolean
search_engines_from_file(GList**, const gchar*, GError**);

gboolean
search_engines_to_file(GList*, const gchar*, GError**);

SearchEngine*
search_engine_new(void);

void
search_engine_free(SearchEngine*);

SearchEngine*
search_engine_copy(SearchEngine*);

GType
search_engine_get_type();

#define G_TYPE_SEARCH_ENGINE search_engine_get_type()

G_CONST_RETURN gchar*
search_engine_get_short_name(SearchEngine*);

G_CONST_RETURN gchar*
search_engine_get_description(SearchEngine*);

G_CONST_RETURN gchar*
search_engine_get_url(SearchEngine*);

G_CONST_RETURN gchar*
search_engine_get_input_encoding(SearchEngine*);

G_CONST_RETURN gchar*
search_engine_get_icon(SearchEngine*);

G_CONST_RETURN gchar*
search_engine_get_keyword(SearchEngine*);

void
search_engine_set_short_name(SearchEngine*, const gchar*);

void
search_engine_set_description(SearchEngine*, const gchar*);

void
search_engine_set_url(SearchEngine*, const gchar*);

void
search_engine_set_input_encoding(SearchEngine*, const gchar*);

void
search_engine_set_icon(SearchEngine*, const gchar*);

void
search_engine_set_keyword(SearchEngine*, const gchar*);

#endif /* !__SEARCH_H__ */
