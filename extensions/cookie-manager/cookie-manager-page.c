/*
 Copyright (C) 2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <gdk/gdkkeysyms.h>

#include <midori/midori.h>
#include <webkit/webkit.h>
#include <time.h>

#include "cookie-manager.h"
#include "cookie-manager-page.h"

#define CM_EMPTY_LABEL_TEXT "\n\n\n\n\n\n"


struct _CookieManagerPagePrivate
{
	CookieManager *parent;

	GtkWidget *treeview;
	GtkTreeStore *store;
	GtkTreeModel *filter;

	GtkWidget *filter_entry;
	gboolean ignore_changed_filter;

	GtkWidget *desc_label;
	GtkWidget *delete_button;
	GtkWidget *delete_popup_button;
	GtkWidget *delete_all_button;
	GtkWidget *expand_buttons[4];

	GtkWidget *toolbar;
	GtkWidget *popup_menu;
};

enum
{
	PROP_0,
	PROP_STORE,
	PROP_PARENT
};


static void cookie_manager_page_finalize(GObject *object);
static void cookie_manager_page_viewable_iface_init(MidoriViewableIface *iface);

static void cm_button_delete_clicked_cb(GtkToolButton *button, CookieManagerPage *cmp);
static void cm_button_delete_all_clicked_cb(GtkToolButton *button, CookieManagerPage *cmp);
static void cm_tree_popup_collapse_activate_cb(GtkMenuItem *item, CookieManagerPage *cmp);
static void cm_tree_popup_expand_activate_cb(GtkMenuItem *item, CookieManagerPage *cmp);
static void cm_filter_tree(CookieManagerPage *cmp, const gchar *filter_text);

typedef void (*CMPathWalkFunc) (GtkTreePath *path);


G_DEFINE_TYPE_WITH_CODE(CookieManagerPage, cookie_manager_page, GTK_TYPE_VBOX,
						G_IMPLEMENT_INTERFACE(MIDORI_TYPE_VIEWABLE,
						cookie_manager_page_viewable_iface_init));



static const gchar *cookie_manager_page_get_label(MidoriViewable *viewable)
{
	return _("Cookie Manager");
}


static const gchar *cookie_manager_page_get_stock_id(MidoriViewable *viewable)
{
	return GTK_STOCK_DIALOG_AUTHENTICATION;
}


static void cm_create_toolbar(CookieManagerPage *cmp)
{
	CookieManagerPagePrivate *priv = cmp->priv;
	GtkWidget *toolbar;
	GtkToolItem *toolitem;

	priv->toolbar = toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_BUTTON);
	gtk_widget_show(toolbar);

	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_tool_item_set_is_important(toolitem, TRUE);
	g_signal_connect(toolitem, "clicked", G_CALLBACK(cm_button_delete_clicked_cb), cmp);
	gtk_widget_show(GTK_WIDGET(toolitem));
	gtk_widget_set_sensitive(GTK_WIDGET(toolitem), FALSE);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
	priv->delete_button = GTK_WIDGET(toolitem);

	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(toolitem), _("Delete All"));
	gtk_tool_item_set_tooltip_text(toolitem,
		_("Deletes all shown cookies. "
		  "If a filter is set, only those cookies are deleted which match the filter."));
	gtk_tool_item_set_is_important(toolitem, TRUE);
	g_signal_connect(toolitem, "clicked", G_CALLBACK(cm_button_delete_all_clicked_cb), cmp);
	gtk_widget_show(GTK_WIDGET(toolitem));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
	priv->delete_all_button = GTK_WIDGET(toolitem);

	toolitem = gtk_separator_tool_item_new();
	gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(toolitem), FALSE);
	gtk_tool_item_set_expand(toolitem, TRUE);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
	gtk_widget_show(GTK_WIDGET(toolitem));

	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_ADD);
	gtk_tool_item_set_tooltip_text(toolitem, _("Expand All"));
	g_signal_connect(toolitem, "clicked", G_CALLBACK(cm_tree_popup_expand_activate_cb), cmp);
	gtk_widget_show(GTK_WIDGET(toolitem));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
	priv->expand_buttons[0] = GTK_WIDGET(toolitem);

	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_REMOVE);
	gtk_tool_item_set_tooltip_text(toolitem, _("Collapse All"));
	g_signal_connect(toolitem, "clicked", G_CALLBACK(cm_tree_popup_collapse_activate_cb), cmp);
	gtk_widget_show(GTK_WIDGET(toolitem));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
	priv->expand_buttons[1] = GTK_WIDGET(toolitem);
}


static GtkWidget *cookie_manager_page_get_toolbar(MidoriViewable *viewable)
{
	CookieManagerPage *cmp = COOKIE_MANAGER_PAGE(viewable);
	CookieManagerPagePrivate *priv = cmp->priv;

	return priv->toolbar;
}


static void cookie_manager_page_viewable_iface_init(MidoriViewableIface* iface)
{
	iface->get_stock_id = cookie_manager_page_get_stock_id;
	iface->get_label = cookie_manager_page_get_label;
	iface->get_toolbar = cookie_manager_page_get_toolbar;
}


static void cookie_manager_page_pre_cookies_change_cb(CookieManager *cm, CookieManagerPage *cmp)
{
	CookieManagerPagePrivate *priv = cmp->priv;

	g_object_ref(priv->filter);
	gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview), NULL);
}


static void cookie_manager_page_cookies_changed_cb(CookieManager *cm, CookieManagerPage *cmp)
{
	const gchar *filter_text;
	CookieManagerPagePrivate *priv = cmp->priv;

	gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview), GTK_TREE_MODEL(priv->filter));
	g_object_unref(priv->filter);

	/* if a filter is set, apply it again */
	filter_text = gtk_entry_get_text(GTK_ENTRY(priv->filter_entry));
	if (*filter_text != '\0')
	{
		cm_filter_tree(cmp, filter_text);
		gtk_tree_view_expand_all(GTK_TREE_VIEW(priv->treeview));
	}
}


