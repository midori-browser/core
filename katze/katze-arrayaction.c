/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze-arrayaction.h"

#include "katze-utils.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

struct _KatzeArrayAction
{
    GtkAction parent_instance;

    KatzeArray* array;
};

struct _KatzeArrayActionClass
{
    GtkActionClass parent_class;
};

G_DEFINE_TYPE (KatzeArrayAction, katze_array_action, GTK_TYPE_ACTION)

enum
{
    PROP_0,

    PROP_ARRAY
};

enum
{
    POPULATE_POPUP,
    ACTIVATE_ITEM,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
katze_array_action_finalize (GObject* object);

static void
katze_array_action_set_property (GObject*      object,
                                 guint         prop_id,
                                 const GValue* value,
                                 GParamSpec*   pspec);

static void
katze_array_action_get_property (GObject*    object,
                                 guint       prop_id,
                                 GValue*     value,
                                 GParamSpec* pspec);

static void
katze_array_action_activate (GtkAction* object);

static GtkWidget*
katze_array_action_create_tool_item (GtkAction* action);

static GtkWidget*
katze_array_action_create_menu_item (GtkAction* action);

static void
katze_array_action_connect_proxy (GtkAction* action,
                                  GtkWidget* proxy);

static void
katze_array_action_disconnect_proxy (GtkAction* action,
                                     GtkWidget* proxy);

static void
katze_array_action_class_init (KatzeArrayActionClass* class)
{
    GObjectClass* gobject_class;
    GtkActionClass* action_class;

    signals[POPULATE_POPUP] = g_signal_new ("populate-popup",
                                       G_TYPE_FROM_CLASS (class),
                                       (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                       0,
                                       0,
                                       NULL,
                                       g_cclosure_marshal_VOID__OBJECT,
                                       G_TYPE_NONE, 1,
                                       GTK_TYPE_MENU);

    signals[ACTIVATE_ITEM] = g_signal_new ("activate-item",
                                       G_TYPE_FROM_CLASS (class),
                                       (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                       0,
                                       0,
                                       NULL,
                                       g_cclosure_marshal_VOID__OBJECT,
                                       G_TYPE_NONE, 1,
                                       KATZE_TYPE_ITEM);

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = katze_array_action_finalize;
    gobject_class->set_property = katze_array_action_set_property;
    gobject_class->get_property = katze_array_action_get_property;

    action_class = GTK_ACTION_CLASS (class);
    action_class->activate = katze_array_action_activate;
    action_class->create_menu_item = katze_array_action_create_menu_item;
    action_class->create_tool_item = katze_array_action_create_tool_item;
    action_class->connect_proxy = katze_array_action_connect_proxy;
    action_class->disconnect_proxy = katze_array_action_disconnect_proxy;

    g_object_class_install_property (gobject_class,
                                     PROP_ARRAY,
                                     g_param_spec_object (
                                     "array",
                                     "Array",
                                     "The array the action represents",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE));
}

static void
katze_array_action_init (KatzeArrayAction* array_action)
{
    array_action->array = NULL;
}

static void
katze_array_action_finalize (GObject* object)
{
    KatzeArrayAction* array_action = KATZE_ARRAY_ACTION (object);

    if (array_action->array)
        g_object_unref (array_action->array);

    G_OBJECT_CLASS (katze_array_action_parent_class)->finalize (object);
}

static void
katze_array_action_set_property (GObject*      object,
                                 guint         prop_id,
                                 const GValue* value,
                                 GParamSpec*   pspec)
{
    KatzeArrayAction* array_action = KATZE_ARRAY_ACTION (object);

    switch (prop_id)
    {
    case PROP_ARRAY:
        katze_array_action_set_array (array_action, g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
katze_array_action_get_property (GObject*    object,
                                 guint       prop_id,
                                 GValue*     value,
                                 GParamSpec* pspec)
{
    KatzeArrayAction* array_action = KATZE_ARRAY_ACTION (object);

    switch (prop_id)
    {
    case PROP_ARRAY:
        g_value_set_object (value, array_action->array);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
katze_array_action_activate (GtkAction* action)
{
    GSList* proxies;

    proxies = gtk_action_get_proxies (action);
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {

    }
    while ((proxies = g_slist_next (proxies)));

    if (GTK_ACTION_CLASS (katze_array_action_parent_class)->activate)
        GTK_ACTION_CLASS (katze_array_action_parent_class)->activate (action);
}

static void
katze_array_action_menu_item_activate_cb (GtkWidget*        proxy,
                                          KatzeArrayAction* array_action)
{
    KatzeItem* item = g_object_get_data (G_OBJECT (proxy), "KatzeItem");
    g_signal_emit (array_action, signals[ACTIVATE_ITEM], 0, item);
}

static void
katze_array_action_proxy_clicked_cb (GtkWidget*        proxy,
                                     KatzeArrayAction* array_action)
{
    GtkWidget* menu;
    guint n, i;
    GtkWidget* menuitem;
    KatzeItem* item;
    GdkPixbuf* pixbuf;
    GtkWidget* icon;

    if (GTK_IS_MENU_ITEM (proxy))
    {
        if (!(menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (proxy))))
        {
            menu = gtk_menu_new ();
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (proxy), menu);
        }
        gtk_container_foreach (GTK_CONTAINER (menu), (GtkCallback)(gtk_widget_destroy), NULL);
    }
    else
        menu = gtk_menu_new ();

    n = katze_array_get_length (array_action->array);
    if (n > 0)
    for (i = 0; i < n; i++)
    {
        item = katze_array_get_nth_item (array_action->array, i);
        /* FIXME: The menu item should reflect changes to the item  */
        menuitem = katze_image_menu_item_new_ellipsized (
            katze_item_get_name (item));
        pixbuf = gtk_widget_render_icon (menuitem, GTK_STOCK_FILE,
                                         GTK_ICON_SIZE_MENU, NULL);
        icon = gtk_image_new_from_pixbuf (pixbuf);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
        g_object_unref (pixbuf);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        g_object_set_data (G_OBJECT (menuitem), "KatzeItem", item);
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (katze_array_action_menu_item_activate_cb), array_action);
        gtk_widget_show (menuitem);
    }
    else
    {
        menuitem = gtk_image_menu_item_new_with_label (_("Empty"));
        gtk_widget_set_sensitive (menuitem, FALSE);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        gtk_widget_show (menuitem);
    }

    g_signal_emit (array_action, signals[POPULATE_POPUP], 0, menu);

    if (!GTK_IS_MENU_ITEM (proxy))
        katze_widget_popup (GTK_WIDGET (proxy), GTK_MENU (menu),
                            NULL, KATZE_MENU_POSITION_LEFT);
}

static GtkWidget*
katze_array_action_create_menu_item (GtkAction* action)
{
    GtkWidget* menuitem;

    menuitem = gtk_menu_item_new ();
    return menuitem;
}

static GtkWidget*
katze_array_action_create_tool_item (GtkAction* action)
{
    GtkWidget* toolitem;

    toolitem = GTK_WIDGET (gtk_tool_button_new (NULL, NULL));
    return toolitem;
}

static void
katze_array_action_connect_proxy (GtkAction* action,
                                  GtkWidget* proxy)
{
    GTK_ACTION_CLASS (katze_array_action_parent_class)->connect_proxy (
        action, proxy);

    if (GTK_IS_TOOL_ITEM (proxy))
    {
        g_signal_connect (proxy, "clicked",
            G_CALLBACK (katze_array_action_proxy_clicked_cb), action);
    }
    else if (GTK_IS_MENU_ITEM (proxy))
    {
        /* FIXME: 'select' doesn't cover all ways of selection */
        g_signal_connect (proxy, "select",
            G_CALLBACK (katze_array_action_proxy_clicked_cb), action);
    }
}

static void
katze_array_action_disconnect_proxy (GtkAction* action,
                                     GtkWidget* proxy)
{
    g_signal_handlers_disconnect_by_func (proxy,
        G_CALLBACK (katze_array_action_proxy_clicked_cb), action);

    GTK_ACTION_CLASS (katze_array_action_parent_class)->disconnect_proxy
        (action, proxy);
}

KatzeArray*
katze_array_action_get_array (KatzeArrayAction* array_action)
{
    g_return_val_if_fail (KATZE_IS_ARRAY_ACTION (array_action), NULL);

    return array_action->array;
}

void
katze_array_action_set_array (KatzeArrayAction* array_action,
                              KatzeArray*       array)
{
    GSList* proxies;

    g_return_if_fail (KATZE_IS_ARRAY_ACTION (array_action));
    g_return_if_fail (!array || katze_array_is_a (array, KATZE_TYPE_ITEM));

    /* FIXME: Disconnect old array */

    if (array)
        g_object_ref (array);
    katze_object_assign (array_action->array, array);

    /* FIXME: Add and remove items dynamically */
    /*g_object_connect (array,
        "signal-after::add-item",
        katze_array_action_engines_add_item_cb, array_action,
        "signal-after::remove-item",
        katze_array_action_engines_remove_item_cb, array_action,
        NULL);*/

    g_object_notify (G_OBJECT (array_action), "array");

    proxies = gtk_action_get_proxies (GTK_ACTION (array_action));
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {

    }
    while ((proxies = g_slist_next (proxies)));
}
