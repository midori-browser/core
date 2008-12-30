/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-extensions.h"

#include "midori-app.h"
#include "midori-extension.h"
#include "midori-stock.h"
#include "midori-viewable.h"

#include "sokoke.h"
#include <glib/gi18n.h>

struct _MidoriExtensions
{
    GtkVBox parent_instance;

    GtkWidget* toolbar;
    GtkWidget* treeview;
    MidoriApp* app;
};

struct _MidoriExtensionsClass
{
    GtkVBoxClass parent_class;
};

static void
midori_extensions_viewable_iface_init (MidoriViewableIface* iface);

G_DEFINE_TYPE_WITH_CODE (MidoriExtensions, midori_extensions, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (MIDORI_TYPE_VIEWABLE,
                             midori_extensions_viewable_iface_init));

enum
{
    PROP_0,

    PROP_APP
};

static void
midori_extensions_set_property (GObject*      object,
                                guint         prop_id,
                                const GValue* value,
                                GParamSpec*   pspec);

static void
midori_extensions_get_property (GObject*    object,
                                guint       prop_id,
                                GValue*     value,
                                GParamSpec* pspec);

static void
midori_extensions_class_init (MidoriExtensionsClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->set_property = midori_extensions_set_property;
    gobject_class->get_property = midori_extensions_get_property;

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
midori_extensions_get_label (MidoriViewable* viewable)
{
    return _("Extensions");
}

static const gchar*
midori_extensions_get_stock_id (MidoriViewable* viewable)
{
    return STOCK_EXTENSIONS;
}

static void
midori_extensions_button_status_clicked_cb (GtkToolItem*      toolitem,
                                            MidoriExtensions* extensions)
{
    GtkTreeView* treeview;
    GtkTreeModel* model;
    GtkTreeIter iter;
    MidoriExtension* extension;

    treeview = GTK_TREE_VIEW (extensions->treeview);

    if (sokoke_tree_view_get_selected_iter (treeview, &model, &iter))
    {
        GtkToolItem* button_enable = gtk_toolbar_get_nth_item (
            GTK_TOOLBAR (extensions->toolbar), 1);
        GtkToolItem* button_disable = gtk_toolbar_get_nth_item (
            GTK_TOOLBAR (extensions->toolbar), 2);

        gtk_tree_model_get (model, &iter, 0, &extension, -1);
        if (toolitem == button_enable)
            g_signal_emit_by_name (extension, "activate", extensions->app);
        else if (toolitem == button_disable)
            midori_extension_deactivate (extension);

        gtk_widget_set_sensitive (GTK_WIDGET (button_enable),
            !midori_extension_is_active (extension));
        gtk_widget_set_sensitive (GTK_WIDGET (button_enable),
            midori_extension_is_active (extension));
    }
}

static GtkWidget*
midori_extensions_get_toolbar (MidoriViewable* extensions)
{
    if (!MIDORI_EXTENSIONS (extensions)->toolbar)
    {
        GtkWidget* toolbar;
        GtkToolItem* toolitem;

        toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        toolitem = gtk_tool_item_new ();
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));

        /* enable button */
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_YES);
        gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), _("_Enable"));
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_extensions_button_status_clicked_cb), extensions);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_set_sensitive (GTK_WIDGET (toolitem), FALSE);
        gtk_widget_show (GTK_WIDGET (toolitem));

        /* disable button */
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_NO);
        gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), _("_Disable"));
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_extensions_button_status_clicked_cb), extensions);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_set_sensitive (GTK_WIDGET (toolitem), FALSE);
        gtk_widget_show (GTK_WIDGET (toolitem));

        MIDORI_EXTENSIONS (extensions)->toolbar = toolbar;
    }

    return MIDORI_EXTENSIONS (extensions)->toolbar;
}

static void
midori_extensions_viewable_iface_init (MidoriViewableIface* iface)
{
    iface->get_stock_id = midori_extensions_get_stock_id;
    iface->get_label = midori_extensions_get_label;
    iface->get_toolbar = midori_extensions_get_toolbar;
}

