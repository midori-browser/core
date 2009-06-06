/*
 Copyright (C) 2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <gtk/gtk.h>

#include <midori/midori.h>
#include <midori/gtkiconentry.h>
#include <webkit/webkit.h>
#include <time.h>

#include "cookie-manager.h"
#include "cookie-manager-page.h"


typedef struct _CookieManagerPagePrivate			CookieManagerPagePrivate;

#define COOKIE_MANAGER_PAGE_GET_PRIVATE(obj)		(G_TYPE_INSTANCE_GET_PRIVATE((obj),\
			COOKIE_MANAGER_PAGE_TYPE, CookieManagerPagePrivate))


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


G_DEFINE_TYPE_WITH_CODE(CookieManagerPage, cookie_manager_page, GTK_TYPE_VBOX,
						G_IMPLEMENT_INTERFACE(MIDORI_TYPE_VIEWABLE,
						cookie_manager_page_viewable_iface_init));



static const gchar *cookie_manager_page_get_label(MidoriViewable *viewable)
{
	return _("Cookie Manager");
}


static const gchar *cookie_manager_page_get_stock_id(MidoriViewable *viewable)
{
	return STOCK_COOKIE_MANAGER;
}


static void cm_create_toolbar(CookieManagerPage *cmp)
{
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);
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
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(viewable);

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
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

	g_object_ref(priv->filter);
	gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview), NULL);
}


static void cookie_manager_page_cookies_changed_cb(CookieManager *cm, CookieManagerPage *cmp)
{
	const gchar *filter_text;
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

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
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

	priv->ignore_changed_filter = TRUE;
	gtk_entry_set_text(GTK_ENTRY(priv->filter_entry), text);
	priv->ignore_changed_filter = FALSE;
}


static void cookie_manager_page_finalize(GObject *object)
{
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(object);

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
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(object);
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
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);
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


static void cm_tree_popup_collapse_activate_cb(GtkMenuItem *item, CookieManagerPage *cmp)
{
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

	gtk_tree_view_collapse_all(GTK_TREE_VIEW(priv->treeview));
}


static void cm_tree_popup_expand_activate_cb(GtkMenuItem *item, CookieManagerPage *cmp)
{
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

	gtk_tree_view_expand_all(GTK_TREE_VIEW(priv->treeview));
}


static void cm_store_remove(CookieManagerPage *cmp, GtkTreeIter *iter_model)
{
	GtkTreeIter iter_store;
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

	gtk_tree_model_filter_convert_iter_to_child_iter(
		GTK_TREE_MODEL_FILTER(priv->filter), &iter_store, iter_model);
	gtk_tree_store_remove(priv->store, &iter_store);
}


static void cm_delete_cookie(CookieManagerPage *cmp, GtkTreeModel *model, GtkTreeIter *child)
{
	SoupCookie *cookie;
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

	gtk_tree_model_get(model, child, COOKIE_MANAGER_COL_COOKIE, &cookie, -1);

	cookie_manager_delete_cookie(priv->parent, cookie);
}


static void cm_button_delete_clicked_cb(GtkToolButton *button, CookieManagerPage *cmp)
{
	GtkTreeIter iter, iter_store, child;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview));
	if (! gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	if (gtk_tree_model_iter_has_child(model, &iter))
	{
		GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

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
}


static void cm_delete_all_cookies_real(CookieManagerPage *cmp)
{
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);
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
	gtk_tree_path_free(path_first);

	/* now that we deleted all matching cookies, we reset the filter */
	gtk_entry_set_text(GTK_ENTRY(priv->filter_entry), "");
	cm_set_button_sensitiveness(cmp, FALSE);
}


static void cm_button_delete_all_clicked_cb(GtkToolButton *button, CookieManagerPage *cmp)
{
	GtkWidget *dialog;
	const gchar *filter_text;
	MidoriBrowser *toplevel = midori_browser_get_for_widget(GTK_WIDGET(button));
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

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


static void cm_tree_drag_data_get_cb(GtkWidget *widget, GdkDragContext *drag_context,
									 GtkSelectionData *data, guint info, guint ltime,
									 CookieManagerPage *cmp)
{
	GtkTreeIter iter, iter_store;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview));
	if (! gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	gtk_tree_model_filter_convert_iter_to_child_iter(
		GTK_TREE_MODEL_FILTER(model), &iter_store, &iter);

	if (gtk_tree_store_iter_is_valid(priv->store, &iter_store))
	{
		SoupCookie *cookie;
		gchar *name, *text;

		gtk_tree_model_get(model, &iter,
			COOKIE_MANAGER_COL_NAME, &name,
			COOKIE_MANAGER_COL_COOKIE, &cookie,
			-1);

		if (cookie == NULL && name != NULL)
		{
			/* skip a leading dot */
			text = (*name == '.') ? name + 1 : name;

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


#if GTK_CHECK_VERSION(2, 12, 0)
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
#endif


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
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

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
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

	if (priv->ignore_changed_filter)
		return;

	text = gtk_entry_get_text(GTK_ENTRY(editable));
	cm_filter_tree(cmp, text);

	cookie_manager_update_filter(priv->parent, text);

	if (*text != '\0')
		gtk_tree_view_expand_all(GTK_TREE_VIEW(priv->treeview));
	else
		gtk_tree_view_collapse_all(GTK_TREE_VIEW(priv->treeview));
}


static void cm_filter_entry_clear_icon_released_cb(GtkIconEntry *e, gint pos, gint btn, gpointer data)
{
	if (pos == GTK_ICON_ENTRY_SECONDARY)
		gtk_entry_set_text(GTK_ENTRY(e), "");
}


static void cm_tree_selection_changed_cb(GtkTreeSelection *selection, CookieManagerPage *cmp)
{
	GtkTreeIter iter, iter_store;
	GtkTreeModel *model;
	gchar *text;
	gboolean valid = TRUE;
	gboolean delete_possible = FALSE;
	SoupCookie *cookie;
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

	if (! gtk_tree_selection_get_selected(selection, &model, &iter))
		valid = FALSE;
	else
		gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model),
			&iter_store, &iter);

	if (valid && gtk_tree_store_iter_is_valid(priv->store, &iter_store))
	{
		delete_possible = TRUE;

		gtk_tree_model_get(model, &iter, COOKIE_MANAGER_COL_COOKIE, &cookie, -1);
		if (cookie != NULL)
		{
			text = cm_get_cookie_description_text(cookie);

			gtk_label_set_markup(GTK_LABEL(priv->desc_label), text);

			g_free(text);
		}
		else
			valid = FALSE;
	}
	/* This is a bit hack'ish but we add some empty lines to get a minimum height of the
	 * label at the bottom without any font size calculation. */
	if (! valid)
		gtk_label_set_text(GTK_LABEL(priv->desc_label), CM_EMPTY_LABEL_TEXT);
	cm_set_button_sensitiveness(cmp, delete_possible);
}


