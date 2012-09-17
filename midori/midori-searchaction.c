/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-searchaction.h"

#include "marshal.h"
#include "midori-platform.h"
#include "midori-core.h"

#include "config.h"
#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

struct _MidoriSearchAction
{
    GtkAction parent_instance;

    KatzeArray* search_engines;
    KatzeItem* current_item;
    KatzeItem* default_item;
    gchar* text;

    GtkWidget* last_proxy;

    GtkWidget* dialog;
    GtkWidget* treeview;
    GtkWidget* edit_button;
    GtkWidget* remove_button;
    GtkWidget* default_button;
};

struct _MidoriSearchActionClass
{
    GtkActionClass parent_class;
};

G_DEFINE_TYPE (MidoriSearchAction, midori_search_action, GTK_TYPE_ACTION)

enum
{
    PROP_0,

    PROP_SEARCH_ENGINES,
    PROP_CURRENT_ITEM,
    PROP_DEFAULT_ITEM,
    PROP_TEXT,
    PROP_DIALOG
};

enum
{
    SUBMIT,
    FOCUS_OUT,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
midori_search_action_finalize (GObject* object);

static void
midori_search_action_set_property (GObject*      object,
                                   guint         prop_id,
                                   const GValue* value,
                                   GParamSpec*   pspec);

static void
midori_search_action_get_property (GObject*    object,
                                   guint       prop_id,
                                   GValue*     value,
                                   GParamSpec* pspec);

static void
midori_search_action_activate (GtkAction* object);

static GtkWidget*
midori_search_action_create_tool_item (GtkAction* action);

static void
midori_search_action_connect_proxy (GtkAction* action,
                                    GtkWidget* proxy);

static void
midori_search_action_disconnect_proxy (GtkAction* action,
                                       GtkWidget* proxy);

static void
midori_search_action_class_init (MidoriSearchActionClass* class)
{
    GObjectClass* gobject_class;
    GtkActionClass* action_class;

    signals[SUBMIT] = g_signal_new ("submit",
                                    G_TYPE_FROM_CLASS (class),
                                    (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                    0,
                                    0,
                                    NULL,
                                    midori_cclosure_marshal_VOID__STRING_BOOLEAN,
                                    G_TYPE_NONE, 2,
                                    G_TYPE_STRING,
                                    G_TYPE_BOOLEAN);

    signals[FOCUS_OUT] = g_signal_new ("focus-out",
                                       G_TYPE_FROM_CLASS (class),
                                       (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                       0,
                                       0,
                                       NULL,
                                       g_cclosure_marshal_VOID__VOID,
                                       G_TYPE_NONE, 0);

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_search_action_finalize;
    gobject_class->set_property = midori_search_action_set_property;
    gobject_class->get_property = midori_search_action_get_property;

    action_class = GTK_ACTION_CLASS (class);
    action_class->activate = midori_search_action_activate;
    action_class->create_tool_item = midori_search_action_create_tool_item;
    action_class->connect_proxy = midori_search_action_connect_proxy;
    action_class->disconnect_proxy = midori_search_action_disconnect_proxy;

    g_object_class_install_property (gobject_class,
                                     PROP_SEARCH_ENGINES,
                                     g_param_spec_object (
                                     "search-engines",
                                     "Search Engines",
                                     "The list of search engines",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_CURRENT_ITEM,
                                     g_param_spec_object (
                                     "current-item",
                                     "Current Item",
                                     "The currently selected item",
                                     KATZE_TYPE_ITEM,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
     * MidoriSearchAction:default-item:
     *
     * The default search engine.
     *
     * Since: 0.1.6
     */
    g_object_class_install_property (gobject_class,
                                     PROP_DEFAULT_ITEM,
                                     g_param_spec_object (
                                     "default-item",
                                     "Default Item",
                                     "The default search engine",
                                     KATZE_TYPE_ITEM,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_TEXT,
                                     g_param_spec_string (
                                     "text",
                                     "Text",
                                     "The current text typed in the entry",
                                     NULL,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_DIALOG,
                                     g_param_spec_object (
                                     "dialog",
                                     "Dialog",
                                     "A dialog to manage search engines",
                                     GTK_TYPE_DIALOG,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
midori_search_action_init (MidoriSearchAction* search_action)
{
    search_action->search_engines = NULL;
    search_action->current_item = NULL;
    search_action->default_item = NULL;
    search_action->text = NULL;

    search_action->last_proxy = NULL;

    search_action->dialog = NULL;
    search_action->treeview = NULL;
    search_action->edit_button = NULL;
    search_action->remove_button = NULL;
    search_action->default_button = NULL;
}

static void
midori_search_action_finalize (GObject* object)
{
    MidoriSearchAction* search_action = MIDORI_SEARCH_ACTION (object);

    katze_assign (search_action->text, NULL);

    G_OBJECT_CLASS (midori_search_action_parent_class)->finalize (object);
}

static void
midori_search_action_set_property (GObject*      object,
                                   guint         prop_id,
                                   const GValue* value,
                                   GParamSpec*   pspec)
{
    MidoriSearchAction* search_action = MIDORI_SEARCH_ACTION (object);

    switch (prop_id)
    {
    case PROP_SEARCH_ENGINES:
        midori_search_action_set_search_engines (search_action,
                                                 g_value_get_object (value));
        break;
    case PROP_CURRENT_ITEM:
        midori_search_action_set_current_item (search_action,
                                               g_value_get_object (value));
        break;
    case PROP_DEFAULT_ITEM:
        midori_search_action_set_default_item (search_action,
                                               g_value_get_object (value));
        break;
    case PROP_TEXT:
        midori_search_action_set_text (search_action,
                                       g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_search_action_get_property (GObject*    object,
                                   guint       prop_id,
                                   GValue*     value,
                                   GParamSpec* pspec)
{
    MidoriSearchAction* search_action = MIDORI_SEARCH_ACTION (object);

    switch (prop_id)
    {
    case PROP_SEARCH_ENGINES:
        g_value_set_object (value, search_action->search_engines);
        break;
    case PROP_CURRENT_ITEM:
        g_value_set_object (value, search_action->current_item);
        break;
    case PROP_DEFAULT_ITEM:
        g_value_set_object (value, search_action->default_item);
        break;
    case PROP_TEXT:
        g_value_set_string (value, search_action->text);
        break;
    case PROP_DIALOG:
        g_value_set_object (value,
            midori_search_action_get_dialog (search_action));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_search_action_activate (GtkAction* action)
{
    GSList* proxies;
    GtkWidget* alignment;
    GtkWidget* entry;

    proxies = gtk_action_get_proxies (action);
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxies->data));
        entry = gtk_bin_get_child (GTK_BIN (alignment));

        /* Obviously only one widget can end up with the focus.
        Yet we can't predict which one that is, can we? */
        gtk_widget_grab_focus (entry);
        MIDORI_SEARCH_ACTION (action)->last_proxy = proxies->data;
    }
    while ((proxies = g_slist_next (proxies)));

    if (GTK_ACTION_CLASS (midori_search_action_parent_class)->activate)
        GTK_ACTION_CLASS (midori_search_action_parent_class)->activate (action);
}

static GtkWidget*
midori_search_action_create_tool_item (GtkAction* action)
{
    GtkWidget* toolitem;
    GtkWidget* entry;
    GtkWidget* alignment;

    toolitem = GTK_WIDGET (gtk_tool_item_new ());
    entry = sokoke_search_entry_new (NULL);
    gtk_icon_entry_set_icon_highlight (GTK_ICON_ENTRY (entry),
                                       GTK_ICON_ENTRY_PRIMARY, TRUE);
    alignment = gtk_alignment_new (0, 0.5, 1, 0.1);
    gtk_container_add (GTK_CONTAINER (alignment), entry);
    gtk_widget_show (entry);
    gtk_container_add (GTK_CONTAINER (toolitem), alignment);
    gtk_widget_show (alignment);

    MIDORI_SEARCH_ACTION (action)->last_proxy = GTK_WIDGET (toolitem);
    return toolitem;
}

static void
_midori_search_action_move_index (MidoriSearchAction* search_action,
                                  guint               n)
{
    gint i;
    KatzeItem* item;

    i = katze_array_get_item_index (search_action->search_engines,
                                    search_action->current_item);
    item = katze_array_get_nth_item (search_action->search_engines, i + n);
    if (item)
        midori_search_action_set_current_item (search_action, item);
}

static gboolean
midori_search_action_key_press_event_cb (GtkWidget*          entry,
                                         GdkEventKey*        event,
                                         MidoriSearchAction* search_action)
{
    const gchar* text;

    switch (event->keyval)
    {
    case GDK_KEY_ISO_Enter:
    case GDK_KEY_KP_Enter:
    case GDK_KEY_Return:
        text = gtk_entry_get_text (GTK_ENTRY (entry));
        g_signal_emit (search_action, signals[SUBMIT], 0, text,
                       MIDORI_MOD_NEW_TAB (event->state));
        search_action->last_proxy = entry;
        return TRUE;
    case GDK_KEY_Up:
        if (MIDORI_MOD_SCROLL (event->state))
            _midori_search_action_move_index (search_action, - 1);
        return TRUE;
    case GDK_KEY_Down:
        if (MIDORI_MOD_SCROLL (event->state))
            _midori_search_action_move_index (search_action, + 1);
        return TRUE;
    }

    return FALSE;
}

static gboolean
midori_search_action_focus_out_event_cb (GtkWidget*   widget,
                                         GdkEventKey* event,
                                         GtkAction*   action)
{
    g_signal_emit (action, signals[FOCUS_OUT], 0);
    return FALSE;
}

static void
midori_search_action_engine_activate_cb (GtkWidget*          menuitem,
                                         MidoriSearchAction* search_action)
{
    KatzeItem* item;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "engine");
    midori_search_action_set_current_item (search_action, item);
}

static void
midori_search_action_manage_activate_cb (GtkWidget*          menuitem,
                                         MidoriSearchAction* search_action)
{
    GtkWidget* dialog;

    dialog = midori_search_action_get_dialog (search_action);
    if (gtk_widget_get_visible (dialog))
        gtk_window_present (GTK_WINDOW (dialog));
    else
        gtk_widget_show (dialog);
}

GdkPixbuf*
midori_search_action_get_icon (KatzeItem*    item,
                               GtkWidget*    widget,
                               const gchar** icon_name,
                               gboolean      in_entry)
{
    const gchar* icon = katze_item_get_uri (item);
    GdkScreen* screen;
    GtkIconTheme* icon_theme;

    if (midori_uri_is_resource (icon))
        return katze_load_cached_icon (icon, widget);

    if (icon_name == NULL)
        return NULL;

    screen = gtk_widget_get_screen (widget);
    icon_theme = gtk_icon_theme_get_for_screen (screen);
    if ((icon = katze_item_get_icon (item)) && *icon)
    {
        if (gtk_icon_theme_has_icon (icon_theme, icon))
        {
            *icon_name = icon;
            return NULL;
        }
    }

    if (in_entry && gtk_icon_theme_has_icon (icon_theme, "edit-find-option-symbolic"))
        *icon_name = "edit-find-option-symbolic";
    else if (gtk_icon_theme_has_icon (icon_theme, "edit-find-option"))
        *icon_name = "edit-find-option";
    else
        *icon_name = STOCK_EDIT_FIND;
    return NULL;
}

static void
midori_search_action_icon_released_cb (GtkWidget*           entry,
                                       GtkIconEntryPosition icon_pos,
                                       gint                 button,
                                       GtkAction*           action)
{
    KatzeArray* search_engines;
    GtkWidget* menu;
    GtkWidget* menuitem;
    KatzeItem* item;
    GdkPixbuf* icon;
    GtkWidget* image;

    if (icon_pos == GTK_ICON_ENTRY_SECONDARY)
        return;

    search_engines = MIDORI_SEARCH_ACTION (action)->search_engines;
    menu = gtk_menu_new ();
    if (!katze_array_is_empty (search_engines))
    {
        KATZE_ARRAY_FOREACH_ITEM (item, search_engines)
        {
            const gchar* icon_name;

            menuitem = gtk_image_menu_item_new_with_label (
                katze_item_get_name (item));
            image = gtk_image_new ();
            icon = midori_search_action_get_icon (item, entry, &icon_name, FALSE);
            if (icon)
            {
                gtk_image_set_from_pixbuf (GTK_IMAGE (image), icon);
                g_object_unref (icon);
            }
            else
                gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name,
                                              GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
            gtk_image_menu_item_set_always_show_image (
                GTK_IMAGE_MENU_ITEM (menuitem), TRUE);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
            g_object_set_data (G_OBJECT (menuitem), "engine", item);
            g_signal_connect (menuitem, "activate",
                G_CALLBACK (midori_search_action_engine_activate_cb), action);
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
    image = gtk_image_new_from_stock (GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_search_action_manage_activate_cb), action);
    gtk_widget_show (menuitem);
    katze_widget_popup (entry, GTK_MENU (menu), NULL, KATZE_MENU_POSITION_LEFT);
}

static gboolean
midori_search_action_scroll_event_cb (GtkWidget*          entry,
                                      GdkEventScroll*     event,
                                      MidoriSearchAction* search_action)
{
    if (event->direction == GDK_SCROLL_DOWN)
        _midori_search_action_move_index (search_action, + 1);
    else if (event->direction == GDK_SCROLL_UP)
        _midori_search_action_move_index (search_action, - 1);
    return FALSE;
}

static void
midori_search_action_set_entry_icon (MidoriSearchAction* search_action,
                                     GtkWidget*          entry)
{
    GdkPixbuf* icon;

    if (search_action->current_item)
    {
        const gchar* icon_name;

        icon = midori_search_action_get_icon (search_action->current_item,
                                              entry, &icon_name, TRUE);
        if (icon)
        {
            gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
                                                 GTK_ICON_ENTRY_PRIMARY, icon);
            g_object_unref (icon);
        }
        else
            gtk_icon_entry_set_icon_from_icon_name (GTK_ICON_ENTRY (entry),
                GTK_ICON_ENTRY_PRIMARY, icon_name);
        gtk_entry_set_placeholder_text (GTK_ENTRY (entry),
            katze_item_get_name (search_action->current_item));
    }
    else
    {
        gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (entry),
                                            GTK_ICON_ENTRY_PRIMARY,
                                            GTK_STOCK_FIND);
        gtk_entry_set_placeholder_text (GTK_ENTRY (entry), "");
    }
}

static void
midori_search_action_connect_proxy (GtkAction* action,
                                      GtkWidget* proxy)
{
    GtkWidget* alignment;
    GtkWidget* entry;

    GTK_ACTION_CLASS (midori_search_action_parent_class)->connect_proxy (
        action, proxy);

    if (GTK_IS_TOOL_ITEM (proxy))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxy));
        entry = gtk_bin_get_child (GTK_BIN (alignment));

        midori_search_action_set_entry_icon (MIDORI_SEARCH_ACTION (action),
                                             entry);
        g_object_connect (entry,
                          "signal::key-press-event",
                          midori_search_action_key_press_event_cb, action,
                          "signal::focus-out-event",
                          midori_search_action_focus_out_event_cb, action,
                          "signal::icon-release",
                          midori_search_action_icon_released_cb, action,
                          "signal::scroll-event",
                          midori_search_action_scroll_event_cb, action,
                          NULL);
    }

    MIDORI_SEARCH_ACTION (action)->last_proxy = proxy;
}

static void
midori_search_action_disconnect_proxy (GtkAction* action,
                                       GtkWidget* proxy)
{
    GSList* proxies;

    /* FIXME: This is wrong */
    g_signal_handlers_disconnect_by_func (proxy,
        G_CALLBACK (gtk_action_activate), action);

    GTK_ACTION_CLASS (midori_search_action_parent_class)->disconnect_proxy
        (action, proxy);

    if (MIDORI_SEARCH_ACTION (action)->last_proxy == proxy)
    {
        proxies = gtk_action_get_proxies (action);
        if (proxies)
            MIDORI_SEARCH_ACTION (action)->last_proxy = proxies->data;
    }
}

const gchar*
midori_search_action_get_text (MidoriSearchAction* search_action)
{
    g_return_val_if_fail (MIDORI_IS_SEARCH_ACTION (search_action), NULL);

    return search_action->text;
}

void
midori_search_action_set_text (MidoriSearchAction* search_action,
                               const gchar*        text)
{
    GSList* proxies;
    GtkWidget* alignment;
    GtkWidget* entry;

    g_return_if_fail (MIDORI_IS_SEARCH_ACTION (search_action));

    katze_assign (search_action->text, g_strdup (text));
    g_object_notify (G_OBJECT (search_action), "text");

    proxies = gtk_action_get_proxies (GTK_ACTION (search_action));
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxies->data));
        entry = gtk_bin_get_child (GTK_BIN (alignment));

        gtk_entry_set_text (GTK_ENTRY (entry), text ? text : "");
        search_action->last_proxy = proxies->data;
    }
    while ((proxies = g_slist_next (proxies)));
}

KatzeArray*
midori_search_action_get_search_engines (MidoriSearchAction* search_action)
{
    g_return_val_if_fail (MIDORI_IS_SEARCH_ACTION (search_action), NULL);

    return search_action->search_engines;
}

static void
midori_search_action_engines_add_item_cb (KatzeArray*         list,
                                          KatzeItem*          item,
                                          MidoriSearchAction* search_action)
{
    if (!search_action->current_item)
        midori_search_action_set_current_item (search_action, item);
}

static void
midori_search_action_engines_remove_item_cb (KatzeArray*         list,
                                             KatzeItem*          item,
                                             MidoriSearchAction* search_action)
{
    KatzeItem* found_item;

    if (search_action->current_item == item)
    {
        found_item = katze_array_get_nth_item (list, 0);
        midori_search_action_set_current_item (search_action, found_item);
    }
}

void
midori_search_action_set_search_engines (MidoriSearchAction* search_action,
                                         KatzeArray*         search_engines)
{
    GSList* proxies;

    g_return_if_fail (MIDORI_IS_SEARCH_ACTION (search_action));
    g_return_if_fail (!search_engines ||
        katze_array_is_a (search_engines, KATZE_TYPE_ITEM));

    /* FIXME: Disconnect old search engines */
    /* FIXME: Disconnect and reconnect dialog signals */

    if (search_engines)
        g_object_ref (search_engines);
    katze_object_assign (search_action->search_engines, search_engines);
    if (!search_engines)
        return;

    g_object_connect (search_engines,
        "signal-after::add-item",
        midori_search_action_engines_add_item_cb, search_action,
        "signal-after::remove-item",
        midori_search_action_engines_remove_item_cb, search_action,
        NULL);

    g_object_notify (G_OBJECT (search_action), "search-engines");

    proxies = gtk_action_get_proxies (GTK_ACTION (search_action));
    if (!proxies)
        return;
}

KatzeItem*
midori_search_action_get_current_item (MidoriSearchAction* search_action)
{
    g_return_val_if_fail (MIDORI_IS_SEARCH_ACTION (search_action), NULL);

    return search_action->current_item;
}

void
midori_search_action_set_current_item (MidoriSearchAction* search_action,
                                       KatzeItem*          item)
{
    GSList* proxies;
    GtkWidget* alignment;
    GtkWidget* entry;

    g_return_if_fail (MIDORI_IS_SEARCH_ACTION (search_action));
    g_return_if_fail (!item || KATZE_IS_ITEM (item));

    if (item)
        g_object_ref (item);
    katze_object_assign (search_action->current_item, item);

    g_object_notify (G_OBJECT (search_action), "current-item");

    proxies = gtk_action_get_proxies (GTK_ACTION (search_action));
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxies->data));
        entry = gtk_bin_get_child (GTK_BIN (alignment));

        midori_search_action_set_entry_icon (search_action, entry);
    }
    while ((proxies = g_slist_next (proxies)));
}

/**
 * midori_search_action_get_default_item:
 * @search_action: a #MidoriSearchAction
 *
 * Retrieves the default search engine.
 *
 * Since 0.1.6
 *
 * Return value: a #KatzeItem
 **/
KatzeItem*
midori_search_action_get_default_item (MidoriSearchAction* search_action)
{
    g_return_val_if_fail (MIDORI_IS_SEARCH_ACTION (search_action), NULL);

    return search_action->default_item;
}

/**
 * midori_search_action_set_default_item:
 * @search_action: a #MidoriSearchAction
 * @item: a #KatzeItem
 *
 * Sets the default search engine.
 *
 * Since 0.1.6
 **/
void
midori_search_action_set_default_item (MidoriSearchAction* search_action,
                                       KatzeItem*          item)
{
    g_return_if_fail (MIDORI_IS_SEARCH_ACTION (search_action));
    g_return_if_fail (!item || KATZE_IS_ITEM (item));

    if (item)
        g_object_ref (item);
    katze_object_assign (search_action->default_item, item);
    g_object_notify (G_OBJECT (search_action), "default-item");
}

static void
midori_search_action_dialog_render_tick_cb (GtkTreeViewColumn* column,
                                            GtkCellRenderer*   renderer,
                                            GtkTreeModel*      model,
                                            GtkTreeIter*       iter,
                                            GtkWidget*         treeview)
{
    KatzeItem* item;
    MidoriSearchAction* search_action;
    gint width;

    gtk_tree_model_get (model, iter, 0, &item, -1);

    search_action = g_object_get_data (G_OBJECT (treeview), "search-action");
    gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (treeview),
                                       GTK_ICON_SIZE_MENU, &width, NULL);
    g_object_set (renderer, "stock-id",
        search_action->default_item == item ? GTK_STOCK_YES : NULL,
        "width", width + 4, NULL);
    g_object_unref (item);
}

static void
midori_search_action_dialog_render_icon_cb (GtkTreeViewColumn* column,
                                            GtkCellRenderer*   renderer,
                                            GtkTreeModel*      model,
                                            GtkTreeIter*       iter,
                                            GtkWidget*         treeview)
{
    KatzeItem* item;
    GdkPixbuf* icon;
    const gchar* icon_name;

    gtk_tree_model_get (model, iter, 0, &item, -1);

    if ((icon = midori_search_action_get_icon (item, treeview, &icon_name, FALSE)))
    {
        g_object_set (renderer, "pixbuf", icon, "yalign", 0.25, NULL);
        g_object_unref (icon);
    }
    else
        g_object_set (renderer, "icon-name", icon_name, "yalign", 0.25, NULL);
    g_object_unref (item);
}

static void
midori_search_action_dialog_render_text (GtkTreeViewColumn* column,
                                         GtkCellRenderer*   renderer,
                                         GtkTreeModel*      model,
                                         GtkTreeIter*       iter,
                                         GtkWidget*         treeview)
{
    KatzeItem* item;
    const gchar* name;
    const gchar* text;
    gchar* markup;

    gtk_tree_model_get (model, iter, 0, &item, -1);
    name = katze_item_get_name (item);
    text = katze_item_get_text (item);
    markup = g_markup_printf_escaped ("<b>%s</b>\n%s", name, text ? text : "");
    g_object_set (renderer, "markup", markup, NULL);
    g_free (markup);
    g_object_unref (item);
}

static void
midori_search_action_dialog_render_token (GtkTreeViewColumn* column,
                                          GtkCellRenderer*   renderer,
                                          GtkTreeModel*      model,
                                          GtkTreeIter*       iter,
                                          GtkWidget*         treeview)
{
    KatzeItem* item;
    const gchar* token;
    gchar* markup;

    gtk_tree_model_get (model, iter, 0, &item, -1);
    token = katze_item_get_token (item);
    markup = g_markup_printf_escaped ("<b>%s</b>", token ? token : "");
    g_object_set (renderer, "markup", markup, "yalign", 0.0, NULL);
    g_free (markup);
    g_object_unref (item);
}

static void
midori_search_action_editor_name_changed_cb (GtkWidget* entry,
                                             GtkWidget* dialog)
{
    const gchar* text = gtk_entry_get_text (GTK_ENTRY (entry));
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
        GTK_RESPONSE_ACCEPT, text && *text);
}