static void cookie_manager_page_filter_changed_cb(CookieManager *cm, const gchar *text,
												  CookieManagerPage *cmp)
{
	CookieManagerPagePrivate *priv = cmp->priv;

	priv->ignore_changed_filter = TRUE;
	gtk_entry_set_text(GTK_ENTRY(priv->filter_entry), text);
	priv->ignore_changed_filter = FALSE;
}


static void cookie_manager_page_finalize(GObject *object)
{
	CookieManagerPage *cmp = COOKIE_MANAGER_PAGE(object);
	CookieManagerPagePrivate *priv = cmp->priv;

	gtk_widget_destroy(priv->popup_menu);

	g_signal_handlers_disconnect_by_func(priv->parent,
		cookie_manager_page_pre_cookies_change_cb, object);
	g_signal_handlers_disconnect_by_func(priv->parent,
		cookie_manager_page_cookies_changed_cb, object);
	g_signal_handlers_disconnect_by_func(priv->parent,
		cookie_manager_page_filter_changed_cb, object);

	G_OBJECT_CLASS(cookie_manager_page_parent_class)->finalize(object);
}


static void cookie_manager_page_set_property(GObject *object, guint prop_id, const GValue *value,
											 GParamSpec *pspec)
{
	CookieManagerPage *cmp = COOKIE_MANAGER_PAGE(object);
	CookieManagerPagePrivate *priv = cmp->priv;
	switch (prop_id)
	{
		case PROP_STORE:
		{
			priv->store = g_value_get_object(value);

			/* setting filter and model */
			priv->filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(priv->store), NULL);
			gtk_tree_model_filter_set_visible_column(GTK_TREE_MODEL_FILTER(priv->filter),
				COOKIE_MANAGER_COL_VISIBLE);
			gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview), GTK_TREE_MODEL(priv->filter));
			g_object_unref(priv->filter);
			break;
		}
		case PROP_PARENT:
		{
			if (priv->parent != NULL)
			{
				g_signal_handlers_disconnect_by_func(priv->parent,
					cookie_manager_page_pre_cookies_change_cb, object);
				g_signal_handlers_disconnect_by_func(priv->parent,
					cookie_manager_page_cookies_changed_cb, object);
				g_signal_handlers_disconnect_by_func(priv->parent,
					cookie_manager_page_filter_changed_cb, object);
			}
			priv->parent = g_value_get_object(value);

			g_signal_connect(priv->parent, "pre-cookies-change",
				G_CALLBACK(cookie_manager_page_pre_cookies_change_cb), object);
			g_signal_connect(priv->parent, "cookies-changed",
				G_CALLBACK(cookie_manager_page_cookies_changed_cb), object);
			g_signal_connect(priv->parent, "filter-changed",
				G_CALLBACK(cookie_manager_page_filter_changed_cb), object);
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}


static void cookie_manager_page_class_init(CookieManagerPageClass *klass)
{
	GObjectClass *g_object_class;
	g_object_class = G_OBJECT_CLASS(klass);

	g_object_class->finalize = cookie_manager_page_finalize;
	g_object_class->set_property = cookie_manager_page_set_property;

	g_object_class_install_property(g_object_class,
		PROP_STORE,
		g_param_spec_object(
		"store",
		"Treestore",
		"The tree store",
		GTK_TYPE_TREE_STORE,
		G_PARAM_WRITABLE));

	g_object_class_install_property(g_object_class,
		PROP_PARENT,
		g_param_spec_object(
		"parent",
		"Parent",
		"The CookieManager parent instance",
		COOKIE_MANAGER_TYPE,
		G_PARAM_WRITABLE));

	g_type_class_add_private(klass, sizeof(CookieManagerPagePrivate));
}


