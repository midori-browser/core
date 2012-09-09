/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-transfers.h"

#include "midori-app.h"
#include "midori-browser.h"
#include "midori-platform.h"
#include "midori-view.h"
#include "midori-core.h"

#include <glib/gi18n.h>

struct _MidoriTransfers
{
    GtkVBox parent_instance;

    GtkWidget* toolbar;
    GtkWidget* treeview;
    MidoriApp* app;
};

struct _MidoriTransfersClass
{
    GtkVBoxClass parent_class;
};

static void
midori_transfers_viewable_iface_init (MidoriViewableIface* iface);

G_DEFINE_TYPE_WITH_CODE (MidoriTransfers, midori_transfers, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (MIDORI_TYPE_VIEWABLE,
                             midori_transfers_viewable_iface_init));

enum
{
    PROP_0,

    PROP_APP
};

static void
midori_transfers_set_property (GObject*      object,
                               guint         prop_id,
                               const GValue* value,
                               GParamSpec*   pspec);

static void
midori_transfers_get_property (GObject*    object,
                               guint       prop_id,
                               GValue*     value,
                               GParamSpec* pspec);

static void
midori_transfers_class_init (MidoriTransfersClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->set_property = midori_transfers_set_property;
    gobject_class->get_property = midori_transfers_get_property;

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
midori_transfers_get_label (MidoriViewable* viewable)
{
    return _("Transfers");
}

static const gchar*
midori_transfers_get_stock_id (MidoriViewable* viewable)
{
    return STOCK_TRANSFER;
}

static void
midori_transfers_button_clear_clicked_cb (GtkToolItem*    toolitem,
                                         MidoriTransfers* transfers)
{
    GtkTreeModel* model = gtk_tree_view_get_model (
        GTK_TREE_VIEW (transfers->treeview));
    GtkTreeIter iter;
    gint n = 0;
    while ((gtk_tree_model_iter_nth_child (model, &iter, NULL, n++)))
    {
        WebKitDownload* download;

        gtk_tree_model_get (model, &iter, 1, &download, -1);

        if (midori_download_is_finished (download))
        {
            gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
            n--; /* Decrement n since we just removed it */
        }
        g_object_unref (download);
    }
}

static GtkWidget*
midori_transfers_get_toolbar (MidoriViewable* transfers)
{
    if (!MIDORI_TRANSFERS (transfers)->toolbar)
    {
        GtkWidget* toolbar;
        GtkToolItem* toolitem;

        toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        toolitem = gtk_tool_item_new ();
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        toolitem = gtk_separator_tool_item_new ();
        gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (toolitem),
                                          FALSE);
        gtk_tool_item_set_expand (toolitem, TRUE);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_CLEAR);
        gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), _("Clear All"));
        gtk_tool_item_set_is_important (toolitem, TRUE);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_transfers_button_clear_clicked_cb), transfers);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        MIDORI_TRANSFERS (transfers)->toolbar = toolbar;
    }

    return MIDORI_TRANSFERS (transfers)->toolbar;
}

static void
midori_transfers_viewable_iface_init (MidoriViewableIface* iface)
{
    iface->get_stock_id = midori_transfers_get_stock_id;
    iface->get_label = midori_transfers_get_label;
    iface->get_toolbar = midori_transfers_get_toolbar;
}

static void
midori_transfers_download_notify_progress_cb (WebKitDownload*  download,
                                              GParamSpec*      pspec,
                                              MidoriTransfers* transfers)
{
    /* FIXME: Update only the appropriate row */
    gtk_widget_queue_draw (transfers->treeview);
}

static void
midori_transfers_download_notify_status_cb (WebKitDownload*  download,
                                            GParamSpec*      pspec,
                                            MidoriTransfers* transfers)
{
    /* FIXME: Update only the appropriate row */
    gtk_widget_queue_draw (transfers->treeview);
}

static void
midori_transfers_browser_add_download_cb (MidoriBrowser*   browser,
                                          WebKitDownload*  download,
                                          MidoriTransfers* transfers)
{
    GtkTreeView* treeview;
    GtkTreeModel* model;

    treeview = GTK_TREE_VIEW (transfers->treeview);
    model = gtk_tree_view_get_model (treeview);
    gtk_list_store_insert_with_values (GTK_LIST_STORE (model),
                                       NULL, G_MAXINT,
                                       0, NULL, 1, download, -1);
    g_signal_connect (download, "notify::progress",
        G_CALLBACK (midori_transfers_download_notify_progress_cb), transfers);
    g_signal_connect (download, "notify::status",
        G_CALLBACK (midori_transfers_download_notify_status_cb), transfers);
}

