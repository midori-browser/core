/*
 Copyright (C) 2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "config.h"
#include <midori/midori.h>

#include "cookie-manager.h"

CookieManager *cm = NULL;


static void cm_deactivate_cb(MidoriExtension *extension, gpointer data)
{
	g_object_unref(cm);
}


static void cm_activate_cb(MidoriExtension *extension, MidoriApp *app, gpointer data)
{
	cm = cookie_manager_new(extension, app);
}


MidoriExtension *extension_init(void)
{
	MidoriExtension *extension = g_object_new(MIDORI_TYPE_EXTENSION,
		"name", _("Cookie Manager"),
		"description", _("List, view and delete cookies"),
		"version", "0.2" MIDORI_VERSION_SUFFIX,
		"authors", "Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>",
		NULL);

	g_signal_connect(extension, "activate", G_CALLBACK(cm_activate_cb), NULL);
	g_signal_connect(extension, "deactivate", G_CALLBACK(cm_deactivate_cb), NULL);

	return extension;
}
