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
        guint i;

        /* FIXME: Handle NULL and subsequent assignments */
        extensions->app = g_value_get_object (value);
        array = katze_object_get_object (extensions->app, "extensions");
        g_signal_connect (array, "add-item",
            G_CALLBACK (midori_extensions_add_item_cb), extensions);

        i = 0;
        while ((extension = katze_array_get_nth_item (array, i++)))
            midori_extensions_add_item_cb (array, extension, extensions);
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

    g_object_set (renderer, "active", midori_extension_is_active (extension), NULL);

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
    text = g_markup_printf_escaped ("<b>%s</b> %s\n%s", name, version, desc);
    g_free (name);
    g_free (version);
    g_free (desc);

    g_object_set (renderer, "markup", text, NULL);

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

        gtk_tree_model_get (model, &iter, 0, &extension, -1);
        if (midori_extension_is_active (extension))
            midori_extension_deactivate (extension);
        else
            g_signal_emit_by_name (extension, "activate", extensions->app);

        g_object_unref (extension);
    }
}

static void
midori_extensions_preferences_activate_cb (GtkWidget*        menuitem,
                                           MidoriExtensions* extensions)
{
    MidoriExtension* extension;

    extension = g_object_get_data (G_OBJECT (menuitem), "MidoriExtension");
    g_return_if_fail (extension != NULL);
}

static void
midori_extensions_website_activate_cb (GtkWidget*        menuitem,
                                       MidoriExtensions* extensions)
{
    gchar*         uri;
    gint           n;
    MidoriBrowser* browser;

    MidoriExtension* extension;

    extension = g_object_get_data (G_OBJECT (menuitem), "MidoriExtension");
    g_return_if_fail (extension != NULL);
    uri = katze_object_get_string (extension, "website");

    browser = midori_browser_get_for_widget (GTK_WIDGET (extensions));
    n = midori_browser_add_uri (browser, uri);
    midori_browser_set_current_page (browser, n);

    g_free (uri);
}

static void
midori_extensions_about_activate_cb (GtkWidget*        menuitem,
                                     MidoriExtensions* extensions)
{
    MidoriExtension* extension;

    extension = g_object_get_data (G_OBJECT (menuitem), "MidoriExtension");
    g_return_if_fail (extension != NULL);
}

static GtkWidget*
midori_extensions_popup_menu_item (GtkMenu*          menu,
                                   const gchar*      stock_id,
                                   const gchar*      label,
                                   MidoriExtension*  extension,
                                   gpointer          callback,
                                   gboolean          enabled,
                                   MidoriExtensions* extensions)
{
    GtkWidget* menuitem;

    menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);
    if (label)
        gtk_label_set_text_with_mnemonic (GTK_LABEL (gtk_bin_get_child (
        GTK_BIN (menuitem))), label);

    if (!enabled)
        gtk_widget_set_sensitive (menuitem, FALSE);

    g_object_set_data (G_OBJECT (menuitem), "MidoriExtension", extension);

    if (callback)
        g_signal_connect (menuitem, "activate", G_CALLBACK (callback), extensions);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);

    return menuitem;
}

static void
midori_extensions_popup (GtkWidget*        widget,
                         GdkEventButton*   event,
                         MidoriExtension*  extension,
                         MidoriExtensions* extensions)
{
    GtkWidget* menu;
    gchar* website;

    website = katze_object_get_string (extension, "website");

    menu = gtk_menu_new ();
    midori_extensions_popup_menu_item (GTK_MENU (menu), GTK_STOCK_PREFERENCES, NULL, extension,
                                       midori_extensions_preferences_activate_cb, FALSE,
                                       extensions);
    midori_extensions_popup_menu_item (GTK_MENU (menu), GTK_STOCK_HOME, NULL, extension,
                                       midori_extensions_website_activate_cb, website != NULL,
                                       extensions);
    midori_extensions_popup_menu_item (GTK_MENU (menu), GTK_STOCK_ABOUT, NULL, extension,
                                       midori_extensions_about_activate_cb, FALSE,
                                       extensions);

    sokoke_widget_popup (widget, GTK_MENU (menu),
                         event, SOKOKE_MENU_POSITION_CURSOR);

    g_free (website);
}

static gboolean
midori_extensions_popup_menu_cb (GtkWidget*        widget,
                                 MidoriExtensions* extensions)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        MidoriExtension *extension;

        gtk_tree_model_get (model, &iter, 0, &extension, -1);

        midori_extensions_popup (widget, NULL, extension, extensions);
        g_object_unref (extension);
        return TRUE;
    }
	return FALSE;
}

static gboolean
midori_extensions_button_press_event_cb (GtkWidget*         widget,
                                         GdkEventButton*    event,
                                         MidoriExtensions*  extensions)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (event->button != 3)
        return FALSE;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        MidoriExtension *extension;

        gtk_tree_model_get (model, &iter, 0, &extension, -1);

        midori_extensions_popup (widget, event, extension, extensions);
        g_object_unref (extension);
        return TRUE;
    }
    return FALSE;
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
        MidoriExtension *extension;

        gtk_tree_model_get (model, &iter, 0, &extension, -1);
        if (midori_extension_is_active (extension))
            midori_extension_deactivate (extension);
        else
            g_signal_emit_by_name (extension, "activate", extensions->app);

        g_object_unref (extension);
    }
}

static void
midori_extensions_init (MidoriExtensions* extensions)
{
    /* Create the treeview */
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_toggle;
    GtkListStore* liststore = gtk_list_store_new (1, G_TYPE_OBJECT);
    extensions->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (liststore));
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
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_extensions_treeview_render_text_cb,
        extensions->treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (extensions->treeview), column);
    g_object_unref (liststore);
    g_object_connect (extensions->treeview,
        "signal::row-activated",
        midori_extensions_treeview_row_activated_cb, extensions,
        "signal::button-press-event",
        midori_extensions_button_press_event_cb, extensions,
        "signal::popup-menu",
        midori_extensions_popup_menu_cb, extensions,
        NULL);
    gtk_widget_show (extensions->treeview);
    gtk_box_pack_start (GTK_BOX (extensions), extensions->treeview, TRUE, TRUE, 0);
}

static void
midori_extensions_finalize (GObject* object)
{
    MidoriExtensions* extensions = MIDORI_EXTENSIONS (object);
    KatzeArray* array = katze_object_get_object (extensions->app, "extensions");
    guint i = 0;
    MidoriExtension* extension;

    while ((extension = katze_array_get_nth_item (array, i++)))
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