static void cm_set_button_sensitiveness(CookieManagerPage *cmp, gboolean set)
{
	CookieManagerPagePrivate *priv = cmp->priv;
	gboolean expand_set = (gtk_tree_model_iter_n_children(priv->filter, NULL) > 0);
	guint i, len;

	gtk_widget_set_sensitive(priv->delete_popup_button, set);
	gtk_widget_set_sensitive(priv->delete_button, set);

	gtk_widget_set_sensitive(priv->delete_all_button, expand_set);
	len = G_N_ELEMENTS(priv->expand_buttons);
	for (i = 0; i < len; i++)
	{
		gtk_widget_set_sensitive(priv->expand_buttons[i], expand_set);
	}
}


static void cm_free_selection_list(GList *rows, GFunc func)
{
	g_list_foreach(rows, func, NULL);
	g_list_free(rows);
}


/* Fast version of g_list_length(). It only checks for the first few elements of
 * the list and returns the length 0, 1 or 2 where 2 means 2 elements or more. */
static gint cm_list_length(GList *list)
{
	if (list == NULL)
		return 0;
	else if (list->next == NULL)
		return 1;
	else if (list->next != NULL)
		return 2;

	return 0; /* safe default */
}


static void cm_tree_popup_collapse_activate_cb(GtkMenuItem *item, CookieManagerPage *cmp)
{
	CookieManagerPagePrivate *priv = cmp->priv;

	gtk_tree_view_collapse_all(GTK_TREE_VIEW(priv->treeview));
}


static void cm_tree_popup_expand_activate_cb(GtkMenuItem *item, CookieManagerPage *cmp)
{
	CookieManagerPagePrivate *priv = cmp->priv;

	gtk_tree_view_expand_all(GTK_TREE_VIEW(priv->treeview));
}


static void cm_store_remove(CookieManagerPage *cmp, GtkTreeIter *iter_model)
{
	GtkTreeIter iter_store;
	CookieManagerPagePrivate *priv = cmp->priv;

	gtk_tree_model_filter_convert_iter_to_child_iter(
		GTK_TREE_MODEL_FILTER(priv->filter), &iter_store, iter_model);
	gtk_tree_store_remove(priv->store, &iter_store);
}


static void cm_delete_cookie(CookieManagerPage *cmp, GtkTreeModel *model, GtkTreeIter *child)
{
	SoupCookie *cookie;
	CookieManagerPagePrivate *priv = cmp->priv;

	gtk_tree_model_get(model, child, COOKIE_MANAGER_COL_COOKIE, &cookie, -1);

	cookie_manager_delete_cookie(priv->parent, cookie);
}


static gboolean cm_try_to_select(CMPathWalkFunc path_func, GtkTreeSelection *selection,
								 GtkTreeModel *model, GtkTreePath *path)
{
	GtkTreeIter iter;

	if (gtk_tree_path_get_depth(path) <= 0) /* sanity check */
		return FALSE;

	/* modify the path using the passed function */
	if (path_func != NULL)
		path_func(path);

	if (gtk_tree_path_get_depth(path) <= 0) /* sanity check */
		return FALSE;

	/* check whether the path points to something valid and if so, select it */
	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		GtkTreeView *treeview = gtk_tree_selection_get_tree_view(selection);
		gboolean was_expanded = gtk_tree_view_row_expanded(treeview, path);
		/* to get gtk_tree_selection_select_path() working, we need to expand the row first
		 * if it isn't expanded yet, at least when the row is a parent item */
		if (! was_expanded)
			gtk_tree_view_expand_to_path(treeview, path);

		gtk_tree_selection_select_path(selection, path);

		if (! was_expanded) /* restore the previous state */
			gtk_tree_view_collapse_row(treeview, path);
		return TRUE;
	}

	return FALSE;
}


/* select an item after deletion */
static void cm_select_path(CookieManagerPage *cmp, GtkTreeModel *model, GtkTreePath *path)
{
	CookieManagerPagePrivate *priv = cmp->priv;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview));
	CMPathWalkFunc path_funcs[] = {
		(CMPathWalkFunc) gtk_tree_path_prev, (CMPathWalkFunc) gtk_tree_path_up,
		(CMPathWalkFunc) gtk_tree_path_next, NULL };
	CMPathWalkFunc *path_func;

	/* first try selecting the item directly to which path points */
	if (! cm_try_to_select(NULL, selection, model, path))
	{	/* if this failed, modify the path until we found something valid */
		path_func = path_funcs;
		while (*path_func != NULL)
		{
			if (cm_try_to_select(*path_func, selection, model, path))
				break;
			path_func++;
		}
	}
}


