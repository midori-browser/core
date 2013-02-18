/*
 Copyright (C) 2013 Stephan Haller <nomad@froevel.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __COOKIE_PERMISSION_MANAGER__
#define __COOKIE_PERMISSION_MANAGER__

#include "config.h"
#include <midori/midori.h>

#define COOKIE_PERMISSION_DATABASE	"domains.db"

G_BEGIN_DECLS

/* Cookie permission manager enums */
typedef enum
{
	COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED,
	COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT,
	COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT_FOR_SESSION,
	COOKIE_PERMISSION_MANAGER_POLICY_BLOCK
} CookiePermissionManagerPolicy;

/* Cookie permission manager object */
#define TYPE_COOKIE_PERMISSION_MANAGER				(cookie_permission_manager_get_type())
#define COOKIE_PERMISSION_MANAGER(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_COOKIE_PERMISSION_MANAGER, CookiePermissionManager))
#define IS_COOKIE_PERMISSION_MANAGER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_COOKIE_PERMISSION_MANAGER))
#define COOKIE_PERMISSION_MANAGER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), TYPE_COOKIE_PERMISSION_MANAGER, CookiePermissionManagerClass))
#define IS_COOKIE_PERMISSION_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_COOKIE_PERMISSION_MANAGER))
#define COOKIE_PERMISSION_MANAGER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_COOKIE_PERMISSION_MANAGER, CookiePermissionManagerClass))

typedef struct _CookiePermissionManager				CookiePermissionManager;
typedef struct _CookiePermissionManagerClass		CookiePermissionManagerClass;
typedef struct _CookiePermissionManagerPrivate		CookiePermissionManagerPrivate;

struct _CookiePermissionManager
{
	/* Parent instance */
	GObject							parent_instance;

	/* Private structure */
	CookiePermissionManagerPrivate	*priv;
};

struct _CookiePermissionManagerClass
{
	/* Parent class */
	GObjectClass					parent_class;
};

/* Public API */
GType cookie_permission_manager_get_type(void);

CookiePermissionManager* cookie_permission_manager_new(MidoriExtension *inExtension, MidoriApp *inApp);

gboolean cookie_permission_manager_get_ask_for_unknown_policy(CookiePermissionManager *self);
void cookie_permission_manager_set_ask_for_unknown_policy(CookiePermissionManager *self, gboolean inDoAsk);

/* Enumeration */
GType cookie_permission_manager_policy_get_type(void) G_GNUC_CONST;
#define COOKIE_PERMISSION_MANAGER_TYPE_POLICY	(cookie_permission_manager_policy_get_type())

G_END_DECLS

#endif /* __COOKIE_PERMISSION_MANAGER__ */
