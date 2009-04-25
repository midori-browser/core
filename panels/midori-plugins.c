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

    /* FIXME: Monitor folders for newly added and removes files */
    if (g_module_supported ())
    {
        /* FIXME: WebKit is also looking in legacy folders,
                  we should have API to obtain that same list. */
        gchar** plugin_dirs;
        gsize i = 0;

        if (g_getenv ("MOZ_PLUGIN_PATH"))
            plugin_dirs = g_strsplit (g_getenv ("MOZ_PLUGIN_PATH"), ":", 0);
        else
            plugin_dirs = g_strsplit ("/usr/lib/mozilla/plugins", ":", 0);

        while (plugin_dirs[i])
        {
            gchar* plugin_path;
            GDir* plugin_dir;

            plugin_path = g_build_filename (plugin_dirs[i], NULL);
            plugin_dir = g_dir_open (plugin_path, 0, NULL);
            if (plugin_dir != 0)
            {
                const gchar* filename;

                while ((filename = g_dir_read_name (plugin_dir)))
                {
                    gchar* fullname;
                    GModule* module;
                    typedef int (*NP_GetValue_func)(void* instance,
                                                    int   variable,
                                                    void* value);
                    NP_GetValue_func NP_GetValue;
                    const gchar* plugin_name;
                    const gchar* plugin_description;

                    /* Ignore files which don't have the correct suffix */
                    if (!g_str_has_suffix (filename, G_MODULE_SUFFIX))
                        continue;

                    fullname = g_build_filename (plugin_path, filename, NULL);
                    module = g_module_open (fullname, G_MODULE_BIND_LOCAL);
                    g_free (fullname);

                    if (module && g_module_symbol (module, "NP_GetValue",
                                                   (gpointer) &NP_GetValue))
                    {
                        typedef const gchar* (*NP_GetMIMEDescription_func)(void);
                        NP_GetMIMEDescription_func NP_GetMIMEDescription;

                        NP_GetValue (NULL, 2, &plugin_name);
                        if (g_module_symbol (module, "NP_GetMIMEDescription",
                                             (gpointer) &NP_GetMIMEDescription))
                            plugin_description = NP_GetMIMEDescription ();
                        else
                            plugin_description = g_module_error ();
                    }
                    else
                    {
                        plugin_name = filename;
                        plugin_description = g_module_error ();
                    }

                    midori_plugins_add_item (plugins, plugin_name, plugin_description);
                }
                g_dir_close (plugin_dir);
            }
            g_free (plugin_path);
            i++;
        }
        g_strfreev (plugin_dirs);
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
