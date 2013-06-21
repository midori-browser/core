/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-extensions.h"

#include "midori-app.h"
#include "midori-extension.h"
#include "midori-platform.h"
#include "midori-core.h"

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
midori_extensions_finalize (GObject* object);

static void
midori_extensions_class_init (MidoriExtensionsClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->set_property = midori_extensions_set_property;
    gobject_class->get_property = midori_extensions_get_property;
    gobject_class->finalize = midori_extensions_finalize;

    flags = G_PARAM_READWRITE;

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
    return STOCK_EXTENSION;
}

static GtkWidget*
midori_extensions_get_toolbar (MidoriViewable* extensions)
{
    if (!MIDORI_EXTENSIONS (extensions)->toolbar)
    {
        GtkWidget* toolbar;

        toolbar = gtk_toolbar_new ();
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
midori_extensions_extension_activate_cb (MidoriExtension*  extension,
                                         MidoriApp*        app,
                                         MidoriExtensions* extensions)
{
    gtk_widget_queue_draw (GTK_WIDGET (extensions->treeview));
}

static void
midori_extensions_extension_deactivate_cb (MidoriExtension*  extension,
                                           MidoriExtensions* extensions)
{
    gtk_widget_queue_draw (GTK_WIDGET (extensions->treeview));
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
    g_signal_connect (extension, "activate",
        G_CALLBACK (midori_extensions_extension_activate_cb), extensions);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (midori_extensions_extension_deactivate_cb), extensions);
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
        MidoriExtension* extension;

        /* FIXME: Handle NULL and subsequent assignments */
        extensions->app = g_value_get_object (value);
        array = katze_object_get_object (extensions->app, "extensions");
        g_signal_connect (array, "add-item",
            G_CALLBACK (midori_extensions_add_item_cb), extensions);

        KATZE_ARRAY_FOREACH_ITEM (extension, array)
            midori_extensions_add_item_cb (array, extension, extensions);
        g_object_unref (array);
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
midori_extensions_treeview_render_tick_cb (GtkTreeViewColumn* column,
                                           GtkCellRenderer*   renderer,
                                           GtkTreeModel*      model,
                                           GtkTreeIter*       iter,
                                           GtkWidget*         treeview)
{
    MidoriExtension* extension;

    gtk_tree_model_get (model, iter, 0, &extension, -1);

    g_object_set (renderer,
        "activatable", midori_extension_is_prepared (extension),
        "active", midori_extension_is_active (extension) || g_object_get_data (G_OBJECT (extension), "static"),
        "xpad", 4,
        NULL);

    g_object_unref (extension);
}

static void
midori_extensions_treeview_render_icon_cb (GtkTreeViewColumn* column,
                                           GtkCellRenderer*   renderer,
                                           GtkTreeModel*      model,
                                           GtkTreeIter*       iter,
                                           GtkWidget*         treeview)
{
    MidoriExtension* extension;
    gchar* stock_id;
    gtk_tree_model_get (model, iter, 0, &extension, -1);

    stock_id = katze_object_get_object (extension, "stock-id");
    g_object_set (renderer, "stock-id", stock_id ? stock_id : STOCK_EXTENSION,
                            "stock-size", GTK_ICON_SIZE_BUTTON,
                            "sensitive", midori_extension_is_prepared (extension),
                            "xpad", 4, NULL);
    g_free (stock_id);
    g_object_unref (extension);
}

static void
midori_extensions_treeview_render_preferences_cb (GtkTreeViewColumn* column,
                                                  GtkCellRenderer*   renderer,
                                                  GtkTreeModel*      model,
                                                  GtkTreeIter*       iter,
                                                  GtkWidget*         treeview)
{
    MidoriExtension* extension;
    gtk_tree_model_get (model, iter, 0, &extension, -1);

    g_object_set (renderer, "stock-id", GTK_STOCK_PREFERENCES,
                            "stock-size", GTK_ICON_SIZE_BUTTON,
                            "visible", midori_extension_has_preferences (extension),
                            "sensitive", midori_extension_is_active (extension),
                            "xpad", 4, NULL);
    g_object_unref (extension);
}

static void
midori_extensions_treeview_render_text_cb (GtkTreeViewColumn* column,
                                           GtkCellRenderer*   renderer,
                                           GtkTreeModel*      model,
                                           GtkTreeIter*       iter,
                                           GtkWidget*         treeview)
{
    MidoriExtension* extension;
    gchar* name;
    gchar* version;
    gchar* desc;
    gchar* text;

    gtk_tree_model_get (model, iter, 0, &extension, -1);

    name = katze_object_get_string (extension, "name");
    version = katze_object_get_string (extension, "version");
    desc = katze_object_get_string (extension, "description");
    if (katze_object_get_boolean (extension, "use-markup"))
        text = g_strdup_printf ("<b>%s</b> %s\n%s",
            name, version && *version ? version : "", desc);
    else
        text = g_markup_printf_escaped ("<b>%s</b> %s\n%s",
            name, version && *version ? version : "", desc);
    g_free (name);
    g_free (version);
    g_free (desc);

    g_object_set (renderer,
        "markup", text,
        "ellipsize", PANGO_ELLIPSIZE_END,
        "sensitive", midori_extension_is_prepared (extension),
        NULL);

    g_free (text);
    g_object_unref (extension);
}

static void
midori_extensions_treeview_row_activated_cb (GtkTreeView*       treeview,
                                             GtkTreePath*       path,
                                             GtkTreeViewColumn* column,
                                             MidoriExtensions*  extensions)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model (treeview);
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        MidoriExtension* extension;
        KatzeArray* array = katze_object_get_object (extensions->app, "extensions");

        gtk_tree_model_get (model, &iter, 0, &extension, -1);
        if (midori_extension_is_active (extension))
            midori_extension_deactivate (extension);
        else if (midori_extension_is_prepared (extension))
            g_signal_emit_by_name (extension, "activate", extensions->app);
        /* Make it easy for listeners to see that extensions changed */
        katze_array_update (array);

        g_object_unref (array);
        g_object_unref (extension);
    }
}

