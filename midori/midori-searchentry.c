/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-searchentry.h"

#include "sokoke.h"

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

struct _MidoriSearchEntry
{
    GtkIconEntry parent_instance;

    MidoriWebList* search_engines;
    MidoriWebItem* current_item;
};

G_DEFINE_TYPE (MidoriSearchEntry, midori_search_entry, GTK_TYPE_ICON_ENTRY)

enum
{
    PROP_0,

    PROP_SEARCH_ENGINES,
    PROP_CURRENT_ITEM,
    PROP_DIALOG
};

static void
midori_search_entry_finalize (GObject* object);

static void
midori_search_entry_set_property (GObject*      object,
                                  guint         prop_id,
                                  const GValue* value,
                                  GParamSpec*   pspec);

static void
midori_search_entry_get_property (GObject*    object,
                                  guint       prop_id,
                                  GValue*     value,
                                  GParamSpec* pspec);

static void
midori_search_entry_class_init (MidoriSearchEntryClass* class)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_search_entry_finalize;
    gobject_class->set_property = midori_search_entry_set_property;
    gobject_class->get_property = midori_search_entry_get_property;

    g_object_class_install_property (gobject_class,
                                     PROP_SEARCH_ENGINES,
                                     g_param_spec_object (
                                     "search-engines",
                                     _("Search Engines"),
                                     _("The list of search engines"),
                                     MIDORI_TYPE_WEB_LIST,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_CURRENT_ITEM,
                                     g_param_spec_object (
                                     "current-item",
                                     _("Current Item"),
                                     _("The currently selected item"),
                                     MIDORI_TYPE_WEB_ITEM,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_DIALOG,
                                     g_param_spec_object (
                                     "dialog",
                                     _("Dialog"),
                                     _("A dialog to manage search engines"),
                                     GTK_TYPE_DIALOG,
                                     G_PARAM_READABLE));
}

static void
midori_search_entry_engine_activate_cb (GtkWidget*         widget,
                                        MidoriSearchEntry* search_entry)
{
    MidoriWebItem* web_item;

    web_item = (MidoriWebItem*)g_object_get_data (G_OBJECT (widget), "engine");
    midori_search_entry_set_current_item (search_entry, web_item);
}

static void
midori_search_entry_manage_activate_cb (GtkWidget*         menuitem,
                                        MidoriSearchEntry* search_entry)
{
    GtkWidget* dialog;

    dialog = midori_search_entry_get_dialog (search_entry);
    if (GTK_WIDGET_VISIBLE (dialog))
        gtk_window_present (GTK_WINDOW (dialog));
    else
        gtk_widget_show (dialog);
}

static void
midori_search_entry_icon_released_cb (GtkWidget*             widget,
                                      GtkIconEntryPosition* pos,
                                      gint                   button)
{
    MidoriSearchEntry* search_entry;
    MidoriWebList* search_engines;
    GtkWidget* menu;
    guint n, i;
    GtkWidget* menuitem;
    MidoriWebItem* web_item;
    GdkPixbuf* pixbuf;
    GtkWidget* icon;

    search_entry = MIDORI_SEARCH_ENTRY (widget);
    search_engines = search_entry->search_engines;
    menu = gtk_menu_new ();
    n = midori_web_list_get_length (search_engines);
    if (n)
    {
        for (i = 0; i < n; i++)
        {
            web_item = midori_web_list_get_nth_item (search_engines, i);
            menuitem = gtk_image_menu_item_new_with_label (
                midori_web_item_get_name (web_item));
            pixbuf = sokoke_web_icon (midori_web_item_get_icon (web_item),
                                      GTK_ICON_SIZE_MENU, menuitem);
            icon = gtk_image_new_from_pixbuf (pixbuf);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
            g_object_unref (pixbuf);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
            g_object_set_data (G_OBJECT (menuitem), "engine", web_item);
            g_signal_connect (menuitem, "activate",
                G_CALLBACK (midori_search_entry_engine_activate_cb), widget);
            gtk_widget_show (menuitem);
        }
    }
    else
    {
        menuitem = gtk_image_menu_item_new_with_label (_("Empty"));
        gtk_widget_set_sensitive (menuitem, FALSE);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        gtk_widget_show (menuitem);
    }

    menuitem = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
    menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Manage Search Engines"));
    icon = gtk_image_new_from_stock (GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_search_entry_manage_activate_cb), widget);
    gtk_widget_show (menuitem);
    sokoke_widget_popup (widget, GTK_MENU (menu),
                         NULL, SOKOKE_MENU_POSITION_LEFT);
}

