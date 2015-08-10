/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-bookmarks.h"

#include "midori-array.h"
#include "midori-app.h"
#include "midori-browser.h"
#include "midori-platform.h"
#include "midori-view.h"
#include "midori-core.h"
#include "midori-bookmarks-db.h"

#include <glib/gi18n.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>

#define COMPLETION_DELAY 200

gboolean
midori_browser_edit_bookmark_dialog_new (MidoriBrowser* browser,
                                         KatzeItem*     bookmark_or_parent,
                                         gboolean       new_bookmark,
                                         gboolean       is_folder,
                                         GtkWidget*     proxy);

void
midori_browser_open_bookmark (MidoriBrowser* browser,
                              KatzeItem*     item);

struct _MidoriBookmarks
{
    GtkVBox parent_instance;
    GtkWidget* toolbar;
    GtkWidget* edit;
    GtkWidget* delete;
    GtkWidget* treeview;
    MidoriApp* app;
    MidoriBookmarksDb* bookmarks_db;

    gint filter_timeout;
    gchar* filter;

    KatzeItem* hovering_item;
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
midori_bookmarks_row_changed_cb (GtkTreeModel*    model,
                                 GtkTreePath*     path,
                                 GtkTreeIter*     iter,
                                 MidoriBookmarks* bookmarks);

static void
midori_bookmarks_add_item_cb (KatzeArray*      array,
                              KatzeItem*       item,
                              MidoriBookmarks* bookmarks);

static void
midori_bookmarks_update_item_cb (KatzeArray*      array,
				 KatzeItem*       item,
				 MidoriBookmarks* bookmarks);

static void
midori_bookmarks_remove_item_cb (KatzeArray*      array,
                                 KatzeItem*       item,
                                 MidoriBookmarks* bookmarks);

static void
midori_bookmarks_update_cb (KatzeArray*      array,
                            MidoriBookmarks* bookmarks);

static void
midori_bookmarks_statusbar_update (MidoriBookmarks *bookmarks);

static void
midori_bookmarks_add_item (KatzeItem*       item,
                           MidoriBookmarks* bookmarks);

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

#if 0 /* Make sure the following function is never used */
/* TODO: Function never used */
void
midori_bookmarks_export_array_db (sqlite3*    db,
                                  KatzeArray* array,
                                  gint64      parentid)
{
    KatzeArray* root_array;
    KatzeArray* subarray;
    KatzeItem* item;
    GList* list;
    gchar* parent_id;

    parent_id = g_strdup_printf ("%" G_GINT64_FORMAT, parentid);
    if (!(root_array = midori_bookmarks_db_query_recursive (array, "*", "parentid = %q", parent_id, FALSE)))
    {
        g_free (parent_id);
        return;
    }
    KATZE_ARRAY_FOREACH_ITEM_L (item, root_array, list)
    {
        if (KATZE_ITEM_IS_FOLDER (item))
        {
            subarray = katze_array_new (KATZE_TYPE_ARRAY);
            katze_item_set_name (KATZE_ITEM (subarray), katze_item_get_name (item));
            midori_bookmarks_export_array_db (db, subarray,
                    katze_item_get_meta_integer (item, "parentid"));
            katze_array_add_item (array, subarray);
        }
        else
            katze_array_add_item (array, item);
    }

    g_free (parent_id);
    g_list_free (list);
}
#endif /* 0 */

static KatzeArray*
midori_bookmarks_read_from_db (MidoriBookmarks* bookmarks,
                               gint64           parentid,
                               const gchar*     keyword)
{
    KatzeArray* array;

    if (keyword && *keyword)
        array = midori_bookmarks_db_query_recursive (bookmarks->bookmarks_db,
                                                     "id, parentid, title, uri, desc, app, toolbar, pos_panel, pos_bar", "title LIKE '%%%q%%'", keyword, FALSE);
    else
    {
        if (parentid > 0)
        {
            gchar* parent_id = g_strdup_printf ("%" G_GINT64_FORMAT, parentid);
            array = midori_bookmarks_db_query_recursive (bookmarks->bookmarks_db,
                                                         "id, parentid, title, uri, desc, app, toolbar, pos_panel, pos_bar", "parentid = %q", parent_id, FALSE);

            g_free (parent_id);
        }
        else
            array = midori_bookmarks_db_query_recursive (bookmarks->bookmarks_db,
                                                         "id, parentid, title, uri, desc, app, toolbar, pos_panel, pos_bar", "parentid IS NULL", NULL, FALSE);
    }
    return array ? array : katze_array_new (KATZE_TYPE_ITEM);
}

static void
midori_bookmarks_read_from_db_to_model (MidoriBookmarks* bookmarks,
                                        GtkTreeStore*    model,
                                        GtkTreeIter*     parent,
                                        gint64           parentid,
                                        const gchar*     keyword)
{
    KatzeArray* array;
    gint last;
    KatzeItem* item;
    GtkTreeIter child;

    array = midori_bookmarks_read_from_db (bookmarks, parentid, keyword);
    katze_bookmark_populate_tree_view (array, model, parent);
    /* Remove invisible dummy row */
    last = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), parent);
    if (!last)
        return;
    gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (model), &child, parent, last - 1);
    gtk_tree_model_get (GTK_TREE_MODEL (model), &child, 0, &item, -1);
    if (KATZE_ITEM_IS_SEPARATOR (item))
        gtk_tree_store_remove (model, &child);
    else
        g_object_unref (item);
    g_object_unref (G_OBJECT (array));
}