static void cm_tree_show_popup_menu(GtkWidget *widget, GdkEventButton *event, CookieManagerPage *cmp)
{
	gint button, event_time;
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

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
	if (ev->button == 3)
	{
		cm_tree_show_popup_menu(widget, ev, cmp);
		return TRUE;
	}
	return FALSE;
}


static gboolean cm_tree_button_press_event_cb(GtkWidget *widget, GdkEventButton *ev,
											  CookieManagerPage *cmp)
{
	if (ev->type == GDK_2BUTTON_PRESS)
	{
		GtkTreeSelection *selection;
		GtkTreeModel *model;
		GtkTreeIter iter;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

		if (gtk_tree_selection_get_selected(selection, &model, &iter))
		{
			/* double click on parent node expands/collapses it */
			if (gtk_tree_model_iter_has_child(model, &iter))
			{
				GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

				if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
					gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
				else
					gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);

				gtk_tree_path_free(path);

				return TRUE;
			}
		}
	}

	return FALSE;
}


static GtkWidget *cm_tree_prepare(CookieManagerPage *cmp)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *sel;
	GtkWidget *item;
	GtkWidget *menu;
	GtkWidget *treeview;
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(cmp);

	treeview = priv->treeview = gtk_tree_view_new();

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
		_("Name"), renderer, "text", COOKIE_MANAGER_COL_NAME, NULL);
	gtk_tree_view_column_set_sort_indicator(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, COOKIE_MANAGER_COL_NAME);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), COOKIE_MANAGER_COL_NAME);

	/* selection handling */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);

	/* signals */
	g_signal_connect(sel, "changed", G_CALLBACK(cm_tree_selection_changed_cb), cmp);
	g_signal_connect(treeview, "button-press-event", G_CALLBACK(cm_tree_button_press_event_cb), cmp);
	g_signal_connect(treeview, "button-release-event", G_CALLBACK(cm_tree_button_release_event_cb), cmp);
	g_signal_connect(treeview, "popup-menu", G_CALLBACK(cm_tree_popup_menu_cb), cmp);

	/* tooltips */
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_has_tooltip(treeview, TRUE);
	g_signal_connect(treeview, "query-tooltip", G_CALLBACK(cm_tree_query_tooltip), cmp);
#endif

	/* drag'n'drop */
	gtk_tree_view_enable_model_drag_source(
		GTK_TREE_VIEW(treeview),
		GDK_BUTTON1_MASK,
		NULL,
		0,
		GDK_ACTION_COPY
	);
	gtk_drag_source_add_text_targets(treeview);
	/*gtk_drag_source_add_uri_targets(treeview);*/
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
	GtkWidget *filter_label;
	GtkWidget *treeview;
	CookieManagerPagePrivate *priv = COOKIE_MANAGER_PAGE_GET_PRIVATE(self);

	priv->parent = NULL;
	priv->store = NULL;
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
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(tree_swin), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(tree_swin), treeview);
	gtk_widget_show(tree_swin);

	filter_label = gtk_label_new(_("Filter:"));
	gtk_widget_show(filter_label);

	priv->filter_entry = gtk_icon_entry_new();
	gtk_widget_set_tooltip_text(priv->filter_entry,
		_("Enter a filter string to show only cookies whose name or domain "
		  "field match the entered filter"));
	gtk_widget_show(priv->filter_entry);
	gtk_icon_entry_set_icon_from_stock(GTK_ICON_ENTRY(priv->filter_entry),
		GTK_ICON_ENTRY_SECONDARY, GTK_STOCK_CLEAR);
	gtk_icon_entry_set_icon_highlight(GTK_ICON_ENTRY (priv->filter_entry),
		GTK_ICON_ENTRY_SECONDARY, TRUE);
	g_signal_connect(priv->filter_entry, "icon-release",
		G_CALLBACK(cm_filter_entry_clear_icon_released_cb), NULL);
	g_signal_connect(priv->filter_entry, "changed", G_CALLBACK(cm_filter_entry_changed_cb), self);
	g_signal_connect(priv->filter_entry, "activate", G_CALLBACK(cm_filter_entry_changed_cb), self);

	filter_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(filter_hbox), filter_label, FALSE, FALSE, 3);
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

