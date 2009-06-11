/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

#include <midori/sokoke.h>
#include "config.h"

static void
shortcuts_app_add_browser_cb (MidoriApp*       app,
                              MidoriBrowser*   browser,
                              MidoriExtension* extension);

static void
shortcuts_deactivate_cb (MidoriExtension* extension,
                         GtkWidget*       menuitem)
{
    MidoriApp* app = midori_extension_get_app (extension);

    gtk_widget_destroy (menuitem);
    g_signal_handlers_disconnect_by_func (
        extension, shortcuts_deactivate_cb, menuitem);
    g_signal_handlers_disconnect_by_func (
        app, shortcuts_app_add_browser_cb, extension);
}

static void
shortcuts_preferences_render_text (GtkTreeViewColumn* column,
                                   GtkCellRenderer*   renderer,
                                   GtkTreeModel*      model,
                                   GtkTreeIter*       iter,
                                   MidoriExtension*   extension)
{
    GtkAction* action;
    gchar* label;

    gtk_tree_model_get (model, iter, 0, &action, -1);
    if ((label = katze_object_get_string (action, "label")))
        g_object_set (renderer, "text", label, NULL);
    else
    {
        GtkStockItem item;
        g_object_get (action, "stock-id", &label, NULL);
        if (gtk_stock_lookup (label, &item))
            g_object_set (renderer, "text", item.label, NULL);
    }
    g_free (label);
    g_object_unref (action);
}

static void
shortcuts_preferences_render_accel (GtkTreeViewColumn* column,
                                    GtkCellRenderer*   renderer,
                                    GtkTreeModel*      model,
                                    GtkTreeIter*       iter,
                                    MidoriExtension*   extension)
{
    GtkAction* action;
    const gchar* accel_path;
    GtkAccelKey key;

    gtk_tree_model_get (model, iter, 0, &action, -1);
    accel_path = gtk_action_get_accel_path (action);
    if (accel_path)
    {
        if (gtk_accel_map_lookup_entry (accel_path, &key))
            g_object_set (renderer,
                          "accel-key", key.accel_key,
                          "accel-mods", key.accel_mods,
                          NULL);
        g_object_set (renderer, "sensitive", TRUE, "editable", TRUE, NULL);
    }
    else
        g_object_set (renderer, "text", "", "sensitive", FALSE, NULL);
    g_object_unref (action);
}

static void
shortcuts_accel_edited_cb (GtkCellRenderer* renderer,
                           const gchar*     tree_path,
                           guint            accel_key,
                           GdkModifierType  accel_mods,
                           guint            keycode,
                           GtkTreeModel*    model)
{
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter_from_string (model, &iter, tree_path))
    {
        GtkAction* action;
        const gchar* accel_path;

        gtk_tree_model_get (model, &iter, 0, &action, -1);
        accel_path = gtk_action_get_accel_path (action);
        gtk_accel_map_change_entry (accel_path, accel_key, accel_mods, TRUE);

        g_object_unref (action);
    }
}

static void
shortcuts_accel_cleared_cb (GtkCellRenderer* renderer,
                            const gchar*     tree_path,
                            GtkTreeModel*    model)
{
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter_from_string (model, &iter, tree_path))
    {
        GtkAction* action;
        const gchar* accel_path;

        gtk_tree_model_get (model, &iter, 0, &action, -1);
        accel_path = gtk_action_get_accel_path (action);
        gtk_accel_map_change_entry (accel_path, 0, 0, FALSE);

        g_object_unref (action);
    }
}