static gboolean
midori_bookmarks_reach_item_recurse (GtkTreeModel* model,
				     GtkTreeIter* iter,
				     gint64 id)
{
    do
    {
        GtkTreeIter child;
        KatzeItem *item;
        gint64 itemid = -1;

        gtk_tree_model_get (model, iter, 0, &item, -1);

        if (!KATZE_ITEM_IS_SEPARATOR(item))
        {
            itemid = katze_item_get_meta_integer (item, "id");
            g_object_unref (item);
        }

        if (id == itemid)
            return TRUE;

        if (gtk_tree_model_iter_children (model, &child, iter))
        {
            if (midori_bookmarks_reach_item_recurse (model, &child, id))
            {
                *iter = child;
                return TRUE;
            }
        }
    }
    while (gtk_tree_model_iter_next(model, iter));

    return FALSE;
}

static gboolean
midori_bookmarks_reach_item (GtkTreeModel* model,
			     GtkTreeIter* iter,
			     gint64 id)
{
    if (!gtk_tree_model_get_iter_first(model, iter))
        return FALSE;

    return midori_bookmarks_reach_item_recurse (model, iter, id);
}

static void
midori_bookmarks_add_item_to_model(GtkTreeStore*    model,
				   GtkTreeIter*     parent,
				   KatzeItem*       item)
{
    if (KATZE_ITEM_IS_BOOKMARK (item))
    {
        gchar* tooltip = g_markup_escape_text (katze_item_get_uri (item), -1);

        gtk_tree_store_insert_with_values (model, NULL, parent,
            0,
            0, item, 1, tooltip, -1);
        g_free (tooltip);
    }
    else
    {
        GtkTreeIter root_iter;

        gtk_tree_store_insert_with_values (model, &root_iter, parent,
            0, 0, item, -1);

        /* That's an invisible dummy, so we always have an expander */
        gtk_tree_store_insert_with_values (model, NULL, &root_iter,
            0,
            0, NULL, -1);
    }
}

static void
midori_bookmarks_update_item_in_model(MidoriBookmarks* bookmarks,
				      GtkTreeStore*    model,
				      GtkTreeIter*     iter,
				      KatzeItem*       item)
{
    g_signal_handlers_block_by_func (model,
        midori_bookmarks_row_changed_cb,
        bookmarks);

    if (KATZE_ITEM_IS_BOOKMARK (item))
    {
        gchar* tooltip = g_markup_escape_text (katze_item_get_uri (item), -1);

        gtk_tree_store_set(model, iter,
            0, item, 1, tooltip, -1);

        g_free (tooltip);
    }
    else
    {
        gtk_tree_store_set(model, iter,
            0, item, -1);
    }

    g_signal_handlers_unblock_by_func (model,
        midori_bookmarks_row_changed_cb,
        bookmarks);
}

static void
midori_bookmarks_add_item_cb (KatzeArray*      array,
                              KatzeItem*       item,
                              MidoriBookmarks* bookmarks)
{
    midori_bookmarks_add_item (item, bookmarks);
}


static void
midori_bookmarks_add_item (KatzeItem*       item,
                           MidoriBookmarks* bookmarks)
{
    gint64 parentid = katze_item_get_meta_integer (item, "parentid");
    GtkTreeModel* model = gtk_tree_view_get_model (GTK_TREE_VIEW (bookmarks->treeview));
    GtkTreeIter iter;

    if (parentid == -1)
    {
        midori_bookmarks_add_item_to_model (GTK_TREE_STORE (model), NULL, item);
    }
    else if (midori_bookmarks_reach_item (model, &iter, parentid))
    {
        GtkTreePath* path = gtk_tree_model_get_path(model, &iter);

        if (gtk_tree_view_row_expanded (GTK_TREE_VIEW (bookmarks->treeview), path))
        {
            midori_bookmarks_add_item_to_model (GTK_TREE_STORE (model), &iter, item);
        }

        gtk_tree_path_free (path);
    }
}

