/*
 Copyright (C) 2010-2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_HISTORY_H__
#define __MIDORI_HISTORY_H__ 1

#include <sqlite3.h>
#include <katze/katze.h>
#include "midori/midori-websettings.h"

KatzeArray*
midori_history_new (char** errmsg);

void
midori_history_on_quit (KatzeArray*        array,
                        MidoriWebSettings* settings);

#endif /* !__MIDORI_HISTORY_H__ */