static void
midori_transfers_set_property (GObject*      object,
                               guint         prop_id,
                               const GValue* value,
                               GParamSpec*   pspec)
{
    MidoriTransfers* transfers = MIDORI_TRANSFERS (object);

    switch (prop_id)
    {
    case PROP_APP:
        transfers->app = g_value_get_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_transfers_get_property (GObject*    object,
                               guint       prop_id,
                               GValue*     value,
                               GParamSpec* pspec)
{
    MidoriTransfers* transfers = MIDORI_TRANSFERS (object);

    switch (prop_id)
    {
    case PROP_APP:
        g_value_set_object (value, transfers->app);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_transfers_treeview_render_icon_cb (GtkTreeViewColumn* column,
                                          GtkCellRenderer*   renderer,
                                          GtkTreeModel*      model,
                                          GtkTreeIter*       iter,
                                          GtkWidget*         treeview)
{
    WebKitDownload* download;
    gchar* content_type;
    GIcon* icon;

    gtk_tree_model_get (model, iter, 1, &download, -1);
    content_type = midori_download_get_content_type (download, NULL);
    icon = g_content_type_get_icon (content_type);
    g_themed_icon_append_name (G_THEMED_ICON (icon), "text-html");

    g_object_set (renderer, "gicon", icon,
                  "stock-size", GTK_ICON_SIZE_DND,
                  "xpad", 1, "ypad", 12, NULL);
    g_free (content_type);
    g_object_unref (icon);
    g_object_unref (download);
}

static void
midori_transfers_treeview_render_text_cb (GtkTreeViewColumn* column,
                                          GtkCellRenderer*   renderer,
                                          GtkTreeModel*      model,
                                          GtkTreeIter*       iter,
                                          GtkWidget*         treeview)
{
    WebKitDownload* download;
    gchar* tooltip;
    gdouble progress;

    gtk_tree_model_get (model, iter, 1, &download, -1);

    tooltip = midori_download_get_tooltip (download);
    progress = midori_download_get_progress (download);
    g_object_set (renderer, "text", tooltip,
                  "value", (gint)(progress * 100),
                  "xpad", 1, "ypad", 6, NULL);
    g_free (tooltip);
    g_object_unref (download);
}

static void
midori_transfers_treeview_render_button_cb (GtkTreeViewColumn* column,
                                            GtkCellRenderer*   renderer,
                                            GtkTreeModel*      model,
                                            GtkTreeIter*       iter,
                                            GtkWidget*         treeview)
{
    WebKitDownload* download;
    const gchar* stock_id;

    gtk_tree_model_get (model, iter, 1, &download, -1);

    stock_id = midori_download_action_stock_id (download);
    g_object_set (renderer, "stock-id", stock_id,
                  "stock-size", GTK_ICON_SIZE_MENU, NULL);

    g_object_unref (download);
}

static void
midori_transfers_treeview_row_activated_cb (GtkTreeView*       treeview,
                                            GtkTreePath*       path,
                                            GtkTreeViewColumn* column,
                                            MidoriTransfers*   transfers)
{
    GtkTreeModel* model = gtk_tree_view_get_model (treeview);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        WebKitDownload* download;

        gtk_tree_model_get (model, &iter, 1, &download, -1);

        if (midori_download_action_clear (download, GTK_WIDGET (treeview), NULL))
            gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        g_object_unref (download);
    }
}

static void
midori_transfers_hierarchy_changed_cb (MidoriTransfers* transfers,
                                       GtkWidget*       old_parent)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (transfers));
    if (MIDORI_IS_BROWSER (browser))
        g_signal_connect (browser, "add-download",
            G_CALLBACK (midori_transfers_browser_add_download_cb), transfers);
    if (old_parent)
        g_signal_handlers_disconnect_by_func (old_parent,
            midori_transfers_browser_add_download_cb, transfers);
}

static GtkWidget*
midori_transfers_popup_menu_item (GtkMenu*         menu,
                                  const gchar*     stock_id,
                                  const gchar*     label,
                                  WebKitDownload*  download,
                                  gpointer         callback,
                                  gboolean         enabled,
                                  MidoriTransfers* transfers)
{
    GtkWidget* menuitem;

    menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);
    if (label)
        gtk_label_set_text_with_mnemonic (GTK_LABEL (gtk_bin_get_child (
                                          GTK_BIN (menuitem))), label);

    if (!enabled)
        gtk_widget_set_sensitive (menuitem, FALSE);

    g_object_set_data (G_OBJECT (menuitem), "WebKitDownload", download);

    if (callback)
        g_signal_connect (menuitem, "activate", G_CALLBACK (callback), transfers);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);

    return menuitem;
}

static void
midori_transfers_open_activate_cb (GtkWidget*       menuitem,
                                   MidoriTransfers* transfers)
{
    WebKitDownload* download;
    const gchar* uri;

    download = g_object_get_data (G_OBJECT (menuitem), "WebKitDownload");
    g_return_if_fail (download != NULL);

    uri = webkit_download_get_destination_uri (download);
    sokoke_show_uri (gtk_widget_get_screen (GTK_WIDGET (transfers->treeview)),
                     uri, gtk_get_current_event_time (), NULL);
}