static void
midori_bookmarks_update_item_cb (KatzeArray*      array,
				 KatzeItem*       item,
				 MidoriBookmarks* bookmarks)
{
    gint64 id = katze_item_get_meta_integer (item, "id");
    gint64 parentid = katze_item_get_meta_integer (item, "parentid");
    GtkTreeModel* model = gtk_tree_view_get_model (GTK_TREE_VIEW (bookmarks->treeview));
    GtkTreeIter iter;

    if (midori_bookmarks_reach_item (model, &iter, id))
    {
        gint64 old_parentid = 0;
        GtkTreeIter parent;

        if (gtk_tree_model_iter_parent (model, &parent, &iter))
        {
            KatzeItem* old_parent;

            gtk_tree_model_get (model, &parent, 0, &old_parent, -1);

            old_parentid = katze_item_get_meta_integer (old_parent, "id");

            g_object_unref (old_parent);

            if (parentid == old_parentid)
            {
                midori_bookmarks_update_item_in_model (bookmarks, GTK_TREE_STORE (model), &iter, item);
            }
            else
            {
                gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

                if (!gtk_tree_model_iter_has_child (model, &parent))
                {
                    GtkTreePath* path = gtk_tree_model_get_path(model, &parent);

                    if (gtk_tree_view_row_expanded (GTK_TREE_VIEW (bookmarks->treeview), path))
                        gtk_tree_view_collapse_row (GTK_TREE_VIEW (bookmarks->treeview), path);

                    gtk_tree_path_free (path);
                }

                midori_bookmarks_add_item (item, bookmarks);
            }
        }
        else if (parentid == 0)
        {
            midori_bookmarks_update_item_in_model (bookmarks, GTK_TREE_STORE (model), &iter, item);
        }
        else
        {
            gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

            midori_bookmarks_add_item (item, bookmarks);
        }
    }
    else
        midori_bookmarks_add_item (item, bookmarks);
}

static void
midori_bookmarks_remove_item_cb (KatzeArray*      array,
                                 KatzeItem*       item,
                                 MidoriBookmarks* bookmarks)
{
    gint64 id = katze_item_get_meta_integer (item, "id");
    GtkTreeModel* model = gtk_tree_view_get_model (GTK_TREE_VIEW (bookmarks->treeview));
    GtkTreeIter iter;

    if (midori_bookmarks_reach_item (model, &iter, id))
    {
        GtkTreeIter parent;

        if (gtk_tree_model_iter_parent (model, &parent, &iter))
        {
            gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

            if (!gtk_tree_model_iter_has_child (model, &parent))
            {
                GtkTreePath* path = gtk_tree_model_get_path(model, &parent);

                if (gtk_tree_view_row_expanded (GTK_TREE_VIEW (bookmarks->treeview), path))
                    gtk_tree_view_collapse_row (GTK_TREE_VIEW (bookmarks->treeview), path);

                gtk_tree_path_free (path);
            }
        }
        else
        {
            gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
        }
    }
}

static void
midori_bookmarks_update_cb (KatzeArray*      array,
                            MidoriBookmarks* bookmarks)
{
    GtkTreeModel* model = gtk_tree_view_get_model (GTK_TREE_VIEW (bookmarks->treeview));
    gtk_tree_store_clear (GTK_TREE_STORE (model));
    midori_bookmarks_read_from_db_to_model (bookmarks,
        GTK_TREE_STORE (model), NULL, 0, bookmarks->filter);
}


static void
midori_bookmarks_row_changed_cb (GtkTreeModel*    model,
                                 GtkTreePath*     path,
                                 GtkTreeIter*     iter,
                                 MidoriBookmarks* bookmarks)
{
    KatzeItem* item;
    GtkTreeIter parent;
    KatzeItem* new_parent = NULL;
    gint64 parentid;

    gtk_tree_model_get (model, iter, 0, &item, -1);

    if (gtk_tree_model_iter_parent (model, &parent, iter))
    {
        gtk_tree_model_get (model, &parent, 0, &new_parent, -1);

        /* Bookmarks must not be moved into non-folder items */
        if (!KATZE_ITEM_IS_FOLDER (new_parent))
            parentid = 0;
        else
            parentid = katze_item_get_meta_integer (new_parent, "id");
    }
    else
        parentid = 0;

    katze_item_set_meta_integer (item, "parentid", parentid);

    g_signal_handlers_block_by_func (bookmarks->bookmarks_db,
        midori_bookmarks_update_item_cb,
        bookmarks);

    midori_bookmarks_db_update_item (bookmarks->bookmarks_db, item);

    g_signal_handlers_unblock_by_func (bookmarks->bookmarks_db,
        midori_bookmarks_update_item_cb,
        bookmarks);

    g_object_unref (item);
    if (new_parent)
        g_object_unref (new_parent);
}

