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
#include <webkit/webkit.h>

#define CM_DEBUG 0

#define STOCK_COOKIE_MANAGER "cookie-manager"
#define CM_EMPTY_LABEL_TEXT "\n\n\n\n\n"

enum
{
	COL_NAME,
	COL_TOOLTIP,
	COL_ITEM, /* indicates whether a row is a child (cookie name) or a parent (domain name) */
	COL_COOKIE,
	N_COLUMNS
};

typedef struct _CMData
{
	MidoriApp *app;

	GtkWidget *panel_page;
	GtkWidget *desc_label;
	GtkWidget *delete_button;

	GtkWidget *treeview;
	GtkTreeStore *store;

	GtkWidget *popup_menu;

	SoupCookieJar *jar;
	GSList *cookies;

	guint timer_id;
	gint ignore_changed_count;
} CMData;

static void cm_app_add_browser_cb(MidoriApp *app, MidoriBrowser *browser, MidoriExtension *ext);


#if CM_DEBUG
static gchar *cookie_to_string(SoupCookie *c)
{
	if (c != NULL)
	{
		static gchar s[256]; /* this might be too small but for debugging it should be ok */
		g_snprintf(s, sizeof(s), "%s\t%s = %s", c->domain, c->name, c->value);
		return s;
	}
	return NULL;
}
#endif


static void cm_free_cookie_list(CMData *cmdata)
{
	if (cmdata->cookies != NULL)
	{
		GSList *l;

		for (l = cmdata->cookies; l != NULL; l = g_slist_next(l))
			soup_cookie_free(l->data);
		g_slist_free(cmdata->cookies);
		cmdata->cookies = NULL;
	}
}


static void cm_deactivate_cb(MidoriExtension *extension, CMData *cmdata)
{
	g_signal_handlers_disconnect_by_func(cmdata->app, cm_app_add_browser_cb, extension);
	g_signal_handlers_disconnect_by_func(extension, cm_deactivate_cb, cmdata);

	cm_free_cookie_list(cmdata);

	gtk_widget_destroy(cmdata->panel_page);
	gtk_widget_destroy(cmdata->popup_menu);
	g_free(cmdata);
}


static void cm_refresh_store(CMData *cmdata)
{
	GSList *l;
	GHashTable *parents;
	GtkTreeIter iter;
	GtkTreeIter *parent_iter;
	gchar *tooltip = NULL;
	SoupCookie *cookie;

	g_object_ref(cmdata->store);
	gtk_tree_view_set_model(GTK_TREE_VIEW(cmdata->treeview), NULL);

	gtk_tree_store_clear(cmdata->store);

	/* free the old list */
	cm_free_cookie_list(cmdata);

	cmdata->cookies = soup_cookie_jar_all_cookies(cmdata->jar);

	/* Hashtable holds domain names as keys, the corresponding tree iters as values */
	parents = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	for (l = cmdata->cookies; l != NULL; l = g_slist_next(l))
	{
		cookie = l->data;

		/* look for the parent item for the current domain name and create it if it doesn't exist */
		if ((parent_iter = (GtkTreeIter*) g_hash_table_lookup(parents, cookie->domain)) == NULL)
		{
			parent_iter = g_new0(GtkTreeIter, 1);

			gtk_tree_store_append(cmdata->store, parent_iter, NULL);
			gtk_tree_store_set(cmdata->store, parent_iter,
				COL_NAME, cookie->domain,
				COL_ITEM, FALSE,
				-1);

			g_hash_table_insert(parents, g_strdup(cookie->domain), parent_iter);
		}

		if (gtk_check_version(2, 12, 0) == NULL)
			tooltip = g_markup_printf_escaped(
				_("<b>Host: %s</b>\nPath: %s\nSecure: %s\nName: %s\nValue: %s"),
				cookie->domain,
				cookie->path,
				cookie->secure ? _("Yes") : _("No"),
				cookie->name,
				cookie->value);

		gtk_tree_store_append(cmdata->store, &iter, parent_iter);
		gtk_tree_store_set(cmdata->store, &iter,
			COL_NAME, cookie->name,
			COL_TOOLTIP, tooltip,
			COL_ITEM, TRUE,
			COL_COOKIE, cookie,
			-1);

		g_free(tooltip);
	}
	g_hash_table_destroy(parents);

	gtk_tree_view_set_model(GTK_TREE_VIEW(cmdata->treeview), GTK_TREE_MODEL(cmdata->store));
	g_object_unref(cmdata->store);
}


