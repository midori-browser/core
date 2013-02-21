/*
 Copyright (C) 2008-2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __PRIVATE_DATA_H__
#define __PRIVATE_DATA_H__ 1

#include <midori/midori-browser.h>
#include "katze/katze.h"

GtkWidget*
midori_private_data_get_dialog (MidoriBrowser* browser);

void
midori_private_data_register_built_ins ();

void
midori_private_data_clear_all (MidoriBrowser* browser);

void
midori_private_data_on_quit (MidoriWebSettings* settings);

typedef struct
{
    gchar* name;
    gchar* label;
    GCallback clear;
} MidoriPrivateDataItem;

GList*
midori_private_data_register_item (const gchar* name,
                                   const gchar* label,
                                   GCallback    clear);

#endif /* !__SOKOKE_H__ */

