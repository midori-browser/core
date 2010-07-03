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
#include "midori-stock.h"
#include "midori-view.h"
#include "midori-viewable.h"

#include "sokoke.h"

#include <glib/gi18n.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>

gboolean
midori_browser_edit_bookmark_dialog_new (MidoriBrowser* browser,
                                         KatzeItem*     bookmark,
                                         gboolean       new_bookmark,
                                         gboolean       is_folder);

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
    KatzeArray* array;
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

#if HAVE_SQLITE
void
midori_bookmarks_import_array_db (sqlite3*     db,
                                  KatzeArray*  array,
                                  const gchar* folder)
{
    GList* list = NULL;
    GList* bookmarks;

    bookmarks = katze_array_get_items ((KatzeArray*)array);
    for (list = bookmarks; list != NULL; list = g_list_next (list))
    {
        KatzeItem* item;

        if (KATZE_IS_ARRAY (list->data))
            midori_bookmarks_import_array_db (db, list->data, folder);
        item = (KatzeItem*) list->data;
        midori_bookmarks_insert_item_db (db, item, folder);
    }
}

static KatzeArray*
midori_bookmarks_read_from_db (MidoriBookmarks* bookmarks,
                               const gchar*     folder)
{
    sqlite3* db;
    sqlite3_stmt* statement;
    gint result;
    const gchar* sqlcmd;

    db = g_object_get_data (G_OBJECT (bookmarks->array), "db");

    sqlcmd = "SELECT uri, title, type, app, toolbar, folder from bookmarks where folder = ?"
             " ORDER BY type DESC";
    result = sqlite3_prepare_v2 (db, sqlcmd, -1, &statement, NULL);
    sqlite3_bind_text (statement, 1, g_strdup(folder), -1, g_free);

    if (result != SQLITE_OK)
        return NULL;

    return katze_array_from_statement (statement);
}

static void
midori_bookmarks_read_from_db_to_model (MidoriBookmarks* bookmarks,
                                        GtkTreeStore*    model,
                                        GtkTreeIter*     parent,
                                        const gchar*     folder)
{
    KatzeArray* array;
    gint last;
    GtkTreeIter child;

    array = midori_bookmarks_read_from_db (bookmarks, folder);
    katze_bookmark_populate_tree_view (array, model, parent);
    /* Remove invisible dummy row */
    last = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), parent);
    gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (model), &child, parent, last - 1);
    gtk_tree_store_remove (model, &child);
}

void
midori_bookmarks_insert_item_db (sqlite3*     db,
                                 KatzeItem*   item,
                                 const gchar* folder)
{
    gchar* sqlcmd;
    char* errmsg = NULL;
    gchar* parent;
    gchar* uri;

    if (KATZE_ITEM_IS_BOOKMARK (item))
        uri = g_strdup (katze_item_get_uri (item));
    else
        uri = g_strdup ("");

    if (folder)
        parent = g_strdup (folder);
    else if (katze_item_get_name (katze_item_get_parent (item)))
        parent = g_strdup (katze_item_get_name (katze_item_get_parent (item)));
    else
        parent = g_strdup ("");

    /* Workaround a sqlite3_mprintf error with
       handling of katze_item_get_meta_integer(). */
    /* FIXME: Need proper single quote escaping. */
    sqlcmd = g_strdup_printf (
            "INSERT into bookmarks (uri, title, folder, toolbar, app) values"
            " ('%s', '%s', '%s', %d, %d)",
            uri,
            katze_item_get_name (item),
            parent,
            katze_item_get_meta_boolean (item, "toolbar"),
            katze_item_get_meta_boolean (item, "app"));

    if (sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        g_printerr (_("Failed to add bookmark item: %s\n"), errmsg);
        sqlite3_free (errmsg);
    }

    g_free (uri);
    g_free (parent);
    g_free (sqlcmd);
}

void
midori_bookmarks_remove_item_from_db (sqlite3*   db,
                                      KatzeItem* item)
{
    gchar* sqlcmd;
    char* errmsg = NULL;

    if (KATZE_ITEM_IS_BOOKMARK (item))
        sqlcmd = sqlite3_mprintf (
            "DELETE FROM bookmarks WHERE uri = '%q' "
            " AND folder = '%q'",
            katze_item_get_uri (item),
            katze_item_get_meta_string (item, "folder"));

    else
       sqlcmd = sqlite3_mprintf (
            "DELETE FROM bookmarks WHERE title = '%q'"
            " AND folder = '%q'",
            katze_item_get_name (item),
            katze_item_get_meta_string (item, "folder"));

    if (sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        g_printerr (_("Failed to remove history item: %s\n"), errmsg);
        sqlite3_free (errmsg);
    }

    sqlite3_free (sqlcmd);
}
#endif

static void
midori_bookmarks_row_changed_cb (GtkTreeModel*    model,
                                 GtkTreePath*     path,
                                 GtkTreeIter*     iter,
                                 MidoriBookmarks* bookmarks)
{
    KatzeItem* parent_item;
    KatzeItem* item;
    GtkTreeIter parent;
    sqlite3* db;
    gchar* parent_name;

    db = g_object_get_data (G_OBJECT (bookmarks->array), "db");
    gtk_tree_model_get (model, iter, 0, &item, -1);

    if (gtk_tree_model_iter_parent (model, &parent, iter))
    {
        gtk_tree_model_get (model, &parent , 0, &parent_item, -1);
        parent_name = g_strdup (katze_item_get_name (parent_item));
    }
    else
        parent_name = g_strdup ("");

    midori_bookmarks_remove_item_from_db (db, item);
    midori_bookmarks_insert_item_db (db, item, parent_name);
}