static void cm_tree_selection_changed_cb(GtkTreeSelection *selection, CMData *cmdata)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *text;
	gboolean is_item;

	gtk_tree_selection_get_selected(selection, &model, &iter);

	if (model == NULL || ! gtk_tree_store_iter_is_valid(GTK_TREE_STORE(model), &iter))
	{
		/* This is a bit hack'ish but we add some empty lines to get a minimum height of the
		 * label at the bottom without any font size calculation. */
		gtk_label_set_text(GTK_LABEL(cmdata->desc_label), CM_EMPTY_LABEL_TEXT);
		gtk_widget_set_sensitive(cmdata->delete_button, FALSE);
		return;
	}

	gtk_tree_model_get(model, &iter, COL_TOOLTIP, &text, COL_ITEM, &is_item, -1);

	gtk_label_set_markup(GTK_LABEL(cmdata->desc_label), text);

	gtk_widget_set_sensitive(cmdata->delete_button, TRUE);

	g_free(text);
}


static gboolean cm_tree_button_press_event_cb(GtkWidget *widget, GdkEventButton *ev, CMData *cmdata)
{
	if (ev->type == GDK_2BUTTON_PRESS)
	{
		GtkTreeSelection *selection;
		GtkTreeModel *model;
		GtkTreeIter iter;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		model = GTK_TREE_MODEL(cmdata->store);

		if (gtk_tree_selection_get_selected(selection, NULL, &iter))
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


static gboolean cm_tree_button_release_event_cb(GtkWidget *widget, GdkEventButton *ev, CMData *cmdata)
{
	if (ev->button == 3)
	{
		gtk_menu_popup(GTK_MENU(cmdata->popup_menu), NULL, NULL, NULL, NULL, ev->button, ev->time);
		return TRUE;
	}
	return FALSE;
}


static void cm_tree_popup_collapse_activate_cb(GtkCheckMenuItem *item, CMData *cmdata)
{
	gtk_tree_view_collapse_all(GTK_TREE_VIEW(cmdata->treeview));
}


static void cm_tree_popup_expand_activate_cb(GtkCheckMenuItem *item, CMData *cmdata)
{
	gtk_tree_view_expand_all(GTK_TREE_VIEW(cmdata->treeview));
}


static void cm_delete_cookie(GtkTreeModel *model, GtkTreeIter *child, CMData *cmdata)
{
	SoupCookie *cookie;

	gtk_tree_model_get(model, child, COL_COOKIE, &cookie, -1);

	if (cookie != NULL)
	{
		cmdata->ignore_changed_count++;

		soup_cookie_jar_delete_cookie(cmdata->jar, cookie);
		/* the SoupCookie object is freed when the whole list gets updated */
	}
}


static void cm_button_delete_clicked_cb(GtkWidget *button, CMData *cmdata)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cmdata->treeview));
	gtk_tree_selection_get_selected(selection, &model, &iter);

	if (gtk_tree_model_iter_has_child(model, &iter))
	{
		gint i, n = gtk_tree_model_iter_n_children(model, &iter);
		GtkTreeIter child;

		for (i = 0; i < n; i++)
		{
			gtk_tree_model_iter_nth_child(model, &child, &iter, i);
			cm_delete_cookie(model, &child, cmdata);
		}
		/* remove the parent */
		/* TODO does this really remove all children automatically? */
		gtk_tree_store_remove(cmdata->store, &iter);
	}
	else
	{
		GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

		cm_delete_cookie(model, &iter, cmdata);
		gtk_tree_store_remove(cmdata->store, &iter);

		/* check whether the parent still has children, otherwise delete it */
		if (gtk_tree_path_up(path))
		{
			gtk_tree_model_get_iter(model, &iter, path);
			if (! gtk_tree_model_iter_has_child(model, &iter))
				/* remove the empty parent */
				gtk_tree_store_remove(cmdata->store, &iter);
		}
		gtk_tree_path_free(path);
	}
}


static void cm_delete_all_cookies_real(CMData *cmdata)
{
	GSList *l;

	for (l = cmdata->cookies; l != NULL; l = g_slist_next(l))
	{
		SoupCookie *cookie = l->data;

		cmdata->ignore_changed_count++;
		soup_cookie_jar_delete_cookie(cmdata->jar, cookie);
		/* the SoupCookie object is freed below when calling cm_free_cookie_list() */
	}

	gtk_tree_store_clear(cmdata->store);
	cm_free_cookie_list(cmdata);
}


static void cm_button_delete_all_clicked_cb(GtkWidget *button, CMData *cmdata)
{
	GtkWidget *dialog;
	GtkWidget *toplevel = gtk_widget_get_toplevel(button);

	dialog = gtk_message_dialog_new(GTK_WINDOW(toplevel),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_YES_NO,
		_("Do you really want to delete all cookies?"));

	gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
	/* steal Midori's icon :) */
	gtk_window_set_icon_name(GTK_WINDOW(dialog), gtk_window_get_icon_name(GTK_WINDOW(toplevel)));

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES)
		cm_delete_all_cookies_real(cmdata);

	gtk_widget_destroy(dialog);
}