static void
_midori_search_entry_move_index (MidoriSearchEntry* search_entry,
                                 guint              n)
{
    gint i;
    MidoriWebItem* web_item;

    i = midori_web_list_get_item_index (search_entry->search_engines,
                                        search_entry->current_item);
    web_item = midori_web_list_get_nth_item (search_entry->search_engines,
                                             i + n);
    if (web_item)
        midori_search_entry_set_current_item (search_entry, web_item);
}

static gboolean
midori_search_entry_key_press_event_cb (MidoriSearchEntry* search_entry,
                                        GdkEventKey*       event)
{
    GdkModifierType state;
    gint x, y;

    gdk_window_get_pointer (NULL, &x, &y, &state);
    if (!(state & GDK_CONTROL_MASK))
        return FALSE;
    switch (event->keyval)
    {
    case GDK_Up:
        _midori_search_entry_move_index (search_entry, - 1);
        return TRUE;
    case GDK_Down:
        _midori_search_entry_move_index (search_entry, + 1);
        return TRUE;
    }
    return FALSE;
}

static gboolean
midori_search_entry_scroll_event_cb (MidoriSearchEntry* search_entry,
                                     GdkEventScroll*    event)
{
    if (event->direction == GDK_SCROLL_DOWN)
        _midori_search_entry_move_index (search_entry, + 1);
    else if (event->direction == GDK_SCROLL_UP)
        _midori_search_entry_move_index (search_entry, - 1);
    return TRUE;
}

static void
midori_search_entry_engines_add_item_cb (MidoriWebList*     web_list,
                                         MidoriWebItem*     item,
                                         MidoriSearchEntry* search_entry)
{
    if (!search_entry->current_item)
        midori_search_entry_set_current_item (search_entry, item);
}

static void
midori_search_entry_engines_remove_item_cb (MidoriWebList*     web_list,
                                            MidoriWebItem*     item,
                                            MidoriSearchEntry* search_entry)
{
    MidoriWebItem* web_item;

    if (search_entry->current_item == item)
    {
        web_item = midori_web_list_get_nth_item (web_list, 0);
        if (web_item)
            midori_search_entry_set_current_item (search_entry, web_item);
        else
        {
            gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (search_entry),
                                                 GTK_ICON_ENTRY_PRIMARY, NULL);
            sokoke_entry_set_default_text (GTK_ENTRY (search_entry), "");

            katze_object_assign (search_entry->current_item, NULL);
            g_object_notify (G_OBJECT (search_entry), "current-item");
        }
    }
}

static void
midori_search_entry_init (MidoriSearchEntry* search_entry)
{
    search_entry->search_engines = midori_web_list_new ();
    search_entry->current_item = NULL;

    gtk_icon_entry_set_icon_highlight (GTK_ICON_ENTRY (search_entry),
                                       GTK_ICON_ENTRY_PRIMARY, TRUE);
    g_object_connect (search_entry,
                      "signal::icon-released",
                      midori_search_entry_icon_released_cb, NULL,
                      "signal::key-press-event",
                      midori_search_entry_key_press_event_cb, NULL,
                      "signal::scroll-event",
                      midori_search_entry_scroll_event_cb, NULL,
                      NULL);

    g_object_connect (search_entry->search_engines,
        "signal-after::add-item",
        midori_search_entry_engines_add_item_cb, search_entry,
        "signal-after::remove-item",
        midori_search_entry_engines_remove_item_cb, search_entry,
        NULL);
}

