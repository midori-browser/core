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
#include <webkit/webkit.h>

#include "cookie-manager.h"
#include "cookie-manager-page.h"



typedef struct
{
	MidoriApp *app;
	MidoriBrowser *browser;
	MidoriExtension *extension;
	GtkWidget *panel_page;
} CMData;

static void cm_app_add_browser_cb(MidoriApp *app, MidoriBrowser *browser, MidoriExtension *ext);
static void cm_deactivate_cb(MidoriExtension *extension, CMData *cmdata);



static void cm_browser_close_cb(GtkObject *browser, CMData *cmdata)
{
	g_signal_handlers_disconnect_by_func(cmdata->extension, cm_deactivate_cb, cmdata);
	g_signal_handlers_disconnect_by_func(cmdata->browser, cm_browser_close_cb, cmdata);

	/* the panel_page widget gets destroyed automatically when a browser is closed but not
	 * when the extension is deactivated */
	if (cmdata->panel_page != NULL && IS_COOKIE_MANAGER_PAGE(cmdata->panel_page))
		gtk_widget_destroy(cmdata->panel_page);

	g_free(cmdata);
}


static void cm_deactivate_cb(MidoriExtension *extension, CMData *cmdata)
{
	g_signal_handlers_disconnect_by_func(cmdata->app, cm_app_add_browser_cb, extension);
	cm_browser_close_cb(NULL, cmdata);
}


static void cm_app_add_browser_cb(MidoriApp *app, MidoriBrowser *browser, MidoriExtension *ext)
{
	GtkWidget *panel;
	GtkWidget *page;
	CMData *cmdata;

	panel = katze_object_get_object(browser, "panel");

	page = cookie_manager_page_new();
	gtk_widget_show(page);
	midori_panel_append_page(MIDORI_PANEL(panel), MIDORI_VIEWABLE(page));

	cmdata = g_new0(CMData, 1);
	cmdata->app = app;
	cmdata->browser = browser;
	cmdata->extension = ext;
	cmdata->panel_page = page;

	g_signal_connect(browser, "destroy", G_CALLBACK(cm_browser_close_cb), cmdata);
	g_signal_connect(ext, "deactivate", G_CALLBACK(cm_deactivate_cb), cmdata);

	g_object_unref(panel);
}


static void cm_activate_cb(MidoriExtension *extension, MidoriApp *app, gpointer data)
{
	guint i;
	KatzeArray *browsers;
	MidoriBrowser *browser;

	/* add the cookie manager panel page to existing browsers */
	browsers = katze_object_get_object(app, "browsers");
	i = 0;
	while ((browser = katze_array_get_nth_item(browsers, i++)))
		cm_app_add_browser_cb(app, browser, extension);
	g_object_unref(browsers);

	g_signal_connect(app, "add-browser", G_CALLBACK(cm_app_add_browser_cb), extension);
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

	return extension;
}