static void cm_tree_drag_data_get_cb(GtkWidget *widget, GdkDragContext *drag_context,
									 GtkSelectionData *data, guint info, guint ltime, CMData *cmdata)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gchar *name, *text;
	gboolean is_item;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cmdata->treeview));
	gtk_tree_selection_get_selected(selection, &model, &iter);

	if (model != NULL && gtk_tree_store_iter_is_valid(GTK_TREE_STORE(model), &iter))
	{
		gtk_tree_model_get(model, &iter, COL_NAME, &name, COL_ITEM, &is_item, -1);

		if (! is_item && name != NULL)
		{
			/* skip a leading dot */
			text = (*name == '.') ? name + 1 : name;

			gtk_selection_data_set_text(data, text, -1);
		}
		g_free(name);
	}
}


static void cm_tree_prepare(CMData *cmdata)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *sel;
	GtkWidget *tree;
	GtkWidget *item;
	GtkWidget *menu;

	cmdata->treeview = tree = gtk_tree_view_new();
	cmdata->store = gtk_tree_store_new(N_COLUMNS,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, SOUP_TYPE_COOKIE);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
		_("Name"), renderer, "text", COL_NAME, NULL);
	gtk_tree_view_column_set_sort_indicator(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, COL_NAME);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(tree), COL_NAME);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(cmdata->store), COL_NAME, GTK_SORT_ASCENDING);

	if (gtk_check_version(2, 12, 0) == NULL)
		g_object_set(tree, "tooltip-column", COL_TOOLTIP, NULL);

	/* selection handling */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);

	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(cmdata->store));
	g_object_unref(cmdata->store);

	/* signals */
	g_signal_connect(sel, "changed", G_CALLBACK(cm_tree_selection_changed_cb), cmdata);
	g_signal_connect(tree, "button-press-event", G_CALLBACK(cm_tree_button_press_event_cb), cmdata);
	g_signal_connect(tree, "button-release-event", G_CALLBACK(cm_tree_button_release_event_cb), cmdata);

	/* drag'n'drop */
	gtk_tree_view_enable_model_drag_source(
		GTK_TREE_VIEW(tree),
		GDK_BUTTON1_MASK,
		NULL,
		0,
		GDK_ACTION_COPY
	);
	gtk_drag_source_add_text_targets(tree);
	/*gtk_drag_source_add_uri_targets(tree);*/
	g_signal_connect(tree, "drag-data-get", G_CALLBACK(cm_tree_drag_data_get_cb), cmdata);

	/* popup menu */
	menu = gtk_menu_new();

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(cm_button_delete_clicked_cb), cmdata);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Expand All"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(cm_tree_popup_expand_activate_cb), cmdata);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Collapse All"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_icon_name(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(cm_tree_popup_collapse_activate_cb), cmdata);

	cmdata->popup_menu = menu;
}


static gboolean cm_delayed_refresh(CMData *cmdata)
{
	cm_refresh_store(cmdata);
	cmdata->timer_id = 0;

	return FALSE;
}


static void cm_jar_changed_cb(SoupCookieJar *jar, SoupCookie *old, SoupCookie *new, CMData *cmdata)
{
	if (cmdata->ignore_changed_count > 0)
	{
		cmdata->ignore_changed_count--;
		return;
	}

	/* We delay these events a little bit to avoid too many rebuilds of the tree.
	 * Some websites (like Flyspray bugtrackers sent a whole bunch of cookies at once. */
	if (cmdata->timer_id == 0)
	{
		cmdata->timer_id = g_timeout_add_seconds(1, (GSourceFunc) cm_delayed_refresh, cmdata);
	}
}


static void cm_page_realize_cb(GtkWidget *widget, CMData *cmdata)
{
	/* Initially fill the tree view. This gets also called when the alignment of the panel is
	 * changed but then we don't need to rebuild the tree. */
	if (cmdata->cookies == NULL)
		cm_refresh_store(cmdata);
}