static void
midori_bookmarks_add_clicked_cb (GtkWidget* toolitem,
                                                 MidoriBookmarks* bookmarks)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (toolitem);
    GtkTreeView* treeview = GTK_TREE_VIEW (bookmarks->treeview);
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* parent = NULL;

    if (katze_tree_view_get_selected_iter (treeview,
            &model, &iter))
    {
        gboolean done = FALSE;
        while (!done)
        {
            gtk_tree_model_get (model, &iter, 0, &parent, -1);

            if (KATZE_ITEM_IS_FOLDER (parent))
            {
                GtkTreePath* path = gtk_tree_model_get_path(model, &iter);

                if (!gtk_tree_view_row_expanded (treeview, path))
                    gtk_tree_view_expand_row (treeview, path, FALSE);

                gtk_tree_path_free (path);
                done = TRUE;
            }
            else
            {
                GtkTreeIter child = iter;

                if (parent) g_object_unref (parent);
                parent = NULL;

                if (!gtk_tree_model_iter_parent (model, &iter, &child))
                {
                    done = TRUE;
                }
            }
        }
    }

    if (g_str_equal (gtk_widget_get_name (toolitem), "BookmarkFolderAdd"))
        midori_browser_edit_bookmark_dialog_new (browser, parent, TRUE, TRUE, toolitem);
    else
        midori_browser_edit_bookmark_dialog_new (browser, parent, TRUE, FALSE, toolitem);

    if (parent)
        g_object_unref (parent);
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
        MidoriBrowser* browser;

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        g_assert (!KATZE_ITEM_IS_SEPARATOR (item));

        browser = midori_browser_get_for_widget (bookmarks->treeview);
        midori_browser_edit_bookmark_dialog_new (
            browser, item, FALSE, KATZE_ITEM_IS_FOLDER (item), NULL);

        g_object_unref (item);
    }
}

static void
midori_bookmarks_toolbar_update (MidoriBookmarks *bookmarks)
{
    gboolean selected;

    selected = katze_tree_view_get_selected_iter (
        GTK_TREE_VIEW (bookmarks->treeview), NULL, NULL);
    gtk_widget_set_sensitive (GTK_WIDGET (bookmarks->delete), selected);
    gtk_widget_set_sensitive (GTK_WIDGET (bookmarks->edit), selected);
}

static gchar* 
midori_bookmarks_statusbar_bookmarks_str (gint count)
{
    if (!count)
        return NULL;

    /* i18n: [n] bookmark(s) */
    return g_strdup_printf (ngettext ("%d bookmark", "%d bookmarks", count), count);
}

static gchar* 
midori_bookmarks_statusbar_subfolders_str (gint count)
{
    if (!count)
        return NULL;

    /* i18n: [n] subfolder(s) */
    return g_strdup_printf (ngettext ("%d subfolder", "%d subfolders", count), count);
}

