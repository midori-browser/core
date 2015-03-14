/*
 Copyright (C) 2013 Stephan Haller <nomad@froevel.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "nojs.h"
#include "nojs-preferences.h"

/* Global instance */
NoJS		*noJS=NULL;

/* This extension was activated */
static void _nojs_on_activate(MidoriExtension *inExtension, MidoriApp *inApp, gpointer inUserData)
{
	g_return_if_fail(noJS==NULL);

	noJS=nojs_new(inExtension, inApp);
	nojs_set_policy_for_unknown_domain(noJS, midori_extension_get_integer(inExtension, "unknown-domain-policy"));
	nojs_set_allow_local_pages(noJS, midori_extension_get_boolean(inExtension, "allow-local-pages"));
	nojs_set_only_second_level_domain(noJS, midori_extension_get_boolean(inExtension, "only-second-level"));
}

/* This extension was deactivated */
static void _nojs_on_deactivate(MidoriExtension *inExtension, gpointer inUserData)
{
	g_return_if_fail(noJS);

	g_object_unref(noJS);
	noJS=NULL;
}

/* Preferences of this extension should be opened */
static void _nojs_on_preferences_response(GtkWidget* inDialog,
											gint inResponse,
											gpointer *inUserData)
{
	gtk_widget_destroy(inDialog);
}

static void _nojs_on_open_preferences(MidoriExtension *inExtension)
{
	g_return_if_fail(noJS);

	/* Show preferences window */
	GtkWidget* dialog;

	dialog=nojs_preferences_new(noJS);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	g_signal_connect(dialog, "response", G_CALLBACK (_nojs_on_preferences_response), NULL);
	gtk_widget_show_all(dialog);
}

/* Main entry for extension */
MidoriExtension *extension_init(void)
{
	/* Set up extension */
	MidoriExtension	*extension=g_object_new(MIDORI_TYPE_EXTENSION,
												"name", _("NoJS"),
												"description", _("Manage javascript permission per site"),
												"version", "0.1" MIDORI_VERSION_SUFFIX,
												"authors", "Stephan Haller <nomad@froevel.de>",
												NULL);

	midori_extension_install_integer(extension, "unknown-domain-policy", NOJS_POLICY_BLOCK);
	midori_extension_install_boolean(extension, "allow-local-pages", TRUE);
	midori_extension_install_boolean(extension, "only-second-level", TRUE);

	g_signal_connect(extension, "activate", G_CALLBACK(_nojs_on_activate), NULL);
	g_signal_connect(extension, "deactivate", G_CALLBACK(_nojs_on_deactivate), NULL);
	g_signal_connect(extension, "open-preferences", G_CALLBACK(_nojs_on_open_preferences), NULL);

	return(extension);
}
