/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_ARRAY_H__
#define __KATZE_ARRAY_H__

#include <katze/katze-item.h>

G_BEGIN_DECLS

#define KATZE_TYPE_ARRAY \
    (katze_array_get_type ())
#define KATZE_ARRAY(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_ARRAY, KatzeArray))
#define KATZE_ARRAY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), KATZE_TYPE_ARRAY, KatzeArrayClass))
#define KATZE_IS_ARRAY(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_ARRAY))
#define KATZE_IS_ARRAY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), KATZE_TYPE_ARRAY))
#define KATZE_ARRAY_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), KATZE_TYPE_ARRAY, KatzeArrayClass))

typedef struct _KatzeArray                       KatzeArray;
typedef struct _KatzeArrayClass                  KatzeArrayClass;

GType
katze_array_get_type               (void);

KatzeArray*
katze_array_new                    (GType         type);

gboolean
katze_array_is_a                   (KatzeArray*   array,
                                    GType         is_a_type);

void
katze_array_add_item               (KatzeArray*   array,
                                    gpointer      item);

void
katze_array_remove_item            (KatzeArray*   array,
                                    gpointer      item);

gpointer
katze_array_get_nth_item           (KatzeArray*   array,
                                    guint         n);

gboolean
katze_array_is_empty               (KatzeArray*   array);

gint
katze_array_get_item_index         (KatzeArray*   array,
                                    gpointer      item);

gpointer
katze_array_find_token             (KatzeArray*   array,
                                    const gchar*  token);

guint
katze_array_get_length             (KatzeArray*   array);

void
katze_array_clear                  (KatzeArray*   array);

G_END_DECLS

#endif /* __KATZE_ARRAY_H__ */
