/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009 Enrico Tröger <enrico.troeger@uvena.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze-arrayaction.h"

#include "katze-net.h"
#include "katze-utils.h"
#include "marshal.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

struct _KatzeArrayAction
{
    GtkAction parent_instance;

    KatzeArray* array;
    KatzeNet* net;
};

struct _KatzeArrayActionClass
{
    GtkActionClass parent_class;
};

G_DEFINE_TYPE (KatzeArrayAction, katze_array_action, GTK_TYPE_ACTION);

enum
{
    PROP_0,

    PROP_ARRAY
};

enum
{
    POPULATE_POPUP,
    ACTIVATE_ITEM,
    ACTIVATE_ITEM_ALT,
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

    /**
     * KatzeArrayAction::activate-item-alt:
     * @array: the object on which the signal is emitted
     * @item: the item being activated
     * @button: the mouse button pressed
     *
     * An item was clicked with a particular button. Use this if you need
     * to handle middle or right clicks specially.
     *
     * Return value: %TRUE if the event was handled. If %FALSE is returned,
     *               the default "activate-item" signal is emitted.
     *
     * Since: 0.1.7
     **/
    signals[ACTIVATE_ITEM_ALT] = g_signal_new ("activate-item-alt",
                                       G_TYPE_FROM_CLASS (class),
                                       (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                       0,
                                       0,
                                       NULL,
                                       katze_cclosure_marshal_BOOLEAN__OBJECT_UINT,
                                       G_TYPE_BOOLEAN, 2,
                                       KATZE_TYPE_ITEM, G_TYPE_UINT);

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
    array_action->net = katze_net_new ();
}

static void
katze_array_action_finalize (GObject* object)
{
    KatzeArrayAction* array_action = KATZE_ARRAY_ACTION (object);

    katze_object_assign (array_action->array, NULL);
    katze_object_assign (array_action->net, NULL);

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
katze_array_action_menu_activate_cb  (GtkWidget*        proxy,
                                      KatzeArrayAction* array_action)
{
    KatzeItem* item = g_object_get_data (G_OBJECT (proxy), "KatzeItem");
    g_signal_emit (array_action, signals[ACTIVATE_ITEM], 0, item);
}

static gboolean
katze_array_action_menu_button_press_cb (GtkWidget*        proxy,
                                         GdkEventButton*   event,
                                         KatzeArrayAction* array_action)
{
    KatzeItem* item = g_object_get_data (G_OBJECT (proxy), "KatzeItem");
    gboolean handled;

    g_signal_emit (array_action, signals[ACTIVATE_ITEM_ALT], 0, item,
        event->button, &handled);

    if (!handled)
        g_signal_emit (array_action, signals[ACTIVATE_ITEM], 0, item);

    /* we need to block the 'activate' handler which would be called
     * otherwise as well */
    g_signal_handlers_block_by_func (proxy,
        katze_array_action_menu_activate_cb, array_action);

    return TRUE;
}

static void
katze_array_action_menu_item_select_cb (GtkWidget*        proxy,
                                        KatzeArrayAction* array_action);

static void
katze_array_action_generate_menu (KatzeArrayAction* array_action,
                                  KatzeArray*       array,
                                  GtkWidget*        menu,
                                  GtkWidget*        proxy)
{
    guint i;
    KatzeItem* item;
    GtkWidget* menuitem;
    const gchar* icon_name;
    GdkPixbuf* icon;
    GtkWidget* image;
    GtkWidget* submenu;

    i = 0;
    while ((item = katze_array_get_nth_item (array, i++)))
    {
        /* FIXME: The menu item should reflect changes to the item  */
        if (!KATZE_IS_ARRAY (item) && !katze_item_get_uri (item))
        {
            menuitem = gtk_separator_menu_item_new ();
            gtk_widget_show (menuitem);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
            continue;
        }
        menuitem = katze_image_menu_item_new_ellipsized (
            katze_item_get_name (item));
        if ((icon_name = katze_item_get_icon (item)) && *icon_name)
            image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
        else
        {
            if (KATZE_IS_ARRAY (item))
                icon = gtk_widget_render_icon (menuitem,
                    GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU, NULL);
            else
                icon = katze_net_load_icon (array_action->net,
                    katze_item_get_uri (item), NULL, proxy, NULL);
            image = gtk_image_new_from_pixbuf (icon);
            g_object_unref (icon);
        }
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
        #if GTK_CHECK_VERSION (2, 16, 0)
        gtk_image_menu_item_set_always_show_image (
            GTK_IMAGE_MENU_ITEM (menuitem), TRUE);
        #endif
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        g_object_set_data (G_OBJECT (menuitem), "KatzeItem", item);
        if (KATZE_IS_ARRAY (item))
        {
            submenu = gtk_menu_new ();
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
            g_signal_connect (menuitem, "select",
                G_CALLBACK (katze_array_action_menu_item_select_cb), array_action);
        }
        else
        {
            g_signal_connect (menuitem, "button-press-event",
                G_CALLBACK (katze_array_action_menu_button_press_cb), array_action);
            /* we need the 'activate' signal as well for keyboard events */
            g_signal_connect (menuitem, "activate",
                G_CALLBACK (katze_array_action_menu_activate_cb), array_action);
        }
        gtk_widget_show (menuitem);
    }
    if (!i)
    {
        menuitem = gtk_image_menu_item_new_with_label (_("Empty"));
        gtk_widget_set_sensitive (menuitem, FALSE);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        gtk_widget_show (menuitem);
    }
}

static void
katze_array_action_menu_item_select_cb (GtkWidget*        proxy,
                                        KatzeArrayAction* array_action)
{
    GtkWidget* menu;
    KatzeArray* array;

    menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (proxy));
    gtk_container_foreach (GTK_CONTAINER (menu),
        (GtkCallback)(gtk_widget_destroy), NULL);

    array = g_object_get_data (G_OBJECT (proxy), "KatzeItem");
    katze_array_action_generate_menu (array_action, array, menu, proxy);
}

static void
katze_array_action_proxy_clicked_cb (GtkWidget*        proxy,
                                     KatzeArrayAction* array_action)
{
    GtkWidget* menu;
    KatzeArray* array;

    if (GTK_IS_MENU_ITEM (proxy))
    {
        g_object_set_data (G_OBJECT (proxy), "KatzeItem", array_action->array);
        katze_array_action_menu_item_select_cb (proxy, array_action);
        g_signal_emit (array_action, signals[POPULATE_POPUP], 0,
            gtk_menu_item_get_submenu (GTK_MENU_ITEM (proxy)));
        return;
    }

    menu = gtk_menu_new ();

    array = (KatzeArray*)g_object_get_data (G_OBJECT (proxy), "KatzeArray");
    if (!array)
        array = array_action->array;
    katze_array_action_generate_menu (array_action, array, menu, proxy);

    /* populate-popup should only affect the main proxy */
    if (array == array_action->array)
        g_signal_emit (array_action, signals[POPULATE_POPUP], 0, menu);

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

    toolitem = GTK_WIDGET (gtk_tool_button_new (NULL, ""));
    return toolitem;
}

static void
katze_array_action_label_notify_cb (GtkToolButton* item,
                                    GParamSpec*    pspec,
                                    GtkLabel*      label)
{
    const gchar* property;
    const gchar* text;

    if (!G_IS_PARAM_SPEC_STRING (pspec))
        return;

    property = g_param_spec_get_name (pspec);
    if (!strcmp (property, "label"))
    {
        text = gtk_tool_button_get_label (item);
        gtk_label_set_text (label, text);
    }
}

static void
katze_array_action_item_notify_cb (KatzeItem*   item,
                                   GParamSpec*  pspec,
                                   GtkToolItem* toolitem)
{
    KatzeArrayAction* array_action;
    const gchar* property;
    const gchar* title;
    const gchar* desc;
    GdkPixbuf* icon;
    GtkWidget* image;

    if (!G_IS_PARAM_SPEC_STRING (pspec))
        return;

    array_action = (KatzeArrayAction*)g_object_get_data (
        G_OBJECT (toolitem), "KatzeArrayAction");
    property = g_param_spec_get_name (pspec);
    if (!strcmp (property, "name"))
    {
        title = katze_item_get_name (item);
        if (title)
            gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), title);
        else
            gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem),
                katze_item_get_uri (item));
    }
    else if (!strcmp (property, "text"))
    {
        desc = katze_item_get_text (item);
        if (desc && *desc)
            gtk_tool_item_set_tooltip_text (toolitem, desc);
        else
            gtk_tool_item_set_tooltip_text (toolitem,
                katze_item_get_uri (item));
    }
    else if (!KATZE_IS_ARRAY (item) && !strcmp (property, "uri"))
    {
        icon = katze_net_load_icon (array_action->net, katze_item_get_uri (item),
            NULL, GTK_WIDGET (toolitem), NULL);
        image = gtk_image_new_from_pixbuf (icon);
        g_object_unref (icon);
        gtk_widget_show (image);
        gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (toolitem), image);
    }
    else if (!strcmp (property, "icon"))
    {
        image = gtk_image_new_from_icon_name (katze_item_get_icon (item),
                                              GTK_ICON_SIZE_MENU);
        gtk_widget_show (image);
        gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (toolitem), image);
    }
}

