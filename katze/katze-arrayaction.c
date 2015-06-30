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

#include "midori/midori-platform.h"
#include "katze-utils.h"
#include "marshal.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#if HAVE_CONFIG_H
    #include "config.h"
#endif

struct _KatzeArrayAction
{
    GtkAction parent_instance;

    KatzeArray* array;
    gboolean reversed;
};

struct _KatzeArrayActionClass
{
    GtkActionClass parent_class;
};

G_DEFINE_TYPE (KatzeArrayAction, katze_array_action, GTK_TYPE_ACTION);

enum
{
    PROP_0,

    PROP_ARRAY,
    PROP_REVERSED
};

enum
{
    POPULATE_POPUP,
    POPULATE_FOLDER,
    ACTIVATE_ITEM,
    ACTIVATE_ITEM_NEW_TAB,
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
katze_array_action_proxy_clicked_cb (GtkWidget*        proxy,
                                     KatzeArrayAction* array_action);

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

    /**
     * KatzeArrayAction::populate-folder:
     * @array: the object on which the signal is emitted
     * @menu: the menu shell being opened
     * @folder: the folder being opened
     *
     * A submenu is going to be opened for @folder,
     * the provided @menu can be populated accordingly.
     *
     * Unlike "populate-popup" this signal is emitted for
     * the toplevel folder and all subfolders.
     *
     * Return value: %TRUE if the event was handled. If %FALSE is returned,
     *               the default "populate-popup" signal is emitted.
     *
     * Since: 0.2.8
     **/