static void cm_delete_item(CookieManagerPage *cmp)
{
	GtkTreeIter iter, iter_store, child;
	GtkTreeModel *model;
	GtkTreePath *path, *last_path;
	GtkTreeSelection *selection;
	GList *rows, *row;
	GList *refs = NULL;
	CookieManagerPagePrivate *priv = cmp->priv;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview));
	rows = gtk_tree_selection_get_selected_rows(selection, &model);
	if (cm_list_length(rows) == 0)
		return;

	last_path = gtk_tree_path_copy(g_list_nth_data(rows, 0));

	/* as paths will change during delete, first create GtkTreeRowReferences for
	 * all selected rows */
	row = rows;
	do
	{
		refs = g_list_append(refs, gtk_tree_row_reference_new(model, (GtkTreePath*) (row->data)));
	} while ((row = row->next) != NULL);

	row = refs;
	do
	{
		/* get iter */
		path = gtk_tree_row_reference_get_path((GtkTreeRowReference*) row->data);
		if (path == NULL)
			continue;
		gtk_tree_model_get_iter(model, &iter, path);

		if (gtk_tree_model_iter_has_child(model, &iter))
		{
			while (gtk_tree_model_iter_children(model, &child, &iter))
			{
				cm_delete_cookie(cmp, model, &child);
				cm_store_remove(cmp, &child);
				/* we retrieve again the iter at path because it got invalid by the delete operation */
				gtk_tree_model_get_iter(model, &iter, path);
			}
			/* remove/hide the parent */
			gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(priv->filter),
				&iter_store, &iter);
			if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(priv->store), &iter_store))
				gtk_tree_store_set(priv->store, &iter_store, COOKIE_MANAGER_COL_VISIBLE, FALSE, -1);
			else
				cm_store_remove(cmp, &iter);
		}
		else
		{
			GtkTreePath *path_store, *path_model;

			gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(priv->filter),
				&iter_store, &iter);
			path_store = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->store), &iter_store);
			path_model = gtk_tree_model_get_path(model, &iter);

			cm_delete_cookie(cmp, model, &iter);
			gtk_tree_store_remove(priv->store, &iter_store);

			/* check whether the parent still has children, otherwise delete it */
			if (gtk_tree_path_up(path_store))
			{
				gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->store), &iter_store, path_store);
				if (! gtk_tree_model_iter_has_child(GTK_TREE_MODEL(priv->store), &iter_store))
					/* remove the empty parent */
					gtk_tree_store_remove(priv->store, &iter_store);
			}
			/* now for the filter model */
			if (gtk_tree_path_up(path_model))
			{
				gtk_tree_model_get_iter(model, &iter, path_model);
				if (! gtk_tree_model_iter_has_child(model, &iter))
				{
					gtk_tree_model_filter_convert_iter_to_child_iter(
						GTK_TREE_MODEL_FILTER(priv->filter), &iter_store, &iter);
					/* hide the empty parent */
					gtk_tree_store_set(priv->store, &iter_store, COOKIE_MANAGER_COL_VISIBLE, FALSE, -1);
				}
			}
			gtk_tree_path_free(path_store);
			gtk_tree_path_free(path_model);
		}
		gtk_tree_path_free(path);
	} while ((row = row->next) != NULL);
	cm_free_selection_list(rows, (GFunc) gtk_tree_path_free);
	cm_free_selection_list(refs, (GFunc) gtk_tree_row_reference_free);

	cm_select_path(cmp, model, last_path);
	gtk_tree_path_free(last_path);
}


static void cm_button_delete_clicked_cb(GtkToolButton *button, CookieManagerPage *cmp)
{
	cm_delete_item(cmp);
}


static void cm_delete_all_cookies_real(CookieManagerPage *cmp)
{
	CookieManagerPagePrivate *priv = cmp->priv;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(priv->treeview));
	GtkTreeIter iter, iter_store, child;
	GtkTreePath *path_first, *path;

	path_first = gtk_tree_path_new_first();
	while (gtk_tree_model_get_iter(model, &iter, path_first))
	{
		path = gtk_tree_model_get_path(model, &iter);
		while (gtk_tree_model_iter_children(model, &child, &iter))
		{
			cm_delete_cookie(cmp, model, &child);
			cm_store_remove(cmp, &child);
			/* we retrieve again the iter at path because it got invalid by the delete operation */
			gtk_tree_model_get_iter(model, &iter, path);
		}
		gtk_tree_path_free(path);
		/* remove/hide the parent */
		gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(priv->filter),
			&iter_store, &iter);
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(priv->store), &iter_store))
			gtk_tree_store_set(priv->store, &iter_store, COOKIE_MANAGER_COL_VISIBLE, FALSE, -1);
		else
			cm_store_remove(cmp, &iter);
	}
	/* now that we deleted all matching cookies, we reset the filter */
	gtk_entry_set_text(GTK_ENTRY(priv->filter_entry), "");
	cm_set_button_sensitiveness(cmp, FALSE);

	cm_select_path(cmp, model, path_first);
	gtk_tree_path_free(path_first);
}


