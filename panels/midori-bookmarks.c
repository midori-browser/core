/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-bookmarks.h"

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
                                         gboolean       new_bookmark,
                                         gboolean       is_folder);

struct _MidoriBookmarks
{
    GtkVBox parent_instance;

    GtkWidget* toolbar;
    GtkWidget* edit;
    GtkWidget* delete;
    GtkWidget* treeview;
    MidoriApp* app;
    KatzeArray* array;
    KatzeNet* net;
};

struct _MidoriBookmarksClass
{
    GtkVBoxClass parent_class;
};

static void
midori_bookmarks_viewable_iface_init (MidoriViewableIface* iface);

G_DEFINE_TYPE_WITH_CODE (MidoriBookmarks, midori_bookmarks, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (MIDORI_TYPE_VIEWABLE,
                             midori_bookmarks_viewable_iface_init));

enum
{
    PROP_0,

    PROP_APP
};

static void
midori_bookmarks_finalize     (GObject* object);

static void
midori_bookmarks_set_property (GObject*      object,
                               guint         prop_id,
                               const GValue* value,
                               GParamSpec*   pspec);

static void
midori_bookmarks_get_property (GObject*    object,
                               guint       prop_id,
                               GValue*     value,
                               GParamSpec* pspec);

static void
midori_bookmarks_class_init (MidoriBookmarksClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_bookmarks_finalize;
    gobject_class->set_property = midori_bookmarks_set_property;
    gobject_class->get_property = midori_bookmarks_get_property;

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
midori_bookmarks_get_label (MidoriViewable* viewable)
{
    return _("Bookmarks");
}

static const gchar*
midori_bookmarks_get_stock_id (MidoriViewable* viewable)
{
    return STOCK_BOOKMARKS;
}

static void
midori_bookmarks_add_clicked_cb (GtkWidget* toolitem)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (toolitem);
    /* FIXME: Take selected folder into account */
    midori_browser_edit_bookmark_dialog_new (browser, NULL, TRUE, FALSE);
}

static void
midori_bookmarks_edit_clicked_cb (GtkWidget*       toolitem,
                                  MidoriBookmarks* bookmarks)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (bookmarks->treeview),
                                           &model, &iter))
    {
        KatzeItem* item;
        gboolean is_separator;

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        is_separator = !KATZE_IS_ARRAY (item) && !katze_item_get_uri (item);
        if (!is_separator)
        {
            MidoriBrowser* browser = midori_browser_get_for_widget (toolitem);
            midori_browser_edit_bookmark_dialog_new (browser, item, FALSE, FALSE);
        }

        g_object_unref (item);
    }
}

static void
midori_bookmarks_delete_clicked_cb (GtkWidget*       toolitem,
                                    MidoriBookmarks* bookmarks)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (bookmarks->treeview),
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
midori_bookmarks_folder_clicked_cb (GtkWidget* toolitem)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (toolitem));
    /* FIXME: Take selected folder into account */
    midori_browser_edit_bookmark_dialog_new (browser,
                                             NULL, TRUE, TRUE);
}

static void
midori_bookmarks_cursor_or_row_changed_cb (GtkTreeView*     treeview,
                                           MidoriBookmarks* bookmarks)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;

    if (!bookmarks->edit)
        return;

    if (katze_tree_view_get_selected_iter (treeview, &model, &iter))
    {
        gboolean is_separator;

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        is_separator = !KATZE_IS_ARRAY (item) && !katze_item_get_uri (item);
        gtk_widget_set_sensitive (bookmarks->edit, !is_separator);
        gtk_widget_set_sensitive (bookmarks->delete, TRUE);

        g_object_unref (item);
    }
    else
    {
        gtk_widget_set_sensitive (bookmarks->edit, FALSE);
        gtk_widget_set_sensitive (bookmarks->delete, FALSE);
    }
}

