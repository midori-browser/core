/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-console.h"

#include "sokoke.h"
#include <glib/gi18n.h>

struct _MidoriConsole
{
    GtkVBox parent_instance;

    GtkWidget* toolbar;
    GtkWidget* treeview;
};

G_DEFINE_TYPE (MidoriConsole, midori_console, GTK_TYPE_VBOX)

static void
midori_console_class_init (MidoriConsoleClass* class)
{
    /* Nothing to do */
}

static void
midori_console_button_clear_clicked_cb (GtkToolItem*   toolitem,
                                        MidoriConsole* console)
{
    GtkTreeModel* model = gtk_tree_view_get_model (
        GTK_TREE_VIEW (console->treeview));
    gtk_tree_store_clear (GTK_TREE_STORE (model));
}

static void
midori_console_treeview_render_icon_cb (GtkTreeViewColumn* column,
                                        GtkCellRenderer*   renderer,
                                        GtkTreeModel*      model,
                                        GtkTreeIter*       iter,
                                        GtkWidget*         treeview)
{
    /* gchar* source_id;
    gtk_tree_model_get (model, iter, 2, &source_id, -1); */

    g_object_set (renderer, "stock-id", GTK_STOCK_DIALOG_WARNING, NULL);

    /* g_free (source_id); */
}

static void
midori_console_treeview_render_text_cb (GtkTreeViewColumn* column,
                                        GtkCellRenderer*   renderer,
                                        GtkTreeModel*      model,
                                        GtkTreeIter*       iter,
                                        GtkWidget*         treeview)
{
    gchar* message;
    gint   line;
    gchar* source_id;
    gtk_tree_model_get (model, iter, 0, &message, 1, &line, 2, &source_id, -1);

    gchar* text = g_strdup_printf ("%d @ %s\n%s", line, source_id, message);
    g_object_set (renderer, "text", text, NULL);
    g_free (text);

    g_free (message);
    g_free (source_id);
}

static void
midori_console_treeview_row_activated_cb (GtkTreeView*       treeview,
                                          GtkTreePath*       path,
                                          GtkTreeViewColumn* column,
                                          MidoriConsole*     console)
{
    /*GtkTreeModel* model = gtk_tree_view_get_model (treeview);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gchar* source_id;
        gtk_tree_model_get (model, &iter, 2, &source_id, -1);
        g_free (source_id);
    }*/
}

static void
midori_console_init (MidoriConsole* console)
{
    /* Create the treeview */
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_pixbuf;
    GtkTreeStore* treestore = gtk_tree_store_new (3, G_TYPE_STRING,
                                                     G_TYPE_INT,
                                                     G_TYPE_STRING);
    console->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (treestore));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (console->treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_console_treeview_render_icon_cb,
        console->treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_console_treeview_render_text_cb,
        console->treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (console->treeview), column);
    g_object_unref (treestore);
    g_signal_connect (console->treeview, "row-activated",
                      G_CALLBACK (midori_console_treeview_row_activated_cb),
                      console);
    gtk_widget_show (console->treeview);
    gtk_box_pack_start (GTK_BOX (console), console->treeview, TRUE, TRUE, 0);
}

/**
 * midori_console_new:
 *
 * Creates a new empty console.
 *
 * Return value: a new #MidoriConsole
 **/
GtkWidget*
midori_console_new (void)
{
    MidoriConsole* console = g_object_new (MIDORI_TYPE_CONSOLE,
                                           NULL);

    return GTK_WIDGET (console);
}

/**
 * midori_console_get_toolbar:
 *
 * Retrieves the toolbar of the console. A new widget is created on
 * the first call of this function.
 *
 * Return value: a new #MidoriConsole
 **/
GtkWidget*
midori_console_get_toolbar (MidoriConsole* console)
{
    g_return_val_if_fail (MIDORI_IS_CONSOLE (console), NULL);

    if (!console->toolbar)
    {
        GtkWidget* toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        GtkToolItem* toolitem = gtk_tool_item_new ();
        /* TODO: What about a find entry here that filters e.g. by url? */
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
            G_CALLBACK (midori_console_button_clear_clicked_cb), console);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        console->toolbar = toolbar;
    }

    return console->toolbar;
}

/**
 * midori_console_add:
 * @console: a #MidoriConsole
 * @message: a descriptive message
 * @line: the line in the source file
 * @source_id: the source
 *
 * Adds a new message to the console.
 **/
void
midori_console_add (MidoriConsole* console,
                    const gchar*   message,
                    gint           line,
                    const gchar*   source_id)
{
    g_return_if_fail (MIDORI_IS_CONSOLE (console));

    GtkTreeView* treeview = GTK_TREE_VIEW (console->treeview);
    GtkTreeModel* treemodel = gtk_tree_view_get_model (treeview);
    gtk_tree_store_insert_with_values (GTK_TREE_STORE (treemodel),
                                       NULL, NULL, G_MAXINT,
                                       0, message, 1, line, 2, source_id, -1);
}