static void
midori_bookmarks_statusbar_update (MidoriBookmarks *bookmarks)
{
    gchar* text = NULL;

    if (bookmarks->hovering_item)
    {
        KatzeItem* item = bookmarks->hovering_item;

        g_assert (!KATZE_ITEM_IS_SEPARATOR (item));

        if (KATZE_ITEM_IS_FOLDER (item))
        {
            gint child_folders_count = midori_bookmarks_db_count_recursive (bookmarks->bookmarks_db,
                "uri = ''", NULL, item, FALSE);
            gint child_bookmarks_count = midori_bookmarks_db_count_recursive (bookmarks->bookmarks_db,
                "uri <> ''", NULL, item, FALSE);
            gchar* child_folders_str = midori_bookmarks_statusbar_subfolders_str (child_folders_count);
            gchar* child_bookmarks_str = midori_bookmarks_statusbar_bookmarks_str (child_bookmarks_count);

            if (!child_bookmarks_count && !child_folders_count)
                /* i18n: Empty folder */
                text = g_strdup_printf (_("Empty folder"));
            else if (!child_bookmarks_count && (child_folders_count >= 1))
                /* i18n: Folder containing [[n] folder(s)] and no bookmark */
                text = g_strdup_printf (_("Folder containing %s and no bookmark"),
                    child_folders_str);
            else if ((child_bookmarks_count >= 1) && !child_folders_count)
                /* i18n: Folder containing [[n] bookmark(s)] */
               text = g_strdup_printf (_("Folder containing %s"), child_bookmarks_str);
            else if ((child_bookmarks_count >= 1) && (child_folders_count >= 1))
                /* i18n: Folder containing [[n] bookmark(s)] and [[n] folder(s)] */
                text = g_strdup_printf (_("Folder containing %s and %s"),
                    child_bookmarks_str, child_folders_str);

            g_free (child_folders_str);
            g_free (child_bookmarks_str);
        }
        else if (KATZE_ITEM_IS_BOOKMARK (item))
        {
            const gchar* uri = katze_item_get_uri (item);

            /* i18n: Bookmark leading to: [bookmark uri] */
            text = g_strdup_printf (_("Bookmark leading to: %s"), uri);
        }
    }
    else
    {
        gint child_folders_count = midori_bookmarks_db_count_recursive (bookmarks->bookmarks_db,
            "uri = ''", NULL, NULL, FALSE);
        gint child_bookmarks_count = midori_bookmarks_db_count_recursive (bookmarks->bookmarks_db,
            "uri <> ''", NULL, NULL, FALSE);
        gchar* child_folders_str = midori_bookmarks_statusbar_subfolders_str (child_folders_count);
        gchar* child_bookmarks_str = midori_bookmarks_statusbar_bookmarks_str (child_bookmarks_count);
        
        if (!child_bookmarks_count && (child_folders_count >= 1))
            /* i18n: [[n] folder(s)] and no bookmark */
            text = g_strdup_printf (_("%s and no bookmark"),
                child_folders_str);
        else if ((child_bookmarks_count >= 1) && !child_folders_count)
            text = g_strdup (child_bookmarks_str);
        else if ((child_bookmarks_count >= 1) && (child_folders_count >= 1))
            /* i18n: [[n] bookmark(s)] and [[n] folder(s)] */
            text = g_strdup_printf (_("%s and %s"),
                child_bookmarks_str, child_folders_str);
        
        g_free (child_folders_str);
        g_free (child_bookmarks_str);
    }

    if (text)
    {
        MidoriBrowser* browser = midori_browser_get_for_widget (bookmarks->treeview);
        
        g_object_set (browser, "statusbar-text", text, NULL);
        
        g_free(text);
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

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        midori_bookmarks_db_remove_item (bookmarks->bookmarks_db, item);

        g_object_unref (item);
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
        bookmarks->toolbar = toolbar;
        toolitem = gtk_tool_button_new_from_stock (STOCK_BOOKMARK_ADD);
        gtk_widget_set_name (GTK_WIDGET (toolitem), "BookmarkAdd");
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
        midori_bookmarks_toolbar_update (bookmarks);
        midori_bookmarks_statusbar_update (bookmarks);
        toolitem = gtk_separator_tool_item_new ();
        gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (toolitem), FALSE);
        gtk_tool_item_set_expand (toolitem, TRUE);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        toolitem = gtk_tool_button_new_from_stock (STOCK_FOLDER_NEW);
        gtk_widget_set_name (GTK_WIDGET (toolitem), "BookmarkFolderAdd");
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                     _("Add a new folder"));
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_bookmarks_add_clicked_cb), bookmarks);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));

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
midori_bookmarks_set_app (MidoriBookmarks* bookmarks,
                          MidoriApp*       app)
{
    GtkTreeModel* model;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (bookmarks->treeview));
    if (bookmarks->bookmarks_db)
    {
        g_object_unref (bookmarks->bookmarks_db);
        gtk_tree_store_clear (GTK_TREE_STORE (model));
    }
    katze_assign (bookmarks->app, app);
    if (!app)
        return;

    g_object_ref (app);
    bookmarks->bookmarks_db = katze_object_get_object (app, "bookmarks");
    midori_bookmarks_read_from_db_to_model (bookmarks, GTK_TREE_STORE (model), NULL, 0, NULL);
    g_signal_connect_object (bookmarks->bookmarks_db, "add-item",
                             G_CALLBACK (midori_bookmarks_add_item_cb), bookmarks, G_CONNECT_AFTER);
    g_signal_connect_object (bookmarks->bookmarks_db, "update-item",
                             G_CALLBACK (midori_bookmarks_update_item_cb), bookmarks, G_CONNECT_AFTER);
    g_signal_connect_object (bookmarks->bookmarks_db, "remove-item",
                             G_CALLBACK (midori_bookmarks_remove_item_cb), bookmarks, 0);
    g_signal_connect_object (bookmarks->bookmarks_db, "update",
                             G_CALLBACK (midori_bookmarks_update_cb), bookmarks, 0);
    g_signal_connect_object (model, "row-changed",
                             G_CALLBACK (midori_bookmarks_row_changed_cb),
                             bookmarks, G_CONNECT_AFTER);
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

    pixbuf = katze_item_get_pixbuf (item, treeview);
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

    if (item && katze_item_get_name (item))
    {
        g_object_set (renderer, "markup", NULL,
                      "ellipsize", PANGO_ELLIPSIZE_END,
                      "text", katze_item_get_name (item), NULL);
    }
    else
        g_object_set (renderer, "markup", _("<i>Separator</i>"), NULL);

    if (item)
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

    model = gtk_tree_view_get_model (treeview);

    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        if (KATZE_ITEM_IS_BOOKMARK (item))
        {
            MidoriBrowser* browser;

            browser = midori_browser_get_for_widget (GTK_WIDGET (bookmarks));
            midori_browser_open_bookmark (browser, item);
            g_object_unref (item);
            return;
        }
        if (gtk_tree_view_row_expanded (treeview, path))
            gtk_tree_view_collapse_row (treeview, path);
        else
            gtk_tree_view_expand_row (treeview, path, FALSE);
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

    uri = KATZE_ITEM_IS_BOOKMARK (item) ? katze_item_get_uri (item) : NULL;

    menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);
    if (label)
        gtk_label_set_text_with_mnemonic (GTK_LABEL (gtk_bin_get_child (
        GTK_BIN (menuitem))), label);
    if (!strcmp (stock_id, GTK_STOCK_EDIT))
        gtk_widget_set_sensitive (menuitem,
            !KATZE_ITEM_IS_SEPARATOR (item));
    else if (!KATZE_ITEM_IS_FOLDER (item) && strcmp (stock_id, GTK_STOCK_DELETE))
        gtk_widget_set_sensitive (menuitem, uri != NULL);
    g_object_set_data (G_OBJECT (menuitem), "KatzeItem", item);
    if (callback)
        g_signal_connect (menuitem, "activate", G_CALLBACK (callback), bookmarks);
    else
        gtk_widget_set_sensitive (menuitem, FALSE);
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

    if ((uri = katze_item_get_uri (item)) && *uri)
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

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    if (KATZE_ITEM_IS_FOLDER (item))
    {
        KatzeItem* child;
        KatzeArray* array;

        array = midori_bookmarks_read_from_db (bookmarks,
                katze_item_get_meta_integer (item, "id"), NULL);

        g_return_if_fail (KATZE_IS_ARRAY (array));
        KATZE_ARRAY_FOREACH_ITEM (child, array)
        {
            if ((uri = katze_item_get_uri (child)) && *uri)
            {
                MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (bookmarks));
                GtkWidget* view = midori_browser_add_item (browser, child);
                midori_browser_set_current_tab_smartly (browser, view);
            }
        }
        g_object_unref (G_OBJECT (array));
    }
    else if ((uri = katze_item_get_uri (item)) && *uri)
    {
        MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (bookmarks));
        GtkWidget* view = midori_browser_add_item (browser, item);
        midori_browser_set_current_tab_smartly (browser, view);
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
        MidoriBrowser* new_browser = midori_app_create_browser (bookmarks->app);
        midori_app_add_browser (bookmarks->app, new_browser);
        gtk_widget_show (GTK_WIDGET (new_browser));
        midori_browser_add_uri (new_browser, uri);
    }
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
    if (KATZE_ITEM_IS_FOLDER (item))
    {
        gint child_bookmarks_count = midori_bookmarks_db_count_recursive (bookmarks->bookmarks_db,
            "uri <> ''", NULL, item, FALSE);

        midori_bookmarks_popup_item (menu,
            STOCK_TAB_NEW, _("Open all in _Tabs"), item, 
            (!child_bookmarks_count ? NULL : midori_bookmarks_open_in_tab_activate_cb), 
            bookmarks);
    }
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
        item, midori_bookmarks_edit_clicked_cb, bookmarks);
    midori_bookmarks_popup_item (menu, GTK_STOCK_DELETE, NULL,
        item, midori_bookmarks_delete_clicked_cb, bookmarks);

    katze_widget_popup (widget, GTK_MENU (menu), event, KATZE_MENU_POSITION_CURSOR);
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
            const gchar* uri;
            if (KATZE_ITEM_IS_BOOKMARK (item) && (uri = katze_item_get_uri (item)) && *uri)
            {
                MidoriBrowser* browser = midori_browser_get_for_widget (widget);
                GtkWidget* view = midori_browser_add_uri (browser, uri);
                midori_browser_set_current_tab (browser, view);
            }
        }
        else
            midori_bookmarks_popup (widget, event, item, bookmarks);

        if (item != NULL)
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
    if (event->keyval == GDK_KEY_Delete)
        midori_bookmarks_delete_clicked_cb (widget, bookmarks);

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
midori_bookmarks_row_expanded_cb (GtkTreeView*     treeview,
                                  GtkTreeIter*     iter,
                                  GtkTreePath*     path,
                                  MidoriBookmarks* bookmarks)
{
    GtkTreeModel* model;
    KatzeItem* item;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
    gtk_tree_model_get (model, iter, 0, &item, -1);
    midori_bookmarks_read_from_db_to_model (bookmarks, GTK_TREE_STORE (model),
                                            iter, katze_item_get_meta_integer (item, "id"), NULL);
    g_object_unref (item);
}

