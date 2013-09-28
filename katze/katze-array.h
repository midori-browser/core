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
typedef struct _KatzeArrayPrivate                KatzeArrayPrivate;

struct _KatzeArray
{
    KatzeItem parent_instance;

    KatzeArrayPrivate* priv;
};

struct _KatzeArrayClass
{
    KatzeItemClass parent_class;

    /* Signals */
    void
    (*add_item)               (KatzeArray* array,
                               gpointer    item);
    void
    (*remove_item)            (KatzeArray* array,
                               gpointer    item);
    void
    (*move_item)              (KatzeArray* array,
                               gpointer    item,
                               gint        index);
    void
    (*clear)                  (KatzeArray* array);

    void
    (*update)                 (KatzeArray* array);
};

GType
katze_array_get_type               (void) G_GNUC_CONST;

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

gpointer
katze_array_find_uri               (KatzeArray*   array,
                                    const gchar*  uri);

guint
katze_array_get_length             (KatzeArray*   array);

void
katze_array_move_item              (KatzeArray*   array,
                                    gpointer      item,
                                    gint          position);

GList*
katze_array_get_items              (KatzeArray*   array);

GList*
katze_array_peek_items             (KatzeArray*   array);

extern GList* kalistglobal;
/* KATZE_ARRAY_FOREACH_ITEM:
 * @item: a #KatzeItem variable
 * @array: a #KatzeArray to loop through
 *
 * Loops through all items of the array. The macro can
 * be used like a for() loop.
 * If the array is modified during the loop, you must
 * use KATZE_ARRAY_FOREACH_ITEM_L instead.
 * */
#define KATZE_ARRAY_FOREACH_ITEM(kaitem, kaarray) \
    for (kalistglobal = katze_array_peek_items (kaarray), \
         kaitem = kalistglobal ? kalistglobal->data : NULL; \
         kalistglobal != NULL; \
         kalistglobal = g_list_next (kalistglobal), \
         kaitem = kalistglobal ? kalistglobal->data : NULL)

/* KATZE_ARRAY_FOREACH_ITEM_L:
 * @item: a #KatzeItem variable
 * @array: a #KatzeArray to loop through
 * @list: a #GList variable
 *
 * Loops through all items of the array. The list must be freed.
 *
 * Since: 0.3.0
 * */
#define KATZE_ARRAY_FOREACH_ITEM_L(kaitem, kaarray, kalist) \
    for (kalist = katze_array_get_items (kaarray), \
         kaitem = kalist ? kalist->data : NULL; \
         kalist != NULL; \
         kalist = g_list_next (kalist), \
         kaitem = kalist ? kalist->data : NULL)

void
katze_array_clear                  (KatzeArray*   array);

void
katze_array_update                 (KatzeArray*   array);

G_END_DECLS

#endif /* __KATZE_ARRAY_H__ */