gchar*
midori_search_action_token_for_uri (const gchar* uri)
{
    guint len, i;
    gchar** parts;
    gchar* hostname = NULL, *path = NULL;

    path = midori_uri_parse_hostname (uri, NULL);
    parts = g_strsplit (path, ".", -1);
    g_free (path);

    len = g_strv_length (parts);
    if (len > 2)
    {
        for (i = len; i == 0; i--)
        {
            if (parts[i] && *parts[i])
                if (strlen (parts[i]) > 3)
                {
                    hostname = g_strdup (parts[i]);
                    break;
                }
        }
    }
    else
        hostname = g_strdup (parts[0]);

    if (!hostname)
        hostname = g_strdup (parts[1]);

    g_strfreev (parts);

    if (strlen (hostname) > 4)
    {
        GString* str = g_string_new (NULL);
        int j, count = 0;

        for (j = 0; count < 4; j++)
        {
            if (hostname[j] == 'a'
            ||  hostname[j] == 'e'
            ||  hostname[j] == 'i'
            ||  hostname[j] == 'o'
            ||  hostname[j] == 'u')
                continue;
            else
            {
                g_string_append_c (str, hostname[j]);
                count++;
            }
        }
        return g_string_free (str, FALSE);
    }
    return g_strdup (hostname);
}