static void
midori_search_entry_finalize (GObject* object)
{
    MidoriSearchEntry* search_entry = MIDORI_SEARCH_ENTRY (object);

    g_object_unref (search_entry->search_engines);
    if (search_entry->current_item)
        g_object_unref (search_entry->current_item);

    G_OBJECT_CLASS (midori_search_entry_parent_class)->finalize (object);
}

static void
midori_search_entry_set_property (GObject*      object,
                                  guint         prop_id,
                                  const GValue* value,
                                  GParamSpec*   pspec)
{
    MidoriSearchEntry* search_entry = MIDORI_SEARCH_ENTRY (object);

    switch (prop_id)
    {
    case PROP_SEARCH_ENGINES:
        midori_search_entry_set_search_engines (search_entry,
                                                g_value_get_object (value));
        break;
    case PROP_CURRENT_ITEM:
        midori_search_entry_set_current_item (search_entry,
                                              g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_search_entry_get_property (GObject*    object,
                                  guint       prop_id,
                                  GValue*     value,
                                  GParamSpec* pspec)
{
    MidoriSearchEntry* search_entry = MIDORI_SEARCH_ENTRY (object);

    switch (prop_id)
    {
    case PROP_SEARCH_ENGINES:
        g_value_set_object (value, search_entry->search_engines);
        break;
    case PROP_CURRENT_ITEM:
        g_value_set_object (value, search_entry->current_item);
        break;
    case PROP_DIALOG:
        g_value_set_object (value, midori_search_entry_get_dialog (search_entry));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * midori_search_entry_new:
 *
 * Creates a new #MidoriSearchEntry.
 *
 * Return value: a new #MidoriSearchEntry
 **/
GtkWidget*
midori_search_entry_new (void)
{
    GtkWidget* search_entry = g_object_new (MIDORI_TYPE_SEARCH_ENTRY, NULL);

    return search_entry;
}

/**
 * midori_search_entry_get_search_engines:
 * @search_entry: a #MidoriSearchEntry
 *
 * Retrieves the list of search engines.
 *
 * Return value: the list of search engines
 **/
MidoriWebList*
midori_search_entry_get_search_engines (MidoriSearchEntry* search_entry)
{
    g_return_val_if_fail (MIDORI_IS_SEARCH_ENTRY (search_entry), NULL);

    return search_entry->search_engines;
}

/**
 * midori_search_entry_set_search_engines:
 * @search_entry: a #MidoriSearchEntry
 * @search_engines: a list of search engines
 *
 * Sets the list of search engines.
 **/
void
midori_search_entry_set_search_engines (MidoriSearchEntry* search_entry,
                                        MidoriWebList*     search_engines)
{
    g_return_if_fail (MIDORI_IS_SEARCH_ENTRY (search_entry));

    g_object_ref (search_engines);
    katze_object_assign (search_entry->search_engines, search_engines);
    g_object_notify (G_OBJECT (search_entry), "search-engines");

    g_object_connect (search_engines,
        "signal-after::add-item",
        midori_search_entry_engines_add_item_cb, search_entry,
        "signal-after::remove-item",
        midori_search_entry_engines_remove_item_cb, search_entry,
        NULL);
}

/**
 * midori_search_entry_set_current_item:
 * @search_entry: a #MidoriSearchEntry
 * @item: a #MidoriWebItem
 *
 * Looks up the specified item in the list of search engines and makes
 * it the currently selected item.
 *
 * This function fails if @item is not in the current list.
 **/
void
midori_search_entry_set_current_item (MidoriSearchEntry* search_entry,
                                      MidoriWebItem*     web_item)
{
    GdkPixbuf* pixbuf;

    g_return_if_fail (MIDORI_IS_SEARCH_ENTRY (search_entry));
    g_return_if_fail (MIDORI_IS_WEB_ITEM (web_item));

    pixbuf = sokoke_web_icon (midori_web_item_get_icon (web_item),
                              GTK_ICON_SIZE_MENU, GTK_WIDGET (search_entry));
    gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (search_entry),
                                         GTK_ICON_ENTRY_PRIMARY,
                                         pixbuf);
    g_object_unref (pixbuf);
    sokoke_entry_set_default_text (GTK_ENTRY (search_entry),
                                   midori_web_item_get_name (web_item));

    g_object_ref (web_item);
    katze_object_assign (search_entry->current_item, web_item);
    g_object_notify (G_OBJECT (search_entry), "current-item");
}

/**
 * midori_search_entry_get_current_item:
 * @search_entry: a #MidoriSearchEntry
 *
 * Retrieves the currently selected item.
 *
 * Return value: the selected web item, or %NULL
 **/
MidoriWebItem*
midori_search_entry_get_current_item (MidoriSearchEntry* search_entry)
{
    g_return_val_if_fail (MIDORI_IS_SEARCH_ENTRY (search_entry), NULL);

    return search_entry->current_item;
}

static void
midori_search_entry_dialog_render_icon_cb (GtkTreeViewColumn* column,
                                           GtkCellRenderer*   renderer,
                                           GtkTreeModel*      model,
                                           GtkTreeIter*       iter,
                                           GtkWidget*         treeview)
{
    MidoriWebItem* web_item;
    const gchar* icon;
    GdkPixbuf* pixbuf;

    gtk_tree_model_get (model, iter, 0, &web_item, -1);

    icon = midori_web_item_get_icon (web_item);
    if (icon)
    {
        pixbuf = sokoke_web_icon (icon, GTK_ICON_SIZE_DND, treeview);
        g_object_set (renderer, "pixbuf", pixbuf, NULL);
        if (pixbuf)
            g_object_unref (pixbuf);
    }
    else
        g_object_set (renderer, "pixbuf", NULL, NULL);
}

static void
midori_search_entry_dialog_render_text (GtkTreeViewColumn* column,
                                        GtkCellRenderer*   renderer,
                                        GtkTreeModel*      model,
                                        GtkTreeIter*       iter,
                                        GtkWidget*         treeview)
{
    MidoriWebItem* web_item;
    const gchar* name;
    const gchar* description;
    gchar* markup;

    gtk_tree_model_get (model, iter, 0, &web_item, -1);
    name = midori_web_item_get_name (web_item);
    description = midori_web_item_get_description (web_item);
    markup = g_markup_printf_escaped ("<b>%s</b>\n%s", name, description);
    g_object_set (renderer, "markup", markup, NULL);
    g_free (markup);
}

static void
midori_search_entry_editor_name_changed_cb (GtkWidget* widget,
                                            GtkWidget* dialog)
{
    const gchar* text = gtk_entry_get_text (GTK_ENTRY (widget));
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
        GTK_RESPONSE_ACCEPT, text && *text);
}

const gchar*
STR_NON_NULL (const gchar* string)
{
    return string ? string : "";
}

static void
midori_search_entry_get_editor (GtkWidget* treeview,
                                gboolean   new_engine)
{
    MidoriSearchEntry* search_entry;
    GtkWidget* dialog;
    GtkSizeGroup* sizegroup;
    MidoriWebItem* web_item;
    GtkWidget* hbox;
    GtkWidget* label;
    GtkTreeModel* liststore;
    GtkTreeIter iter;
    GtkTreeSelection* selection;
    GtkWidget* entry_name;
    GtkWidget* entry_description;
    GtkWidget* entry_uri;
    GtkWidget* entry_icon;
    GtkWidget* entry_token;

    GtkWidget* toplevel = gtk_widget_get_toplevel (treeview);
    dialog = gtk_dialog_new_with_buttons (
        new_engine ? _("Add search engine") : _("Edit search engine"),
        toplevel ? GTK_WINDOW (toplevel) : NULL,
        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        new_engine ? GTK_STOCK_ADD : GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_window_set_icon_name (GTK_WINDOW (dialog),
        new_engine ? GTK_STOCK_ADD : GTK_STOCK_REMOVE);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 5);
    sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    if (new_engine)
    {
        web_item = midori_web_item_new ();
        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_ACCEPT, FALSE);
    }
    else
    {
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        gtk_tree_selection_get_selected (selection, &liststore, &iter);
        gtk_tree_model_get (liststore, &iter, 0, &web_item, -1);
    }

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Name:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    entry_name = gtk_entry_new ();
    g_signal_connect (entry_name, "changed",
        G_CALLBACK (midori_search_entry_editor_name_changed_cb), dialog);
    gtk_entry_set_activates_default (GTK_ENTRY (entry_name), TRUE);
    if (!new_engine)
        gtk_entry_set_text (GTK_ENTRY (entry_name),
            STR_NON_NULL (midori_web_item_get_name (web_item)));
    gtk_box_pack_start (GTK_BOX (hbox), entry_name, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
    gtk_widget_show_all (hbox);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Description:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    entry_description = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry_description), TRUE);
    if (!new_engine)
        gtk_entry_set_text (GTK_ENTRY (entry_description)
         , STR_NON_NULL (midori_web_item_get_description (web_item)));
    gtk_box_pack_start (GTK_BOX (hbox), entry_description, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
    gtk_widget_show_all (hbox);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_URL:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    entry_uri = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry_uri), TRUE);
    if (!new_engine)
        gtk_entry_set_text (GTK_ENTRY (entry_uri)
         , STR_NON_NULL (midori_web_item_get_uri (web_item)));
    gtk_box_pack_start (GTK_BOX (hbox), entry_uri, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
    gtk_widget_show_all (hbox);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Icon (name or file):"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    entry_icon = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry_icon), TRUE);
    if (!new_engine)
        gtk_entry_set_text (GTK_ENTRY (entry_icon)
         , STR_NON_NULL (midori_web_item_get_icon (web_item)));
    gtk_box_pack_start (GTK_BOX (hbox), entry_icon, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
    gtk_widget_show_all (hbox);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER(hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Token:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    entry_token = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry_token), TRUE);
    if (!new_engine)
        gtk_entry_set_text (GTK_ENTRY (entry_token)
         , STR_NON_NULL (midori_web_item_get_token (web_item)));
    gtk_box_pack_start (GTK_BOX (hbox), entry_token, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
    gtk_widget_show_all (hbox);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        g_object_set (web_item,
            "name", gtk_entry_get_text (GTK_ENTRY (entry_name)),
            "description", gtk_entry_get_text (GTK_ENTRY (entry_description)),
            "uri", gtk_entry_get_text (GTK_ENTRY (entry_uri)),
            "icon", gtk_entry_get_text (GTK_ENTRY (entry_icon)),
            "token", gtk_entry_get_text (GTK_ENTRY (entry_token)),
            NULL);

        search_entry = g_object_get_data (G_OBJECT (treeview), "search-entry");
        if (new_engine)
            midori_web_list_add_item (search_entry->search_engines, web_item);
    }
    gtk_widget_destroy (dialog);
}

static void
midori_search_entry_dialog_add_cb (GtkWidget* widget,
                                   GtkWidget* treeview)
{
    MidoriSearchEntry* search_entry;

    search_entry = g_object_get_data (G_OBJECT (treeview), "search-entry");
    midori_search_entry_get_editor (treeview, TRUE);
}

static void
midori_search_entry_dialog_edit_cb (GtkWidget* widget,
                                    GtkWidget* treeview)
{
    MidoriSearchEntry* search_entry;
    GtkTreeSelection* selection;

    search_entry = g_object_get_data (G_OBJECT (treeview), "search-entry");
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    if (gtk_tree_selection_get_selected (selection, NULL, NULL))
        midori_search_entry_get_editor (treeview, FALSE);
}

static void
midori_search_entry_dialog_remove_cb (GtkWidget* widget,
                                      GtkWidget* treeview)
{
    MidoriSearchEntry* search_entry;
    GtkTreeSelection* selection;
    GtkTreeModel* liststore;
    GtkTreeIter iter;
    MidoriWebItem* web_item;
    MidoriWebList* search_engines;

    search_entry = g_object_get_data (G_OBJECT (treeview), "search-entry");
    search_engines = search_entry->search_engines;
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    if (gtk_tree_selection_get_selected (selection, &liststore, &iter))
    {
        gtk_tree_model_get (liststore, &iter, 0, &web_item, -1);
        midori_web_list_remove_item (search_engines, web_item);
        g_object_unref (web_item);
        /* FIXME: we want to allow undo of some kind */
    }
}

static void
midori_search_entry_treeview_selection_cb (GtkTreeSelection* selection,
                                           GtkWidget*        treeview)
{
    gboolean selected;
    GtkWidget* edit_button;
    GtkWidget* remove_button;

    selected = gtk_tree_selection_get_selected (selection, NULL, NULL);

    edit_button = g_object_get_data (G_OBJECT (treeview), "edit-button");
    remove_button = g_object_get_data (G_OBJECT (treeview), "remove-button");

    gtk_widget_set_sensitive (edit_button, selected);
    gtk_widget_set_sensitive (remove_button, selected);
}

static void
midori_search_entry_dialog_engines_add_item_cb (MidoriWebList* web_list,
                                                MidoriWebItem* item,
                                                GtkWidget*     treeview)
{
    GtkTreeModel* liststore;
    GtkTreeIter iter;

    liststore = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
    gtk_list_store_append (GTK_LIST_STORE (liststore), &iter);
    gtk_list_store_set (GTK_LIST_STORE (liststore), &iter, 0, item, -1);
}

static void
midori_search_entry_dialog_engines_remove_item_cb (MidoriWebList* web_list,
                                                   MidoriWebItem* item,
                                                   GtkWidget*     treeview)
{
    GtkTreeModel* liststore;
    GtkTreeIter iter;
    gboolean valid;
    MidoriWebItem* web_item;

    liststore = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
    valid = gtk_tree_model_get_iter_first (liststore, &iter);
    while (valid)
    {
        gtk_tree_model_get (liststore, &iter, 0, &web_item, -1);
        if (web_item == item)
        {
            gtk_list_store_remove (GTK_LIST_STORE (liststore), &iter);
            valid = FALSE;
        }
        else
            valid = gtk_tree_model_iter_next (liststore, &iter);
    }
}

static void
midori_search_entry_treeview_destroy_cb (GtkWidget* treeview,
                                         GtkWidget* search_entry)
{
    g_signal_handlers_disconnect_by_func (
        MIDORI_SEARCH_ENTRY (search_entry)->search_engines,
        midori_search_entry_dialog_engines_add_item_cb, treeview);
    g_signal_handlers_disconnect_by_func (
        MIDORI_SEARCH_ENTRY (search_entry)->search_engines,
        midori_search_entry_dialog_engines_remove_item_cb, treeview);
}

/**
 * midori_search_entry_get_dialog:
 * @search_entry: a #MidoriSearchEntry
 *
 * Obtains a dialog that provides an interface for managing
 * the list of search engines.
 *
 * A new dialog is created each time, so it may be a good idea
 * to store the pointer for the life time of the dialog.
 *
 * Return value: a dialog
 **/
GtkWidget*
midori_search_entry_get_dialog (MidoriSearchEntry* search_entry)
{
    static GtkWidget* dialog = NULL;
    const gchar* dialog_title;
    GtkWidget* toplevel;
    gint width, height;
    GtkWidget* xfce_heading;
    GtkWidget* hbox;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_pixbuf;
    GtkListStore* liststore;
    GtkWidget* treeview;
    GtkWidget* scrolled;
    guint n, i;
    MidoriWebItem* web_item;
    GtkWidget* vbox;
    GtkWidget* button;

    g_return_val_if_fail (MIDORI_IS_SEARCH_ENTRY (search_entry), NULL);

    /* If there is a dialog, use that. We want only one. */
    if (dialog)
        return dialog;

    dialog_title = _("Manage search engines");
    toplevel = gtk_widget_get_toplevel (GTK_WIDGET (search_entry));
    dialog = gtk_dialog_new_with_buttons (dialog_title,
        toplevel ? GTK_WINDOW (toplevel) : NULL,
        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
        GTK_STOCK_HELP, GTK_RESPONSE_HELP,
        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
        NULL);
    g_signal_connect (dialog, "destroy",
                      G_CALLBACK (gtk_widget_destroyed), &dialog);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_PROPERTIES);
    /* TODO: Implement some kind of help function */
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                       GTK_RESPONSE_HELP, FALSE);
    sokoke_widget_get_text_size (dialog, "M", &width, &height);
    gtk_window_set_default_size (GTK_WINDOW (dialog), width * 45, -1);
    g_signal_connect (dialog, "response",
                      G_CALLBACK (gtk_widget_destroy), dialog);
    /* TODO: Do we want tooltips for explainations or can we omit that?
             We need mnemonics */
    if ((xfce_heading = sokoke_xfce_header_new (
        gtk_window_get_icon_name (GTK_WINDOW (dialog)), dialog_title)))
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                            xfce_heading, FALSE, FALSE, 0);
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
                                 TRUE, TRUE, 12);
    liststore = gtk_list_store_new (1, MIDORI_TYPE_WEB_ITEM);
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (liststore));
    g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
        "changed", G_CALLBACK (midori_search_entry_treeview_selection_cb),
        treeview);
    g_object_set_data (G_OBJECT (treeview), "search-entry", search_entry);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_search_entry_dialog_render_icon_cb,
        treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, TRUE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_search_entry_dialog_render_text,
        treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (scrolled), treeview);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                         GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (hbox), scrolled, TRUE, TRUE, 5);
    n = midori_web_list_get_length (search_entry->search_engines);
    for (i = 0; i < n; i++)
    {
        web_item = midori_web_list_get_nth_item (search_entry->search_engines, i);
        gtk_list_store_insert_with_values (GTK_LIST_STORE (liststore),
                                           NULL, i, 0, web_item, -1);
    }
    g_object_unref (liststore);
    g_signal_connect (treeview, "destroy",
        G_CALLBACK (midori_search_entry_treeview_destroy_cb), search_entry);
    vbox = gtk_vbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 4);
    button = gtk_button_new_from_stock (GTK_STOCK_ADD);
    g_signal_connect (button, "clicked",
        G_CALLBACK (midori_search_entry_dialog_add_cb), treeview);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    button = gtk_button_new_from_stock (GTK_STOCK_EDIT);
    g_signal_connect (button, "clicked",
        G_CALLBACK (midori_search_entry_dialog_edit_cb), treeview);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    g_object_set_data (G_OBJECT (treeview), "edit-button", button);
    if (!n)
        gtk_widget_set_sensitive (button, FALSE);
    button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
    g_signal_connect (button, "clicked",
        G_CALLBACK (midori_search_entry_dialog_remove_cb), treeview);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    if (!n)
        gtk_widget_set_sensitive (button, FALSE);
    g_object_set_data (G_OBJECT (treeview), "remove-button", button);
    button = gtk_label_new (""); /* This is an invisible separator */
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 12);
    button = gtk_button_new_from_stock (GTK_STOCK_GO_DOWN);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    button = gtk_button_new_from_stock (GTK_STOCK_GO_UP);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);

    g_object_connect (search_entry->search_engines,
        "signal-after::add-item",
        midori_search_entry_dialog_engines_add_item_cb, treeview,
        "signal-after::remove-item",
        midori_search_entry_dialog_engines_remove_item_cb, treeview,
        NULL);

    return dialog;
}
