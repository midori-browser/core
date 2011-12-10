/*
 Copyright (C) 2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __COOKIE_MANAGER_PAGE_H__
#define __COOKIE_MANAGER_PAGE_H__

G_BEGIN_DECLS

#define COOKIE_MANAGER_PAGE_TYPE				(cookie_manager_page_get_type())
#define COOKIE_MANAGER_PAGE(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			COOKIE_MANAGER_PAGE_TYPE, CookieManagerPage))
#define COOKIE_MANAGER_PAGE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			COOKIE_MANAGER_PAGE_TYPE, CookieManagerPageClass))
#define IS_COOKIE_MANAGER_PAGE(obj)				(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			COOKIE_MANAGER_PAGE_TYPE))
#define IS_COOKIE_MANAGER_PAGE_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass),\
			COOKIE_MANAGER_PAGE_TYPE))

typedef struct _CookieManagerPage				CookieManagerPage;
typedef struct _CookieManagerPageClass			CookieManagerPageClass;
typedef struct _CookieManagerPagePrivate			CookieManagerPagePrivate;

struct _CookieManagerPage
{
	GtkVBox parent;
	CookieManagerPagePrivate* priv;
};

struct _CookieManagerPageClass
{
	GtkVBoxClass parent_class;
};

GType		cookie_manager_page_get_type		(void);
GtkWidget*	cookie_manager_page_new				(CookieManager *parent,
												 GtkTreeStore *store,
												 const gchar *filter_text);

G_END_DECLS

#endif /* __COOKIE_MANAGER_PAGE_H__ */