    signals[POPULATE_FOLDER] = g_signal_new ("populate-folder",
                                       G_TYPE_FROM_CLASS (class),
                                       (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                       0,
                                       0,
                                       NULL,
                                       midori_cclosure_marshal_BOOLEAN__OBJECT_OBJECT,
                                       G_TYPE_BOOLEAN, 2,
                                       GTK_TYPE_MENU_SHELL, KATZE_TYPE_ITEM);

    /**
     * KatzeArrayAction::activate-item:
     * @array: the object on which the signal is emitted
     * @item: the item being activated
     * @event: (allow-none): the event that caused the activation
     *
     * An item was activated.
     **/
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
     * KatzeArrayAction::activate-item-new-tab:
     * @array: the object on which the signal is emitted
     * @item: the item being activated
     * @event: (allow-none): the event that caused the activation
     *
     * An item was activated and should be opened in a new tab.
     *
     * Since: 0.5.10
     **/
    signals[ACTIVATE_ITEM_NEW_TAB] = g_signal_new ("activate-item-new-tab",
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
     * @proxy: the %GtkWidget that caught the event
     * @item: the item being activated
     * @event: the mouse button pressed event
     *
     * The specified @button has been pressed (but not yet released) on the item.
     *
     * Return value: %TRUE if the event was handled.
     **/
    signals[ACTIVATE_ITEM_ALT] = g_signal_new ("activate-item-alt",
                                       G_TYPE_FROM_CLASS (class),
                                       (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                       0,
                                       0,
                                       NULL,
				       midori_cclosure_marshal_BOOLEAN__OBJECT_OBJECT_POINTER,
				       G_TYPE_BOOLEAN, 3,
                                       KATZE_TYPE_ITEM, GTK_TYPE_WIDGET, G_TYPE_POINTER);

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

    /**
     * KatzeArrayAction:reversed:
     *
     * Whether the array should be walked backwards when building menus.
     *
     * Since: 0.2.2
     **/
    g_object_class_install_property (gobject_class,
                                     PROP_REVERSED,
                                     g_param_spec_boolean (
                                     "reversed",
                                     "Reversed",
        "Whether the array should be walked backwards when building menus",
                                     FALSE,
                                     G_PARAM_READWRITE));
}

static void
katze_array_action_init (KatzeArrayAction* array_action)
{
    array_action->array = NULL;
    array_action->reversed = FALSE;
}

static void
katze_array_action_finalize (GObject* object)
{
    KatzeArrayAction* array_action = KATZE_ARRAY_ACTION (object);

    katze_object_assign (array_action->array, NULL);

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
    case PROP_REVERSED:
        array_action->reversed = g_value_get_boolean (value);
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
    case PROP_REVERSED:
        g_value_set_boolean (value, array_action->reversed);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
katze_array_action_activate (GtkAction* action)
{
    if (GTK_ACTION_CLASS (katze_array_action_parent_class)->activate)
        GTK_ACTION_CLASS (katze_array_action_parent_class)->activate (action);
}

static void
katze_array_action_activate_item (KatzeArrayAction* action,
                                  KatzeItem*        item)
{
    g_signal_emit (action, signals[ACTIVATE_ITEM], 0, item);
}

static void
katze_array_action_activate_item_new_tab (KatzeArrayAction* action,
                                          KatzeItem*        item)
{
    g_signal_emit (action, signals[ACTIVATE_ITEM_NEW_TAB], 0, item);
}

static gboolean
katze_array_action_activate_item_alt (KatzeArrayAction* action,
                                      KatzeItem*        item,
                                      GdkEventButton*   event,
                                      GtkWidget*        proxy)
{
    /* katze_array_action_activate_item emits the signal.
     * It can result from "clicked" event where the button event
     * is not provided.
     */

    gboolean handled = FALSE;

    g_assert (event);

    g_signal_emit (action, signals[ACTIVATE_ITEM_ALT], 0, item,
                       proxy, event, &handled);

    return handled;
}

static void
katze_array_action_menu_activate_cb  (GtkWidget*        proxy,
                                      KatzeArrayAction* array_action)
{
    KatzeItem* item = g_object_get_data (G_OBJECT (proxy), "KatzeItem");

    GdkEvent* event = gtk_get_current_event();

    if (event && MIDORI_EVENT_NEW_TAB (event))
        katze_array_action_activate_item_new_tab (array_action, item);
    else
        katze_array_action_activate_item (array_action, item);

    if (event)
        gdk_event_free (event);
}

static gboolean
katze_array_action_menu_item_button_press_cb (GtkWidget*        proxy,
                                              GdkEventButton*   event,
                                              KatzeArrayAction* array_action)
{
    KatzeItem* item = g_object_get_data (G_OBJECT (proxy), "KatzeItem");

    return katze_array_action_activate_item_alt (array_action, item, event, proxy);
}

static gboolean
katze_array_action_menu_button_press_cb (GtkWidget*        proxy,
                                         GdkEventButton*   event,
                                         KatzeArrayAction* array_action)
{
    /* Override the button-press handler for menus which would
       open a submenu to instead show a popup menu
    */

    return katze_array_action_menu_item_button_press_cb (gtk_get_event_widget ((GdkEvent *) event), event, array_action);
}

static gboolean
katze_array_action_tool_item_child_button_press_cb (GtkWidget*        proxy,
                                                    GdkEventButton*   event,
                                                    KatzeArrayAction* array_action)
{
    GtkWidget* toolitem = gtk_widget_get_parent (proxy);
    KatzeItem* item = g_object_get_data (G_OBJECT (toolitem), "KatzeItem");

    /* Left button clicks (e.g. for activation) should not be handled until button release */
    if (event->button == 1)
        return FALSE;

    return katze_array_action_activate_item_alt (array_action, item, event, proxy);
}

/* GtkToolItem does not emit the "clicked" event for middle-clicks, so handle them (but not
   regular or ctrl-clicks) manually via the button-release-event signal. */
static gboolean
katze_array_action_tool_item_child_button_release_cb (GtkWidget*        proxy,
                                                      GdkEventButton*   event,
                                                      KatzeArrayAction* array_action)
{
    GtkWidget* toolitem = gtk_widget_get_parent (proxy);
    KatzeItem* item = g_object_get_data (G_OBJECT (toolitem), "KatzeItem");

    /* We only need to handle middle-clicks here, since proper handling for 
     * ctrl-clicks, right-clicks, and left-clicks has been done elsewhere. */
    if (event && MIDORI_EVENT_NEW_TAB (event))
    {
        katze_array_action_activate_item_new_tab (array_action, item);
        return TRUE;
    }

    return FALSE;
}

static void
katze_array_action_menu_item_select_cb (GtkWidget*        proxy,
                                        KatzeArrayAction* array_action);

static GtkWidget*
katze_array_action_menu_item_new (KatzeArrayAction* array_action,
                                  KatzeItem* item)
{
    GtkWidget* menuitem = katze_image_menu_item_new_ellipsized (
        katze_item_get_name (item));
    GtkWidget* image = katze_item_get_image (item, menuitem);

    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
    gtk_image_menu_item_set_always_show_image (
        GTK_IMAGE_MENU_ITEM (menuitem), TRUE);

    g_object_set_data (G_OBJECT (menuitem), "KatzeItem", item);

    if (KATZE_ITEM_IS_FOLDER (item))
    {
        GtkWidget* submenu = gtk_menu_new ();
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
        /* Handle popup menus on menuitems which have open submenus */
        g_signal_connect (submenu, "button-press-event",
            G_CALLBACK (katze_array_action_menu_button_press_cb), array_action);
        g_signal_connect (menuitem, "select",
            G_CALLBACK (katze_array_action_menu_item_select_cb), array_action);
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (katze_array_action_menu_item_select_cb), array_action);
    }
    else
    {
        /* We need the 'activate' signal for actual item activation */
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (katze_array_action_menu_activate_cb), array_action);
    }

    /* Connect to button-press-event to catch right-clicks to open popup menus.
       Note that this is also necessary for menuitems which have submenus
       but whose submenus are not open at present! */
    g_signal_connect (menuitem, "button-press-event",
        G_CALLBACK (katze_array_action_menu_item_button_press_cb), array_action);

    return menuitem;
}

/**
 * katze_array_action_generate_menu:
 * @array_action: a #KatzeArrayAction
 * @folder: the folder to represent
 * @menu: the menu shell to populate
 * @proxy: the proxy, or alternatively a widget in the same window
 *
 * Generates menu items according to @folder, in the way they
 * appear in automatically generated action proxies.
 * The primary use is for implementing "populate-folder".
 *
 * It is worth noting that @folder can be any folder and can
 * be generated dynamically if needed.
 *
 * The @proxy widget must be a related widget on the same screen,
 * but doesn't have to be a proxy of the action.
 *
 * Since: 0.2.8
 **/
void
katze_array_action_generate_menu (KatzeArrayAction* array_action,
                                  KatzeArray*       array,
                                  GtkMenuShell*     menu,
                                  GtkWidget*        proxy)
{
    gint i;
    gint summand;
    KatzeItem* item;
    GtkWidget* menuitem;

    g_return_if_fail (KATZE_IS_ARRAY_ACTION (array_action));
    g_return_if_fail (KATZE_IS_ITEM (array));
    g_return_if_fail (GTK_IS_MENU_SHELL (menu));
    g_return_if_fail (GTK_IS_TOOL_ITEM (proxy)
        || GTK_IS_MENU_ITEM (proxy)
        || GTK_IS_WINDOW (proxy));

    if (!KATZE_IS_ARRAY (array))
        return;

    if (array_action->reversed)
    {
        i = katze_array_get_length (array);
        summand = -1;
    }
    else
    {
        i = -1;
        summand = +1;
    }
    while ((item = katze_array_get_nth_item (array, i += summand)))
    {
        /* FIXME: The menu item should reflect changes to the item  */
        if (KATZE_ITEM_IS_SEPARATOR (item))
        {
            menuitem = gtk_separator_menu_item_new ();
            gtk_widget_show (menuitem);
            gtk_menu_shell_append (menu, menuitem);
            continue;
        }

        menuitem = katze_array_action_menu_item_new (array_action, item);

        if (KATZE_ITEM_IS_FOLDER (item))
        {
            GtkWidget* submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menuitem));
            /* Make sure menu appears to contain items */
            gtk_menu_shell_append (GTK_MENU_SHELL (submenu),
                gtk_separator_menu_item_new ());
        }

        gtk_widget_show (menuitem);
        gtk_menu_shell_append (menu, menuitem);
    }
}

