/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "tab-panel-extension.h"

#include <midori/midori.h>

struct _TabPanelExtension
{
    MidoriExtension parent_instance;
};

struct _TabPanelExtensionClass
{
    MidoriExtensionClass parent_class;
};

G_DEFINE_TYPE (TabPanelExtension, tab_panel_extension, MIDORI_TYPE_EXTENSION);

static void
tab_panel_extension_class_init (TabPanelExtensionClass* class)
{
    /* Nothing to do. */
}

static void
tab_panel_extension_init (TabPanelExtension* source)
{
    /* Nothing to do. */
}
