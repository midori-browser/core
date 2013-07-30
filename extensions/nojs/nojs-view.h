/*
 Copyright (C) 2013 Stephan Haller <nomad@froevel.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __NOJS_VIEW__
#define __NOJS_VIEW__

#include "config.h"
#include "nojs.h"
#include <midori/midori.h>

G_BEGIN_DECLS

/* NoJS view enums */
typedef enum
{
	NOJS_MENU_ICON_STATE_UNDETERMINED,
	NOJS_MENU_ICON_STATE_ALLOWED,
	NOJS_MENU_ICON_STATE_MIXED,
	NOJS_MENU_ICON_STATE_DENIED
} NoJSMenuIconState;

/* NoJS view object */
#define TYPE_NOJS_VIEW				(nojs_view_get_type())
#define NOJS_VIEW(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_NOJS_VIEW, NoJSView))
#define NOJS_IS_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_NOJS_VIEW))
#define NOJS_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), TYPE_NOJS_VIEW, NoJSViewClass))
#define NOJS_IS_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_NOJS_VIEW))
#define NOJS_VIEW_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_NOJS_VIEW, NoJSViewClass))

typedef struct _NoJSView			NoJSView;
typedef struct _NoJSViewClass		NoJSViewClass;
typedef struct _NoJSViewPrivate		NoJSViewPrivate;

struct _NoJSView
{
	/* Parent instance */
	GObject				parent_instance;

	/* Private structure */
	NoJSViewPrivate		*priv;
};

struct _NoJSViewClass
{
	/* Parent class */
	GObjectClass		parent_class;
};

/* Public API */
GType nojs_view_get_type(void);

NoJSView* nojs_view_new(NoJS *inNoJS, MidoriBrowser *inBrowser, MidoriView *inView);

GtkMenu* nojs_view_get_menu(NoJSView *self);
NoJSMenuIconState nojs_view_get_menu_icon_state(NoJSView *self);

/* Enumeration */
GType nojs_menu_icon_state_get_type(void) G_GNUC_CONST;
#define NOJS_TYPE_MENU_ICON_STATE	(nojs_menu_icon_state_get_type())

G_END_DECLS

#endif /* __NOJS_VIEW__ */
