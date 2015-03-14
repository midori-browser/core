/*
 Copyright (C) 2013 Stephan Haller <nomad@froevel.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __NOJS__
#define __NOJS__

#include "config.h"
#include <midori/midori.h>

#define NOJS_DATABASE	"nojs.db"

G_BEGIN_DECLS

/* NoJS manager enums */
typedef enum
{
	NOJS_POLICY_UNDETERMINED,
	NOJS_POLICY_ACCEPT,
	NOJS_POLICY_ACCEPT_TEMPORARILY,
	NOJS_POLICY_BLOCK
} NoJSPolicy;

/* NoJS manager object */
#define TYPE_NOJS				(nojs_get_type())
#define NOJS(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_NOJS, NoJS))
#define IS_NOJS(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_NOJS))
#define NOJS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), TYPE_NOJS, NoJSClass))
#define IS_NOJS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_NOJS))
#define NOJS_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_NOJS, NoJSClass))

typedef struct _NoJS			NoJS;
typedef struct _NoJSClass		NoJSClass;
typedef struct _NoJSPrivate		NoJSPrivate;

struct _NoJS
{
	/* Parent instance */
	GObject			parent_instance;

	/* Private structure */
	NoJSPrivate		*priv;
};

struct _NoJSClass
{
	/* Parent class */
	GObjectClass	parent_class;

	/* Virtual functions */
	void (*uri_load_policy_status)(NoJS *self, gchar *inURI, NoJSPolicy inPolicy);
	void (*policy_changed)(NoJS *self, gchar *inDomain);
};

/* Public API */
GType nojs_get_type(void);

NoJS* nojs_new(MidoriExtension *inExtension, MidoriApp *inApp);

gchar* nojs_get_domain(NoJS *self, SoupURI *inURI);

gint nojs_get_policy(NoJS *self, SoupURI *inURI);
void nojs_set_policy(NoJS *self, const gchar *inDomain, NoJSPolicy inPolicy);

NoJSPolicy nojs_get_policy_for_unknown_domain(NoJS *self);
void nojs_set_policy_for_unknown_domain(NoJS *self, NoJSPolicy inPolicy);

gboolean nojs_get_allow_local_pages(NoJS *self);
void nojs_set_allow_local_pages(NoJS *self, gboolean inAllow);

gboolean nojs_get_only_second_level_domain(NoJS *self);
void nojs_set_only_second_level_domain(NoJS *self, gboolean inOnlySecondLevel);

gchar* nojs_get_icon_path (const gchar* icon);

/* Enumeration */
GType nojs_policy_get_type(void) G_GNUC_CONST;
#define NOJS_TYPE_POLICY	(nojs_policy_get_type())

G_END_DECLS

#endif /* __NOJS__ */
