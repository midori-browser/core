/*
 Copyright (C) 2009-2010 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "feed-panel.h"

#include <midori/midori.h>
#include <time.h>

#define STOCK_FEED_PANEL "feed-panel"

struct _FeedPanel
{
    GtkVBox parent_instance;

    GtkWidget* toolbar;
    GtkWidget* treeview;
    GtkWidget* webview;
    GtkWidget* delete;
};

struct _FeedPanelClass
{
    GtkVBoxClass parent_class;
};

static void
feed_panel_viewable_iface_init (MidoriViewableIface* iface);

static void
feed_panel_insert_item (FeedPanel*    panel,
                        GtkTreeStore* treestore,
                        GtkTreeIter*  parent,
                        KatzeItem*    item);

static void
feed_panel_disconnect_feed (FeedPanel*  panel,
                            KatzeArray* feed);

G_DEFINE_TYPE_WITH_CODE (FeedPanel, feed_panel, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (MIDORI_TYPE_VIEWABLE,
                            feed_panel_viewable_iface_init));

enum
{
    ADD_FEED,
    REMOVE_FEED,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
feed_panel_treeview_render_icon_cb (GtkTreeViewColumn* column,
                                    GtkCellRenderer*   renderer,
                                    GtkTreeModel*      model,
                                    GtkTreeIter*       iter,
                                    FeedPanel*         panel)
{
    GdkPixbuf* pixbuf;
    KatzeItem* item;
    KatzeItem* pitem;
    const gchar* uri;

    gtk_tree_model_get (model, iter, 0, &item, -1);
    g_assert (KATZE_IS_ITEM (item));

    if (!KATZE_IS_ARRAY (item))
    {
        pitem = katze_item_get_parent (item);
        g_assert (KATZE_IS_ITEM (pitem));
    }
    else
        pitem = item;

    if ((uri = katze_item_get_uri (pitem)))
    {
        if (!(pixbuf = midori_paths_get_icon (uri, NULL)))
            pixbuf = gtk_widget_render_icon (panel->treeview, STOCK_NEWS_FEED, GTK_ICON_SIZE_MENU, NULL);
    }
    else
        pixbuf = gtk_widget_render_icon (panel->treeview, GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_MENU, NULL);

    g_object_set (renderer, "pixbuf", pixbuf, NULL);

    if (pixbuf)
        g_object_unref (pixbuf);
}

static void
feed_panel_treeview_render_text_cb (GtkTreeViewColumn* column,
                                    GtkCellRenderer*   renderer,
                                    GtkTreeModel*      model,
                                    GtkTreeIter*       iter,
                                    GtkWidget*         treeview)
{
    KatzeItem* item;
    const gchar* title;

    gtk_tree_model_get (model, iter, 0, &item, -1);
    g_assert (KATZE_IS_ITEM (item));

    title = katze_item_get_name (item);
    if (!title || !*title || g_str_equal (title, " "))
        title = katze_item_get_text (item);
    if (!title || !*title || g_str_equal (title, " "))
        title = katze_item_get_uri (item);

    g_object_set (renderer, "text", title, NULL);
    g_object_unref (item);
}

static void
feed_panel_add_item_cb (KatzeArray* parent,
                        KatzeItem*  child,
                        FeedPanel*  panel)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    GtkTreeIter child_iter;
    KatzeItem* item;
    gint i;

    g_return_if_fail (FEED_IS_PANEL (panel));
    g_return_if_fail (KATZE_IS_ARRAY (parent));
    g_return_if_fail (KATZE_IS_ITEM (child));

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (panel->treeview));

    if (katze_item_get_parent (KATZE_ITEM (parent)))
    {
        if (KATZE_IS_ARRAY (child))
        {
            gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &child_iter,
                NULL, G_MAXINT, 0, child, -1);
        }
        else
        {

            i = 0;
            while (gtk_tree_model_iter_nth_child (model, &iter, NULL, i++))
            {
                gtk_tree_model_get (model, &iter, 0, &item, -1);
                if (item == KATZE_ITEM (parent))
                {
                    gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &child_iter,
                        &iter, 0, 0, child, -1);

                    g_object_unref (child);
                    g_object_unref (item);
                    break;
                }
                g_object_unref (item);
            }
        }
    }
    feed_panel_insert_item (panel, GTK_TREE_STORE (model), &child_iter, child);
}

static void
feed_panel_remove_iter (GtkTreeModel* model,
                        KatzeItem*    removed_item)
{
    guint i;
    GtkTreeIter iter;

    i = 0;
    while (gtk_tree_model_iter_nth_child (model, &iter, NULL, i))
    {
        KatzeItem* item;

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        if (item == removed_item)
        {
            gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
            g_object_unref (item);
            break;
        }
        g_object_unref (item);
        i++;
    }
}

static void
feed_panel_remove_item_cb (KatzeArray* item,
                           KatzeItem*  child,
                           FeedPanel*  panel)
{
    GtkTreeModel* model;
    KatzeItem* pitem;

    g_return_if_fail (FEED_IS_PANEL (panel));
    g_return_if_fail (KATZE_IS_ARRAY (item));
    g_return_if_fail (KATZE_IS_ITEM (child));

    if (KATZE_IS_ARRAY (child))
        feed_panel_disconnect_feed (panel, KATZE_ARRAY (child));

    if (!katze_item_get_parent (KATZE_ITEM (item)))
    {
        gint n;

        n = katze_array_get_length (KATZE_ARRAY (child));
        g_assert (n == 1);
        pitem = katze_array_get_nth_item (KATZE_ARRAY (child), 0);
    }
    else
        pitem = child;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (panel->treeview));
    feed_panel_remove_iter (model, pitem);
    g_object_unref (pitem);
}

static void
feed_panel_move_item_cb (KatzeArray* feed,
                         KatzeItem*  child,
                         gint        position,
                         FeedPanel*  panel)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    guint i;

    g_return_if_fail (FEED_IS_PANEL (panel));
    g_return_if_fail (KATZE_IS_ARRAY (feed));
    g_return_if_fail (KATZE_IS_ITEM (child));

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (panel->treeview));

    i = 0;
    while (gtk_tree_model_iter_nth_child (model, &iter, NULL, i))
    {
        KatzeItem* item;

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        if (item == child)
        {
            gtk_tree_store_move_after (GTK_TREE_STORE (model), &iter, NULL);
            g_object_unref (item);
            break;
        }
        g_object_unref (item);
        i++;
    }
}

static void
feed_panel_disconnect_feed (FeedPanel*  panel,
                            KatzeArray* feed)
{
    KatzeItem* item;

    g_return_if_fail (KATZE_IS_ARRAY (feed));

    g_signal_handlers_disconnect_by_func (feed,
            feed_panel_add_item_cb, panel);
    g_signal_handlers_disconnect_by_func (feed,
            feed_panel_remove_item_cb, panel);
    g_signal_handlers_disconnect_by_func (feed,
            feed_panel_move_item_cb, panel);

    KATZE_ARRAY_FOREACH_ITEM (item, feed)
    {
        if (KATZE_IS_ARRAY (item))
            feed_panel_disconnect_feed (panel, KATZE_ARRAY (item));
        g_object_unref (item);
    }
}

static void
feed_panel_insert_item (FeedPanel*    panel,
                        GtkTreeStore* treestore,
                        GtkTreeIter*  parent,
                        KatzeItem*    item)
{
    g_return_if_fail (KATZE_IS_ITEM (item));

    if (KATZE_IS_ARRAY (item))
    {
        g_signal_connect_after (item, "add-item",
            G_CALLBACK (feed_panel_add_item_cb), panel);
        g_signal_connect_after (item, "move-item",
            G_CALLBACK (feed_panel_move_item_cb), panel);

        if (!parent)
        {
            g_signal_connect (item, "remove-item",
                G_CALLBACK (feed_panel_remove_item_cb), panel);
        }
    }
}

static void
feed_panel_row_activated_cb (GtkTreeView*       treeview,
                             GtkTreePath*       path,
                             GtkTreeViewColumn* column,
                             FeedPanel*         panel)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;
    const gchar* uri;

    model = gtk_tree_view_get_model (treeview);

    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        uri = katze_item_get_uri (item);
        if (uri && *uri)
        {
            MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (panel));
            GtkWidget* view = midori_browser_add_item (browser, item);
            MidoriWebSettings* settings = midori_browser_get_settings (browser);
            if (!katze_object_get_boolean (settings, "open-tabs-in-the-background"))
                midori_browser_set_current_tab (browser, view);
        }
        g_object_unref (item);
    }
}

static void
feed_panel_cursor_or_row_changed_cb (GtkTreeView* treeview,
                                     FeedPanel*   panel)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;
    gboolean sensitive = FALSE;

    if (katze_tree_view_get_selected_iter (treeview, &model, &iter))
    {
        const gchar* uri;
        const gchar* text;

        gtk_tree_model_get (model, &iter, 0, &item, -1);
        uri = katze_item_get_uri (item);

        if (KATZE_IS_ARRAY (item))
        {
            text = NULL;
            if (!uri)
                text = g_strdup (katze_item_get_text (KATZE_ITEM (item)));
            else
            {
                KatzeItem* parent = katze_item_get_parent (item);
                gint64 added = katze_item_get_added (item);
                g_assert (KATZE_IS_ARRAY (parent));
                if (added)
                {
                    GDateTime* date = g_date_time_new_from_unix_local (added);
                    gchar* pretty = g_date_time_format (date, "%c");
                    g_date_time_unref (date);

    /* i18n: The local date a feed was last updated */
                    gchar* last_updated = g_strdup_printf (C_("Feed", "Last updated: %s."), pretty);
                    text = g_strdup_printf (
                            "<html><head><title>feed</title></head>"
                            "<body><h3>%s</h3><p />%s</body></html>",
                            katze_item_get_uri (KATZE_ITEM (parent)), last_updated);
                    g_free (pretty);
                    g_free (last_updated);
                }
                else
                {
                    text = g_strdup_printf (
                        "<html><head><title>feed</title></head>"
                        "<body><h3>%s</h3></body></html>", katze_item_get_uri (KATZE_ITEM (parent)));
                }
            }
            midori_view_set_html (MIDORI_VIEW (panel->webview), text ? text : "", uri, NULL);
            g_free ((gchar*) text);

            sensitive = TRUE;
        }
        else
        {
            text = katze_item_get_text (item);
            midori_view_set_html (MIDORI_VIEW (panel->webview), text ? text : "", uri, NULL);
        }
        g_object_unref (item);
    }
    if (GTK_IS_WIDGET (panel->delete))
        gtk_widget_set_sensitive (panel->delete, sensitive);
}