static void cm_button_delete_all_clicked_cb(GtkToolButton *button, CookieManagerPage *cmp)
{
	GtkWidget *dialog;
	const gchar *filter_text;
	MidoriBrowser *toplevel = midori_browser_get_for_widget(GTK_WIDGET(button));
	CookieManagerPagePrivate *priv = cmp->priv;

	dialog = gtk_message_dialog_new(GTK_WINDOW(toplevel),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_YES_NO,
		_("Do you really want to delete all cookies?"));

	gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
	/* steal Midori's icon :) */
	if (toplevel != NULL)
		gtk_window_set_icon_name(GTK_WINDOW(dialog), gtk_window_get_icon_name(GTK_WINDOW(toplevel)));

	filter_text = gtk_entry_get_text(GTK_ENTRY(priv->filter_entry));
	if (*filter_text != '\0')
	{
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
			_("Only cookies which match the filter will be deleted."));
	}

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES)
		cm_delete_all_cookies_real(cmp);

	gtk_widget_destroy(dialog);
}


static const gchar *cm_skip_leading_dot(const gchar *text)
{
	return (*text == '.') ? text + 1 : text;
}


static void cm_tree_drag_data_get_cb(GtkWidget *widget, GdkDragContext *drag_context,
									 GtkSelectionData *data, guint info, guint ltime,
									 CookieManagerPage *cmp)
{
	GtkTreeIter iter, iter_store;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GList *rows;
	CookieManagerPagePrivate *priv = cmp->priv;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview));
	rows = gtk_tree_selection_get_selected_rows(selection, &model);
	if (cm_list_length(rows) != 1)
	{
		cm_free_selection_list(rows, (GFunc) gtk_tree_path_free);
		return;
	}
	/* get iter */
	gtk_tree_model_get_iter(model, &iter, (GtkTreePath*) (g_list_nth_data(rows, 0)));

	gtk_tree_model_filter_convert_iter_to_child_iter(
		GTK_TREE_MODEL_FILTER(model), &iter_store, &iter);

	if (gtk_tree_store_iter_is_valid(priv->store, &iter_store))
	{
		SoupCookie *cookie;
		gchar *name;
		const gchar *text;

		gtk_tree_model_get(model, &iter,
			COOKIE_MANAGER_COL_NAME, &name,
			COOKIE_MANAGER_COL_COOKIE, &cookie,
			-1);

		if (name != NULL)
		{
			GtkTreeIter parent;
			/* get the name of the parent item which should be a domain item */
			if (cookie != NULL && gtk_tree_model_iter_parent(model, &parent, &iter))
			{
				g_free(name);
				gtk_tree_model_get(model, &parent, COOKIE_MANAGER_COL_NAME, &name, -1);
			}

			text = cm_skip_leading_dot(name);
			gtk_selection_data_set_text(data, text, -1);
		}
		g_free(name);
	}
}


static gchar *cm_get_cookie_description_text(SoupCookie *cookie)
{
	static gchar date_fmt[512];
	gchar *expires;
	gchar *text;
	time_t expiration_time;
	const struct tm *tm;

	g_return_val_if_fail(cookie != NULL, NULL);

	if (cookie->expires != NULL)
	{
		expiration_time = soup_date_to_time_t(cookie->expires);
		tm = localtime(&expiration_time);
		/* even if some gcc versions complain about "%c", there is nothing wrong with and here we
		 * want to use it */
		strftime(date_fmt, sizeof(date_fmt), "%c", tm);
		expires = date_fmt;
	}
	else
		expires = _("At the end of the session");

	text = g_markup_printf_escaped(
			_("<b>Host</b>: %s\n<b>Name</b>: %s\n<b>Value</b>: %s\n<b>Path</b>: %s\n"
			  "<b>Secure</b>: %s\n<b>Expires</b>: %s"),
			cookie->domain,
			cookie->name,
			cookie->value,
			cookie->path,
			cookie->secure ? _("Yes") : _("No"),
			expires);

	return text;
}


static gchar *cm_get_domain_description_text(const gchar *domain, gint cookie_count)
{
	gchar *str, *text;

	domain = cm_skip_leading_dot(domain);

	text = g_markup_printf_escaped(
		_("<b>Domain</b>: %s\n<b>Cookies</b>: %d"),
		domain, cookie_count);

	str = g_strconcat(text, "\n\n\n\n", NULL);

	g_free(text);

	return str;
}


