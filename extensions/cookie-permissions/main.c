/*
 Copyright (C) 2013 Stephan Haller <nomad@froevel.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "cookie-permission-manager.h"
#include "cookie-permission-manager-preferences-window.h"

/* Global instance */
CookiePermissionManager		*cpm=NULL;

/* This extension was activated */
static void _cpm_on_activate(MidoriExtension *inExtension, MidoriApp *inApp, gpointer inUserData)
{
	g_return_if_fail(cpm==NULL);

	cpm=cookie_permission_manager_new(inExtension, inApp);
	g_object_set(cpm, "unknown-policy", midori_extension_get_integer(inExtension, "unknown-policy"), NULL);
}

/* This extension was deactivated */
static void _cpm_on_deactivate(MidoriExtension *inExtension, gpointer inUserData)
{
	g_return_if_fail(cpm);

	g_object_unref(cpm);
	cpm=NULL;
}

/* Preferences of this extension should be opened */
static void _cpm_on_open_preferences_response(GtkWidget* inDialog,
												gint inResponse,
												MidoriExtension* inExtension)
{
	gtk_widget_destroy(inDialog);
}

static void _cpm_on_open_preferences(MidoriExtension *inExtension)
{
	g_return_if_fail(cpm);

	/* Show preferences window */
	GtkWidget* dialog;

	dialog=cookie_permission_manager_preferences_window_new(cpm);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	g_signal_connect(dialog, "response", G_CALLBACK (_cpm_on_open_preferences_response), inExtension);
	gtk_widget_show_all(dialog);
}

/* Main entry for extension */
MidoriExtension *extension_init(void)
{
	/* Set up extension */
	MidoriExtension	*extension=g_object_new(MIDORI_TYPE_EXTENSION,
												"name", _("Cookie Security Manager"),
												"description", _("Manage cookie permission per site"),
												"version", "0.1" MIDORI_VERSION_SUFFIX,
												"authors", "Stephan Haller <nomad@froevel.de>",
												NULL);

	midori_extension_install_integer(extension, "unknown-policy", COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED);
	midori_extension_install_boolean(extension, "show-details-when-ask", FALSE);

	g_signal_connect(extension, "activate", G_CALLBACK(_cpm_on_activate), NULL);
	g_signal_connect(extension, "deactivate", G_CALLBACK(_cpm_on_deactivate), NULL);
	g_signal_connect(extension, "open-preferences", G_CALLBACK(_cpm_on_open_preferences), NULL);

	return(extension);
}