KatzeItem*
midori_search_action_get_engine_for_form (WebKitWebView*     web_view,
                                          PangoEllipsizeMode ellipsize)
{
    #if WEBKIT_CHECK_VERSION (1, 5, 0)
    WebKitDOMDocument* doc;
    WebKitDOMHTMLFormElement* active_form;
    WebKitDOMHTMLCollection* form_nodes;
    WebKitDOMElement* active_element;
    gchar* token_element;
    const gchar* title;
    GString* uri_str;
    gulong form_len;
    const gchar* action;
    guint i;
    KatzeItem* item;
    gchar** parts;

    #if WEBKIT_CHECK_VERSION (1, 9, 5)
    doc = webkit_web_frame_get_dom_document (web_view);
    #else
    if (webkit_web_view_get_focused_frame (web_view) != webkit_web_view_get_main_frame (web_view))
        return NULL;
    doc = webkit_web_view_get_dom_document (web_view);
    #endif

    active_element = webkit_dom_html_document_get_active_element ((WebKitDOMHTMLDocument*)doc);
    active_form = webkit_dom_html_input_element_get_form ((WebKitDOMHTMLInputElement*)active_element);

    if (!active_form)
        return NULL;

    token_element = webkit_dom_element_get_attribute (active_element, "name");

    form_nodes = webkit_dom_html_form_element_get_elements (active_form);
    form_len = webkit_dom_html_form_element_get_length (active_form);

    /* action NULL or "": relative path */
    if ((action = webkit_dom_html_form_element_get_action (active_form)) && *action)
        uri_str = g_string_new (action);
    else
    {
        gchar* hostname = midori_uri_parse_hostname (webkit_web_view_get_uri (web_view), NULL);
        uri_str = g_string_new ("http://");
        g_string_append (uri_str, hostname);
        g_free (hostname);
    }
    g_string_append_c (uri_str, '?');

    for (i = 0; i < form_len; i++)
    {
        WebKitDOMNode* form_node = webkit_dom_html_collection_item (form_nodes, i);
        WebKitDOMElement* form_element = (WebKitDOMElement*) form_node;
        gchar* name = webkit_dom_element_get_attribute (form_element, "name");

        if (name && *name)
        {
            if (!g_strcmp0 (token_element, name))
                g_string_append_printf (uri_str, "%s=%s&", name, "\%s");
            else
            {
                gchar* value;
                if (!g_strcmp0 (webkit_dom_element_get_tag_name (form_element), "SELECT"))
                {
                    WebKitDOMHTMLSelectElement* select_element = (WebKitDOMHTMLSelectElement*) form_element;
                    gulong pos = webkit_dom_html_select_element_get_selected_index (select_element);
                    WebKitDOMNode* selected_node = webkit_dom_html_select_element_item (select_element, pos);
                    WebKitDOMElement* selected_element = (WebKitDOMElement*) selected_node;

                    value = webkit_dom_element_get_attribute (selected_element, "value");
                }
                else
                    value = webkit_dom_element_get_attribute (form_element, "value");

                g_string_append_printf (uri_str, "%s=%s&", name, value);
                g_free (value);
            }
            g_free (name);
        }
    }

    title = webkit_web_view_get_title (web_view);

    item = katze_item_new ();
    item->uri = g_string_free (uri_str, FALSE);
    item->token = midori_search_action_token_for_uri (webkit_web_view_get_uri (web_view));

    if (strstr (title, " - "))
        parts = g_strsplit (title, " - ", 2);
    else if (strstr (title, ": "))
        parts = g_strsplit (title, ": ", 2);
    else
        parts = NULL;
    if (parts != NULL)
    {
        /* See midori_view_set_title: title can be first or last */
        if (ellipsize == PANGO_ELLIPSIZE_END)
        {
            item->name = g_strdup (parts[0]);
            item->text = g_strdup (parts[1]);
        }
        else
        {
            item->name = g_strdup (parts[1]);
            item->text = g_strdup (parts[2]);
        }
        g_strfreev (parts);
    }
    else
        item->name = g_strdup (title);

    g_free (token_element);
    return item;
    #else
    return NULL;
    #endif
}