static gboolean
katze_array_action_proxy_create_menu_proxy_cb (GtkWidget* proxy,
                                               KatzeItem* item)
{
    KatzeArrayAction* array_action;
    GtkWidget* menuitem;
    const gchar* icon_name;
    GtkWidget* image;
    GdkPixbuf* icon;

    array_action = g_object_get_data (G_OBJECT (proxy), "KatzeArrayAction");
    menuitem = katze_image_menu_item_new_ellipsized (
        katze_item_get_name (item));
    if ((icon_name = katze_item_get_icon (item)) && *icon_name)
        image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
    else
    {
        if (KATZE_IS_ARRAY (item))
            icon = gtk_widget_render_icon (menuitem,
                GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU, NULL);
        else
            icon = katze_net_load_icon (array_action->net,
                katze_item_get_uri (item), NULL, proxy, NULL);
        image = gtk_image_new_from_pixbuf (icon);
        g_object_unref (icon);
    }
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
    #if GTK_CHECK_VERSION (2, 16, 0)
    gtk_image_menu_item_set_always_show_image (
        GTK_IMAGE_MENU_ITEM (menuitem), TRUE);
    #endif
    g_object_set_data (G_OBJECT (menuitem), "KatzeItem", item);
    if (KATZE_IS_ARRAY (item))
    {
        GtkWidget* submenu = gtk_menu_new ();
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
        g_signal_connect (menuitem, "select",
            G_CALLBACK (katze_array_action_menu_item_select_cb), array_action);
    }
    else
    {
        g_signal_connect (menuitem, "button-press-event",
            G_CALLBACK (katze_array_action_menu_button_press_cb), array_action);
        /* we need the 'activate' signal as well for keyboard events */
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (katze_array_action_menu_activate_cb), array_action);
    }
    gtk_tool_item_set_proxy_menu_item (GTK_TOOL_ITEM (proxy),
        "katze-tool-item-menu", menuitem);
    return TRUE;
}

