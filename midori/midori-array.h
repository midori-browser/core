/*
 Copyright (C) 2007-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_ARRAY_H__
#define __MIDORI_ARRAY_H__ 1

#include <katze/katze.h>

gboolean
midori_array_from_file (KatzeArray*  array,
                        const gchar* filename,
                        const gchar* format,
                        GError**     error);

gboolean
midori_array_to_file   (KatzeArray*  array,
                        const gchar* filename,
                        const gchar* format,
                        GError**     error);

#endif /* !__MIDORI_ARRAY_H__ */
