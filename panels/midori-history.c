/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-history.h"

#include "midori-app.h"
#include "midori-browser.h"
#include "midori-stock.h"
#include "midori-view.h"
#include "midori-viewable.h"

#include "sokoke.h"

#include <glib/gi18n.h>
#include <string.h>

#include <katze/katze.h>
#include <gdk/gdkkeysyms.h>

void
midori_browser_edit_bookmark_dialog_new (MidoriBrowser* browser,
                                         KatzeItem*     bookmark,
                                         gboolean       new_bookmark);

struct _MidoriHistory
{
    GtkVBox parent_instance;

    GtkWidget* toolbar;
    GtkWidget* bookmark;
    GtkWidget* delete;
    GtkWidget* clear;
    GtkWidget* treeview;
    MidoriApp* app;
    KatzeArray* array;
    KatzeNet* net;
};

struct _MidoriHistoryClass
{
    GtkVBoxClass parent_class;
};

static void
midori_history_viewable_iface_init (MidoriViewableIface* iface);

G_DEFINE_TYPE_WITH_CODE (MidoriHistory, midori_history, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (MIDORI_TYPE_VIEWABLE,
                             midori_history_viewable_iface_init));

enum
{
    PROP_0,

    PROP_APP
};

static void
midori_history_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec);

static void
midori_history_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec);

static void
midori_history_class_init (MidoriHistoryClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->set_property = midori_history_set_property;
    gobject_class->get_property = midori_history_get_property;

    flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    g_object_class_install_property (gobject_class,
                                     PROP_APP,
                                     g_param_spec_object (
                                     "app",
                                     "App",
                                     "The app",
                                     MIDORI_TYPE_APP,
                                     flags));
}

static const gchar*
midori_history_get_label (MidoriViewable* viewable)
{
    return _("History");
}

static const gchar*
midori_history_get_stock_id (MidoriViewable* viewable)
{
    return STOCK_HISTORY;
}

static void
midori_history_add_clicked_cb (GtkWidget* toolitem)
{
    GtkWidget* browser = gtk_widget_get_toplevel (toolitem);
    /* FIXME: Take selected folder into account */
    midori_browser_edit_bookmark_dialog_new (MIDORI_BROWSER (browser),
                                             NULL, TRUE);
}

static void
midori_history_delete_clicked_cb (GtkWidget*     toolitem,
                                  MidoriHistory* history)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (history->treeview),
                                           &model, &iter))
    {
        KatzeItem* item;
        KatzeArray* parent;

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        /* FIXME: Even toplevel items should technically have a parent */
        g_return_if_fail (katze_item_get_parent (item));

        parent = katze_item_get_parent (item);
        katze_array_remove_item (parent, item);

        g_object_unref (item);
    }
}

static void
midori_history_clear_clicked_cb (GtkWidget*     toolitem,
                                 MidoriHistory* history)
{
    GtkWidget* browser;
    GtkWidget* dialog;
    gint result;

    browser = gtk_widget_get_toplevel (GTK_WIDGET (history));
    dialog = gtk_message_dialog_new (GTK_WINDOW (browser),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
        _("Are you sure you want to remove all history items?"));
    result = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    if (result != GTK_RESPONSE_YES)
        return;

    katze_array_clear (history->array);
}

static void
midori_history_cursor_or_row_changed_cb (GtkTreeView*   treeview,
                                         MidoriHistory* history)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;

    if (!history->bookmark)
        return;

    if (katze_tree_view_get_selected_iter (treeview, &model, &iter))
    {
        gboolean is_page;

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        is_page = !KATZE_IS_ARRAY (item) && katze_item_get_uri (item);
        gtk_widget_set_sensitive (history->bookmark, is_page);
        gtk_widget_set_sensitive (history->delete, TRUE);
        gtk_widget_set_sensitive (history->clear, TRUE);

        g_object_unref (item);
    }
    else
    {
        gtk_widget_set_sensitive (history->bookmark, FALSE);
        gtk_widget_set_sensitive (history->delete, FALSE);
        gtk_widget_set_sensitive (history->clear, FALSE);
    }
}