static void
midori_bookmarks_add_clicked_cb (GtkWidget* toolitem)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (toolitem);
    /* FIXME: Take selected folder into account */
    if (g_str_equal (gtk_widget_get_name (toolitem), "BookmarkFolderAdd"))
        midori_browser_edit_bookmark_dialog_new (browser, NULL, TRUE, TRUE);
    else
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
        MidoriBrowser* browser;

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        g_assert (!KATZE_ITEM_IS_SEPARATOR (item));

        browser = midori_browser_get_for_widget (bookmarks->treeview);
        if (midori_browser_edit_bookmark_dialog_new (
            browser, item, FALSE, KATZE_ITEM_IS_FOLDER (item)))
        {
            gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
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
    #if HAVE_SQLITE
    sqlite3* db;

    db = g_object_get_data (G_OBJECT (bookmarks->array), "db");
    #endif

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (bookmarks->treeview),
                                           &model, &iter))
    {
        KatzeItem* item;

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        #if HAVE_SQLITE
        midori_bookmarks_remove_item_from_db (db, item);
        #endif
        gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

        g_object_unref (item);
    }
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
        gtk_tree_model_get (model, &iter, 0, &item, -1);

        gtk_widget_set_sensitive (bookmarks->edit, !KATZE_ITEM_IS_SEPARATOR (item));
        gtk_widget_set_sensitive (bookmarks->delete, TRUE);

        if (item != NULL)
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
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        bookmarks->toolbar = toolbar;
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_ADD);
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
        toolitem = gtk_separator_tool_item_new ();
        gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (toolitem), FALSE);
        gtk_tool_item_set_expand (toolitem, TRUE);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_DIRECTORY);
        gtk_widget_set_name (GTK_WIDGET (toolitem), "BookmarkFolderAdd");
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                     _("Add a new folder"));
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_bookmarks_add_clicked_cb), bookmarks);
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
midori_bookmarks_set_app (MidoriBookmarks* bookmarks,
                          MidoriApp*       app)
{
    GtkTreeModel* model;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (bookmarks->treeview));
    if (bookmarks->array)
    {
        g_object_unref (bookmarks->array);
        gtk_tree_store_clear (GTK_TREE_STORE (model));
    }
    katze_assign (bookmarks->app, app);
    if (!app)
        return;

    g_object_ref (app);
    bookmarks->array = katze_object_get_object (app, "bookmarks");
    g_object_set_data (G_OBJECT (bookmarks->array), "treeview", bookmarks->treeview);

    #if HAVE_SQLITE
    midori_bookmarks_read_from_db_to_model (bookmarks, GTK_TREE_STORE (model), NULL, "");
    g_signal_connect_after (model, "row-changed",
                            G_CALLBACK (midori_bookmarks_row_changed_cb),
                            bookmarks);
    #endif
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
    if (KATZE_ITEM_IS_FOLDER (item))
        pixbuf = gtk_widget_render_icon (treeview, GTK_STOCK_DIRECTORY,
                                         GTK_ICON_SIZE_MENU, NULL);
    else if (KATZE_ITEM_IS_BOOKMARK (item))
        pixbuf = katze_load_cached_icon (katze_item_get_uri (item), treeview);
    g_object_set (renderer, "pixbuf", pixbuf, NULL);

    if (pixbuf)
        g_object_unref (pixbuf);

    if (item)
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
        g_object_set (renderer, "markup", NULL,
                      "text", katze_item_get_name (item), NULL);
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
    guint n;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    if (KATZE_ITEM_IS_FOLDER (item))
    {
        KatzeItem* child;
        KatzeArray* array;
        guint i = 0;

        array = midori_bookmarks_read_from_db (bookmarks, katze_item_get_name (item));
        g_return_if_fail (KATZE_IS_ARRAY (array));
        while ((child = katze_array_get_nth_item (KATZE_ARRAY (array), i)))
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
    else if ((uri = katze_item_get_uri (item)) && *uri)
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
                MidoriBrowser* browser;
                gint n;

                browser = midori_browser_get_for_widget (widget);
                n = midori_browser_add_uri (browser, uri);
                midori_browser_set_current_page (browser, n);
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
midori_bookmarks_row_expanded_cb (GtkTreeView*     treeview,
                                  GtkTreeIter*     iter,
                                  GtkTreePath*     path,
                                  MidoriBookmarks* bookmarks)
{
    GtkTreeModel* model;
    KatzeItem* item;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
    gtk_tree_model_get (model, iter, 0, &item, -1);
    #if HAVE_SQLITE
    midori_bookmarks_read_from_db_to_model (bookmarks, GTK_TREE_STORE (model),
                                            iter, katze_item_get_name (item));
    #endif
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
midori_bookmarks_init (MidoriBookmarks* bookmarks)
{
    GtkTreeStore* model;
    GtkWidget* treeview;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_pixbuf;
    GtkCellRenderer* renderer_text;

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
    gtk_tree_view_set_reorderable (GTK_TREE_VIEW (treeview), TRUE);
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
                      "signal::row-expanded",
                      midori_bookmarks_row_expanded_cb, bookmarks,
                      "signal::row-collapsed",
                      midori_bookmarks_row_collapsed_cb, bookmarks,
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