void
midori_search_action_get_editor (MidoriSearchAction* search_action,
                                 KatzeItem*          item,
                                 gboolean            new_engine)
{
    GtkWidget* toplevel;
    GtkWidget* dialog;
    GtkWidget* content_area;
    GtkSizeGroup* sizegroup;
    GtkWidget* hbox;
    GtkWidget* label;
    GtkTreeModel* liststore;
    GtkTreeIter iter;
    GtkTreeSelection* selection;
    GtkWidget* entry_name;
    GtkWidget* entry_description;
    GtkWidget* entry_uri;
    GtkWidget* entry_token;

    toplevel = gtk_widget_get_toplevel (search_action->treeview);
    dialog = gtk_dialog_new_with_buttons (
        new_engine ? _("Add search engine") : _("Edit search engine"),
        toplevel ? GTK_WINDOW (toplevel) : NULL,
        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        new_engine ? GTK_STOCK_ADD : GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_window_set_icon_name (GTK_WINDOW (dialog),
        new_engine ? GTK_STOCK_ADD : GTK_STOCK_REMOVE);
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);
    sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    if (new_engine)
    {
        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_ACCEPT, FALSE);
    }
    else
    {
        selection = gtk_tree_view_get_selection (
            GTK_TREE_VIEW (search_action->treeview));
        gtk_tree_selection_get_selected (selection, &liststore, &iter);
        gtk_tree_model_get (liststore, &iter, 0, &item, -1);
    }

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Name:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    entry_name = gtk_entry_new ();
    g_signal_connect (entry_name, "changed",
        G_CALLBACK (midori_search_action_editor_name_changed_cb), dialog);
    gtk_entry_set_activates_default (GTK_ENTRY (entry_name), TRUE);
    gtk_entry_set_text (GTK_ENTRY (entry_name),
        katze_str_non_null (katze_item_get_name (item)));
    gtk_box_pack_start (GTK_BOX (hbox), entry_name, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (content_area), hbox);
    gtk_widget_show_all (hbox);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Description:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    entry_description = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry_description), TRUE);
    gtk_entry_set_text (GTK_ENTRY (entry_description)
         , katze_str_non_null (katze_item_get_text (item)));
    gtk_box_pack_start (GTK_BOX (hbox), entry_description, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (content_area), hbox);
    gtk_widget_show_all (hbox);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Address:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    entry_uri = katze_uri_entry_new (
    #if GTK_CHECK_VERSION (2, 20, 0)
        gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT));
    #else
        NULL);
    #endif
    g_object_set_data (G_OBJECT (entry_uri), "allow_%s", (void*)1);
    gtk_entry_set_activates_default (GTK_ENTRY (entry_uri), TRUE);
    gtk_entry_set_text (GTK_ENTRY (entry_uri),
        katze_str_non_null (katze_item_get_uri (item)));
    gtk_box_pack_start (GTK_BOX (hbox), entry_uri, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (content_area), hbox);
    gtk_widget_show_all (hbox);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Token:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    entry_token = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry_token), TRUE);
    gtk_entry_set_text (GTK_ENTRY (entry_token)
         , katze_str_non_null (katze_item_get_token (item)));
    gtk_box_pack_start (GTK_BOX (hbox), entry_token, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (content_area), hbox);
    gtk_widget_show_all (hbox);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        g_object_set (item,
            "name", gtk_entry_get_text (GTK_ENTRY (entry_name)),
            "text", gtk_entry_get_text (GTK_ENTRY (entry_description)),
            "uri", gtk_entry_get_text (GTK_ENTRY (entry_uri)),
            "token", gtk_entry_get_text (GTK_ENTRY (entry_token)),
            NULL);

        if (new_engine)
            katze_array_add_item (search_action->search_engines, item);
        /* If it isn't a new search engine but the old default one,
           we need to update the default search engine after editing it. */
        else if (item == midori_search_action_get_default_item (search_action))
            midori_search_action_set_default_item (search_action, item);
        g_object_unref (item);
    }
    gtk_widget_destroy (dialog);
}

