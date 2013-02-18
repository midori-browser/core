/*
 Copyright (C) 2013 Stephan Haller <nomad@froevel.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW__
#define __COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW__

#include "config.h"
#include <midori/midori.h>

#include "cookie-permission-manager.h"

G_BEGIN_DECLS

#define TYPE_COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW				(cookie_permission_manager_preferences_window_get_type())
#define COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW, CookiePermissionManagerPreferencesWindow))
#define IS_COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW))
#define COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), TYPE_COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW, CookiePermissionManagerPreferencesWindowClass))
#define IS_COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW))
#define COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW, CookiePermissionManagerPreferencesWindowClass))

typedef struct _CookiePermissionManagerPreferencesWindow				CookiePermissionManagerPreferencesWindow;
typedef struct _CookiePermissionManagerPreferencesWindowClass			CookiePermissionManagerPreferencesWindowClass;
typedef struct _CookiePermissionManagerPreferencesWindowPrivate			CookiePermissionManagerPreferencesWindowPrivate;

struct _CookiePermissionManagerPreferencesWindow
{
	/* Parent instance */
	GtkDialog										parent_instance;

	/* Private structure */
	CookiePermissionManagerPreferencesWindowPrivate	*priv;
};

struct _CookiePermissionManagerPreferencesWindowClass
{
	/* Parent class */
	GtkDialogClass									parent_class;
};

/* Public API */
GType cookie_permission_manager_preferences_window_get_type(void);

GtkWidget* cookie_permission_manager_preferences_window_new(CookiePermissionManager *inManager);

G_END_DECLS

#endif /* __COOKIE_PERMISSION_MANAGER_PREFERENCES_WINDOW__ */