static GtkWidget*
midori_bookmarks_get_toolbar (MidoriViewable* viewable)
{
    MidoriBookmarks* bookmarks = MIDORI_BOOKMARKS (viewable);

    if (!bookmarks->toolbar)
    {
        GtkWidget* toolbar;
        GtkToolItem* toolitem;

        toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        bookmarks->toolbar = toolbar;
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_ADD);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                     _("Add a new bookmark"));
        gtk_tool_item_set_is_important (toolitem, TRUE);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_bookmarks_add_clicked_cb), bookmarks);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_EDIT);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                     _("Edit the selected bookmark"));
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_bookmarks_edit_clicked_cb), bookmarks);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        bookmarks->edit = GTK_WIDGET (toolitem);
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_DELETE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                     _("Delete the selected bookmark"));
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_bookmarks_delete_clicked_cb), bookmarks);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        bookmarks->delete = GTK_WIDGET (toolitem);
        toolitem = gtk_separator_tool_item_new ();
        gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (toolitem), FALSE);
        gtk_tool_item_set_expand (toolitem, TRUE);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_DIRECTORY);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                     _("Add a new folder"));
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_bookmarks_folder_clicked_cb), bookmarks);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));

        midori_bookmarks_cursor_or_row_changed_cb (
            GTK_TREE_VIEW (bookmarks->treeview), bookmarks);
        g_signal_connect (bookmarks->edit, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &bookmarks->edit);
        g_signal_connect (bookmarks->delete, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &bookmarks->delete);
    }

    return bookmarks->toolbar;
}

static void
midori_bookmarks_viewable_iface_init (MidoriViewableIface* iface)
{
    iface->get_stock_id = midori_bookmarks_get_stock_id;
    iface->get_label = midori_bookmarks_get_label;
    iface->get_toolbar = midori_bookmarks_get_toolbar;
}

static void
midori_bookmarks_add_item_cb (KatzeArray*      array,
                              KatzeItem*       added_item,
                              MidoriBookmarks* bookmarks);

static void
midori_bookmarks_remove_item_cb (KatzeArray*      array,
                                 KatzeItem*       removed_item,
                                 MidoriBookmarks* bookmarks);

static void
midori_bookmarks_clear_cb (KatzeArray*      array,
                           MidoriBookmarks* bookmarks);

static void
midori_bookmarks_disconnect_folder (MidoriBookmarks* bookmarks,
                                    KatzeArray*      array)
{
    KatzeItem* item;
    guint i;

    g_return_if_fail (KATZE_IS_ARRAY (array));

    g_signal_handlers_disconnect_by_func (array,
        midori_bookmarks_add_item_cb, bookmarks);
    g_signal_handlers_disconnect_by_func (array,
        midori_bookmarks_remove_item_cb, bookmarks);
    g_signal_handlers_disconnect_by_func (array,
        midori_bookmarks_clear_cb, bookmarks);

    i = 0;
    while ((item = katze_array_get_nth_item (array, i++)))
    {
        if (KATZE_IS_ARRAY (item))
            midori_bookmarks_disconnect_folder (bookmarks, KATZE_ARRAY (item));
        g_object_unref (item);
    }
}

static void
midori_bookmarks_add_item_cb (KatzeArray*      array,
                              KatzeItem*       added_item,
                              MidoriBookmarks* bookmarks)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    guint i;

    g_return_if_fail (KATZE_IS_ARRAY (array));
    g_return_if_fail (KATZE_IS_ITEM (added_item));

    if (KATZE_IS_ARRAY (added_item))
    {
        g_signal_connect (added_item, "add-item",
            G_CALLBACK (midori_bookmarks_add_item_cb), bookmarks);
        g_signal_connect (added_item, "remove-item",
            G_CALLBACK (midori_bookmarks_remove_item_cb), bookmarks);
        g_signal_connect (added_item, "clear",
            G_CALLBACK (midori_bookmarks_clear_cb), bookmarks);
    }

    g_object_ref (added_item);
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (bookmarks->treeview));

    if (array == bookmarks->array)
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
midori_bookmarks_remove_iter (GtkTreeModel* model,
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
            midori_bookmarks_remove_iter (model, &iter, removed_item);

        g_object_unref (item);
        i++;
    }
}

static void
midori_bookmarks_remove_item_cb (KatzeArray*      array,
                                 KatzeItem*       removed_item,
                                 MidoriBookmarks* bookmarks)
{
    GtkTreeModel* model;

    g_return_if_fail (KATZE_IS_ARRAY (array));
    g_return_if_fail (KATZE_IS_ITEM (removed_item));

    if (KATZE_IS_ARRAY (removed_item))
        midori_bookmarks_disconnect_folder (bookmarks, KATZE_ARRAY (removed_item));

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (bookmarks->treeview));
    midori_bookmarks_remove_iter (model, NULL, removed_item);
    g_object_unref (removed_item);
}