static gboolean
katze_array_action_menu_item_need_update (KatzeArrayAction* array_action,
                                          GtkWidget*        proxy)
{
    GtkWidget* menu;
    KatzeArray* array;
    gint last_array_update, last_proxy_update;
    gboolean handled;

    array = g_object_get_data (G_OBJECT (proxy), "KatzeItem");
    /* last-update is set on all arrays; consider public API */
    last_array_update = GPOINTER_TO_INT (
        g_object_get_data (G_OBJECT (array), "last-update"));
    last_proxy_update = GPOINTER_TO_INT (
        g_object_get_data (G_OBJECT (proxy), "last-update"));
    if (last_proxy_update > last_array_update)
        return FALSE;

    menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (proxy));
    gtk_container_foreach (GTK_CONTAINER (menu),
        (GtkCallback)(gtk_widget_destroy), NULL);
    katze_array_action_generate_menu (array_action, array, GTK_MENU_SHELL (menu), proxy);
    g_signal_emit (array_action, signals[POPULATE_FOLDER], 0, menu, array, &handled);
    g_object_set_data (G_OBJECT (proxy), "last-update",
        GINT_TO_POINTER (time (NULL)));
    return TRUE;
}

static void
katze_array_action_menu_item_select_cb (GtkWidget*        proxy,
                                        KatzeArrayAction* array_action)
{
    katze_array_action_menu_item_need_update (array_action, proxy);
}