static GtkWidget*
shortcuts_get_preferences_dialog (MidoriExtension* extension)
{
    MidoriApp* app;
    GtkWidget* browser;
    const gchar* dialog_title;
    GtkWidget* dialog;
    gint width, height;
    GtkWidget* xfce_heading;
    GtkWidget* hbox;
    GtkListStore* liststore;
    GtkWidget* treeview;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_accel;
    GtkWidget* scrolled;
    GtkActionGroup* action_group;
    GList* actions;
    guint i;
    GtkAction* action;
    #if HAVE_OSX
    GtkWidget* icon;
    #endif

    app = midori_extension_get_app (extension);
    browser = katze_object_get_object (app, "browser");

    dialog_title = _("Configure Keyboard shortcuts");
    dialog = gtk_dialog_new_with_buttons (dialog_title, GTK_WINDOW (browser),
        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
        #if !HAVE_OSX
        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
        #endif
        NULL);
    g_signal_connect (dialog, "destroy",
                      G_CALLBACK (gtk_widget_destroyed), &dialog);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_PROPERTIES);
    sokoke_widget_get_text_size (dialog, "M", &width, &height);
    gtk_window_set_default_size (GTK_WINDOW (dialog), width * 52, height * 24);
    g_signal_connect (dialog, "response",
                      G_CALLBACK (gtk_widget_destroy), dialog);
    if ((xfce_heading = sokoke_xfce_header_new (
        gtk_window_get_icon_name (GTK_WINDOW (dialog)), dialog_title)))
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                            xfce_heading, FALSE, FALSE, 0);
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
                                 TRUE, TRUE, 12);
    liststore = gtk_list_store_new (1, GTK_TYPE_ACTION);
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (liststore));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)shortcuts_preferences_render_text,
        extension, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    column = gtk_tree_view_column_new ();
    renderer_accel = gtk_cell_renderer_accel_new ();
    gtk_tree_view_column_pack_start (column, renderer_accel, TRUE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_accel,
        (GtkTreeCellDataFunc)shortcuts_preferences_render_accel,
        extension, NULL);
    g_signal_connect (renderer_accel, "accel-edited",
        G_CALLBACK (shortcuts_accel_edited_cb), liststore);
    g_signal_connect (renderer_accel, "accel-cleared",
        G_CALLBACK (shortcuts_accel_cleared_cb), liststore);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (scrolled), treeview);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                         GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (hbox), scrolled, TRUE, TRUE, 5);

    action_group = midori_browser_get_action_group (MIDORI_BROWSER (browser));
    actions = gtk_action_group_list_actions (action_group);
    i = 0;
    /* FIXME: Catch added and removed actions */
    while ((action = g_list_nth_data (actions, i++)))
        gtk_list_store_insert_with_values (GTK_LIST_STORE (liststore),
                                           NULL, G_MAXINT, 0, action, -1);
    g_list_free (actions);

    g_object_unref (liststore);

    gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);

    g_object_unref (browser);

    return dialog;
}

static void
shortcuts_menu_configure_shortcuts_activate_cb (GtkWidget*       menuitem,
                                                MidoriExtension* extension)
{
    static GtkWidget* dialog = NULL;

    if (!dialog)
    {
        dialog = shortcuts_get_preferences_dialog (extension);
        g_signal_connect (dialog, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &dialog);
        gtk_widget_show (dialog);
    }
    else
        gtk_window_present (GTK_WINDOW (dialog));
}

static void
shortcuts_app_add_browser_cb (MidoriApp*       app,
                              MidoriBrowser*   browser,
                              MidoriExtension* extension)
{
    GtkWidget* panel;
    GtkWidget* menu;
    GtkWidget* menuitem;

    panel = katze_object_get_object (browser, "panel");
    menu = katze_object_get_object (panel, "menu");
    g_object_unref (panel);
    menuitem = gtk_menu_item_new_with_mnemonic (_("Configure Sh_ortcuts..."));
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (shortcuts_menu_configure_shortcuts_activate_cb), extension);
    gtk_widget_show (menuitem);
    gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 3);
    g_object_unref (menu);

    g_signal_connect (extension, "deactivate",
        G_CALLBACK (shortcuts_deactivate_cb), menuitem);
}

static void
shortcuts_activate_cb (MidoriExtension* extension,
                       MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;
    guint i;

    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++)))
        shortcuts_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (shortcuts_app_add_browser_cb), extension);
    g_object_unref (browsers);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Shortcuts"),
        "description", _("View and edit keyboard shortcuts"),
        "version", "0.1",
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (shortcuts_activate_cb), NULL);

    return extension;
}