static void
feed_panel_popup_item (GtkWidget*     menu,
                       const gchar*   stock_id,
                       const gchar*   label,
                       KatzeItem*     item,
                       gpointer       callback,
                       FeedPanel*     panel)
{
    GtkWidget* menuitem;

    menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);
    if (label)
        gtk_label_set_text_with_mnemonic (GTK_LABEL (gtk_bin_get_child (
        GTK_BIN (menuitem))), label);
    g_object_set_data (G_OBJECT (menuitem), "KatzeItem", item);
    g_signal_connect (menuitem, "activate", G_CALLBACK (callback), panel);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
}

static void
feed_panel_open_activate_cb (GtkWidget* menuitem,
                             FeedPanel* panel)
{
    KatzeItem* item;
    const gchar* uri;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    uri = katze_item_get_uri (item);

    if (uri && *uri)
    {
        MidoriBrowser* browser;

        browser = midori_browser_get_for_widget (GTK_WIDGET (panel));
        midori_browser_set_current_uri (browser, uri);
    }
}

static void
feed_panel_open_in_tab_activate_cb (GtkWidget* menuitem,
                                    FeedPanel* panel)
{
    KatzeItem* item;
    const gchar* uri;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");

    if ((uri = katze_item_get_uri (item)) && *uri)
    {
        MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (panel));
        GtkWidget* view = midori_browser_add_item (browser, item);
        MidoriWebSettings* settings = midori_browser_get_settings (browser);
        if (!katze_object_get_boolean (settings, "open-tabs-in-the-background"))
            midori_browser_set_current_tab (browser, view);
    }
}