static void
midori_extensions_cell_renderer_toggled_cb (GtkCellRendererToggle* renderer,
                                            const gchar*           path,
                                            MidoriExtensions*      extensions)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (extensions->treeview));
    if (gtk_tree_model_get_iter_from_string (model, &iter, path))
    {
        MidoriExtension* extension;
        KatzeArray* array = katze_object_get_object (extensions->app, "extensions");

        gtk_tree_model_get (model, &iter, 0, &extension, -1);
        if (midori_extension_is_active (extension))
            midori_extension_deactivate (extension);
        else if (midori_extension_is_prepared (extension))
            g_signal_emit_by_name (extension, "activate", extensions->app);
        /* Make it easy for listeners to see that extensions changed */
        katze_array_update (array);

        g_object_unref (array);
        g_object_unref (extension);
    }
}

static gint
midori_extensions_tree_sort_func (GtkTreeModel* model,
                                  GtkTreeIter*  a,
                                  GtkTreeIter*  b,
                                  gpointer      data)
{
    MidoriExtension* e1, *e2;
    gchar* name1, *name2;
    gint result = 0;

    gtk_tree_model_get (model, a, 0, &e1, -1);
    gtk_tree_model_get (model, b, 0, &e2, -1);

    name1 = katze_object_get_string (e1, "name");
    name2 = katze_object_get_string (e2, "name");

    g_object_unref (e1);
    g_object_unref (e2);

    result = g_strcmp0 (name1, name2);

    g_free (name1);
    g_free (name2);

    return result;
}

static void
midori_extensions_treeview_column_preference_clicked_cb (GtkWidget*   widget,
                                                         GtkTreeView* treeview,
                                                         GtkTreePath* path)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model (treeview);
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        MidoriExtension* extension;

        gtk_tree_model_get (model, &iter, 0, &extension, -1);
        if (midori_extension_is_active (extension))
            g_signal_emit_by_name (extension, "open-preferences");
        g_object_unref (extension);
    }

}

static gboolean
midori_extensions_treeview_button_pressed_cb (GtkWidget*      view,
                                              GdkEventButton* bevent,
                                              gpointer        data)
{
    gboolean ret = FALSE;
    GtkTreePath* path;
    GtkTreeViewColumn* column;
    guint signal_id;

    if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (view),
                bevent->x, bevent->y, &path, &column, NULL, NULL))
    {
        if (path != NULL)
        {
            if (MIDORI_IS_EXTENSIONS_COLUMN (column))
            {
                signal_id = g_signal_lookup ("row-clicked", G_OBJECT_TYPE (column));

                if (signal_id && g_signal_has_handler_pending (column, signal_id, 0, FALSE)) {
                    g_signal_emit (column, signal_id, 0, GTK_TREE_VIEW (view), path);
                    ret = TRUE;
                }
            }
            gtk_tree_path_free (path);
        }
    }
    return ret;
}

