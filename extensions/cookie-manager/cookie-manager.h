/*
 Copyright (C) 2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/


#ifndef __COOKIE_MANAGER_H__
#define __COOKIE_MANAGER_H__

G_BEGIN_DECLS

#define STOCK_COOKIE_MANAGER "cookie-manager"

enum
{
	COOKIE_MANAGER_COL_NAME,
	COOKIE_MANAGER_COL_COOKIE,
	COOKIE_MANAGER_COL_VISIBLE,
	COOKIE_MANAGER_N_COLUMNS
};


#define COOKIE_MANAGER_TYPE				(cookie_manager_get_type())
#define COOKIE_MANAGER(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			COOKIE_MANAGER_TYPE, CookieManager))
#define COOKIE_MANAGER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			COOKIE_MANAGER_TYPE, CookieManagerClass))
#define IS_COOKIE_MANAGER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			COOKIE_MANAGER_TYPE))
#define IS_COOKIE_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			COOKIE_MANAGER_TYPE))

typedef struct _CookieManager			CookieManager;
typedef struct _CookieManagerClass		CookieManagerClass;

GType			cookie_manager_get_type		(void);
CookieManager*	cookie_manager_new			(MidoriExtension *extension, MidoriApp *app);

void			cookie_manager_delete_cookie(CookieManager *cm, SoupCookie *cookie);
void			cookie_manager_update_filter(CookieManager *cm, const gchar *text);

G_END_DECLS

#endif /* __COOKIE-MANAGER_H__ */