static void
feed_panel_open_in_window_activate_cb (GtkWidget* menuitem,
                                       FeedPanel* panel)
{
    KatzeItem* item;
    const gchar* uri;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    uri = katze_item_get_uri (item);

    if (uri && *uri)
    {
        MidoriBrowser* browser;
        MidoriBrowser* new_browser;

        browser = midori_browser_get_for_widget (GTK_WIDGET (panel));
        g_signal_emit_by_name (browser, "new-window", NULL, &new_browser);
        midori_browser_add_uri (new_browser, uri);
    }
}

static void
feed_panel_delete_activate_cb (GtkWidget* menuitem,
                               FeedPanel* panel)
{
    KatzeItem* item;

    g_return_if_fail (FEED_IS_PANEL (panel));

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    g_signal_emit (panel, signals[REMOVE_FEED], 0, item);
}

static void
feed_panel_popup (GtkWidget*      widget,
                  GdkEventButton* event,
                  KatzeItem*      item,
                  FeedPanel*      panel)
{
    GtkWidget* menu;

    menu = gtk_menu_new ();
    if (!KATZE_IS_ARRAY (item))
    {
        feed_panel_popup_item (menu, GTK_STOCK_OPEN, NULL,
            item, feed_panel_open_activate_cb, panel);
        feed_panel_popup_item (menu, STOCK_TAB_NEW, _("Open in New _Tab"),
            item, feed_panel_open_in_tab_activate_cb, panel);
        feed_panel_popup_item (menu, STOCK_WINDOW_NEW, _("Open in New _Window"),
            item, feed_panel_open_in_window_activate_cb, panel);
    }
    else
    {
        feed_panel_popup_item (menu, GTK_STOCK_DELETE, NULL,
            item, feed_panel_delete_activate_cb, panel);
    }

    katze_widget_popup (widget, GTK_MENU (menu),
                        event, KATZE_MENU_POSITION_CURSOR);
}

