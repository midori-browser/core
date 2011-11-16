/*
 Copyright (C) 2011 Peter Hatina <phatina@redhat.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_PANED_ACTION_H__
#define __MIDORI_PANED_ACTION_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_PANED_ACTION \
    (midori_paned_action_get_type ())
#define MIDORI_PANED_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_PANED_ACTION, MidoriPanedAction))
#define MIDORI_PANED_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass),  MIDORI_PANED_ACTION, MidoriPanedActionClass))
#define MIDORI_IS_PANED_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_PANED_ACTION))
#define MIDORI_IS_PANED_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass),  MIDORI_TYPE_PANED_ACTION))
#define MIDORI_PANED_ACTION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj),  MIDORI_TYPE_PANED_ACTION, MidoriPanedActionClass))

typedef struct _MidoriPanedAction                  MidoriPanedAction;
typedef struct _MidoriPanedActionClass             MidoriPanedActionClass;

GType
midori_paned_action_get_type                       (void) G_GNUC_CONST;

void
midori_paned_action_set_child1                     (MidoriPanedAction* paned_action,
                                                    GtkWidget* child1,
                                                    const gchar* name,
                                                    gboolean resize,
                                                    gboolean shrink);

void
midori_paned_action_set_child2                     (MidoriPanedAction* paned_action,
                                                    GtkWidget* child2,
                                                    const gchar* name,
                                                    gboolean resize,
                                                    gboolean shrink);

GtkWidget*
midori_paned_action_get_child1                     (MidoriPanedAction* paned_action);

GtkWidget*
midori_paned_action_get_child2                     (MidoriPanedAction* paned_action);

GtkWidget*
midori_paned_action_get_child_by_name              (MidoriPanedAction* paned_action,
                                                    const gchar* name);

const gchar*
midori_paned_action_get_child1_name                (MidoriPanedAction* paned_action);

const gchar*
midori_paned_action_get_child2_name                (MidoriPanedAction* paned_action);

G_END_DECLS

#endif // __MIDORI_PANED_ACTION_H__
