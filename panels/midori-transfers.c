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
#include "midori-stock.h"
#include "midori-view.h"

#include "sokoke.h"
#include "compat.h"
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
    return STOCK_TRANSFERS;
}

static void
midori_transfers_button_clear_clicked_cb (GtkToolItem*    toolitem,
                                         MidoriTransfers* transfers)
{
    GtkTreeModel* model = gtk_tree_view_get_model (
        GTK_TREE_VIEW (transfers->treeview));
    /* FIXME: Clear only finished and cancelled downloads */
    gtk_tree_store_clear (GTK_TREE_STORE (model));
}

static GtkWidget*
midori_transfers_get_toolbar (MidoriViewable* transfers)
{
    if (!MIDORI_TRANSFERS (transfers)->toolbar)
    {
        GtkWidget* toolbar;
        GtkToolItem* toolitem;

        toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
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

#if WEBKIT_CHECK_VERSION (1, 1, 3)
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
    gtk_tree_store_insert_with_values (GTK_TREE_STORE (model),
                                       NULL, NULL, G_MAXINT,
                                       0, NULL, 1, download, -1);
    g_signal_connect (download, "notify::progress",
        G_CALLBACK (midori_transfers_download_notify_progress_cb), transfers);
    g_signal_connect (download, "notify::status",
        G_CALLBACK (midori_transfers_download_notify_status_cb), transfers);
}
#endif

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
    g_object_set (renderer, "stock-id", STOCK_TRANSFER,
                  "stock-size", GTK_ICON_SIZE_DND,
                  "xpad", 1, "ypad", 12, NULL);
}

static void
midori_transfers_treeview_render_text_cb (GtkTreeViewColumn* column,
                                          GtkCellRenderer*   renderer,
                                          GtkTreeModel*      model,
                                          GtkTreeIter*       iter,
                                          GtkWidget*         treeview)
{
    #if WEBKIT_CHECK_VERSION (1, 1, 3)
    WebKitDownload* download;
    gchar* current;
    gchar* total;
    gchar* size_text;
    gchar* text;
    gdouble progress;

    gtk_tree_model_get (model, iter, 1, &download, -1);

    /* FIXME: Ellipsize filename */
    current = g_format_size_for_display (webkit_download_get_current_size (download));
    total = g_format_size_for_display (webkit_download_get_total_size (download));
    size_text = g_strdup_printf (_("%s of %s"), current, total);
    g_free (current);
    g_free (total);
    text = g_strdup_printf ("%s\n%s",
        webkit_download_get_suggested_filename (download), size_text);
    g_free (size_text);
    /* Avoid a bug in WebKit */
    if (webkit_download_get_status (download) != WEBKIT_DOWNLOAD_STATUS_CREATED)
        progress = webkit_download_get_progress (download);
    else
        progress = 0.0;
    g_object_set (renderer, "text", text,
                  "value", (gint)(progress * 100),
                  "xpad", 1, "ypad", 6, NULL);
    g_free (text);
    g_object_unref (download);
    #endif
}

static void
midori_transfers_treeview_render_button_cb (GtkTreeViewColumn* column,
                                            GtkCellRenderer*   renderer,
                                            GtkTreeModel*      model,
                                            GtkTreeIter*       iter,
                                            GtkWidget*         treeview)
{
    #if WEBKIT_CHECK_VERSION (1, 1, 3)
    WebKitDownload* download;
    const gchar* stock_id;

    gtk_tree_model_get (model, iter, 1, &download, -1);

    switch (webkit_download_get_status (download))
    {
        case WEBKIT_DOWNLOAD_STATUS_STARTED:
            stock_id = GTK_STOCK_CANCEL;
            break;
        case WEBKIT_DOWNLOAD_STATUS_FINISHED:
            stock_id = GTK_STOCK_OPEN;
            break;
        default:
            stock_id = GTK_STOCK_CLEAR;
    }
    g_object_set (renderer, "stock-id", stock_id,
                  "stock-size", GTK_ICON_SIZE_MENU, NULL);

    g_object_unref (download);
    #endif
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
        #if WEBKIT_CHECK_VERSION (1, 1, 3)
        WebKitDownload* download;

        gtk_tree_model_get (model, &iter, 1, &download, -1);

        switch (webkit_download_get_status (download))
        {
            case WEBKIT_DOWNLOAD_STATUS_STARTED:
                webkit_download_cancel (download);
                break;
            case WEBKIT_DOWNLOAD_STATUS_FINISHED:
            {
                const gchar* uri;

                uri = webkit_download_get_destination_uri (download);
                sokoke_show_uri (gtk_widget_get_screen (GTK_WIDGET (
                    treeview)), uri, gtk_get_current_event_time (), NULL);
                break;
            }
            case WEBKIT_DOWNLOAD_STATUS_CANCELLED:
                /* FIXME: Remove this item from the model */
            default:
                break;
        }
        g_object_unref (download);
        #endif
    }
}

static void
midori_transfers_hierarchy_changed_cb (MidoriTransfers* transfers,
                                       GtkWidget*       old_parent)
{
    #if WEBKIT_CHECK_VERSION (1, 1, 3)
    MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (transfers));
    if (MIDORI_IS_BROWSER (browser))
        g_signal_connect (browser, "add-download",
            G_CALLBACK (midori_transfers_browser_add_download_cb), transfers);
    if (old_parent)
        g_signal_handlers_disconnect_by_func (old_parent,
            midori_transfers_browser_add_download_cb, transfers);
    #endif
}

static void
midori_transfers_init (MidoriTransfers* transfers)
{
    /* Create the treeview */
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_pixbuf;
    GtkCellRenderer* renderer_text;
    GtkTreeStore* treestore = gtk_tree_store_new (2, GDK_TYPE_PIXBUF, G_TYPE_OBJECT);
    transfers->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (treestore));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (transfers->treeview), FALSE);
    column = gtk_tree_view_column_new ();
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
    g_signal_connect (transfers->treeview, "row-activated",
                      G_CALLBACK (midori_transfers_treeview_row_activated_cb),
                      transfers);
    gtk_widget_show (transfers->treeview);
    gtk_box_pack_start (GTK_BOX (transfers), transfers->treeview, TRUE, TRUE, 0);

    g_signal_connect (transfers, "hierarchy-changed",
        G_CALLBACK (midori_transfers_hierarchy_changed_cb), NULL);
}

/**
 * midori_transfers_new:
 *
 * Creates a new empty transfers.
 *
 * Return value: a new #MidoriTransfers
 *
 * Since 0.1.5
 **/
GtkWidget*
midori_transfers_new (void)
{
    MidoriTransfers* transfers = g_object_new (MIDORI_TYPE_TRANSFERS, NULL);

    return GTK_WIDGET (transfers);
}