static gboolean
feed_panel_button_release_event_cb (GtkWidget*      widget,
                                    GdkEventButton* event,
                                    FeedPanel*      panel)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (event->button != 2 && event->button != 3)
        return FALSE;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        KatzeItem* item;

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        if (event->button == 2)
        {
            const gchar* uri = katze_item_get_uri (item);

            if (uri && *uri)
            {
                MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (panel));
                GtkWidget* view = midori_browser_add_item (browser, item);
                MidoriWebSettings* settings = midori_browser_get_settings (browser);
                if (!katze_object_get_boolean (settings, "open-tabs-in-the-background"))
                    midori_browser_set_current_tab (browser, view);
            }
        }
        else
            feed_panel_popup (widget, event, item, panel);

        g_object_unref (item);
        return TRUE;
    }
    return FALSE;
}

static void
feed_panel_popup_menu_cb (GtkWidget* widget,
                          FeedPanel* panel)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        feed_panel_popup (widget, NULL, item, panel);
        g_object_unref (item);
    }
}

void
feed_panel_add_feeds (FeedPanel* panel,
                      KatzeItem* feed)
{
    GtkTreeModel* model;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (panel->treeview));
    g_assert (GTK_IS_TREE_MODEL (model));

    feed_panel_insert_item (panel, GTK_TREE_STORE (model), NULL, feed);
}