static void
midori_transfers_open_folder_activate_cb (GtkWidget*       menuitem,
                                          MidoriTransfers* transfers)
{
    WebKitDownload* download;
    const gchar* uri;
    GFile* file;
    GFile* folder;

    download = g_object_get_data (G_OBJECT (menuitem), "WebKitDownload");
    g_return_if_fail (download != NULL);

    uri = webkit_download_get_destination_uri (download);
    file = g_file_new_for_uri (uri);
    if ((folder = g_file_get_parent (file)))
    {
        gchar* folder_uri = g_file_get_uri (folder);
        sokoke_show_uri (gtk_widget_get_screen (GTK_WIDGET (transfers->treeview)),
            folder_uri, gtk_get_current_event_time (), NULL);
        g_free (folder_uri);
        g_object_unref (folder);
    }
    g_object_unref (file);
}

static void
midori_transfers_copy_address_activate_cb (GtkWidget*       menuitem,
                                           MidoriTransfers* transfers)
{
    WebKitDownload* download;
    const gchar* uri;
    GtkClipboard* clipboard;

    download = g_object_get_data (G_OBJECT (menuitem), "WebKitDownload");
    g_return_if_fail (download != NULL);

    uri = webkit_download_get_uri (download);
    clipboard = gtk_clipboard_get_for_display (
        gtk_widget_get_display (GTK_WIDGET (menuitem)),
        GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text (clipboard, uri, -1);
}

static void
midori_transfers_popup (GtkWidget*       widget,
                        GdkEventButton*  event,
                        WebKitDownload*  download,
                        MidoriTransfers* transfers)
{
    GtkWidget* menu;
    gboolean finished = FALSE;

    if (webkit_download_get_status (download) == WEBKIT_DOWNLOAD_STATUS_FINISHED)
        finished = TRUE;

    menu = gtk_menu_new ();
    midori_transfers_popup_menu_item (GTK_MENU (menu), GTK_STOCK_OPEN, NULL, download,
        midori_transfers_open_activate_cb, finished, transfers);
    midori_transfers_popup_menu_item (GTK_MENU (menu), GTK_STOCK_DIRECTORY,
        _("Open Destination _Folder"), download,
        midori_transfers_open_folder_activate_cb, TRUE, transfers);
    midori_transfers_popup_menu_item (GTK_MENU (menu), GTK_STOCK_COPY,
        _("Copy Link Loc_ation"), download,
        midori_transfers_copy_address_activate_cb, TRUE, transfers);

    katze_widget_popup (widget, GTK_MENU (menu), event, KATZE_MENU_POSITION_CURSOR);
}

static gboolean
midori_transfers_popup_menu_cb (GtkWidget*       widget,
                                MidoriTransfers* transfers)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        WebKitDownload* download;

        gtk_tree_model_get (model, &iter, 1, &download, -1);

        midori_transfers_popup (widget, NULL, download, transfers);
        g_object_unref (download);
        return TRUE;
    }
    return FALSE;
}

static gboolean
midori_transfers_button_release_event_cb (GtkWidget*      widget,
                                         GdkEventButton*  event,
                                         MidoriTransfers* transfers)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (event->button != 3)
        return FALSE;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        WebKitDownload* download;

        gtk_tree_model_get (model, &iter, 1, &download, -1);

        midori_transfers_popup (widget, NULL, download, transfers);
        g_object_unref (download);
        return TRUE;
    }
    return FALSE;
}

static void
midori_transfers_init (MidoriTransfers* transfers)
{
    /* Create the treeview */
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_pixbuf;
    GtkCellRenderer* renderer_text;
    GtkListStore* treestore = gtk_list_store_new (2, GDK_TYPE_PIXBUF, G_TYPE_OBJECT);
    transfers->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (treestore));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (transfers->treeview), FALSE);
    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_expand (column, TRUE);
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_transfers_treeview_render_icon_cb,
        transfers->treeview, NULL);
    renderer_text = gtk_cell_renderer_progress_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, TRUE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_transfers_treeview_render_text_cb,
        transfers->treeview, NULL);
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_transfers_treeview_render_button_cb,
        transfers->treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (transfers->treeview), column);
    g_object_unref (treestore);
    g_object_connect (transfers->treeview,
        "signal::row-activated",
        midori_transfers_treeview_row_activated_cb, transfers,
        "signal::button-release-event",
        midori_transfers_button_release_event_cb, transfers,
        "signal::popup-menu",
        midori_transfers_popup_menu_cb, transfers,
        NULL);
    gtk_widget_show (transfers->treeview);
    gtk_box_pack_start (GTK_BOX (transfers), transfers->treeview, TRUE, TRUE, 0);

    g_signal_connect (transfers, "hierarchy-changed",
        G_CALLBACK (midori_transfers_hierarchy_changed_cb), NULL);
}