static void
midori_bookmarks_row_collapsed_cb (GtkTreeView *treeview,
                                   GtkTreeIter *parent,
                                   GtkTreePath *path,
                                   gpointer     user_data)
{
    GtkTreeModel* model;
    GtkTreeStore* treestore;
    GtkTreeIter child;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
    treestore = GTK_TREE_STORE (model);
    while (gtk_tree_model_iter_nth_child (model, &child, parent, 0))
        gtk_tree_store_remove (treestore, &child);
    /* That's an invisible dummy, so we always have an expander */
    gtk_tree_store_insert_with_values (treestore, &child, parent,
        0, 0, NULL, -1);
}

static void
midori_bookmarks_selection_changed_cb (GtkTreeSelection *treeview,
                                       MidoriBookmarks  *bookmarks)
{
    midori_bookmarks_toolbar_update (bookmarks);
}

static KatzeItem*
midori_bookmarks_get_item_at_pos (GtkTreeView *treeview,
                                  gint x, gint y)
{    
    GtkTreeModel* model = gtk_tree_view_get_model (treeview);
    GtkTreePath* path;
    GtkTreeIter iter;
    KatzeItem* item = NULL;

    gtk_tree_view_get_path_at_pos (treeview, x, y,
                                   &path, NULL, NULL, NULL);
    
    if (!path)
        return NULL;
    
    if (gtk_tree_model_get_iter (model, &iter, path))
        gtk_tree_model_get (model, &iter, 0, &item, -1);
    
    gtk_tree_path_free (path);

    return item;
}

