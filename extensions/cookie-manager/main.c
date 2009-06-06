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
	MidoriExtension *extension;
	GtkIconFactory *factory;
	GtkIconSource *icon_source;
	GtkIconSet *icon_set;
	static GtkStockItem items[] =
	{
		{ STOCK_COOKIE_MANAGER, N_("_Cookie Manager"), 0, 0, NULL }
	};

	factory = gtk_icon_factory_new();
	gtk_stock_add(items, G_N_ELEMENTS(items));
	icon_set = gtk_icon_set_new();
	icon_source = gtk_icon_source_new();
	gtk_icon_source_set_icon_name(icon_source, GTK_STOCK_DIALOG_AUTHENTICATION);
	gtk_icon_set_add_source(icon_set, icon_source);
	gtk_icon_source_free(icon_source);
	gtk_icon_factory_add(factory, STOCK_COOKIE_MANAGER, icon_set);
	gtk_icon_set_unref(icon_set);
	gtk_icon_factory_add_default(factory);
	g_object_unref(factory);

	extension = g_object_new(MIDORI_TYPE_EXTENSION,
		"name", _("Cookie Manager"),
		"description", _("List, view and delete cookies"),
		"version", "0.2",
		"authors", "Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>",
		NULL);

	g_signal_connect(extension, "activate", G_CALLBACK(cm_activate_cb), NULL);
	g_signal_connect(extension, "deactivate", G_CALLBACK(cm_deactivate_cb), NULL);

	return extension;
}