static GtkWidget*
midori_history_get_toolbar (MidoriViewable* viewable)
{
    MidoriHistory* history = MIDORI_HISTORY (viewable);

    if (!history->toolbar)
    {
        GtkWidget* toolbar;
        GtkToolItem* toolitem;

        toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        history->toolbar = toolbar;
        toolitem = gtk_tool_button_new_from_stock (STOCK_BOOKMARK_ADD);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                     _("Bookmark the selected history item"));
        gtk_tool_item_set_is_important (toolitem, TRUE);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_history_add_clicked_cb), history);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        history->bookmark = GTK_WIDGET (toolitem);
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_DELETE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                     _("Delete the selected history item"));
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_history_delete_clicked_cb), history);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        history->delete = GTK_WIDGET (toolitem);
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_CLEAR);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                     _("Clear the entire history"));
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_history_clear_clicked_cb), history);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        history->clear = GTK_WIDGET (toolitem);
        midori_history_cursor_or_row_changed_cb (
            GTK_TREE_VIEW (history->treeview), history);
        g_signal_connect (history->bookmark, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &history->bookmark);
        g_signal_connect (history->delete, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &history->delete);
        g_signal_connect (history->clear, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &history->clear);
    }

    return history->toolbar;
}

static void
midori_history_viewable_iface_init (MidoriViewableIface* iface)
{
    iface->get_stock_id = midori_history_get_stock_id;
    iface->get_label = midori_history_get_label;
    iface->get_toolbar = midori_history_get_toolbar;
}

static void
midori_history_add_item_cb (KatzeArray*    array,
                            KatzeItem*     added_item,
                            MidoriHistory* history);

static void
midori_history_remove_item_cb (KatzeArray*    array,
                               KatzeItem*     removed_item,
                               MidoriHistory* history);

static void
midori_history_clear_cb (KatzeArray*    array,
                         MidoriHistory* history);

static void
midori_history_disconnect_folder (MidoriHistory* history,
                                  KatzeArray*    array)
{
    KatzeItem* item;
    guint i;

    g_return_if_fail (KATZE_IS_ARRAY (array));

    g_signal_handlers_disconnect_by_func (array,
        midori_history_add_item_cb, history);
    g_signal_handlers_disconnect_by_func (array,
        midori_history_remove_item_cb, history);
    g_signal_handlers_disconnect_by_func (array,
        midori_history_clear_cb, history);

    i = 0;
    while ((item = katze_array_get_nth_item (array, i++)))
    {
        if (KATZE_IS_ARRAY (item))
            midori_history_disconnect_folder (history, KATZE_ARRAY (item));
        g_object_unref (item);
    }
}

static void
midori_history_add_item_cb (KatzeArray*    array,
                            KatzeItem*     added_item,
                            MidoriHistory* history)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    guint i;

    g_return_if_fail (KATZE_IS_ARRAY (array));
    g_return_if_fail (KATZE_IS_ITEM (added_item));

    if (KATZE_IS_ARRAY (added_item))
    {
        g_signal_connect (added_item, "add-item",
            G_CALLBACK (midori_history_add_item_cb), history);
        g_signal_connect (added_item, "remove-item",
            G_CALLBACK (midori_history_remove_item_cb), history);
        g_signal_connect (added_item, "clear",
            G_CALLBACK (midori_history_clear_cb), history);
    }

    g_object_ref (added_item);
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (history->treeview));

    if (array == history->array)
    {
        gtk_tree_store_insert_with_values (GTK_TREE_STORE (model),
            &iter, NULL, G_MAXINT, 0, added_item, -1);
        return;
    }

    i = 0;
    /* FIXME: Recurse over children of folders, too */
    while (gtk_tree_model_iter_nth_child (model, &iter, NULL, i))
    {
        KatzeItem* item;

        gtk_tree_model_get (model, &iter, 0, &item, -1);
        if (item == (KatzeItem*)array)
        {
            GtkTreeIter child_iter;

            gtk_tree_store_insert_with_values (GTK_TREE_STORE (model),
                &child_iter, &iter, G_MAXINT, 0, added_item, -1);
            break;
        }
        g_object_unref (item);

        i++;
    }
}

static void
midori_history_remove_iter (GtkTreeModel* model,
                            GtkTreeIter*  parent,
                            KatzeItem*    removed_item)
{
    guint i;
    GtkTreeIter iter;

    i = 0;
    while (gtk_tree_model_iter_nth_child (model, &iter, parent, i))
    {
        KatzeItem* item;

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        if (item == removed_item)
        {
            gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
            g_object_unref (item);
            break;
        }

        if (KATZE_IS_ARRAY (item))
            midori_history_remove_iter (model, &iter, removed_item);

        g_object_unref (item);
        i++;
    }
}

static void
midori_history_remove_item_cb (KatzeArray*    array,
                               KatzeItem*     removed_item,
                               MidoriHistory* history)
{
    GtkTreeModel* model;

    g_assert (KATZE_IS_ARRAY (array));
    g_assert (KATZE_IS_ITEM (removed_item));

    if (KATZE_IS_ARRAY (removed_item))
        midori_history_disconnect_folder (history, KATZE_ARRAY (removed_item));

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (history->treeview));
    midori_history_remove_iter (model, NULL, removed_item);
    g_object_unref (removed_item);
}

