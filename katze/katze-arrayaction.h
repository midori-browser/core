/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_ARRAY_ACTION_H__
#define __KATZE_ARRAY_ACTION_H__

#include "katze-array.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define KATZE_TYPE_ARRAY_ACTION \
    (katze_array_action_get_type ())
#define KATZE_ARRAY_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_ARRAY_ACTION, KatzeArrayAction))
#define KATZE_ARRAY_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass),  KATZE_TYPE_ARRAY_ACTION, KatzeArrayActionClass))
#define KATZE_IS_ARRAY_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_ARRAY_ACTION))
#define KATZE_IS_ARRAY_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass),  KATZE_TYPE_ARRAY_ACTION))
#define KATZE_ARRAY_ACTION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj),  KATZE_TYPE_ARRAY_ACTION, KatzeArrayActionClass))

typedef struct _KatzeArrayAction         KatzeArrayAction;
typedef struct _KatzeArrayActionClass    KatzeArrayActionClass;

GType
katze_array_action_get_type              (void) G_GNUC_CONST;

KatzeArray*
katze_array_action_get_array            (KatzeArrayAction* array_action);

void
katze_array_action_set_array            (KatzeArrayAction* array_action,
                                         KatzeArray*       array);

GtkToolItem*
katze_array_action_create_tool_item_for (KatzeArrayAction* array_action,
                                         KatzeItem*        item);

void
katze_array_action_generate_menu        (KatzeArrayAction* array_action,
                                         KatzeArray*       folder,
                                         GtkMenuShell*     menu,
                                         GtkWidget*        proxy);

G_END_DECLS

#endif /* __KATZE_ARRAY_ACTION_H__ */
