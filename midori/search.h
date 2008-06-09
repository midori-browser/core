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

GList*
search_engines_new(void);

void
search_engines_free(GList*);

gboolean
search_engines_from_file(GList**, const gchar*, GError**);

gboolean
search_engines_to_file(GList*, const gchar*, GError**);

#endif /* !__SEARCH_H__ */