static void
midori_search_action_activate_edit_cb (GtkTreeView *treeview,
                                       GtkTreePath *path,
                                       GtkTreeViewColumn *column,
                                       MidoriSearchAction* search_action)
{
    GtkTreeSelection* selection;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    if (gtk_tree_selection_get_selected (selection, NULL, NULL))
        midori_search_action_get_editor (search_action, NULL, FALSE);
}


static void
midori_search_action_dialog_add_cb (GtkWidget*          widget,
                                    MidoriSearchAction* search_action)
{
    midori_search_action_get_editor (search_action, katze_item_new (), TRUE);
}

static void
midori_search_action_dialog_edit_cb (GtkWidget*  widget,
                                     MidoriSearchAction* search_action)
{
    GtkWidget* treeview;
    GtkTreeSelection* selection;

    treeview = search_action->treeview;
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    if (gtk_tree_selection_get_selected (selection, NULL, NULL))
        midori_search_action_get_editor (search_action, NULL, FALSE);
}

static void
midori_search_action_dialog_remove_cb (GtkWidget*          widget,
                                       MidoriSearchAction* search_action)
{
    KatzeArray* search_engines;
    GtkWidget* treeview;
    GtkTreeSelection* selection;
    GtkTreeModel* liststore;
    GtkTreeIter iter;
    KatzeItem* item;

    search_engines = search_action->search_engines;
    treeview = search_action->treeview;
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    if (gtk_tree_selection_get_selected (selection, &liststore, &iter))
    {
        gtk_tree_model_get (liststore, &iter, 0, &item, -1);
        katze_array_remove_item (search_engines, item);
        g_object_unref (item);
        /* FIXME: we want to allow undo of some kind */
    }
}