static gboolean
extensions_column_search_equal_func (GtkTreeModel* model,
                                     gint          column,
                                     const gchar*  key,
                                     GtkTreeIter*  iter,
                                     gpointer      search_data)
{
    MidoriExtension* extension;
    gchar* name;
    gchar* lower;
    gboolean match;

    gtk_tree_model_get (model, iter, 0, &extension, -1);
    name = katze_object_get_string (extension, "name");
    lower = g_utf8_strdown (name, -1);
    match = !strstr (lower, key);

    g_free (lower);
    g_free (name);

    return match;
}

static void
midori_extensions_init (MidoriExtensions* extensions)
{
    /* Create the treeview */
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_icon;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_toggle;
    GtkCellRenderer* renderer_preferences;
    GtkListStore* liststore = gtk_list_store_new (1, G_TYPE_OBJECT);
    extensions->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (liststore));
    g_object_connect (extensions->treeview,
        "signal::button-press-event",
        midori_extensions_treeview_button_pressed_cb, NULL,
        NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (liststore),
        0, GTK_SORT_ASCENDING);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (liststore),
        0, midori_extensions_tree_sort_func, NULL, NULL);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (extensions->treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_toggle = gtk_cell_renderer_toggle_new ();
    gtk_tree_view_column_pack_start (column, renderer_toggle, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_toggle,
        (GtkTreeCellDataFunc)midori_extensions_treeview_render_tick_cb,
        extensions->treeview, NULL);
    g_signal_connect (renderer_toggle, "toggled",
        G_CALLBACK (midori_extensions_cell_renderer_toggled_cb), extensions);
    gtk_tree_view_append_column (GTK_TREE_VIEW (extensions->treeview), column);
    column = gtk_tree_view_column_new ();
    renderer_icon = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_icon, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_icon,
        (GtkTreeCellDataFunc)midori_extensions_treeview_render_icon_cb,
        extensions->treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (extensions->treeview), column);
    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_expand (column, TRUE);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, TRUE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_extensions_treeview_render_text_cb,
        extensions->treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (extensions->treeview), column);
    column = GTK_TREE_VIEW_COLUMN (midori_extensions_column_new ());
    g_signal_connect (column,
        "row-clicked",
        G_CALLBACK (midori_extensions_treeview_column_preference_clicked_cb),
        NULL);
    renderer_preferences = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_preferences, FALSE);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width (column, 30);
    gtk_tree_view_column_set_cell_data_func (column, renderer_preferences,
        (GtkTreeCellDataFunc)midori_extensions_treeview_render_preferences_cb,
        extensions->treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (extensions->treeview), column);
    g_object_unref (liststore);
    g_object_connect (extensions->treeview,
        "signal::row-activated",
        midori_extensions_treeview_row_activated_cb, extensions,
        NULL);
    gtk_tree_view_set_search_column (GTK_TREE_VIEW (extensions->treeview), 0);
    gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (extensions->treeview),
                                         extensions_column_search_equal_func, NULL, NULL);
    gtk_widget_show (extensions->treeview);
    gtk_box_pack_start (GTK_BOX (extensions), extensions->treeview, TRUE, TRUE, 0);
}

static void
midori_extensions_finalize (GObject* object)
{
    MidoriExtensions* extensions = MIDORI_EXTENSIONS (object);
    KatzeArray* array = katze_object_get_object (extensions->app, "extensions");
    MidoriExtension* extension;

    KATZE_ARRAY_FOREACH_ITEM (extension, array)
    {
        g_signal_handlers_disconnect_by_func (extension,
            midori_extensions_extension_activate_cb, extensions);
        g_signal_handlers_disconnect_by_func (extension,
            midori_extensions_extension_deactivate_cb, extensions);
    }
    g_signal_handlers_disconnect_by_func (array,
            midori_extensions_add_item_cb, extensions);

    g_object_unref (array);
}

