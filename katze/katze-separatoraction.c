/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze-separatoraction.h"

struct _KatzeSeparatorAction
{
    GtkAction parent_instance;
};

struct _KatzeSeparatorActionClass
{
    GtkActionClass parent_class;
};

G_DEFINE_TYPE (KatzeSeparatorAction, katze_separator_action, GTK_TYPE_ACTION);

static void
katze_separator_action_finalize (GObject* object);

static void
katze_separator_action_activate (GtkAction* object);

static GtkWidget*
katze_separator_action_create_tool_item (GtkAction* action);

static GtkWidget*
katze_separator_action_create_menu_item (GtkAction* action);

static void
katze_separator_action_connect_proxy (GtkAction* action,
                                      GtkWidget* proxy);

static void
katze_separator_action_disconnect_proxy (GtkAction* action,
                                         GtkWidget* proxy);

static void
katze_separator_action_class_init (KatzeSeparatorActionClass* class)
{
    GObjectClass* gobject_class;
    GtkActionClass* action_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = katze_separator_action_finalize;

    action_class = GTK_ACTION_CLASS (class);
    action_class->activate = katze_separator_action_activate;
    action_class->create_menu_item = katze_separator_action_create_menu_item;
    action_class->create_tool_item = katze_separator_action_create_tool_item;
    action_class->connect_proxy = katze_separator_action_connect_proxy;
    action_class->disconnect_proxy = katze_separator_action_disconnect_proxy;
}

static void
katze_separator_action_init (KatzeSeparatorAction* separator_action)
{
    /* Nothing to do. */
}

static void
katze_separator_action_finalize (GObject* object)
{
    G_OBJECT_CLASS (katze_separator_action_parent_class)->finalize (object);
}

static void
katze_separator_action_activate (GtkAction* action)
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

    if (GTK_ACTION_CLASS (katze_separator_action_parent_class)->activate)
        GTK_ACTION_CLASS (katze_separator_action_parent_class)->activate (action);
}

static GtkWidget*
katze_separator_action_create_menu_item (GtkAction* action)
{
    GtkWidget* menuitem;

    menuitem = gtk_separator_menu_item_new ();
    return menuitem;
}

static GtkWidget*
katze_separator_action_create_tool_item (GtkAction* action)
{
    GtkWidget* toolitem;

    toolitem = GTK_WIDGET (gtk_separator_tool_item_new ());
    return toolitem;
}

static void
katze_separator_action_connect_proxy (GtkAction* action,
                                      GtkWidget* proxy)
{
    GTK_ACTION_CLASS (katze_separator_action_parent_class)->connect_proxy (
        action, proxy);

    if (GTK_IS_TOOL_ITEM (proxy))
    {
    }
    else if (GTK_IS_MENU_ITEM (proxy))
    {
    }
}

static void
katze_separator_action_disconnect_proxy (GtkAction* action,
                                         GtkWidget* proxy)
{
    GTK_ACTION_CLASS (katze_separator_action_parent_class)->disconnect_proxy
        (action, proxy);
}