static void
katze_array_action_menu_deactivate_cb (GtkWidget* menu,
                                       GtkWidget* proxy)
{
    void* array_action = g_object_get_data (G_OBJECT (menu), "KatzeArrayAction");
    g_signal_handlers_block_by_func (proxy,
        katze_array_action_proxy_clicked_cb, array_action);
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (proxy), FALSE);
    g_signal_handlers_unblock_by_func (proxy,
        katze_array_action_proxy_clicked_cb, array_action);
}

static void
katze_array_action_proxy_clicked_cb (GtkWidget*        proxy,
                                     KatzeArrayAction* array_action)
{
    GtkWidget* menu;
    KatzeArray* array;
    gboolean handled = FALSE;

    array = KATZE_ARRAY (g_object_get_data (G_OBJECT (proxy), "KatzeItem"));

    if (GTK_IS_MENU_ITEM (proxy))
    {
        if (katze_array_action_menu_item_need_update (array_action, proxy))
        {
            g_signal_emit (array_action, signals[POPULATE_FOLDER], 0,
                gtk_menu_item_get_submenu (GTK_MENU_ITEM (proxy)),
                array, &handled);
            if (!handled)
                g_signal_emit (array_action, signals[POPULATE_POPUP], 0,
                    gtk_menu_item_get_submenu (GTK_MENU_ITEM (proxy)));
        }
        return;
    }

    menu = gtk_menu_new ();
    gtk_menu_attach_to_widget (GTK_MENU (menu), proxy, NULL);

    if (!array)
        array = array_action->array;
    katze_array_action_generate_menu (array_action, array, GTK_MENU_SHELL (menu), proxy);

    g_signal_emit (array_action, signals[POPULATE_FOLDER], 0, menu, array, &handled);
    if (!handled)
    {
        /* populate-popup should only affect the main proxy */
        if (array == array_action->array)
            g_signal_emit (array_action, signals[POPULATE_POPUP], 0, menu);
    }

    katze_widget_popup (GTK_WIDGET (proxy), GTK_MENU (menu),
        NULL, KATZE_MENU_POSITION_LEFT);
    gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), TRUE);
    g_object_set_data (G_OBJECT (menu), "KatzeArrayAction", array_action);
    g_signal_connect (menu, "deactivate",
        G_CALLBACK (katze_array_action_menu_deactivate_cb), proxy);
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

    toolitem = GTK_WIDGET (gtk_toggle_tool_button_new ());
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
    const gchar* property;
    const gchar* title;
    const gchar* desc;
    GtkWidget* image;

    if (!G_IS_PARAM_SPEC_STRING (pspec))
        return;

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
    else if (KATZE_ITEM_IS_BOOKMARK (item) && !strcmp (property, "uri"))
    {
        image = katze_item_get_image (item, GTK_WIDGET (toolitem));
        gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (toolitem), image);
    }
    else if (!strcmp (property, "icon"))
    {
        image = katze_item_get_image (item, GTK_WIDGET (toolitem));
        gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (toolitem), image);
    }
}

static gboolean
katze_array_action_proxy_create_menu_proxy_cb (GtkWidget* proxy,
                                               KatzeItem* item)
{
    KatzeArrayAction* array_action;
    GtkWidget* menuitem;

    array_action = g_object_get_data (G_OBJECT (proxy), "KatzeArrayAction");
    menuitem = katze_array_action_menu_item_new (array_action, item);
    gtk_tool_item_set_proxy_menu_item (GTK_TOOL_ITEM (proxy),
        "katze-tool-item-menu", menuitem);
    return TRUE;
}

