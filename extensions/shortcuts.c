/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

#include "config.h"

static void
shortcuts_browser_populate_tool_menu_cb (MidoriBrowser*   browser,
                                         GtkWidget*       menu,
                                         MidoriExtension* extension);

static void
shortcuts_app_add_browser_cb (MidoriApp*       app,
                              MidoriBrowser*   browser,
                              MidoriExtension* extension);

static void
shortcuts_deactivate_cb (MidoriExtension* extension,
                         MidoriBrowser*   browser)
{
    MidoriApp* app = midori_extension_get_app (extension);

    g_signal_handlers_disconnect_by_func (
        browser, shortcuts_browser_populate_tool_menu_cb, extension);
    g_signal_handlers_disconnect_by_func (
        extension, shortcuts_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, shortcuts_app_add_browser_cb, extension);
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
        GtkTreeIter child_iter;
        GtkTreeModel* liststore;

        gtk_tree_model_get (model, &iter, 6, &action, -1);
        accel_path = gtk_action_get_accel_path (action);
        gtk_accel_map_change_entry (accel_path, accel_key, accel_mods, TRUE);

        gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
                                                        &child_iter, &iter);
        liststore = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (model));
        gtk_list_store_set (GTK_LIST_STORE (liststore),
            &child_iter, 1, accel_key, 2, accel_mods, -1);

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
        GtkTreeIter child_iter;
        GtkTreeModel* liststore;

        gtk_tree_model_get (model, &iter, 6, &action, -1);
        accel_path = gtk_action_get_accel_path (action);
        gtk_accel_map_change_entry (accel_path, 0, 0, FALSE);

        gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
                                                        &child_iter, &iter);
        liststore = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (model));
        gtk_list_store_set (GTK_LIST_STORE (liststore),
            &child_iter, 1, 0, 2, 0, -1);

        g_object_unref (action);
    }
}

static gchar*
shortcuts_label_for_action (GtkAction* action)
{
    const gchar* name = gtk_action_get_name (action);
    gchar* label;
    gchar* stripped;

    if (g_str_equal (name, "ReloadStop"))
    {
        label = NULL;
        stripped = g_strdup (_("Reload page or stop loading"));
    }
    else if ((label = katze_object_get_string (action, "label")))
        stripped = katze_strip_mnemonics (label);
    else
    {
        GtkStockItem item;

        g_object_get (action, "stock-id", &label, NULL);
        if (gtk_stock_lookup (label, &item))
            stripped = katze_strip_mnemonics (item.label);
        else
            stripped = g_strdup ("");
    }

    g_free (label);
    return stripped;
}

static gboolean
shortcuts_hotkey_for_action (GtkAction*   action,
                             GtkAccelKey* key)
{
    const gchar* accel_path = gtk_action_get_accel_path (action);
    if (accel_path)
        if (gtk_accel_map_lookup_entry (accel_path, key))
            return TRUE;

    return FALSE;
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
    GtkTreeModel* model;
    GtkWidget* treeview;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer;
    GtkWidget* scrolled;
    GtkActionGroup* action_group;
    GList* actions;
    guint i;
    GtkAction* action;
    GtkWidget* dialog_vbox;
    #if HAVE_OSX
    GtkWidget* icon;
    #endif

    app = midori_extension_get_app (extension);
    browser = katze_object_get_object (app, "browser");

    dialog_title = _("Customize Keyboard shortcuts");
    dialog = gtk_dialog_new_with_buttons (dialog_title, GTK_WINDOW (browser),
        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
        #if !HAVE_OSX
        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
        #endif
        NULL);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_PROPERTIES);
    sokoke_widget_get_text_size (dialog, "M", &width, &height);
    gtk_window_set_default_size (GTK_WINDOW (dialog), width * 52, height * 24);
    g_signal_connect_swapped (dialog, "response",
        G_CALLBACK (gtk_widget_destroy), dialog);

    dialog_vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    if ((xfce_heading = sokoke_xfce_header_new (
        gtk_window_get_icon_name (GTK_WINDOW (dialog)), dialog_title)))
        gtk_box_pack_start (GTK_BOX (dialog_vbox),
                            xfce_heading, FALSE, FALSE, 0);
    hbox = gtk_hbox_new (FALSE, 0);

    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox,
                                 TRUE, TRUE, 12);
    liststore = gtk_list_store_new (7,
        G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_BOOLEAN,
        G_TYPE_STRING, GTK_TYPE_ACTION);
    model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (liststore));
    treeview = gtk_tree_view_new_with_model (model);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), renderer, "text", 0);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    column = gtk_tree_view_column_new ();
    renderer = gtk_cell_renderer_accel_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), renderer, "accel-key", 1);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), renderer, "accel-mods", 2);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), renderer, "accel-mode", 3);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), renderer, "sensitive", 4);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), renderer, "editable", 4);
    g_signal_connect (renderer, "accel-edited",
        G_CALLBACK (shortcuts_accel_edited_cb), model);
    g_signal_connect (renderer, "accel-cleared",
        G_CALLBACK (shortcuts_accel_cleared_cb), model);
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
    {
        gchar* label = shortcuts_label_for_action (action);
        GtkAccelKey key;
        gboolean has_hotkey = shortcuts_hotkey_for_action (action, &key);
        gtk_list_store_insert_with_values (GTK_LIST_STORE (liststore),
            NULL, G_MAXINT, 0, label, 1, key.accel_key, 2, key.accel_mods,
                            3, GTK_CELL_RENDERER_ACCEL_MODE_OTHER,
                            4, has_hotkey, 6, action, -1);
        g_free (label);
    }
    g_list_free (actions);

    g_object_unref (liststore);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
                                          0, GTK_SORT_ASCENDING);
    g_object_unref (model);

    gtk_widget_show_all (gtk_dialog_get_content_area(GTK_DIALOG (dialog)));

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
shortcuts_browser_populate_tool_menu_cb (MidoriBrowser*   browser,
                                         GtkWidget*       menu,
                                         MidoriExtension* extension)
{
    GtkWidget* menuitem;

    menuitem = gtk_menu_item_new_with_mnemonic (_("Customize Sh_ortcuts…"));
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (shortcuts_menu_configure_shortcuts_activate_cb), extension);
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
}

static void
shortcuts_app_add_browser_cb (MidoriApp*       app,
                              MidoriBrowser*   browser,
                              MidoriExtension* extension)
{
    g_signal_connect (browser, "populate-tool-menu",
        G_CALLBACK (shortcuts_browser_populate_tool_menu_cb), extension);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (shortcuts_deactivate_cb), browser);
}

static void
shortcuts_activate_cb (MidoriExtension* extension,
                       MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
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
        "version", "0.1" MIDORI_VERSION_SUFFIX,
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);

    g_signal_connect (extension, "activate",
        G_CALLBACK (shortcuts_activate_cb), NULL);

    return extension;
}