static gboolean
midori_bookmarks_enter_notify_event_cb (GtkTreeView      *treeview,
                                        GdkEventCrossing *event,
                                        MidoriBookmarks  *bookmarks)
{
    KatzeItem* item = midori_bookmarks_get_item_at_pos (treeview, event->x, event->y);

    if (bookmarks->hovering_item)
        g_object_unref (bookmarks->hovering_item);

    bookmarks->hovering_item = item;

    midori_bookmarks_statusbar_update (bookmarks);

    return FALSE;
}

static gboolean
midori_bookmarks_motion_notify_event_cb (GtkTreeView     *treeview,
                                         GdkEventMotion  *event,
                                         MidoriBookmarks *bookmarks)
{
    gboolean item_changed;
    KatzeItem* item;
    gint x;
    gint y;

    if (event->is_hint)
        gtk_widget_get_pointer (GTK_WIDGET (treeview), &x, &y);
    else
    {
        x = event->x;
        y = event->y;
    }

    item = midori_bookmarks_get_item_at_pos (treeview, x, y);

    item_changed = (bookmarks->hovering_item != item) ? TRUE : FALSE;

    if (!item_changed)
    {
        if (item)
            g_object_unref (item);
    }
    else
    {
        if (bookmarks->hovering_item)
            g_object_unref (bookmarks->hovering_item);

        bookmarks->hovering_item = item;

        midori_bookmarks_statusbar_update (bookmarks);
    }

    return FALSE;
}

