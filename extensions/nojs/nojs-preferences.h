/*
 Copyright (C) 2013 Stephan Haller <nomad@froevel.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __NOJS_PREFERENCES__
#define __NOJS_PREFERENCES__

#include "config.h"
#include <midori/midori.h>

#include "nojs.h"

G_BEGIN_DECLS

#define TYPE_NOJS_PREFERENCES				(nojs_preferences_get_type())
#define NOJS_PREFERENCES(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_NOJS_PREFERENCES, NoJSPreferences))
#define IS_NOJS_PREFERENCES(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_NOJS_PREFERENCES))
#define NOJS_PREFERENCES_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), TYPE_NOJS_PREFERENCES, NoJSPreferencesClass))
#define IS_NOJS_PREFERENCES_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_NOJS_PREFERENCES))
#define NOJS_PREFERENCES_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_NOJS_PREFERENCES, NoJSPreferencesClass))

typedef struct _NoJSPreferences				NoJSPreferences;
typedef struct _NoJSPreferencesClass		NoJSPreferencesClass;
typedef struct _NoJSPreferencesPrivate		NoJSPreferencesPrivate;

struct _NoJSPreferences
{
	/* Parent instance */
	GtkDialog				parent_instance;

	/* Private structure */
	NoJSPreferencesPrivate	*priv;
};

struct _NoJSPreferencesClass
{
	/* Parent class */
	GtkDialogClass			parent_class;
};

/* Public API */
GType nojs_preferences_get_type(void);

GtkWidget* nojs_preferences_new(NoJS *inManager);

G_END_DECLS

#endif /* __NOJS_PREFERENCES__ */