static gboolean cm_tree_query_tooltip(GtkWidget *widget, gint x, gint y, gboolean keyboard_mode,
									  GtkTooltip *tooltip, CookieManagerPage *cmp)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_view_get_tooltip_context(GTK_TREE_VIEW(widget), &x, &y,
			keyboard_mode, &model, NULL, &iter))
	{
		gchar *tooltip_text;
		SoupCookie *cookie;

		gtk_tree_model_get(model, &iter, COOKIE_MANAGER_COL_COOKIE, &cookie, -1);

		if (cookie == NULL) /* not an item */
			return FALSE;

		tooltip_text = cm_get_cookie_description_text(cookie);

		gtk_tooltip_set_markup(tooltip, tooltip_text);

		g_free(tooltip_text);

		return TRUE;
	}

	return FALSE;
}

static gboolean cm_filter_match(const gchar *haystack, const gchar *needle)
{
	gchar *haystack_lowered, *needle_lowered;
	gboolean result;

	/* empty strings always match */
	if (haystack == NULL || needle == NULL || *needle == '\0')
		return TRUE;

	haystack_lowered = g_utf8_strdown(haystack, -1);
	needle_lowered = g_utf8_strdown(needle, -1);

	/* if one of both could not be converted into lower case, skip those */
	if (haystack_lowered == NULL || needle_lowered == NULL)
		return FALSE;

	result = (strstr(haystack_lowered, needle_lowered) != NULL);

	g_free(haystack_lowered);
	g_free(needle_lowered);

	return result;
}


static void cm_filter_tree(CookieManagerPage *cmp, const gchar *filter_text)
{
	GtkTreeIter iter, child;
	GtkTreeModel *model;
	gboolean show_child, show_parent;
	gboolean child_visible;
	gint i, n;
	gchar *name;
	gchar *domain;
	CookieManagerPagePrivate *priv = cmp->priv;

	model = GTK_TREE_MODEL(priv->store);
	if (! gtk_tree_model_get_iter_first(model, &iter))
		return;

	do
	{
		if (gtk_tree_model_iter_has_child(model, &iter))
		{
			child_visible = FALSE;

			gtk_tree_model_get(model, &iter, COOKIE_MANAGER_COL_NAME, &domain, -1);
			show_parent = cm_filter_match(domain, filter_text);
			g_free(domain);
			n = gtk_tree_model_iter_n_children(model, &iter);
			for (i = 0; i < n; i++)
			{
				gtk_tree_model_iter_nth_child(model, &child, &iter, i);

				gtk_tree_model_get(model, &child, COOKIE_MANAGER_COL_NAME, &name, -1);
				show_child = show_parent || cm_filter_match(name, filter_text);
				g_free(name);

				if (show_child)
					child_visible = TRUE;

				gtk_tree_store_set(priv->store, &child, COOKIE_MANAGER_COL_VISIBLE, show_child, -1);
			}
			gtk_tree_store_set(priv->store, &iter, COOKIE_MANAGER_COL_VISIBLE, child_visible, -1);
		}
	}
	while (gtk_tree_model_iter_next(model, &iter));
}


static void cm_filter_entry_changed_cb(GtkEditable *editable, CookieManagerPage *cmp)
{
	const gchar *text;
	CookieManagerPagePrivate *priv = cmp->priv;

	if (priv->ignore_changed_filter)
		return;

	if (!g_object_get_data (G_OBJECT (editable), "sokoke_has_default"))
		text = gtk_entry_get_text(GTK_ENTRY(editable));
	else
		text = NULL;
	cm_filter_tree(cmp, text);

	cookie_manager_update_filter(priv->parent, text);

	if (text && *text)
		gtk_tree_view_collapse_all(GTK_TREE_VIEW(priv->treeview));
	else
		gtk_tree_view_expand_all(GTK_TREE_VIEW(priv->treeview));
}