static void
midori_history_clear_cb (KatzeArray*    array,
                         MidoriHistory* history)
{
    GtkTreeView* treeview;
    GtkTreeStore* store;

    g_assert (KATZE_IS_ARRAY (array));

    if (array == history->array)
    {
        treeview = GTK_TREE_VIEW (history->treeview);
        store = GTK_TREE_STORE (gtk_tree_view_get_model (treeview));
        gtk_tree_store_clear (store);
    }
    else
    {
        KatzeItem* item;
        guint i;

        i = 0;
        while ((item = katze_array_get_nth_item (array, i++)))
            midori_history_remove_item_cb (array, item, history);
    }

    midori_history_disconnect_folder (history, array);
}

static void
midori_history_insert_item (MidoriHistory* history,
                            GtkTreeStore*  treestore,
                            GtkTreeIter*   parent,
                            KatzeItem*     item,
                            gint64         day)
{
    GtkTreeIter iter;
    gint64 age = -1;

    g_return_if_fail (KATZE_IS_ITEM (item));

    if (KATZE_IS_ARRAY (item))
    {
        GtkTreeIter* piter;
        gint64 pday;
        guint i, n;

        g_signal_connect (item, "add-item",
            G_CALLBACK (midori_history_add_item_cb), history);
        g_signal_connect (item, "remove-item",
            G_CALLBACK (midori_history_remove_item_cb), history);
        g_signal_connect (item, "clear",
            G_CALLBACK (midori_history_clear_cb), history);

        piter = parent;
        if ((pday = katze_item_get_added (item)))
        {
            age = day - pday;
            gtk_tree_store_insert_with_values (treestore, &iter, parent,
                                               0, 0, item, 1, age, -1);
            g_object_unref (item);
            piter = &iter;
        }
        n = katze_array_get_length (KATZE_ARRAY (item));
        for (i = 0; i < n; i++)
        {
            KatzeItem* child;

            child = katze_array_get_nth_item (KATZE_ARRAY (item), i);
            midori_history_insert_item (history, treestore, piter, child, day);
        }
    }
    else
    {
        gtk_tree_store_insert_with_values (treestore, &iter, parent,
                                           0, 0, item, 1, age, -1);
    }
}

static void
midori_history_set_app (MidoriHistory* history,
                        MidoriApp*     app)
{
    GtkTreeModel* model;

    if (history->array)
    {
        midori_history_disconnect_folder (history, history->array);
        g_object_unref (history->array);
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (history->treeview));
        gtk_tree_store_clear (GTK_TREE_STORE (model));
    }
    katze_assign (history->app, app);
    if (!app)
        return;

    g_object_ref (app);
    history->array = katze_object_get_object (app, "history");
    if (history->array)
    {
        time_t now = time (NULL);
        gint64 day = sokoke_time_t_to_julian (&now);

        /* FIXME: Dereference the app on finalization */
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (history->treeview));
        midori_history_insert_item (history, GTK_TREE_STORE (model),
            NULL, KATZE_ITEM (g_object_ref (history->array)), day);
    }
}

