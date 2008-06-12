/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "config.h"

#include "midori-addons.h"

#include "sokoke.h"
#include "gjs.h"
#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>
#include <glib/gi18n.h>

struct _MidoriAddons
{
    GtkVBox parent_instance;

    MidoriAddonKind kind;
    GtkWidget* toolbar;
    GtkWidget* treeview;
};

G_DEFINE_TYPE (MidoriAddons, midori_addons, GTK_TYPE_VBOX)

GType
midori_addon_kind_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_ADDON_EXTENSIONS, "MIDORI_ADDON_EXTENSIONS", N_("Extensions") },
         { MIDORI_ADDON_USER_SCRIPTS, "MIDORI_USER_SCRIPTS", N_("Userscripts") },
         { MIDORI_ADDON_USER_STYLES, "MIDORI_USER_STYLES", N_("Userstyles") },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriAddonKind", values);
    }
    return type;
}

static void
midori_addons_class_init (MidoriAddonsClass* class)
{
    /* Nothing to do */
}

static const
gchar* _folder_for_kind (MidoriAddonKind kind)
{
    switch (kind)
    {
    case MIDORI_ADDON_EXTENSIONS:
        return "extensions";
    case MIDORI_ADDON_USER_SCRIPTS:
        return "scripts";
    case MIDORI_ADDON_USER_STYLES:
        return "styles";
    default:
        return NULL;
    }
}

static void
midori_addons_button_add_clicked_cb (GtkToolItem*  toolitem,
                                     MidoriAddons* addons)
{
    GtkWidget* dialog = gtk_message_dialog_new (
        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (addons))),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
        "Put scripts in the folder ~/.local/share/midori/%s",
        _folder_for_kind (addons->kind));
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

static void
midori_addons_treeview_render_icon_cb (GtkTreeViewColumn* column,
                                       GtkCellRenderer*   renderer,
                                       GtkTreeModel*      model,
                                       GtkTreeIter*       iter,
                                       GtkWidget*         treeview)
{
    /* gchar* source_id;
    gtk_tree_model_get (model, iter, 2, &source_id, -1); */

    g_object_set (renderer, "stock-id", GTK_STOCK_FILE, NULL);

    /* g_free (source_id); */
}

static void
midori_addons_treeview_render_text_cb (GtkTreeViewColumn* column,
                                       GtkCellRenderer*   renderer,
                                       GtkTreeModel*      model,
                                       GtkTreeIter*       iter,
                                       GtkWidget*         treeview)
{
    gchar* filename;
    gint   a;
    gchar* b;
    gtk_tree_model_get (model, iter, 0, &filename, 1, &a, 2, &b, -1);

    /* FIXME: Convert filename to UTF8 */
    gchar* text = g_strdup_printf ("%s", filename);
    g_object_set (renderer, "text", text, NULL);
    g_free (text);

    g_free (filename);
    g_free (b);
}

static void
midori_addons_treeview_row_activated_cb (GtkTreeView*       treeview,
                                         GtkTreePath*       path,
                                         GtkTreeViewColumn* column,
                                         MidoriAddons*     addons)
{
    /*GtkTreeModel* model = gtk_tree_view_get_model (treeview);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gchar* b;
        gtk_tree_model_get (model, &iter, 2, &b, -1);
        g_free (b);
    }*/
}

static void
midori_addons_init (MidoriAddons* addons)
{
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_pixbuf;

    addons->treeview = gtk_tree_view_new ();
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (addons->treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_addons_treeview_render_icon_cb,
        addons->treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_addons_treeview_render_text_cb,
        addons->treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (addons->treeview), column);
    g_signal_connect (addons->treeview, "row-activated",
                      G_CALLBACK (midori_addons_treeview_row_activated_cb),
                      addons);
    gtk_widget_show (addons->treeview);
    gtk_box_pack_start (GTK_BOX (addons), addons->treeview, TRUE, TRUE, 0);
}

static gboolean
_js_script_from_file (JSContextRef js_context,
                      const gchar* filename,
                      gchar**      exception)
{
    gboolean result = FALSE;
    gchar* script;
    GError* error = NULL;

    if (g_file_get_contents (filename, &script, NULL, &error))
    {
        /* Wrap the script to prevent global variables */
        gchar* wrapped_script = g_strdup_printf (
            "var wrapped = function () { %s }; wrapped ();", script);
        if (gjs_script_eval (js_context, wrapped_script, exception))
            result = TRUE;
        g_free (wrapped_script);
        g_free (script);
    }
    else
    {
        *exception = g_strdup (error->message);
        g_error_free (error);
    }
    return result;
}