static void cm_tree_selection_changed_cb(GtkTreeSelection *selection, CookieManagerPage *cmp)
{
	GList *rows;
	GtkTreeIter iter, iter_store;
	GtkTreeModel *model;
	gchar *text, *name;
	gboolean valid = TRUE;
	gboolean delete_possible = TRUE;
	guint rows_len;
	SoupCookie *cookie;
	CookieManagerPagePrivate *priv = cmp->priv;

	rows = gtk_tree_selection_get_selected_rows(selection, &model);
	rows_len = cm_list_length(rows);
	if (rows_len == 0)
	{
		valid = FALSE;
		delete_possible = FALSE;
	}
	else if (rows_len == 1)
	{
		/* get iter */
		gtk_tree_model_get_iter(model, &iter, (GtkTreePath*) (g_list_nth_data(rows, 0)));

		gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model),
			&iter_store, &iter);
	}
	else
		valid = FALSE;

	if (valid && gtk_tree_store_iter_is_valid(priv->store, &iter_store))
	{
		gtk_tree_model_get(model, &iter, COOKIE_MANAGER_COL_COOKIE, &cookie, -1);
		if (cookie != NULL)
		{
			text = cm_get_cookie_description_text(cookie);

			gtk_label_set_markup(GTK_LABEL(priv->desc_label), text);

			g_free(text);
		}
		else
		{
			gtk_tree_model_get(model, &iter, COOKIE_MANAGER_COL_NAME, &name, -1);
			if (name != NULL)
			{
				gint cookie_count = gtk_tree_model_iter_n_children(model, &iter);

				text = cm_get_domain_description_text(name, cookie_count);
				gtk_label_set_markup(GTK_LABEL(priv->desc_label), text);

				g_free(text);
				g_free(name);
			}
		}
	}
	/* This is a bit hack'ish but we add some empty lines to get a minimum height of the
	 * label at the bottom without any font size calculation. */
	if (! valid)
		gtk_label_set_text(GTK_LABEL(priv->desc_label), CM_EMPTY_LABEL_TEXT);
	cm_set_button_sensitiveness(cmp, delete_possible);

	cm_free_selection_list(rows, (GFunc) gtk_tree_path_free);
}


static void cm_tree_show_popup_menu(GtkWidget *widget, GdkEventButton *event, CookieManagerPage *cmp)
{
	gint button, event_time;
	CookieManagerPagePrivate *priv = cmp->priv;

	if (event != NULL)
	{
		button = event->button;
		event_time = event->time;
	}
	else
	{
		button = 0;
		event_time = gtk_get_current_event_time ();
	}

	gtk_menu_popup(GTK_MENU(priv->popup_menu), NULL, NULL, NULL, NULL, button, event_time);
}


static gboolean  cm_tree_popup_menu_cb(GtkWidget *widget, CookieManagerPage *cmp)
{
	cm_tree_show_popup_menu(widget, NULL, cmp);
	return TRUE;
}


static gboolean cm_tree_button_release_event_cb(GtkWidget *widget, GdkEventButton *ev,
												CookieManagerPage *cmp)
{
	if (MIDORI_EVENT_CONTEXT_MENU(ev))
	{
		cm_tree_show_popup_menu(widget, ev, cmp);
		return TRUE;
	}
	return FALSE;
}


static gboolean cm_tree_key_press_cb(GtkWidget *widget, GdkEventKey *event, CookieManagerPage *cmp)
{
	if (event->keyval == GDK_KEY_Delete && !
		(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)))
	{
		cm_delete_item(cmp);
		return TRUE;
	}
	return FALSE;
}


static gboolean cm_tree_button_press_event_cb(GtkWidget *widget, GdkEventButton *ev,
											  CookieManagerPage *cmp)
{
	gboolean ret = FALSE;

	if (ev->type == GDK_2BUTTON_PRESS)
	{
		GtkTreeSelection *selection;
		GtkTreeModel *model;
		GtkTreeIter iter;
		GList *rows;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		rows = gtk_tree_selection_get_selected_rows(selection, &model);
		if (cm_list_length(rows) == 1)
		{
			/* get iter */
			gtk_tree_model_get_iter(model, &iter, (GtkTreePath*) (g_list_nth_data(rows, 0)));
			/* double click on parent node expands/collapses it */
			if (gtk_tree_model_iter_has_child(model, &iter))
			{
				GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

				if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
					gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
				else
					gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);

				gtk_tree_path_free(path);

				ret = TRUE;
			}
		}
		cm_free_selection_list(rows, (GFunc) gtk_tree_path_free);
	}
	return ret;
}


static void cm_tree_render_text_cb(GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model,
								   GtkTreeIter *iter, gpointer data)
{
	gchar *name;

	gtk_tree_model_get(model, iter, COOKIE_MANAGER_COL_NAME, &name, -1);

	if (name != NULL && *name != '.')
	{
		gchar *display_name = g_strconcat(" ", name, NULL);
		g_object_set(renderer, "text", display_name, NULL);
		g_free(display_name);
	}
	else
		g_object_set(renderer, "text", name, NULL);
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	g_free(name);
}