static void
midori_bookmarks_clear_cb (KatzeArray*      array,
                           MidoriBookmarks* bookmarks)
{
    GtkTreeView* treeview;
    GtkTreeStore* store;

    g_return_if_fail (KATZE_IS_ARRAY (array));

    if (array == bookmarks->array)
    {
        treeview = GTK_TREE_VIEW (bookmarks->treeview);
        store = GTK_TREE_STORE (gtk_tree_view_get_model (treeview));
        gtk_tree_store_clear (store);
    }
    else
    {
        KatzeItem* item;
        guint i;

        i = 0;
        while ((item = katze_array_get_nth_item (array, i++)))
            midori_bookmarks_remove_item_cb (array, item, bookmarks);
    }
}

static void
midori_bookmarks_insert_folder (MidoriBookmarks* bookmarks,
                                GtkTreeStore*    treestore,
                                GtkTreeIter*     parent,
                                KatzeArray*      array)
{
    KatzeItem* item;
    guint i;
    GtkTreeIter iter;

    g_return_if_fail (KATZE_IS_ARRAY (array));

    g_signal_connect (array, "add-item",
        G_CALLBACK (midori_bookmarks_add_item_cb), bookmarks);
    g_signal_connect (array, "remove-item",
        G_CALLBACK (midori_bookmarks_remove_item_cb), bookmarks);
    g_signal_connect (array, "clear",
            G_CALLBACK (midori_bookmarks_clear_cb), bookmarks);

    i = 0;
    while ((item = katze_array_get_nth_item (array, i++)))
    {
        g_object_ref (item);
        gtk_tree_store_insert_with_values (treestore, &iter, parent, i,
                                           0, item, -1);
        if (KATZE_IS_ARRAY (item))
            midori_bookmarks_insert_folder (bookmarks, treestore,
                                            &iter, KATZE_ARRAY (item));
    }
}

static void
midori_bookmarks_set_app (MidoriBookmarks* bookmarks,
                          MidoriApp*       app)
{
    GtkTreeModel* model;

    if (bookmarks->array)
    {
        midori_bookmarks_disconnect_folder (bookmarks, bookmarks->array);
        g_object_unref (bookmarks->array);
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (bookmarks->treeview));
        gtk_tree_store_clear (GTK_TREE_STORE (model));
    }
    katze_assign (bookmarks->app, app);
    if (!app)
        return;

    g_object_ref (app);
    bookmarks->array = katze_object_get_object (app, "bookmarks");
    if (bookmarks->array)
    {
        /* FIXME: Dereference the app on finalization */
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (bookmarks->treeview));
        midori_bookmarks_insert_folder (bookmarks, GTK_TREE_STORE (model),
                                        NULL, g_object_ref (bookmarks->array));
    }
}

static void
midori_bookmarks_set_property (GObject*      object,
                               guint         prop_id,
                               const GValue* value,
                               GParamSpec*   pspec)
{
    MidoriBookmarks* bookmarks = MIDORI_BOOKMARKS (object);

    switch (prop_id)
    {
    case PROP_APP:
        midori_bookmarks_set_app (bookmarks, g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_bookmarks_get_property (GObject*    object,
                               guint       prop_id,
                               GValue*     value,
                               GParamSpec* pspec)
{
    MidoriBookmarks* bookmarks = MIDORI_BOOKMARKS (object);

    switch (prop_id)
    {
    case PROP_APP:
        g_value_set_object (value, bookmarks->app);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_bookmarks_treeview_render_icon_cb (GtkTreeViewColumn* column,
                                          GtkCellRenderer*   renderer,
                                          GtkTreeModel*      model,
                                          GtkTreeIter*       iter,
                                          GtkWidget*         treeview)
{
    KatzeItem* item;
    GdkPixbuf* pixbuf;

    gtk_tree_model_get (model, iter, 0, &item, -1);

    /* TODO: Would it be better to not do this on every redraw? */
    pixbuf = NULL;
    if (KATZE_IS_ARRAY (item))
        pixbuf = gtk_widget_render_icon (treeview, GTK_STOCK_DIRECTORY,
                                         GTK_ICON_SIZE_MENU, NULL);
    else if (katze_item_get_uri (item))
        pixbuf = katze_net_load_icon (
            MIDORI_BOOKMARKS (gtk_widget_get_parent (treeview))->net,
            katze_item_get_uri (item), NULL, treeview, NULL);
    g_object_set (renderer, "pixbuf", pixbuf, NULL);

    if (pixbuf)
        g_object_unref (pixbuf);

    g_object_unref (item);
}

static void
midori_bookmarks_treeview_render_text_cb (GtkTreeViewColumn* column,
                                          GtkCellRenderer*   renderer,
                                          GtkTreeModel*      model,
                                          GtkTreeIter*       iter,
                                          GtkWidget*         treeview)
{
    KatzeItem* item;

    gtk_tree_model_get (model, iter, 0, &item, -1);

    if (KATZE_IS_ARRAY (item) || katze_item_get_uri (item))
        g_object_set (renderer, "markup", NULL,
                      "text", katze_item_get_name (item), NULL);
    else
        g_object_set (renderer, "markup", _("<i>Separator</i>"), NULL);

    g_object_unref (item);
}

static void
midori_bookmarks_row_activated_cb (GtkTreeView*       treeview,
                                   GtkTreePath*       path,
                                   GtkTreeViewColumn* column,
                                   MidoriBookmarks*   bookmarks)
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
            MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (bookmarks));
            midori_browser_set_current_uri (browser, uri);
        }

        g_object_unref (item);
    }
}

static void
midori_bookmarks_popup_item (GtkWidget*       menu,
                             const gchar*     stock_id,
                             const gchar*     label,
                             KatzeItem*       item,
                             gpointer         callback,
                             MidoriBookmarks* bookmarks)
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
    else if (!KATZE_IS_ARRAY (item) && strcmp (stock_id, GTK_STOCK_DELETE))
        gtk_widget_set_sensitive (menuitem, uri != NULL);
    g_object_set_data (G_OBJECT (menuitem), "KatzeItem", item);
    g_signal_connect (menuitem, "activate", G_CALLBACK (callback), bookmarks);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
}