static void
midori_history_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec)
{
    MidoriHistory* history = MIDORI_HISTORY (object);

    switch (prop_id)
    {
    case PROP_APP:
        midori_history_set_app (history, g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_history_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec)
{
    MidoriHistory* history = MIDORI_HISTORY (object);

    switch (prop_id)
    {
    case PROP_APP:
        g_value_set_object (value, history->app);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_history_treeview_render_icon_cb (GtkTreeViewColumn* column,
                                        GtkCellRenderer*   renderer,
                                        GtkTreeModel*      model,
                                        GtkTreeIter*       iter,
                                        GtkWidget*         treeview)
{
    KatzeItem* item;
    GdkPixbuf* pixbuf = NULL;

    gtk_tree_model_get (model, iter, 0, &item, -1);

    g_assert (KATZE_IS_ITEM (item));

    if (KATZE_IS_ARRAY (item))
        pixbuf = gtk_widget_render_icon (treeview, GTK_STOCK_DIRECTORY,
                                         GTK_ICON_SIZE_MENU, NULL);
    else
        pixbuf = katze_net_load_icon (
            MIDORI_HISTORY (gtk_widget_get_parent (treeview))->net,
            katze_item_get_uri (item), NULL, treeview, NULL);

    g_object_set (renderer, "pixbuf", pixbuf, NULL);

    if (pixbuf)
        g_object_unref (pixbuf);

    g_object_unref (item);
}

static void
midori_history_treeview_render_text_cb (GtkTreeViewColumn* column,
                                        GtkCellRenderer*   renderer,
                                        GtkTreeModel*      model,
                                        GtkTreeIter*       iter,
                                        GtkWidget*         treeview)
{
    KatzeItem* item;
    gint64 age;

    gtk_tree_model_get (model, iter, 0, &item, 1, &age, -1);

    g_assert (KATZE_IS_ITEM (item));

    if (KATZE_IS_ARRAY (item))
    {
        gchar* sdate;

        g_assert (age >= 0);

        if (age > 7)
        {
            g_object_set (renderer, "text", katze_item_get_token (item), NULL);
        }
        else if (age > 6)
        {
            sdate = g_strdup_printf (_("A week ago"));
            g_object_set (renderer, "text", sdate, NULL);
            g_free (sdate);
        }
        else if (age > 1)
        {
            sdate = g_strdup_printf (_("%d days ago"), (gint)age);
            g_object_set (renderer, "text", sdate, NULL);
            g_free (sdate);
        }
        else
        {
            if (age == 0)
                sdate = _("Today");
            else
                sdate = _("Yesterday");
            g_object_set (renderer, "text", sdate, NULL);
        }
    }
    else
        g_object_set (renderer, "text", katze_item_get_name (item), NULL);

    g_object_unref (item);
}

static void
midori_history_row_activated_cb (GtkTreeView*       treeview,
                                   GtkTreePath*       path,
                                   GtkTreeViewColumn* column,
                                   MidoriHistory*   history)
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
            GtkWidget* browser;

            browser = gtk_widget_get_toplevel (GTK_WIDGET (history));
            midori_browser_set_current_uri (MIDORI_BROWSER (browser), uri);
        }

        g_object_unref (item);
    }
}

static void
midori_history_popup_item (GtkWidget*     menu,
                           const gchar*   stock_id,
                           const gchar*   label,
                           KatzeItem*     item,
                           gpointer       callback,
                           MidoriHistory* history)
{
    const gchar* uri;
    GtkWidget* menuitem;

    uri = katze_item_get_uri (item);

    menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);
    if (label)
        gtk_label_set_text_with_mnemonic (GTK_LABEL (gtk_bin_get_child (
        GTK_BIN (menuitem))), label);
    if (!strcmp (stock_id, GTK_STOCK_EDIT))
        gtk_widget_set_sensitive (menuitem,
            KATZE_IS_ARRAY (item) || uri != NULL);
    else if (strcmp (stock_id, GTK_STOCK_DELETE))
        gtk_widget_set_sensitive (menuitem, uri != NULL);
    g_object_set_data (G_OBJECT (menuitem), "KatzeItem", item);
    g_signal_connect (menuitem, "activate", G_CALLBACK (callback), history);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
}

static void
midori_history_open_activate_cb (GtkWidget*     menuitem,
                                 MidoriHistory* history)
{
    KatzeItem* item;
    const gchar* uri;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    uri = katze_item_get_uri (item);

    if (uri && *uri)
    {
        GtkWidget* browser = gtk_widget_get_toplevel (GTK_WIDGET (history));
        midori_browser_set_current_uri (MIDORI_BROWSER (browser), uri);
    }
}

static void
midori_history_open_in_tab_activate_cb (GtkWidget*     menuitem,
                                        MidoriHistory* history)
{
    KatzeItem* item;
    const gchar* uri;
    guint n;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    uri = katze_item_get_uri (item);

    if (uri && *uri)
    {
        GtkWidget* browser;
        MidoriWebSettings* settings;

        browser = gtk_widget_get_toplevel (GTK_WIDGET (history));
        n = midori_browser_add_item (MIDORI_BROWSER (browser), item);
        settings = katze_object_get_object (browser, "settings");
        if (!katze_object_get_boolean (settings, "open-tabs-in-the-background"))
            midori_browser_set_current_page (MIDORI_BROWSER (browser), n);
    }
}

static void
midori_history_open_in_window_activate_cb (GtkWidget*     menuitem,
                                           MidoriHistory* history)
{
    KatzeItem* item;
    const gchar* uri;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    uri = katze_item_get_uri (item);

    if (uri && *uri)
    {
        GtkWidget* browser = gtk_widget_get_toplevel (GTK_WIDGET (history));
        g_signal_emit_by_name (browser, "new-window", uri);
    }
}

static void
midori_history_bookmark_activate_cb (GtkWidget*     menuitem,
                                     MidoriHistory* history)
{
    KatzeItem* item;
    const gchar* uri;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    uri = katze_item_get_uri (item);

    if (uri && *uri)
    {
        GtkWidget* browser = gtk_widget_get_toplevel (GTK_WIDGET (history));
        midori_browser_edit_bookmark_dialog_new (MIDORI_BROWSER (browser), item, TRUE);
    }
}