static gboolean
webview_button_press_event_cb (GtkWidget*      widget,
                               GdkEventButton* event)
{
    /* Disable the popup menu */
    return MIDORI_EVENT_CONTEXT_MENU (event);
}

#ifndef HAVE_WEBKIT2
static gboolean
webview_navigation_request_cb (WebKitWebView*             web_view,
                               WebKitWebFrame*            frame,
                               WebKitNetworkRequest*      request,
                               WebKitWebNavigationAction* navigation_action,
                               WebKitWebPolicyDecision*   policy_decision,
                               FeedPanel*                 panel)
{
    if (webkit_web_navigation_action_get_reason (navigation_action) ==
        WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED)
    {
        MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (panel));
        const gchar* uri = webkit_network_request_get_uri (request);
        GtkWidget* view = midori_browser_add_uri (browser, uri);
        midori_browser_set_current_tab (browser, view);
        webkit_web_policy_decision_ignore (policy_decision);

        return TRUE;
    }

    return FALSE;
}
#endif

static const gchar*
feed_panel_get_label (MidoriViewable* viewable)
{
    return _("Feeds");
}

static const gchar*
feed_panel_get_stock_id (MidoriViewable* viewable)
{
    return STOCK_FEED_PANEL;
}

static void
feed_panel_add_clicked_cb (GtkWidget* toolitem,
                           FeedPanel* panel)
{
    g_return_if_fail (FEED_IS_PANEL (panel));

    g_signal_emit (panel, signals[ADD_FEED], 0);
}

static void
feed_panel_delete_clicked_cb (GtkWidget* toolitem,
                              FeedPanel* panel)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    g_return_if_fail (FEED_IS_PANEL (panel));

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (panel->treeview),
                                           &model, &iter))
    {
        KatzeItem* item;

        gtk_tree_model_get (model, &iter, 0, &item, -1);
        g_signal_emit (panel, signals[REMOVE_FEED], 0, item);
        g_object_unref (item);
    }
}

static GtkWidget*
feed_panel_get_toolbar (MidoriViewable* viewable)
{
    FeedPanel* panel = FEED_PANEL (viewable);

    if (!panel->toolbar)
    {
        GtkWidget* toolbar;
        GtkToolItem* toolitem;

        toolbar = gtk_toolbar_new ();
        panel->toolbar = toolbar;
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_ADD);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem), _("Add new feed"));
        gtk_tool_item_set_is_important (toolitem, TRUE);
        g_signal_connect (toolitem, "clicked",
                G_CALLBACK (feed_panel_add_clicked_cb), panel);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_DELETE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem), _("Delete feed"));
        g_signal_connect (toolitem, "clicked",
                G_CALLBACK (feed_panel_delete_clicked_cb), panel);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        panel->delete = GTK_WIDGET (toolitem);;

        feed_panel_cursor_or_row_changed_cb (
                GTK_TREE_VIEW (panel->treeview), panel);
        g_signal_connect (panel->delete, "destroy",
                G_CALLBACK (gtk_widget_destroyed), &panel->delete);
    }

    return panel->toolbar;
}

static void
feed_panel_finalize (GObject* object)
{
}

static void
feed_panel_viewable_iface_init (MidoriViewableIface* iface)
{
    iface->get_stock_id = feed_panel_get_stock_id;
    iface->get_label = feed_panel_get_label;
    iface->get_toolbar = feed_panel_get_toolbar;
}