static void
midori_search_action_dialog_move_up_cb (GtkWidget*          widget,
                                        MidoriSearchAction* search_action)
{
    KatzeArray* search_engines;
    GtkWidget* treeview;
    GtkTreeSelection* selection;
    GtkTreeModel* liststore;
    GtkTreeIter iter, prev;
    GtkTreePath* path;
    KatzeItem* item;
    gint i;

    search_engines = search_action->search_engines;
    treeview = search_action->treeview;
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    if (gtk_tree_selection_get_selected (selection, &liststore, &iter))
    {
        path = gtk_tree_model_get_path (liststore, &iter);
        if(gtk_tree_path_prev(path))
        {
            gtk_tree_model_get (liststore, &iter, 0, &item, -1);
            gtk_tree_model_get_iter (liststore, &prev, path);
            gtk_list_store_swap (GTK_LIST_STORE(liststore), &iter, &prev);

            i = katze_array_get_item_index (search_engines, item);
            katze_array_move_item (search_engines, item, i - 1);
            g_object_unref (item);
            /* If the index of the current item has changed it needs to be reset */
            g_object_notify (G_OBJECT (search_action), "current-item");
        }
        gtk_tree_path_free (path);
    }
}

static void
midori_search_action_dialog_move_down_cb (GtkWidget*          widget,
                                          MidoriSearchAction* search_action)
{
    KatzeArray* search_engines;
    GtkWidget* treeview;
    GtkTreeSelection* selection;
    GtkTreeModel* liststore;
    GtkTreeIter iter, next;
    KatzeItem* item;
    gint i;

    search_engines = search_action->search_engines;
    treeview = search_action->treeview;
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    if (gtk_tree_selection_get_selected (selection, &liststore, &iter))
    {
        next = iter;
        if (gtk_tree_model_iter_next (liststore, &next))
        {
            gtk_tree_model_get (liststore, &iter, 0, &item, -1);
            gtk_list_store_swap (GTK_LIST_STORE(liststore), &iter, &next);

            i = katze_array_get_item_index (search_engines, item);
            katze_array_move_item (search_engines, item, i + 1);
            g_object_unref (item);
            /* If the index of the current item has changed it needs to be reset */
            g_object_notify (G_OBJECT (search_action), "current-item");
        }
    }
}