static void
midori_web_widget_window_object_cleared_cb (GtkWidget*         web_widget,
                                            WebKitWebFrame*    web_frame,
                                            JSGlobalContextRef js_context,
                                            JSObjectRef        js_window,
                                            MidoriAddons*      addons)
{
    /* FIXME: We want to honor system installed addons as well */
    gchar* addon_path = g_build_filename (g_get_user_data_dir (), PACKAGE_NAME,
                                          _folder_for_kind (addons->kind), NULL);
    GDir* addon_dir = g_dir_open (addon_path, 0, NULL);
    if (addon_dir)
    {
        const gchar* filename;
        while ((filename = g_dir_read_name (addon_dir)))
        {
            gchar* fullname = g_build_filename (addon_path, filename, NULL);
            gchar* exception;
            if (!_js_script_from_file (js_context, fullname, &exception))
            {
                gchar* message = g_strdup_printf ("console.error ('%s');",
                                                  exception);
                gjs_script_eval (js_context, message, NULL);
                g_free (message);
                g_free (exception);
            }
            g_free (fullname);
        }
        g_dir_close (addon_dir);
    }
}

/**
 * midori_addons_new:
 * @web_widget: a web widget
 * @kind: the kind of addon
 * @extension: a file extension mask
 *
 * Creates a new addons widget.
 *
 * @web_widget can be one of the following:
 *     %MidoriBrowser, %MidoriWebView, %WebKitWebView
 *
 * Note: Currently @extension has no effect.
 *
 * Return value: a new #MidoriAddons
 **/
GtkWidget*
midori_addons_new (GtkWidget*      web_widget,
                   MidoriAddonKind kind)
{
    g_return_val_if_fail (GTK_IS_WIDGET (web_widget), NULL);

    MidoriAddons* addons = g_object_new (MIDORI_TYPE_ADDONS,
                                         /* "kind", kind, */
                                         NULL);

    addons->kind = kind;
    if (kind == MIDORI_ADDON_USER_SCRIPTS)
        g_signal_connect (web_widget, "window-object-cleared",
            G_CALLBACK (midori_web_widget_window_object_cleared_cb), addons);

    GtkListStore* liststore = gtk_list_store_new (3, G_TYPE_STRING,
                                                     G_TYPE_INT,
                                                     G_TYPE_STRING);
    /* FIXME: We want to honor system installed addons as well */
    gchar* addon_path = g_build_filename (g_get_user_data_dir (), PACKAGE_NAME,
                                          _folder_for_kind (addons->kind), NULL);
    GDir* addon_dir = g_dir_open (addon_path, 0, NULL);
    if (addon_dir)
    {
        const gchar* filename;
        while ((filename = g_dir_read_name (addon_dir)))
        {
            GtkTreeIter iter;
            gtk_list_store_append (liststore, &iter);
            gtk_list_store_set (liststore, &iter,
                0, filename, 1, 0, 2, "", -1);
        }
        g_dir_close (addon_dir);
    }
    gtk_tree_view_set_model (GTK_TREE_VIEW (addons->treeview),
                             GTK_TREE_MODEL (liststore));

    return GTK_WIDGET (addons);
}

/**
 * midori_addons_get_toolbar:
 *
 * Retrieves the toolbar of the addons. A new widget is created on
 * the first call of this function.
 *
 * Return value: a new #MidoriAddons
 **/
GtkWidget*
midori_addons_get_toolbar (MidoriAddons* addons)
{
    g_return_val_if_fail (MIDORI_IS_ADDONS (addons), NULL);

    if (!addons->toolbar)
    {
        GtkWidget* toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        GtkToolItem* toolitem = gtk_tool_item_new ();
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        toolitem = gtk_separator_tool_item_new ();
        gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (toolitem),
                                          FALSE);
        gtk_tool_item_set_expand (toolitem, TRUE);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_ADD);
        gtk_tool_item_set_is_important (toolitem, TRUE);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_addons_button_add_clicked_cb), addons);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        addons->toolbar = toolbar;
    }

    return addons->toolbar;
}
