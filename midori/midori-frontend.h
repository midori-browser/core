/*
 Copyright (C) 2012-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_FRONTEND_H__
#define __MIDORI_FRONTEND_H__

#include "midori/midori-app.h"

MidoriBrowser*
midori_web_app_new (const gchar* webapp,
                    gchar**      open_uris,
                    gchar**      execute_commands,
                    gint         inactivity_reset,
                    const gchar* block_uris);

MidoriBrowser*
midori_private_app_new (const gchar* config,
                        const gchar* webapp,
                        gchar**      open_uris,
                        gchar**      execute_commands,
                        gint         inactivity_reset,
                        const gchar* block_uris);

MidoriApp*
midori_normal_app_new (const gchar* config,
                       gchar*       nickname,
                       gboolean     diagnostic_dialog,
                       gchar**      open_uris,
                       gchar**      execute_commands,
                       gint         inactivity_reset,
                       const gchar* block_uris);

void
midori_normal_app_on_quit (MidoriApp* app);

G_END_DECLS

#endif /* __MIDORI_FRONTEND_H__ */

