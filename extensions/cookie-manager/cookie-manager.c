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
#include "katze/katze.h"

#include "cookie-manager.h"
#include "cookie-manager-page.h"

typedef struct _CookieManagerPrivate			CookieManagerPrivate;

struct _CookieManager
{
	GObject parent;
	CookieManagerPrivate* priv;
};

struct _CookieManagerClass
{
	GObjectClass parent_class;
};

struct _CookieManagerPrivate
{
	MidoriApp *app;
	MidoriExtension *extension;

	GSList *panel_pages;

	GtkTreeStore *store;
	GSList *cookies;
	SoupCookieJar *jar;
	guint timer_id;
	gint ignore_changed_count;

	gchar *filter_text;
};

static void cookie_manager_finalize(GObject *object);
static void cookie_manager_app_add_browser_cb(MidoriApp *app, MidoriBrowser *browser,
											  CookieManager *cm);

enum
{
	COOKIES_CHANGED,
	PRE_COOKIES_CHANGE,
	FILTER_CHANGED,

	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];


G_DEFINE_TYPE(CookieManager, cookie_manager, G_TYPE_OBJECT);


static void cookie_manager_class_init(CookieManagerClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);

	g_object_class->finalize = cookie_manager_finalize;

	signals[COOKIES_CHANGED] = g_signal_new(
		"cookies-changed",
		G_TYPE_FROM_CLASS(klass),
		(GSignalFlags) 0,
		0,
		0,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[PRE_COOKIES_CHANGE] = g_signal_new(
		"pre-cookies-change",
		G_TYPE_FROM_CLASS(klass),
		(GSignalFlags) 0,
		0,
		0,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[FILTER_CHANGED] = g_signal_new(
		"filter-changed",
		G_TYPE_FROM_CLASS(klass),
		(GSignalFlags) 0,
		0,
		0,
		NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1, G_TYPE_STRING);

	g_type_class_add_private(klass, sizeof(CookieManagerPrivate));
}


static void cookie_manager_panel_pages_foreach(gpointer ptr, gpointer data)
{
	if (ptr != NULL && GTK_IS_WIDGET(ptr))
		gtk_widget_destroy(GTK_WIDGET(ptr));
}


static void cookie_manager_page_destroy_cb(GObject *page, CookieManager *cm)
{
	CookieManagerPrivate *priv = cm->priv;

	priv->panel_pages = g_slist_remove(priv->panel_pages, page);
}


static void cookie_manager_app_add_browser_cb(MidoriApp *app, MidoriBrowser *browser,
											  CookieManager *cm)
{
	MidoriPanel *panel;
	GtkWidget *page;
	CookieManagerPrivate *priv = cm->priv;

	panel = katze_object_get_object(browser, "panel");

	page = cookie_manager_page_new(cm, priv->store, priv->filter_text);
	gtk_widget_show(page);
	midori_panel_append_page(panel, MIDORI_VIEWABLE(page));
	g_signal_connect(page, "destroy", G_CALLBACK(cookie_manager_page_destroy_cb), cm);

	priv->panel_pages = g_slist_append(priv->panel_pages, page);

	g_object_unref(panel);
}


static void cookie_manager_free_cookie_list(CookieManager *cm)
{
	CookieManagerPrivate *priv = cm->priv;

	if (priv->cookies != NULL)
	{
		GSList *l;

		for (l = priv->cookies; l != NULL; l = g_slist_next(l))
			soup_cookie_free(l->data);
		g_slist_free(priv->cookies);
		priv->cookies = NULL;
	}
}


static void cookie_manager_refresh_store(CookieManager *cm)
{
	GSList *l;
	GHashTable *parents;
	GtkTreeIter iter;
	GtkTreeIter *parent_iter;
	SoupCookie *cookie;
	CookieManagerPrivate *priv = cm->priv;

	g_signal_emit(cm, signals[PRE_COOKIES_CHANGE], 0);

	gtk_tree_store_clear(priv->store);

	/* free the old list */
	cookie_manager_free_cookie_list(cm);

	priv->cookies = soup_cookie_jar_all_cookies(priv->jar);

	/* Hashtable holds domain names as keys, the corresponding tree iters as values */
	parents = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	for (l = priv->cookies; l != NULL; l = g_slist_next(l))
	{
		cookie = l->data;

		/* look for the parent item for the current domain name and create it if it doesn't exist */
		if ((parent_iter = (GtkTreeIter*) g_hash_table_lookup(parents, cookie->domain)) == NULL)
		{
			parent_iter = g_new0(GtkTreeIter, 1);

			gtk_tree_store_append(priv->store, parent_iter, NULL);
			gtk_tree_store_set(priv->store, parent_iter,
				COOKIE_MANAGER_COL_NAME, cookie->domain,
				COOKIE_MANAGER_COL_COOKIE, NULL,
				COOKIE_MANAGER_COL_VISIBLE, TRUE,
				-1);

			g_hash_table_insert(parents, g_strdup(cookie->domain), parent_iter);
		}

		gtk_tree_store_append(priv->store, &iter, parent_iter);
		gtk_tree_store_set(priv->store, &iter,
			COOKIE_MANAGER_COL_NAME, cookie->name,
			COOKIE_MANAGER_COL_COOKIE, cookie,
			COOKIE_MANAGER_COL_VISIBLE, TRUE,
			-1);
	}
	g_hash_table_destroy(parents);

	g_signal_emit(cm, signals[COOKIES_CHANGED], 0);
}


static gboolean cookie_manager_delayed_refresh(CookieManager *cm)
{
	CookieManagerPrivate *priv = cm->priv;

	cookie_manager_refresh_store(cm);
	priv->timer_id = 0;

	return FALSE;
}


static void cookie_manager_jar_changed_cb(SoupCookieJar *jar, SoupCookie *old, SoupCookie *new,
							  CookieManager *cm)
{
	CookieManagerPrivate *priv = cm->priv;

	if (priv->ignore_changed_count > 0)
	{
		priv->ignore_changed_count--;
		return;
	}

	/* We delay these events a little bit to avoid too many rebuilds of the tree.
	 * Some websites (like Flyspray bugtrackers sent a whole bunch of cookies at once. */
	if (priv->timer_id == 0)
		priv->timer_id = midori_timeout_add_seconds(
			1, (GSourceFunc) cookie_manager_delayed_refresh, cm, NULL);
}


static void cookie_manager_finalize(GObject *object)
{
	CookieManager *cm = COOKIE_MANAGER(object);
	CookieManagerPrivate *priv = cm->priv;

	g_signal_handlers_disconnect_by_func(priv->app, cookie_manager_app_add_browser_cb, cm);
	g_signal_handlers_disconnect_by_func(priv->jar, cookie_manager_jar_changed_cb, cm);

	/* remove all panel pages from open windows */
	g_slist_foreach(priv->panel_pages, cookie_manager_panel_pages_foreach, NULL);
	g_slist_free(priv->panel_pages);

	/* clean cookies */
	if (priv->timer_id > 0)
		g_source_remove(priv->timer_id);

	cookie_manager_free_cookie_list(cm);

	g_object_unref(priv->store);
	g_free(priv->filter_text);

	G_OBJECT_CLASS(cookie_manager_parent_class)->finalize(object);
}


static void cookie_manager_init(CookieManager *self)
{
	CookieManagerPrivate *priv;
	SoupSession *session;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
	    COOKIE_MANAGER_TYPE, CookieManagerPrivate);
	priv = self->priv;
	/* create the main store */
	priv->store = gtk_tree_store_new(COOKIE_MANAGER_N_COLUMNS,
		G_TYPE_STRING, SOUP_TYPE_COOKIE, G_TYPE_BOOLEAN);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(priv->store),
		COOKIE_MANAGER_COL_NAME, GTK_SORT_ASCENDING);

	/* setup soup */
	session = webkit_get_default_session();
	priv->jar = SOUP_COOKIE_JAR(soup_session_get_feature(session, soup_cookie_jar_get_type()));
	g_signal_connect(priv->jar, "changed", G_CALLBACK(cookie_manager_jar_changed_cb), self);

	cookie_manager_refresh_store(self);
}