static void
midori_bookmarks_open_activate_cb (GtkWidget*       menuitem,
                                   MidoriBookmarks* bookmarks)
{
    KatzeItem* item;
    const gchar* uri;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    uri = katze_item_get_uri (item);

    if (uri && *uri)
    {
        MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (bookmarks));
        midori_browser_set_current_uri (browser, uri);
    }
}

static void
midori_bookmarks_open_in_tab_activate_cb (GtkWidget*       menuitem,
                                          MidoriBookmarks* bookmarks)
{
    KatzeItem* item;
    const gchar* uri;
    guint n;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    if (KATZE_IS_ARRAY (item))
    {
        KatzeItem* child;
        guint i = 0;

        while ((child = katze_array_get_nth_item (KATZE_ARRAY (item), i)))
        {
            if ((uri = katze_item_get_uri (child)) && *uri)
            {
                MidoriBrowser* browser;
                MidoriWebSettings* settings;

                browser = midori_browser_get_for_widget (GTK_WIDGET (bookmarks));
                n = midori_browser_add_item (browser, child);
                settings = katze_object_get_object (browser, "settings");
                if (!katze_object_get_boolean (settings, "open-tabs-in-the-background"))
                    midori_browser_set_current_page (browser, n);
                g_object_unref (settings);
            }
            i++;
        }
    }
    else
    {
        if ((uri = katze_item_get_uri (item)) && *uri)
        {
            MidoriBrowser* browser;
            MidoriWebSettings* settings;

            browser = midori_browser_get_for_widget (GTK_WIDGET (bookmarks));
            n = midori_browser_add_item (browser, item);
            settings = katze_object_get_object (browser, "settings");
            if (!katze_object_get_boolean (settings, "open-tabs-in-the-background"))
                midori_browser_set_current_page (browser, n);
            g_object_unref (settings);
        }
    }
}

static void
midori_bookmarks_open_in_window_activate_cb (GtkWidget*       menuitem,
                                             MidoriBookmarks* bookmarks)
{
    KatzeItem* item;
    const gchar* uri;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    uri = katze_item_get_uri (item);

    if (uri && *uri)
    {
        MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (bookmarks));
        g_signal_emit_by_name (browser, "new-window", uri);
    }
}

static void
midori_bookmarks_edit_activate_cb (GtkWidget*       menuitem,
                                   MidoriBookmarks* bookmarks)
{
    KatzeItem* item;
    gboolean is_separator;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    is_separator = !KATZE_IS_ARRAY (item) && !katze_item_get_uri (item);

    if (!is_separator)
    {
        MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (bookmarks));
        midori_browser_edit_bookmark_dialog_new (browser, item, FALSE, FALSE);
    }
}