static void cm_app_add_browser_cb(MidoriApp *app, MidoriBrowser *browser, MidoriExtension *ext)
{
	GtkWidget *panel;
	GtkWidget *tree_swin;
	GtkWidget *desc_swin;
	GtkWidget *toolbar;
	GtkToolItem *toolitem;
	SoupSession *session;
	CMData *cmdata;

	cmdata = g_new0(CMData, 1);
	cmdata->app = app;

	panel = katze_object_get_object(browser, "panel");
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_BUTTON);
	gtk_widget_show(toolbar);

	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_tool_item_set_is_important(toolitem, TRUE);
	g_signal_connect(toolitem, "clicked", G_CALLBACK(cm_button_delete_clicked_cb), cmdata);
	gtk_widget_show(GTK_WIDGET(toolitem));
	gtk_widget_set_sensitive(GTK_WIDGET(toolitem), FALSE);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
	cmdata->delete_button = GTK_WIDGET(toolitem);

	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(toolitem), _("Delete All"));
	gtk_tool_item_set_is_important(toolitem, TRUE);
	g_signal_connect(toolitem, "clicked", G_CALLBACK(cm_button_delete_all_clicked_cb), cmdata);
	gtk_widget_show(GTK_WIDGET(toolitem));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);

    toolitem = gtk_separator_tool_item_new();
    gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(toolitem), FALSE);
    gtk_tool_item_set_expand(toolitem, TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
    gtk_widget_show(GTK_WIDGET(toolitem));

	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_ADD);
	gtk_tool_item_set_tooltip_text(toolitem, _("Expand All"));
	g_signal_connect(toolitem, "clicked", G_CALLBACK(cm_tree_popup_expand_activate_cb), cmdata);
	gtk_widget_show(GTK_WIDGET(toolitem));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);

	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_REMOVE);
	gtk_tool_item_set_tooltip_text(toolitem, _("Collapse All"));
	g_signal_connect(toolitem, "clicked", G_CALLBACK(cm_tree_popup_collapse_activate_cb), cmdata);
	gtk_widget_show(GTK_WIDGET(toolitem));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);

	cmdata->desc_label = gtk_label_new(CM_EMPTY_LABEL_TEXT);
	gtk_label_set_selectable(GTK_LABEL(cmdata->desc_label), TRUE);
	gtk_label_set_line_wrap(GTK_LABEL(cmdata->desc_label), TRUE);
	gtk_label_set_line_wrap_mode(GTK_LABEL(cmdata->desc_label), PANGO_WRAP_CHAR);
	gtk_misc_set_alignment(GTK_MISC(cmdata->desc_label), 0, 0);
    gtk_widget_show(cmdata->desc_label);

	desc_swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(desc_swin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(desc_swin), GTK_SHADOW_IN);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(desc_swin), cmdata->desc_label);
    gtk_widget_show(desc_swin);

	cm_tree_prepare(cmdata);
	gtk_widget_show(cmdata->treeview);

	tree_swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tree_swin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(tree_swin), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(tree_swin), cmdata->treeview);
    gtk_widget_show(tree_swin);

	cmdata->panel_page = gtk_vpaned_new();
	gtk_paned_pack1(GTK_PANED(cmdata->panel_page), tree_swin, TRUE, FALSE);
	gtk_paned_pack2(GTK_PANED(cmdata->panel_page), desc_swin, FALSE, FALSE);
    gtk_widget_show(cmdata->panel_page);

	/* setup soup */
	session = webkit_get_default_session();
	cmdata->jar = SOUP_COOKIE_JAR(soup_session_get_feature(session, soup_cookie_jar_get_type()));
	g_signal_connect(cmdata->jar, "changed", G_CALLBACK(cm_jar_changed_cb), cmdata);

	midori_panel_append_widget(MIDORI_PANEL(panel), cmdata->panel_page,
		STOCK_COOKIE_MANAGER, _("Cookie Manager"), toolbar);

	g_signal_connect(cmdata->panel_page, "realize", G_CALLBACK(cm_page_realize_cb), cmdata);
	g_signal_connect(ext, "deactivate", G_CALLBACK(cm_deactivate_cb), cmdata);
}


static void cm_activate_cb(MidoriExtension *extension, MidoriApp *app, gpointer data)
{
	g_signal_connect(app, "add-browser", G_CALLBACK(cm_app_add_browser_cb), extension);
}


MidoriExtension *extension_init(void)
{
	MidoriExtension *extension;
	GtkIconFactory *factory;
	GtkIconSource *icon_source;
	GtkIconSet *icon_set;
	static GtkStockItem items[] =
	{
		{ STOCK_COOKIE_MANAGER, N_("_Cookie Manager"), 0, 0, NULL }
	};

	factory = gtk_icon_factory_new();
	gtk_stock_add(items, G_N_ELEMENTS(items));
	icon_set = gtk_icon_set_new();
	icon_source = gtk_icon_source_new();
	gtk_icon_source_set_icon_name(icon_source, GTK_STOCK_DIALOG_AUTHENTICATION);
	gtk_icon_set_add_source(icon_set, icon_source);
	gtk_icon_source_free(icon_source);
	gtk_icon_factory_add(factory, STOCK_COOKIE_MANAGER, icon_set);
	gtk_icon_set_unref(icon_set);
	gtk_icon_factory_add_default(factory);
	g_object_unref(factory);

	extension = g_object_new(MIDORI_TYPE_EXTENSION,
		"name", _("Cookie Manager"),
		"description", _("List, view and delete cookies"),
		"version", "0.1",
		"authors", "Enrico Tröger <enrico(at)xfce(dot)org>",
		NULL);

	g_signal_connect(extension, "activate", G_CALLBACK(cm_activate_cb), NULL);

	return extension;
}