static void
feed_panel_class_init (FeedPanelClass* class)
{
    GObjectClass* gobject_class;

    signals[ADD_FEED] = g_signal_new (
        "add-feed",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    signals[REMOVE_FEED] = g_signal_new (
        "remove-feed",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE, 1,
        G_TYPE_POINTER);

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = feed_panel_finalize;
}

static void
feed_panel_init (FeedPanel* panel)
{
    GtkTreeStore* model;
    GtkWidget* treewin;
    GtkWidget* treeview;
    GtkWidget* webview;
    GtkWidget* paned;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_pixbuf;
    GtkCellRenderer* renderer_text;
    GtkIconFactory *factory;
    GtkIconSource *icon_source;
    GtkIconSet *icon_set;
    MidoriWebSettings* settings;
    PangoFontDescription* font_desc;
    const gchar* family;
    gint size;
    GtkStockItem items[] =
    {
        { STOCK_FEED_PANEL, N_("_Feeds"), 0, 0, NULL }
    };

    factory = gtk_icon_factory_new ();
    gtk_stock_add (items, G_N_ELEMENTS (items));
    icon_set = gtk_icon_set_new ();
    icon_source = gtk_icon_source_new ();
    gtk_icon_source_set_icon_name (icon_source, STOCK_NEWS_FEED);
    gtk_icon_set_add_source (icon_set, icon_source);
    gtk_icon_source_free (icon_source);
    gtk_icon_factory_add (factory, STOCK_FEED_PANEL, icon_set);
    gtk_icon_set_unref (icon_set);
    gtk_icon_factory_add_default (factory);
    g_object_unref (factory);

    model = gtk_tree_store_new (1, KATZE_TYPE_ITEM);
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
    panel->treeview = treeview;
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
            (GtkTreeCellDataFunc)feed_panel_treeview_render_icon_cb,
            panel, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
            (GtkTreeCellDataFunc)feed_panel_treeview_render_text_cb,
            treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    g_object_unref (model);
    g_object_connect (treeview,
                      "signal::row-activated",
                      feed_panel_row_activated_cb, panel,
                      "signal::cursor-changed",
                      feed_panel_cursor_or_row_changed_cb, panel,
                      "signal::columns-changed",
                      feed_panel_cursor_or_row_changed_cb, panel,
                      "signal::button-release-event",
                      feed_panel_button_release_event_cb, panel,
                      "signal::popup-menu",
                      feed_panel_popup_menu_cb, panel,
                      NULL);
    gtk_widget_show (treeview);

#if GTK_CHECK_VERSION(3,0,0)
    font_desc = (PangoFontDescription*)gtk_style_context_get_font (
        gtk_widget_get_style_context (treeview), GTK_STATE_FLAG_NORMAL);
#else
    font_desc = treeview->style->font_desc;
#endif
    family = pango_font_description_get_family (font_desc);
    size = pango_font_description_get_size (font_desc) / PANGO_SCALE;
    settings = midori_web_settings_new ();
    g_object_set (settings, "default-font-family", family,
                            "default-font-size", size, NULL);
    webview = midori_view_new_with_item (NULL, settings);
    gtk_widget_set_size_request (webview, -1, 50);
    g_object_connect (midori_tab_get_web_view (MIDORI_TAB (webview)),
                      #ifndef HAVE_WEBKIT2
                      "signal::navigation-policy-decision-requested",
                      webview_navigation_request_cb, panel,
                      #endif
                      "signal::button-press-event",
                      webview_button_press_event_cb, NULL,
                      "signal::button-release-event",
                      webview_button_press_event_cb, NULL,
                      NULL);
    panel->webview = webview;

    treewin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (treewin),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (treewin),
            GTK_SHADOW_IN);
    gtk_container_add (GTK_CONTAINER (treewin), treeview);
    gtk_widget_show (treewin);

    paned = gtk_vpaned_new ();
    gtk_paned_pack1 (GTK_PANED (paned), treewin, TRUE, FALSE);
    gtk_paned_pack2 (GTK_PANED (paned), webview, TRUE, TRUE);
    gtk_box_pack_start (GTK_BOX (panel), paned, TRUE, TRUE, 0);
    gtk_widget_show (webview);
    gtk_widget_show (paned);
}

GtkWidget*
feed_panel_new (void)
{
    FeedPanel* panel = g_object_new (FEED_TYPE_PANEL, NULL);

    return GTK_WIDGET (panel);
}