/**
 * katze_array_action_create_tool_item_for:
 * @array_action: a #KatzeArrayAction
 * @item: a #KatzeItem
 *
 * Creates a tool item for a particular @item, that also
 * reflects changes to its properties. In the case of
 * an array, the item will create a popup menu with
 * the contained items.
 *
 * Note that the label is reasonably ellipsized for you,
 * much like katze_image_menu_item_new_ellipsized().
 *
 * Return value: a new tool item
 **/
GtkToolItem*
katze_array_action_create_tool_item_for (KatzeArrayAction* array_action,
                                         KatzeItem*        item)
{
    const gchar* title;
    const gchar* uri;
    const gchar* desc;
    GtkToolItem* toolitem;
    GdkPixbuf* icon;
    GtkWidget* image;
    GtkWidget* label;

    title = katze_item_get_name (item);
    uri = katze_item_get_uri (item);
    desc = katze_item_get_text (item);

    if (!KATZE_IS_ARRAY (item) && !uri)
        return gtk_separator_tool_item_new ();

    toolitem = gtk_tool_button_new (NULL, "");
    g_signal_connect (toolitem, "create-menu-proxy",
        G_CALLBACK (katze_array_action_proxy_create_menu_proxy_cb), item);
    if (KATZE_IS_ARRAY (item))
        icon = gtk_widget_render_icon (GTK_WIDGET (toolitem),
            GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU, NULL);
    else
        icon = katze_net_load_icon (array_action->net, uri,
            NULL, GTK_WIDGET (toolitem), NULL);
    image = gtk_image_new_from_pixbuf (icon);
    g_object_unref (icon);
    gtk_widget_show (image);
    gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (toolitem), image);
    label = gtk_label_new (NULL);
    /* FIXME: Should text direction be respected here? */
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 25);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_show (label);
    gtk_tool_button_set_label_widget (GTK_TOOL_BUTTON (toolitem), label);
    /* GtkToolItem won't update our custom label, so we
       apply a little bit of 'magic' to fix that.  */
    g_signal_connect (toolitem, "notify",
        G_CALLBACK (katze_array_action_label_notify_cb), label);
    if (title)
        gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), title);
    else
        gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), uri);
    gtk_tool_item_set_is_important (toolitem, TRUE);
    if (desc && *desc)
        gtk_tool_item_set_tooltip_text (toolitem, desc);
    else
        gtk_tool_item_set_tooltip_text (toolitem, uri);
    if (KATZE_IS_ARRAY (item))
    {
        g_object_set_data (G_OBJECT (toolitem), "KatzeArray", item);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (katze_array_action_proxy_clicked_cb), array_action);
    }

    g_object_set_data (G_OBJECT (toolitem), "KatzeArrayAction", array_action);
    g_signal_connect (item, "notify",
        G_CALLBACK (katze_array_action_item_notify_cb), toolitem);
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
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (proxy), gtk_menu_new ());
        /* FIXME: 'select' doesn't cover all ways of selection */
        g_signal_connect (proxy, "select",
            G_CALLBACK (katze_array_action_proxy_clicked_cb), action);
    }
    gtk_widget_set_sensitive (proxy, KATZE_ARRAY_ACTION (action)->array != NULL);
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
    {
        gtk_widget_set_sensitive (proxies->data, array != NULL);
    }
    while ((proxies = g_slist_next (proxies)));
}