static GtkWidget *cm_tree_prepare(CookieManagerPage *cmp)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *sel;
	GtkWidget *item;
	GtkWidget *menu;
	GtkWidget *treeview;
	CookieManagerPagePrivate *priv = cmp->priv;

	treeview = priv->treeview = gtk_tree_view_new();

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
		_("Name"), renderer, "text", COOKIE_MANAGER_COL_NAME, NULL);
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_sort_indicator(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, COOKIE_MANAGER_COL_NAME);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_cell_data_func(column, renderer,
        (GtkTreeCellDataFunc) cm_tree_render_text_cb, NULL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), COOKIE_MANAGER_COL_NAME);

	/* selection handling */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);

	/* signals */
	g_signal_connect(sel, "changed", G_CALLBACK(cm_tree_selection_changed_cb), cmp);
	g_signal_connect(treeview, "key-press-event", G_CALLBACK(cm_tree_key_press_cb), cmp);
	g_signal_connect(treeview, "button-press-event", G_CALLBACK(cm_tree_button_press_event_cb), cmp);
	g_signal_connect(treeview, "button-release-event", G_CALLBACK(cm_tree_button_release_event_cb), cmp);
	g_signal_connect(treeview, "popup-menu", G_CALLBACK(cm_tree_popup_menu_cb), cmp);

	/* tooltips */
	gtk_widget_set_has_tooltip(treeview, TRUE);
	g_signal_connect(treeview, "query-tooltip", G_CALLBACK(cm_tree_query_tooltip), cmp);

	/* drag'n'drop */
	gtk_tree_view_enable_model_drag_source(
		GTK_TREE_VIEW(treeview),
		GDK_BUTTON1_MASK,
		NULL,
		0,
		GDK_ACTION_COPY
	);
	gtk_drag_source_add_text_targets(treeview);
	g_signal_connect(treeview, "drag-data-get", G_CALLBACK(cm_tree_drag_data_get_cb), cmp);

	/* popup menu */
	priv->popup_menu = menu = gtk_menu_new();

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(cm_button_delete_clicked_cb), cmp);
	priv->delete_popup_button = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Expand All"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(cm_tree_popup_expand_activate_cb), cmp);
	priv->expand_buttons[2] = item;

	item = gtk_image_menu_item_new_with_mnemonic(_("_Collapse All"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_icon_name(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(cm_tree_popup_collapse_activate_cb), cmp);
	priv->expand_buttons[3] = item;

	return treeview;
}


static void cookie_manager_page_init(CookieManagerPage *self)
{
	GtkWidget *tree_swin;
	GtkWidget *desc_swin;
	GtkWidget *paned;
	GtkWidget *filter_hbox;
	GtkWidget *treeview;
	CookieManagerPagePrivate *priv;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
	    COOKIE_MANAGER_PAGE_TYPE, CookieManagerPagePrivate);
	priv = self->priv;
	priv->ignore_changed_filter = FALSE;

	cm_create_toolbar(self);

	priv->desc_label = gtk_label_new(CM_EMPTY_LABEL_TEXT);
	gtk_label_set_selectable(GTK_LABEL(priv->desc_label), TRUE);
	gtk_label_set_line_wrap(GTK_LABEL(priv->desc_label), TRUE);
	gtk_label_set_line_wrap_mode(GTK_LABEL(priv->desc_label), PANGO_WRAP_CHAR);
	gtk_misc_set_alignment(GTK_MISC(priv->desc_label), 0, 0);
	gtk_misc_set_padding(GTK_MISC(priv->desc_label), 3, 3);
	gtk_widget_show(priv->desc_label);

	desc_swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(desc_swin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(desc_swin), GTK_SHADOW_NONE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(desc_swin), priv->desc_label);
	gtk_widget_show(desc_swin);

	treeview = cm_tree_prepare(self);
	gtk_widget_show(treeview);

	tree_swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tree_swin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(tree_swin), treeview);
	gtk_widget_show(tree_swin);

	priv->filter_entry = sokoke_search_entry_new (_("Search Cookies by Name or Domain"));
	gtk_widget_show(priv->filter_entry);
	g_signal_connect(priv->filter_entry, "changed", G_CALLBACK(cm_filter_entry_changed_cb), self);
	g_signal_connect(priv->filter_entry, "activate", G_CALLBACK(cm_filter_entry_changed_cb), self);

	filter_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(filter_hbox), priv->filter_entry, TRUE, TRUE, 3);
	gtk_widget_show(filter_hbox);

	paned = gtk_vpaned_new();
	gtk_paned_pack1(GTK_PANED(paned), tree_swin, TRUE, FALSE);
	gtk_paned_pack2(GTK_PANED(paned), desc_swin, FALSE, FALSE);
	gtk_widget_show(paned);

	gtk_box_pack_start(GTK_BOX(self), filter_hbox, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(self), paned, TRUE, TRUE, 0);
}


GtkWidget *cookie_manager_page_new(CookieManager *parent, GtkTreeStore *store,
								   const gchar *filter_text)
{
	GtkWidget *cmp;

	cmp = g_object_new(COOKIE_MANAGER_PAGE_TYPE, "parent", parent, "store", store, NULL);

	if (filter_text != NULL)
		cookie_manager_page_filter_changed_cb(parent, filter_text, COOKIE_MANAGER_PAGE(cmp));

	return cmp;
}

