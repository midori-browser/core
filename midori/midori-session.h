/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_SESSION_H__
#define __MIDORI_SESSION_H__

#include <glib/gstdio.h>
#include "midori-app.h"
#include "midori-websettings.h"

gboolean
midori_load_soup_session (gpointer settings);

gboolean
midori_load_soup_session_full (gpointer settings);

gboolean
midori_load_extensions (gpointer data);

void
midori_session_persistent_settings (MidoriWebSettings* settings,
                                    MidoriApp*         app);

#endif /* __MIDORI_SESSION_H__ */