static void
katze_array_action_toolitem_destroy_cb (GtkToolItem* toolitem,
                                        KatzeItem*   item)
{
    g_signal_handlers_disconnect_by_func (item,
        G_CALLBACK (katze_array_action_item_notify_cb), toolitem);
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
 * Return value: (transfer full): a new tool item
 **/
GtkToolItem*
katze_array_action_create_tool_item_for (KatzeArrayAction* array_action,
                                         KatzeItem*        item)
{
    const gchar* title;
    const gchar* uri;
    const gchar* desc;
    GtkToolItem* toolitem;
    GtkWidget* image;
    GtkWidget* label;

    title = katze_item_get_name (item);
    uri = katze_item_get_uri (item);
    desc = katze_item_get_text (item);

    if (KATZE_ITEM_IS_SEPARATOR (item))
        return gtk_separator_tool_item_new ();

    if (KATZE_ITEM_IS_FOLDER (item))
        toolitem = gtk_toggle_tool_button_new ();
    else
        toolitem = gtk_tool_button_new (NULL, "");
    g_signal_connect (toolitem, "create-menu-proxy",
        G_CALLBACK (katze_array_action_proxy_create_menu_proxy_cb), item);
    image = katze_item_get_image (item, GTK_WIDGET (toolitem));
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

    g_object_set_data (G_OBJECT (toolitem), "KatzeItem", item);
    if (KATZE_ITEM_IS_FOLDER (item))
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (katze_array_action_proxy_clicked_cb), array_action);
    else if (KATZE_IS_ITEM (item))
    {
        /* Connect to "button-press-event" to handle right-clicks.
         * Tool items block the "button-press-event" but we can get it
         * when connecting it to the tool item's child widget
         */
        GtkWidget* child = gtk_bin_get_child (GTK_BIN (toolitem));
        g_signal_connect (child, "button-press-event",
            G_CALLBACK (katze_array_action_tool_item_child_button_press_cb), array_action);

        /* Connect to the "clicked" signal to handle normal keyboard and mouse
         * activations, checking the event to see if a new tab should be
         * opened. */
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (katze_array_action_menu_activate_cb), array_action);

        /* Connect to the "button-released-event" signal to handle middle clicks, since
         * GtkToolButton does not emit "clicked" for middle-clicks. */
        g_signal_connect (child, "button-release-event",
            G_CALLBACK (katze_array_action_tool_item_child_button_release_cb), array_action);
    }

    g_object_set_data (G_OBJECT (toolitem), "KatzeArrayAction", array_action);
    g_signal_connect (item, "notify",
        G_CALLBACK (katze_array_action_item_notify_cb), toolitem);
    g_signal_connect (toolitem, "destroy",
        G_CALLBACK (katze_array_action_toolitem_destroy_cb), item);
    return toolitem;
}

static void
katze_array_action_connect_proxy (GtkAction* action,
                                  GtkWidget* proxy)
{
    KatzeArrayAction* array_action = KATZE_ARRAY_ACTION (action);
    g_object_set_data (G_OBJECT (proxy), "KatzeItem", array_action->array);

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
        g_signal_connect (proxy, "select",
            G_CALLBACK (katze_array_action_proxy_clicked_cb), action);
        g_signal_connect (proxy, "activate",
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

/**
 * katze_array_action_get_array:
 * @array_action: a #KatzeArrayAction
 *
 * Obtain the array which backs the array action.
 *
 * Return value: (transfer none): the KatzeArray used by @array_action
 **/
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
    KatzeArray *old_array = NULL;

    g_return_if_fail (KATZE_IS_ARRAY_ACTION (array_action));
    g_return_if_fail (!array || katze_array_is_a (array, KATZE_TYPE_ITEM));

    /* FIXME: Disconnect old array */

    old_array = array_action->array;
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
        KatzeArray* item = g_object_get_data (G_OBJECT (proxies->data), "KatzeItem");

        if (item && (item == old_array))
            g_object_set_data (G_OBJECT (proxies->data), "KatzeItem", array);

        gtk_widget_set_sensitive (proxies->data, array != NULL);
    }
    while ((proxies = g_slist_next (proxies)));

    if (array)
        katze_array_update (KATZE_ARRAY (array));
}