static void
midori_search_action_dialog_default_cb (GtkWidget*          widget,
                                        MidoriSearchAction* search_action)
{
    GtkWidget* treeview;
    GtkTreeSelection* selection;
    GtkTreeModel* liststore;
    GtkTreeIter iter;
    KatzeItem* item;

    treeview = search_action->treeview;
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    if (gtk_tree_selection_get_selected (selection, &liststore, &iter))
    {
        gtk_tree_model_get (liststore, &iter, 0, &item, -1);
        midori_search_action_set_default_item (search_action, item);
        g_object_unref (item);
        gtk_widget_queue_draw (treeview);
    }
}

static void
midori_search_action_treeview_selection_cb (GtkTreeSelection*   selection,
                                            MidoriSearchAction* search_action)
{
    gboolean selected;

    selected = gtk_tree_selection_get_selected (selection, NULL, NULL);

    gtk_widget_set_sensitive (search_action->edit_button, selected);
    gtk_widget_set_sensitive (search_action->remove_button, selected);
    gtk_widget_set_sensitive (search_action->default_button, selected);
}

static void
midori_search_action_dialog_engines_add_item_cb (KatzeArray* list,
                                                 KatzeItem*  item,
                                                 GtkAction*  action)
{
    MidoriSearchAction* search_action;
    GtkTreeModel* liststore;
    GtkTreeIter iter;

    search_action = MIDORI_SEARCH_ACTION (action);
    liststore = gtk_tree_view_get_model (GTK_TREE_VIEW (search_action->treeview));
    gtk_list_store_append (GTK_LIST_STORE (liststore), &iter);
    gtk_list_store_set (GTK_LIST_STORE (liststore), &iter, 0, item, -1);
}

static void
midori_search_action_dialog_engines_remove_item_cb (KatzeArray* list,
                                                    KatzeItem*  item,
                                                    GtkAction*  action)
{
    MidoriSearchAction* search_action;
    GtkTreeModel* liststore;
    GtkTreeIter iter;
    gboolean valid;
    KatzeItem* found_item;

    search_action = MIDORI_SEARCH_ACTION (action);
    liststore = gtk_tree_view_get_model (GTK_TREE_VIEW (search_action->treeview));
    valid = gtk_tree_model_get_iter_first (liststore, &iter);
    while (valid)
    {
        gtk_tree_model_get (liststore, &iter, 0, &found_item, -1);
        if (found_item == item)
        {
            gtk_list_store_remove (GTK_LIST_STORE (liststore), &iter);
            valid = FALSE;
        }
        else
            valid = gtk_tree_model_iter_next (liststore, &iter);
        g_object_unref (found_item);
    }
}

static void
midori_search_action_treeview_destroy_cb (GtkWidget*          treeview,
                                          MidoriSearchAction* search_action)
{
    if (search_action->search_engines)
    {
        g_signal_handlers_disconnect_by_func (
            search_action->search_engines,
            midori_search_action_dialog_engines_add_item_cb, search_action);
        g_signal_handlers_disconnect_by_func (
            search_action->search_engines,
            midori_search_action_dialog_engines_remove_item_cb, search_action);
    }
}

static void
midori_search_action_dialog_respnse_cb (GtkWidget* dialog,
                                        gint       response,
                                        gpointer   data)
{
    gtk_widget_destroy (dialog);
}

/**
 * midori_search_action_get_dialog:
 * @search_action: a #MidoriSearchAction
 *
 * Obtains a dialog that provides an interface for managing
 * the list of search engines.
 *
 * The dialog is created once and this function will return
 * the very same dialog until it is destroyed, in which case
 * a new dialog is created.
 *
 * Return value: a #GtkDialog
 **/