void cookie_manager_update_filter(CookieManager *cm, const gchar *text)
{
	CookieManagerPrivate *priv = cm->priv;

	katze_assign(priv->filter_text, g_strdup(text));

	g_signal_emit(cm, signals[FILTER_CHANGED], 0, text);
}


void cookie_manager_delete_cookie(CookieManager *cm, SoupCookie *cookie)
{
	CookieManagerPrivate *priv = cm->priv;

	if (cookie != NULL)
	{
		priv->ignore_changed_count++;

		soup_cookie_jar_delete_cookie(priv->jar, cookie);
		/* the SoupCookie object is freed when the whole list gets updated */
	}
}


CookieManager *cookie_manager_new(MidoriExtension *extension, MidoriApp *app)
{
	CookieManager *cm;
	CookieManagerPrivate *priv;
	KatzeArray *browsers;
	MidoriBrowser *browser;

	cm = g_object_new(COOKIE_MANAGER_TYPE, NULL);

	priv = cm->priv;
	priv->app = app;
	priv->extension = extension;

	/* add the cookie manager panel page to existing browsers */
	browsers = katze_object_get_object(app, "browsers");
	KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
		cookie_manager_app_add_browser_cb(app, browser, cm);
	g_object_unref(browsers);

	g_signal_connect(app, "add-browser", G_CALLBACK(cookie_manager_app_add_browser_cb), cm);

	return cm;
}

