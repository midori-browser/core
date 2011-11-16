/*
 Copyright (C) 2011 Peter Hatina <phatina@redhat.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <string.h>
#include <katze/katze.h>
#include "midori-panedaction.h"

struct _MidoriPanedActionChild
{
    GtkWidget* widget;
    gchar* name;
    gboolean resize;
    gboolean shrink;
};

struct _MidoriPanedAction
{
    GtkAction parent_instance;
    GtkWidget* hpaned;
    GtkWidget* toolitem;
    struct _MidoriPanedActionChild child1;
    struct _MidoriPanedActionChild child2;
};

struct _MidoriPanedActionClass
{
    GtkActionClass parent_class;
};

G_DEFINE_TYPE (MidoriPanedAction, midori_paned_action, GTK_TYPE_ACTION);

static GtkWidget*
midori_paned_action_create_tool_item (GtkAction *action);

static void
midori_paned_action_finalize (GObject* object);

static void
midori_paned_action_init (MidoriPanedAction* paned_action)
{
    paned_action->hpaned = NULL;
    paned_action->toolitem = NULL;
    memset ((void*) &paned_action->child1, 0, sizeof (struct _MidoriPanedActionChild));
    memset ((void*) &paned_action->child2, 0, sizeof (struct _MidoriPanedActionChild));
}

static void
midori_paned_action_finalize (GObject* object)
{
    MidoriPanedAction* paned_action = MIDORI_PANED_ACTION (object);

    g_object_unref (G_OBJECT (paned_action->toolitem));
    g_object_unref (G_OBJECT (paned_action->hpaned));
    katze_assign (paned_action->child1.name, NULL);
    katze_assign (paned_action->child2.name, NULL);

    G_OBJECT_CLASS (midori_paned_action_parent_class)->finalize (object);
}

static void
midori_paned_action_class_init (MidoriPanedActionClass* class)
{
    GObjectClass* gobject_class;
    GtkActionClass* action_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_paned_action_finalize;

    action_class = GTK_ACTION_CLASS (class);
    action_class->create_tool_item = midori_paned_action_create_tool_item;
}

static GtkWidget*
midori_paned_action_create_tool_item (GtkAction* action)
{
    MidoriPanedAction* paned_action = MIDORI_PANED_ACTION (action);
    GtkWidget* alignment = gtk_alignment_new (0.0f, 0.5f, 1.0f, 0.1f);
    paned_action->hpaned = gtk_hpaned_new ();
    paned_action->toolitem = GTK_WIDGET (gtk_tool_item_new ());
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (paned_action->toolitem), TRUE);
    gtk_container_add (GTK_CONTAINER (paned_action->toolitem), alignment);
    gtk_container_add (GTK_CONTAINER (alignment), GTK_WIDGET (paned_action->hpaned));

    gtk_paned_pack1 (GTK_PANED (paned_action->hpaned),
        paned_action->child1.widget,
        paned_action->child1.resize,
        paned_action->child1.shrink);
    gtk_paned_pack2 (GTK_PANED (paned_action->hpaned),
        paned_action->child2.widget,
        paned_action->child2.resize,
        paned_action->child2.shrink);

    gtk_widget_show_all (GTK_WIDGET (paned_action->toolitem));
    return paned_action->toolitem;
}

/**
 * midori_paned_action_set_child1:
 * @paned_action: a #MidoriPanedAction
 * @child1: a #GtkWidget to be added into GtkHPaned container
 * @name: string name for the child2
 * @resize: should child1 expand when the MidoriPanedAction is resized
 * @shrink: can child1 be made smaller than its requisition
 **/
void
midori_paned_action_set_child1 (MidoriPanedAction* paned_action,
                                GtkWidget* child1,
                                const gchar* name,
                                gboolean resize,
                                gboolean shrink)
{
    g_return_if_fail (MIDORI_IS_PANED_ACTION (paned_action));

    katze_assign (paned_action->child1.name, g_strdup (name));
    paned_action->child1.widget = child1;
    paned_action->child1.resize = resize;
    paned_action->child1.shrink = shrink;
}

/**
 * midori_paned_action_set_child1:
 * @paned_action: a #MidoriPanedAction
 * @child2: a #GtkWidget to be added into GtkHPaned container
 * @name: string name for the child2
 * @resize: should child2 expand when the MidoriPanedAction is resized
 * @shrink: can child2 be made smaller than its requisition
 **/
void
midori_paned_action_set_child2 (MidoriPanedAction* paned_action,
                                GtkWidget* child2,
                                const gchar* name,
                                gboolean resize,
                                gboolean shrink)
{
    g_return_if_fail (MIDORI_IS_PANED_ACTION (paned_action));

    katze_assign (paned_action->child2.name, g_strdup (name));
    paned_action->child2.widget = child2;
    paned_action->child2.resize = resize;
    paned_action->child2.shrink = shrink;
}

/**
 * midori_paned_action_get_child1:
 * @paned_action: a #MidoriPanedAction
 *
 * returns the first child held in GtkHPaned container
 **/
GtkWidget*
midori_paned_action_get_child1 (MidoriPanedAction* paned_action)
{
    g_return_val_if_fail (MIDORI_IS_PANED_ACTION (paned_action), NULL);

    return paned_action->child1.widget;
}

/**
 * midori_paned_action_get_child1:
 * @paned_action: a #MidoriPanedAction
 *
 * returns the second child held in GtkHPaned container
 **/
GtkWidget*
midori_paned_action_get_child2 (MidoriPanedAction* paned_action)
{
    g_return_val_if_fail (MIDORI_IS_PANED_ACTION (paned_action), NULL);

    return paned_action->child2.widget;
}

/**
 * midori_paned_action_get_child1:
 * @paned_action: a #MidoriPanedAction
 * @name: string name for one of the children
 *
 * returns a child specified by its name
 **/
GtkWidget*
midori_paned_action_get_child_by_name (MidoriPanedAction* paned_action,
                                       const gchar* name)
{
    g_return_val_if_fail (MIDORI_IS_PANED_ACTION (paned_action), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    if (g_strcmp0 (name, paned_action->child1.name) == 0)
        return midori_paned_action_get_child1 (paned_action);
    else if (g_strcmp0 (name, paned_action->child2.name) == 0)
        return midori_paned_action_get_child2 (paned_action);

    return NULL;
}

/**
 * midori_paned_action_get_child1_name:
 * @paned_action a #MidoriPanedAction
 *
 * Returns: The name of the first child
 **/
const gchar*
midori_paned_action_get_child1_name (MidoriPanedAction* paned_action)
{
    g_return_val_if_fail (MIDORI_IS_PANED_ACTION (paned_action), NULL);

    return paned_action->child1.name;
}

/**
 * midori_paned_action_get_child2_name:
 * @paned_action a #MidoriPanedAction
 *
 * Returns: The name of the second child
 **/
const gchar*
midori_paned_action_get_child2_name (MidoriPanedAction* paned_action)
{
    g_return_val_if_fail (MIDORI_IS_PANED_ACTION (paned_action), NULL);

    return paned_action->child2.name;
}