GtkWidget*
midori_search_action_get_dialog (MidoriSearchAction* search_action)
{
    const gchar* dialog_title;
    GtkWidget* toplevel;
    GtkWidget* dialog;
    GtkWidget* content_area;
    gint width, height;
    GtkWidget* xfce_heading;
    GtkWidget* hbox;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_pixbuf;
    GtkListStore* liststore;
    GtkWidget* treeview;
    GtkWidget* scrolled;
    guint i;
    KatzeItem* item;
    GtkWidget* vbox;
    GtkWidget* button;
    GtkWidget* image;

    g_return_val_if_fail (MIDORI_IS_SEARCH_ACTION (search_action), NULL);

    /* If there is a dialog, use that. We want only one. */
    if (search_action->dialog)
        return search_action->dialog;

    dialog_title = _("Manage Search Engines");
    toplevel = search_action->last_proxy ?
        gtk_widget_get_toplevel (search_action->last_proxy) : NULL;
    dialog = gtk_dialog_new_with_buttons (dialog_title,
        toplevel ? GTK_WINDOW (toplevel) : NULL,
        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
        #if !HAVE_OSX
        #if !HAVE_HILDON
        GTK_STOCK_HELP, GTK_RESPONSE_HELP,
        #endif
        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
        #endif
        NULL);
    katze_widget_add_class (gtk_dialog_get_widget_for_response (
        GTK_DIALOG (dialog), GTK_RESPONSE_HELP), "help_button");
    g_signal_connect (dialog, "destroy",
                      G_CALLBACK (gtk_widget_destroyed), &search_action->dialog);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_PROPERTIES);
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    /* TODO: Implement some kind of help function */
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                       GTK_RESPONSE_HELP, FALSE);
    sokoke_widget_get_text_size (dialog, "M", &width, &height);
    gtk_window_set_default_size (GTK_WINDOW (dialog), width * 52, -1);
    g_signal_connect (dialog, "response",
                      G_CALLBACK (midori_search_action_dialog_respnse_cb), NULL);
    /* TODO: Do we want tooltips for explainations or can we omit that?
             We need mnemonics */
    if ((xfce_heading = sokoke_xfce_header_new (
        gtk_window_get_icon_name (GTK_WINDOW (dialog)), dialog_title)))
        gtk_box_pack_start (GTK_BOX (content_area), xfce_heading, FALSE, FALSE, 0);
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (content_area), hbox, TRUE, TRUE, 12);
    liststore = gtk_list_store_new (1, KATZE_TYPE_ITEM);
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (liststore));
    search_action->treeview = treeview;
    g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
        "changed", G_CALLBACK (midori_search_action_treeview_selection_cb),
        search_action);
    g_signal_connect (treeview, "row-activated",
        G_CALLBACK (midori_search_action_activate_edit_cb), search_action);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    g_object_set_data (G_OBJECT (treeview), "search-action", search_action);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_search_action_dialog_render_tick_cb,
        treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_search_action_dialog_render_token,
        treeview, NULL);
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_search_action_dialog_render_icon_cb,
        treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, TRUE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_search_action_dialog_render_text,
        treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (scrolled), treeview);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                         GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (hbox), scrolled, TRUE, TRUE, 5);
    i = 0;
    if (search_action->search_engines)
    KATZE_ARRAY_FOREACH_ITEM (item, search_action->search_engines)
        gtk_list_store_insert_with_values (GTK_LIST_STORE (liststore),
                                           NULL, i++, 0, item, -1);
    g_object_unref (liststore);
    g_signal_connect (treeview, "destroy",
        G_CALLBACK (midori_search_action_treeview_destroy_cb), search_action);
    vbox = gtk_vbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 4);
    button = gtk_button_new_from_stock (GTK_STOCK_ADD);
    g_signal_connect (button, "clicked",
        G_CALLBACK (midori_search_action_dialog_add_cb), search_action);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    button = gtk_button_new_from_stock (GTK_STOCK_EDIT);
    search_action->edit_button = button;
    g_signal_connect (button, "clicked",
        G_CALLBACK (midori_search_action_dialog_edit_cb), search_action);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    if (!i)
        gtk_widget_set_sensitive (button, FALSE);
    button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
    search_action->remove_button = button;
    g_signal_connect (button, "clicked",
        G_CALLBACK (midori_search_action_dialog_remove_cb), search_action);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    if (!i)
        gtk_widget_set_sensitive (button, FALSE);
    button = gtk_label_new (""); /* This is an invisible separator */
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 8);
    button = gtk_button_new_with_mnemonic (_("Use as _default"));
    image = gtk_image_new_from_stock (GTK_STOCK_YES, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (button), image);
    search_action->default_button = button;
    g_signal_connect (button, "clicked",
        G_CALLBACK (midori_search_action_dialog_default_cb), search_action);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    if (!i)
        gtk_widget_set_sensitive (button, FALSE);
    button = gtk_label_new (""); /* This is an invisible separator */
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 12);
    button = gtk_button_new_from_stock (GTK_STOCK_GO_DOWN);
    g_signal_connect (button, "clicked",
        G_CALLBACK (midori_search_action_dialog_move_down_cb), search_action);
    gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    button = gtk_button_new_from_stock (GTK_STOCK_GO_UP);
    g_signal_connect (button, "clicked",
        G_CALLBACK (midori_search_action_dialog_move_up_cb), search_action);
    gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

    #if HAVE_OSX
    GtkWidget* icon;
    hbox = gtk_hbox_new (FALSE, 0);
    button = gtk_button_new ();
    icon = gtk_image_new_from_stock (GTK_STOCK_HELP, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (button), icon);
    /* TODO: Implement some kind of help function */
    gtk_widget_set_sensitive (button, FALSE);
    /* g_signal_connect (button, "clicked",
        G_CALLBACK (midori_search_action_dialog_help_clicked_cb), dialog); */
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 4);
    gtk_box_pack_end (GTK_BOX (content_area),  hbox, FALSE, FALSE, 0);
    #endif
    gtk_widget_show_all (content_area);

    if (search_action->search_engines)
        g_object_connect (search_action->search_engines,
            "signal-after::add-item",
            midori_search_action_dialog_engines_add_item_cb, search_action,
            "signal-after::remove-item",
            midori_search_action_dialog_engines_remove_item_cb, search_action,
            NULL);

    search_action->dialog = dialog;
    return dialog;
}