static void
midori_history_delete_activate_cb (GtkWidget*     menuitem,
                                   MidoriHistory* history)
{
    KatzeItem* item;
    KatzeArray* parent;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");

    /* FIXME: Even toplevel items should technically have a parent */
    g_return_if_fail (katze_item_get_parent (item));

    parent = katze_item_get_parent (item);
    katze_array_remove_item (parent, item);
}

static void
midori_history_popup (GtkWidget*      widget,
                      GdkEventButton* event,
                      KatzeItem*      item,
                      MidoriHistory*  history)
{
    GtkWidget* menu;
    GtkWidget* menuitem;

    menu = gtk_menu_new ();
    midori_history_popup_item (menu, GTK_STOCK_OPEN, NULL,
        item, midori_history_open_activate_cb, history);
    midori_history_popup_item (menu, STOCK_TAB_NEW, _("Open in New _Tab"),
        item, midori_history_open_in_tab_activate_cb, history);
    midori_history_popup_item (menu, STOCK_WINDOW_NEW, _("Open in New _Window"),
        item, midori_history_open_in_window_activate_cb, history);
    if (!KATZE_IS_ARRAY (item))
        midori_history_popup_item (menu, STOCK_BOOKMARK_ADD, NULL,
            item, midori_history_bookmark_activate_cb, history);
    menuitem = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
    midori_history_popup_item (menu, GTK_STOCK_DELETE, NULL,
        item, midori_history_delete_activate_cb, history);

    sokoke_widget_popup (widget, GTK_MENU (menu),
                         event, SOKOKE_MENU_POSITION_CURSOR);
}

static gboolean
midori_history_button_release_event_cb (GtkWidget*      widget,
                                        GdkEventButton* event,
                                        MidoriHistory*  history)
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
                GtkWidget* browser;
                gint n;

                browser = gtk_widget_get_toplevel (widget);
                n = midori_browser_add_uri (MIDORI_BROWSER (browser), uri);
                midori_browser_set_current_page (MIDORI_BROWSER (browser), n);
            }
        }
        else
            midori_history_popup (widget, event, item, history);

        g_object_unref (item);
        return TRUE;
    }
    return FALSE;
}

static gboolean
midori_history_key_release_event_cb (GtkWidget*     widget,
                                     GdkEventKey*   event,
                                     MidoriHistory* history)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (event->keyval != GDK_Delete)
        return FALSE;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        KatzeItem* item;
        KatzeArray* parent;

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        parent = katze_item_get_parent (item);
        katze_array_remove_item (parent, item);

        g_object_unref (item);
    }

    return FALSE;
}

static void
midori_history_popup_menu_cb (GtkWidget*     widget,
                              MidoriHistory* history)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        midori_history_popup (widget, NULL, item, history);
        g_object_unref (item);
    }
}

static void
midori_history_init (MidoriHistory* history)
{
    GtkTreeStore* model;
    GtkWidget* treeview;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_pixbuf;
    GtkCellRenderer* renderer_text;

    history->net = katze_net_new ();
    /* FIXME: Dereference the net on finalization */

    /* Create the treeview */
    model = gtk_tree_store_new (2, KATZE_TYPE_ITEM, G_TYPE_INT64);
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_history_treeview_render_icon_cb,
        treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_history_treeview_render_text_cb,
        treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    g_object_unref (model);
    g_object_connect (treeview,
                      "signal::row-activated",
                      midori_history_row_activated_cb, history,
                      "signal::cursor-changed",
                      midori_history_cursor_or_row_changed_cb, history,
                      "signal::columns-changed",
                      midori_history_cursor_or_row_changed_cb, history,
                      "signal::button-release-event",
                      midori_history_button_release_event_cb, history,
                      "signal::key-release-event",
                      midori_history_key_release_event_cb, history,
                      "signal::popup-menu",
                      midori_history_popup_menu_cb, history,
                      NULL);
    gtk_widget_show (treeview);
    gtk_box_pack_start (GTK_BOX (history), treeview, TRUE, TRUE, 0);
    history->treeview = treeview;
}

/**
 * midori_history_new:
 *
 * Creates a new empty history.
 *
 * Return value: a new #MidoriHistory
 *
 * Since: 0.1.3
 **/
GtkWidget*
midori_history_new (void)
{
    MidoriHistory* history = g_object_new (MIDORI_TYPE_HISTORY, NULL);

    return GTK_WIDGET (history);
}