static void
midori_bookmarks_delete_activate_cb (GtkWidget*       menuitem,
                                     MidoriBookmarks* bookmarks)
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
midori_bookmarks_popup (GtkWidget*       widget,
                        GdkEventButton*  event,
                        KatzeItem*       item,
                        MidoriBookmarks* bookmarks)
{
    GtkWidget* menu;
    GtkWidget* menuitem;

    menu = gtk_menu_new ();
    if (KATZE_IS_ARRAY (item))
        midori_bookmarks_popup_item (menu,
            STOCK_TAB_NEW, _("Open all in _Tabs"),
            item, midori_bookmarks_open_in_tab_activate_cb, bookmarks);
    else
    {
        midori_bookmarks_popup_item (menu, GTK_STOCK_OPEN, NULL,
            item, midori_bookmarks_open_activate_cb, bookmarks);
        midori_bookmarks_popup_item (menu, STOCK_TAB_NEW, _("Open in New _Tab"),
            item, midori_bookmarks_open_in_tab_activate_cb, bookmarks);
        midori_bookmarks_popup_item (menu, STOCK_WINDOW_NEW, _("Open in New _Window"),
            item, midori_bookmarks_open_in_window_activate_cb, bookmarks);
    }
    menuitem = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
    midori_bookmarks_popup_item (menu, GTK_STOCK_EDIT, NULL,
        item, midori_bookmarks_edit_activate_cb, bookmarks);
    midori_bookmarks_popup_item (menu, GTK_STOCK_DELETE, NULL,
        item, midori_bookmarks_delete_activate_cb, bookmarks);

    sokoke_widget_popup (widget, GTK_MENU (menu),
                         event, SOKOKE_MENU_POSITION_CURSOR);
}

static gboolean
midori_bookmarks_button_release_event_cb (GtkWidget*       widget,
                                          GdkEventButton*  event,
                                          MidoriBookmarks* bookmarks)
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
                MidoriBrowser* browser;
                gint n;

                browser = midori_browser_get_for_widget (widget);
                n = midori_browser_add_uri (browser, uri);
                midori_browser_set_current_page (browser, n);
            }
        }
        else
            midori_bookmarks_popup (widget, event, item, bookmarks);

        g_object_unref (item);
        return TRUE;
    }
    return FALSE;
}

static gboolean
midori_bookmarks_key_release_event_cb (GtkWidget*       widget,
                                       GdkEventKey*     event,
                                       MidoriBookmarks* bookmarks)
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
midori_bookmarks_popup_menu_cb (GtkWidget*       widget,
                                MidoriBookmarks* bookmarks)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        midori_bookmarks_popup (widget, NULL, item, bookmarks);
        g_object_unref (item);
    }
}

static void
midori_bookmarks_init (MidoriBookmarks* bookmarks)
{
    GtkTreeStore* model;
    GtkWidget* treeview;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_pixbuf;
    GtkCellRenderer* renderer_text;

    bookmarks->net = katze_net_new ();

    /* Create the treeview */
    model = gtk_tree_store_new (1, KATZE_TYPE_ITEM);
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_bookmarks_treeview_render_icon_cb,
        treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_bookmarks_treeview_render_text_cb,
        treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    g_object_unref (model);
    g_object_connect (treeview,
                      "signal::row-activated",
                      midori_bookmarks_row_activated_cb, bookmarks,
                      "signal::cursor-changed",
                      midori_bookmarks_cursor_or_row_changed_cb, bookmarks,
                      "signal::columns-changed",
                      midori_bookmarks_cursor_or_row_changed_cb, bookmarks,
                      "signal::button-release-event",
                      midori_bookmarks_button_release_event_cb, bookmarks,
                      "signal::key-release-event",
                      midori_bookmarks_key_release_event_cb, bookmarks,
                      "signal::popup-menu",
                      midori_bookmarks_popup_menu_cb, bookmarks,
                      NULL);
    gtk_widget_show (treeview);
    gtk_box_pack_start (GTK_BOX (bookmarks), treeview, TRUE, TRUE, 0);
    bookmarks->treeview = treeview;
}

static void
midori_bookmarks_finalize (GObject* object)
{
    MidoriBookmarks* bookmarks = MIDORI_BOOKMARKS (object);

    if (bookmarks->app)
        g_object_unref (bookmarks->app);
    g_object_unref (bookmarks->net);
}

/**
 * midori_bookmarks_new:
 *
 * Creates a new empty bookmarks.
 *
 * Return value: a new #MidoriBookmarks
 *
 * Since: 0.1.3
 **/
GtkWidget*
midori_bookmarks_new (void)
{
    MidoriBookmarks* bookmarks = g_object_new (MIDORI_TYPE_BOOKMARKS, NULL);

    return GTK_WIDGET (bookmarks);
}
