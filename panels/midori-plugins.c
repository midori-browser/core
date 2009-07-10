/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-plugins.h"

#include "midori-app.h"
#include "midori-stock.h"
#include "midori-viewable.h"

#include "sokoke.h"
#include <string.h>
#include <glib/gi18n.h>

struct _MidoriPlugins
{
    GtkVBox parent_instance;

    GtkWidget* toolbar;
    GtkWidget* treeview;
    MidoriApp* app;
};

struct _MidoriPluginsClass
{
    GtkVBoxClass parent_class;
};

static void
midori_plugins_viewable_iface_init (MidoriViewableIface* iface);

G_DEFINE_TYPE_WITH_CODE (MidoriPlugins, midori_plugins, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (MIDORI_TYPE_VIEWABLE,
                             midori_plugins_viewable_iface_init));

enum
{
    PROP_0,

    PROP_APP
};

static void
midori_plugins_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec);

static void
midori_plugins_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec);

static void
midori_plugins_class_init (MidoriPluginsClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->set_property = midori_plugins_set_property;
    gobject_class->get_property = midori_plugins_get_property;

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
midori_plugins_get_label (MidoriViewable* viewable)
{
    return _("Netscape plugins");
}

static const gchar*
midori_plugins_get_stock_id (MidoriViewable* viewable)
{
    return STOCK_PLUGINS;
}

static GtkWidget*
midori_plugins_get_toolbar (MidoriViewable* plugins)
{
    if (!MIDORI_PLUGINS (plugins)->toolbar)
    {
        GtkWidget* toolbar;
        GtkToolItem* toolitem;

        toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        toolitem = gtk_tool_item_new ();
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));

        MIDORI_PLUGINS (plugins)->toolbar = toolbar;
    }

    return MIDORI_PLUGINS (plugins)->toolbar;
}

static void
midori_plugins_viewable_iface_init (MidoriViewableIface* iface)
{
    iface->get_stock_id = midori_plugins_get_stock_id;
    iface->get_label = midori_plugins_get_label;
    iface->get_toolbar = midori_plugins_get_toolbar;
}

static void
midori_plugins_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec)
{
    MidoriPlugins* plugins = MIDORI_PLUGINS (object);

    switch (prop_id)
    {
    case PROP_APP:
        plugins->app = g_value_get_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_plugins_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec)
{
    MidoriPlugins* plugins = MIDORI_PLUGINS (object);

    switch (prop_id)
    {
    case PROP_APP:
        g_value_set_object (value, plugins->app);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_plugins_treeview_render_icon_cb (GtkTreeViewColumn* column,
                                        GtkCellRenderer*   renderer,
                                        GtkTreeModel*      model,
                                        GtkTreeIter*       iter,
                                        GtkWidget*         treeview)
{
    g_object_set (renderer, "stock-id", GTK_STOCK_EXECUTE, NULL);
}

static void
midori_plugins_treeview_render_text_cb (GtkTreeViewColumn* column,
                                        GtkCellRenderer*   renderer,
                                        GtkTreeModel*      model,
                                        GtkTreeIter*       iter,
                                        GtkWidget*         treeview)
{
    gchar* name;
    gchar* text;
    gchar* description;

    gtk_tree_model_get (model, iter, 0, &name, 1, &description, -1);

    text = g_strdup_printf ("%s\n%s", name, description);
    g_free (name);
    g_free (description);
    g_object_set (renderer, "text", text, NULL);
    g_free (text);
}

static void
midori_plugins_add_item (MidoriPlugins* plugins,
                         const gchar*   name,
                         const gchar*   description)
{
    gchar* desc;
    GtkTreeIter iter;
    GtkTreeModel* model;

    desc = g_strdup (description);
    if (desc)
    {
        gsize i, n;

        n = strlen (desc);
        for (i = 0; i < n; i++)
            if (desc[i] == ';')
                desc[i] = '\n';
    }
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (plugins->treeview));
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        0, name, 1, desc, -1);
    g_free (desc);
}

static void
midori_plugins_init (MidoriPlugins* plugins)
{
    /* Create the treeview */
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_pixbuf;
    GtkListStore* liststore = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

    plugins->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (liststore));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (plugins->treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_plugins_treeview_render_icon_cb,
        plugins->treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_plugins_treeview_render_text_cb,
        plugins->treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (plugins->treeview), column);
    g_object_unref (liststore);
    gtk_widget_show (plugins->treeview);
    gtk_box_pack_start (GTK_BOX (plugins), plugins->treeview, TRUE, TRUE, 0);

    if (1)
    {
        /* FIXME: WebKit should have API to obtain the list of plugins. */
        /* FIXME: Monitor folders for newly added and removes files */
        GtkWidget* web_view = webkit_web_view_new ();
        WebKitWebFrame* web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (web_view));
        JSContextRef js_context = webkit_web_frame_get_global_context (web_frame);
        /* This snippet joins the available plugins into a string like this:
           URI1|title1,URI2|title2
           FIXME: Ensure separators contained in the string can't break it */
        gchar* value = sokoke_js_script_eval (js_context,
            "function plugins (l) { var f = new Array (); for (i in l) "
            "{ var t = l[i].name; "
            "f.push (l[i].name + '|' + l[i].filename); } return f; }"
            "plugins (navigator.plugins)", NULL);
        gchar** items = g_strsplit (value, ",", 0);
        guint i = 0;

        if (items != NULL)
        while (items[i] != NULL)
        {
            gchar** parts = g_strsplit (items[i], "|", 2);
            if (parts && *parts && !g_str_equal (parts[1], "undefined"))
                midori_plugins_add_item (plugins, *parts, parts[1]);
            g_strfreev (parts);
            i++;
        }
        g_strfreev (items);
    }
}

/**
 * midori_plugins_new:
 *
 * Creates a new empty plugins.
 *
 * Return value: a new #MidoriPlugins
 *
 * Since: 0.1.3
 **/
GtkWidget*
midori_plugins_new (void)
{
    MidoriPlugins* plugins = g_object_new (MIDORI_TYPE_PLUGINS, NULL);

    return GTK_WIDGET (plugins);
}
