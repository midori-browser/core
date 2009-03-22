/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-console.h"

#include "midori-app.h"
#include "midori-browser.h"
#include "midori-stock.h"
#include "midori-view.h"

#include "sokoke.h"
#include <glib/gi18n.h>

struct _MidoriConsole
{
    GtkVBox parent_instance;

    GtkWidget* toolbar;
    GtkWidget* treeview;
    MidoriApp* app;
};

struct _MidoriConsoleClass
{
    GtkVBoxClass parent_class;
};

static void
midori_console_viewable_iface_init (MidoriViewableIface* iface);

G_DEFINE_TYPE_WITH_CODE (MidoriConsole, midori_console, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (MIDORI_TYPE_VIEWABLE,
                             midori_console_viewable_iface_init));

enum
{
    PROP_0,

    PROP_APP
};

static void
midori_console_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec);

static void
midori_console_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec);

static void
midori_console_class_init (MidoriConsoleClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->set_property = midori_console_set_property;
    gobject_class->get_property = midori_console_get_property;

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
midori_console_get_label (MidoriViewable* viewable)
{
    return _("Console");
}

static const gchar*
midori_console_get_stock_id (MidoriViewable* viewable)
{
    return STOCK_CONSOLE;
}

static void
midori_console_viewable_iface_init (MidoriViewableIface* iface)
{
    iface->get_stock_id = midori_console_get_stock_id;
    iface->get_label = midori_console_get_label;
    iface->get_toolbar = midori_console_get_toolbar;
}

static void
midori_view_console_message_cb (GtkWidget*     view,
                                const gchar*   message,
                                gint           line,
                                const gchar*   source_id,
                                MidoriConsole* console)
{
    midori_console_add (console, message, line, source_id);
}

static void
midori_console_browser_add_tab_cb (MidoriBrowser* browser,
                                   MidoriView*    view,
                                   MidoriConsole* console)
{
    g_signal_connect (view, "console-message",
        G_CALLBACK (midori_view_console_message_cb), console);
}

static void
midori_console_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec)
{
    MidoriConsole* console = MIDORI_CONSOLE (object);

    switch (prop_id)
    {
    case PROP_APP:
        console->app = g_value_get_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_console_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec)
{
    MidoriConsole* console = MIDORI_CONSOLE (object);

    switch (prop_id)
    {
    case PROP_APP:
        g_value_set_object (value, console->app);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_console_button_copy_clicked_cb (GtkToolItem*   toolitem,
                                       MidoriConsole* console)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (console->treeview),
                                           &model, &iter))
    {
        GdkDisplay* display;
        GtkClipboard* clipboard;
        gchar* text;
        gchar* message;
        gint line;
        gchar* source_id;

        display = gtk_widget_get_display (GTK_WIDGET (console));
        clipboard = gtk_clipboard_get_for_display (display, GDK_SELECTION_CLIPBOARD);
        gtk_tree_model_get (model, &iter, 0, &message, 1, &line, 2, &source_id, -1);
        text = g_strdup_printf ("%d @ %s: %s", line, source_id, message);
        g_free (source_id);
        g_free (message);
        gtk_clipboard_set_text (clipboard, text, -1);
        g_free (text);
    }
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
    g_object_set (renderer, "stock-id", GTK_STOCK_DIALOG_WARNING, NULL);
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
    gchar* text;

    gtk_tree_model_get (model, iter, 0, &message, 1, &line, 2, &source_id, -1);

    text = g_strdup_printf ("%d @ %s\n%s", line, source_id, message);
    g_object_set (renderer, "text", text, NULL);
    g_free (text);

    g_free (message);
    g_free (source_id);
}

static void
midori_console_hierarchy_changed_cb (MidoriConsole* console,
                                     GtkWidget*     old_parent)
{
    GtkWidget* browser = gtk_widget_get_toplevel (GTK_WIDGET (console));
    if (GTK_WIDGET_TOPLEVEL (browser))
        g_signal_connect (browser, "add-tab",
            G_CALLBACK (midori_console_browser_add_tab_cb), console);
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
    gtk_widget_show (console->treeview);
    gtk_box_pack_start (GTK_BOX (console), console->treeview, TRUE, TRUE, 0);

    g_signal_connect (console, "hierarchy-changed",
        G_CALLBACK (midori_console_hierarchy_changed_cb), NULL);
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
 * @console: a #MidoriConsole
 *
 * Retrieves the toolbar of the console. A new widget is created on
 * the first call of this function.
 *
 * Return value: a toolbar widget
 *
 * Deprecated: 0.1.2: Use midori_viewable_get_toolbar() instead.
 **/
GtkWidget*
midori_console_get_toolbar (MidoriViewable* console)
{
    g_return_val_if_fail (MIDORI_IS_CONSOLE (console), NULL);

    if (!MIDORI_CONSOLE (console)->toolbar)
    {
        GtkWidget* toolbar;
        GtkToolItem* toolitem;

        toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_COPY);
        gtk_tool_item_set_is_important (toolitem, TRUE);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_console_button_copy_clicked_cb), console);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        /* TODO: What about a find entry here that filters e.g. by url? */
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
        MIDORI_CONSOLE (console)->toolbar = toolbar;
    }

    return MIDORI_CONSOLE (console)->toolbar;
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
    GtkTreeView* treeview;
    GtkTreeModel* model;

    g_return_if_fail (MIDORI_IS_CONSOLE (console));

    treeview = GTK_TREE_VIEW (console->treeview);
    model = gtk_tree_view_get_model (treeview);
    gtk_tree_store_insert_with_values (GTK_TREE_STORE (model),
                                       NULL, NULL, G_MAXINT,
                                       0, message, 1, line, 2, source_id, -1);
}