static gboolean
midori_bookmarks_leave_notify_event_cb (GtkTreeView      *treeview,
                                        GdkEventCrossing *event,
                                        MidoriBookmarks  *bookmarks)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (bookmarks->treeview);

    if (bookmarks->hovering_item)
        g_object_unref (bookmarks->hovering_item);
    
    bookmarks->hovering_item = NULL;

    g_object_set (browser, "statusbar-text", "", NULL);

    return FALSE;
}

static gboolean
midori_bookmarks_filter_timeout_cb (gpointer data)
{
    MidoriBookmarks* bookmarks = data;
    GtkTreeModel* model;
    GtkTreeStore* treestore;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (bookmarks->treeview));
    treestore = GTK_TREE_STORE (model);

    gtk_tree_store_clear (treestore);
    midori_bookmarks_read_from_db_to_model (bookmarks,
        treestore, NULL, 0, bookmarks->filter);

    return FALSE;
}

static void
midori_bookmarks_filter_entry_changed_cb (GtkEntry*        entry,
                                          MidoriBookmarks* bookmarks)
{
    if (bookmarks->filter_timeout)
        g_source_remove (bookmarks->filter_timeout);

    if (!g_object_get_data (G_OBJECT (entry), "sokoke_showing_default"))
        katze_assign (bookmarks->filter, g_strdup (gtk_entry_get_text (entry)));
    else
        katze_assign (bookmarks->filter, NULL);

    bookmarks->filter_timeout = midori_timeout_add (COMPLETION_DELAY,
        midori_bookmarks_filter_timeout_cb, bookmarks, NULL);
}

static void
midori_bookmarks_init (MidoriBookmarks* bookmarks)
{
    GtkWidget* entry;
    GtkWidget* box;
    GtkTreeStore* model;
    GtkWidget* treeview;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_pixbuf;
    GtkCellRenderer* renderer_text;
    GtkTreeSelection* selection;

    /* Create the filter entry */
    entry = sokoke_search_entry_new (_("Search Bookmarks"));
    g_signal_connect_after (entry, "changed",
        G_CALLBACK (midori_bookmarks_filter_entry_changed_cb), bookmarks);
    box = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 3);
    gtk_widget_show_all (box);
    gtk_box_pack_start (GTK_BOX (bookmarks), box, FALSE, FALSE, 5);

    /* Create the treeview */
    model = gtk_tree_store_new (2, KATZE_TYPE_ITEM, G_TYPE_STRING);
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (treeview), 1);
    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_expand (column, TRUE);
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_bookmarks_treeview_render_icon_cb,
        treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, TRUE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_bookmarks_treeview_render_text_cb,
        treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    gtk_tree_view_set_reorderable (GTK_TREE_VIEW (treeview), TRUE);
    g_object_unref (model);
    g_object_connect (treeview,
                      "signal::row-activated",
                      midori_bookmarks_row_activated_cb, bookmarks,
                      "signal::button-release-event",
                      midori_bookmarks_button_release_event_cb, bookmarks,
                      "signal::key-release-event",
                      midori_bookmarks_key_release_event_cb, bookmarks,
                      "signal::popup-menu",
                      midori_bookmarks_popup_menu_cb, bookmarks,
                      "signal::row-expanded",
                      midori_bookmarks_row_expanded_cb, bookmarks,
                      "signal::row-collapsed",
                      midori_bookmarks_row_collapsed_cb, bookmarks,
                      "signal::enter-notify-event",
                      midori_bookmarks_enter_notify_event_cb, bookmarks,
                      "signal::motion-notify-event",
                      midori_bookmarks_motion_notify_event_cb, bookmarks,
                      "signal::leave-notify-event",
                      midori_bookmarks_leave_notify_event_cb, bookmarks,
                      NULL);
    gtk_widget_add_events (GTK_WIDGET (treeview), 
                           GDK_POINTER_MOTION_MASK
                           | GDK_POINTER_MOTION_HINT_MASK);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    g_signal_connect_after (selection, "changed",
                            G_CALLBACK (midori_bookmarks_selection_changed_cb),
                            bookmarks);
    gtk_widget_show (treeview);
    gtk_box_pack_start (GTK_BOX (bookmarks), treeview, TRUE, TRUE, 0);
    bookmarks->treeview = treeview;
    bookmarks->hovering_item = NULL;
}

static void
midori_bookmarks_finalize (GObject* object)
{
    MidoriBookmarks* bookmarks = MIDORI_BOOKMARKS (object);

    if (bookmarks->app)
        g_object_unref (bookmarks->app);
    if (bookmarks->hovering_item)
	g_object_unref (bookmarks->hovering_item);
}