static void
midori_extensions_add_item_cb (KatzeArray*       array,
                               MidoriExtension*  extension,
                               MidoriExtensions* extensions)
{
    GtkTreeIter iter;
    GtkTreeModel* model;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (extensions->treeview));
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, extension, -1);
}

static void
midori_extensions_set_property (GObject*      object,
                                guint         prop_id,
                                const GValue* value,
                                GParamSpec*   pspec)
{
    MidoriExtensions* extensions = MIDORI_EXTENSIONS (object);

    switch (prop_id)
    {
    case PROP_APP:
    {
        KatzeArray* array;
        guint i, n;

        /* FIXME: Handle NULL and subsequent assignments */
        extensions->app = g_value_get_object (value);
        array = katze_object_get_object (extensions->app, "extensions");
        g_signal_connect (array, "add-item",
            G_CALLBACK (midori_extensions_add_item_cb), extensions);

        if ((n = katze_array_get_length (array)))
            for (i = 0; i < n; i++)
                midori_extensions_add_item_cb (array,
                    katze_array_get_nth_item (array, i), extensions);
    }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_extensions_get_property (GObject*    object,
                                guint       prop_id,
                                GValue*     value,
                                GParamSpec* pspec)
{
    MidoriExtensions* extensions = MIDORI_EXTENSIONS (object);

    switch (prop_id)
    {
    case PROP_APP:
        g_value_set_object (value, extensions->app);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_extensions_treeview_render_icon_cb (GtkTreeViewColumn* column,
                                           GtkCellRenderer*   renderer,
                                           GtkTreeModel*      model,
                                           GtkTreeIter*       iter,
                                           GtkWidget*         treeview)
{
    g_object_set (renderer, "stock-id", GTK_STOCK_EXECUTE, NULL);
}

static void
midori_extensions_treeview_render_text_cb (GtkTreeViewColumn* column,
                                           GtkCellRenderer*   renderer,
                                           GtkTreeModel*      model,
                                           GtkTreeIter*       iter,
                                           GtkWidget*         treeview)
{
    MidoriExtension* extension;
    gchar* text;

    gtk_tree_model_get (model, iter, 0, &extension, -1);

    text = g_strdup_printf ("%s\n%s",
        katze_object_get_string (extension, "name"),
        katze_object_get_string (extension, "description"));
    g_object_set (renderer, "text", text, NULL);
    g_free (text);
}

static void
midori_extensions_treeview_row_activated_cb (GtkTreeView*       treeview,
                                             GtkTreePath*       path,
                                             GtkTreeViewColumn* column,
                                             MidoriExtensions*  extensions)
{
    GtkTreeModel* model = gtk_tree_view_get_model (treeview);
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        MidoriExtension* extension;

        gtk_tree_model_get (model, &iter, 0, &extension, -1);
        if (midori_extension_is_active (extension))
        {
            midori_extension_deactivate (extension);
            gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        }
        else
            g_signal_emit_by_name (extension, "activate", extensions->app);
    }
}

static void
midori_extensions_init (MidoriExtensions* extensions)
{
    /* Create the treeview */
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_pixbuf;
    GtkListStore* liststore = gtk_list_store_new (1, G_TYPE_OBJECT);
    extensions->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (liststore));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (extensions->treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_extensions_treeview_render_icon_cb,
        extensions->treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_extensions_treeview_render_text_cb,
        extensions->treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (extensions->treeview), column);
    g_object_unref (liststore);
    g_signal_connect (extensions->treeview, "row-activated",
                      G_CALLBACK (midori_extensions_treeview_row_activated_cb),
                      extensions);
    gtk_widget_show (extensions->treeview);
    gtk_box_pack_start (GTK_BOX (extensions), extensions->treeview, TRUE, TRUE, 0);
}

/**
 * midori_extensions_new:
 *
 * Creates a new empty extensions.
 *
 * Return value: a new #MidoriExtensions
 *
 * Since: 0.1.2
 **/
GtkWidget*
midori_extensions_new (void)
{
    MidoriExtensions* extensions = g_object_new (MIDORI_TYPE_EXTENSIONS,
                                                 NULL);

    return GTK_WIDGET (extensions);
}
